#include "emulationstation2.h"

#include "config.h"
#include "strtools.h"

#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QStringBuilder>

EmulationStation2::EmulationStation2() {}

bool EmulationStation2::canSkip() { return false; }

bool EmulationStation2::loadOldGameList(const QString &) { return false; }

void EmulationStation2::assembleList(QString &finalOutput,
                                     QList<GameEntry> &gameEntries) {
    auto boolElem = [this](const QString &name, bool val) {
        return QString(INDENT % INDENT % "<%1>%2</%1>")
            .arg(name, val ? "true" : "false");
    };

    auto textElem = [this](const QString &name, const QString &value,
                           bool isPath = false) {
        QString v = value;
        if (isPath && !v.isEmpty()) {
            if (config->relativePaths) {
                v = "./" + Config::lexicallyRelativePath(config->inputFolder, v);
            } else {
                v = Config::lexicallyNormalPath(v);
            }
        }
        v = StrTools::xmlEscape(v);
        if (v.isEmpty()) {
            return QString(INDENT % INDENT % "<%1/>").arg(name);
        }
        return QString(INDENT % INDENT % "<%1>%2</%1>").arg(name, v);
    };

    auto releaseDateValue = [](QString v) {
        if (v.isEmpty()) {
            return QString();
        }
        if (!v.contains("T")) {
            v.append("T000000");
        }
        return v;
    };

    auto listElems = [this](const QString &container, const QString &entry,
                            QStringList values) {
        QStringList out;
        values.removeAll("");
        values.removeDuplicates();
        if (values.isEmpty()) {
            return out;
        }
        out.append(QString(INDENT % INDENT % INDENT % INDENT % "<%1>")
                       .arg(container));
        for (const auto &v : values) {
            out.append(QString(INDENT % INDENT % INDENT % INDENT % INDENT %
                               "<%1>%2</%1>")
                           .arg(entry, StrTools::xmlEscape(v)));
        }
        out.append(QString(INDENT % INDENT % INDENT % INDENT % "</%1>")
                       .arg(container));
        return out;
    };

    auto mergeValue = [](const QList<GameEntry> &group,
                         QString GameEntry::*member) {
        for (const auto &entry : group) {
            if (!(entry.*member).isEmpty()) {
                return entry.*member;
            }
        }
        return QString();
    };

    auto normalizedRegionCode = [](QString region) {
        QString r = region.trimmed().toLower();
        r.remove(QRegularExpression("[^a-z0-9]"));

        static const QHash<QString, QString> regionAlias = {
            {"us", "us"},         {"usa", "us"},
            {"unitedstates", "us"},
            {"eu", "eu"},         {"europe", "eu"},
            {"wor", "wor"},       {"world", "wor"},
            {"jp", "jp"},         {"jpn", "jp"},
            {"japan", "jp"},      {"br", "br"},
            {"brazil", "br"},     {"kr", "kr"},
            {"korea", "kr"},      {"tw", "tw"},
            {"taiwan", "tw"},     {"fr", "fr"},
            {"france", "fr"},     {"de", "de"},
            {"germany", "de"},    {"it", "it"},
            {"italy", "it"},      {"sp", "sp"},
            {"es", "sp"},         {"spain", "sp"},
            {"cn", "cn"},         {"china", "cn"},
            {"au", "au"},         {"australia", "au"},
            {"se", "se"},         {"sweden", "se"},
            {"ca", "ca"},         {"canada", "ca"},
            {"nl", "nl"},         {"netherlands", "nl"},
            {"dk", "dk"},         {"denmark", "dk"},
            {"asi", "asi"},       {"asia", "asi"},
            {"uk", "uk"},         {"unitedkingdom", "uk"},
            {"ame", "ame"},       {"cus", "cus"},
            {"gr", "gr"},         {"no", "no"},
            {"nz", "nz"},         {"pl", "pl"},
            {"ru", "ru"},         {"ss", "ss"}};

        return regionAlias.value(r, r);
    };

    auto regionCodesFromText = [&normalizedRegionCode](const QString &text) {
        QString t = text.toLower();
        QStringList out;

        const QList<QPair<QString, QString>> patterns = {
            {"europe", "eu"}, {"\\(e\\)", "eu"},
            {"usa", "us"},    {"\\(u\\)", "us"},
            {"world", "wor"}, {"japan", "jp"},
            {"\\(j\\)", "jp"},
        };

        for (const auto &p : patterns) {
            if (QRegularExpression(p.first).match(t).hasMatch()) {
                QString c = normalizedRegionCode(p.second);
                if (!c.isEmpty() && !out.contains(c)) {
                    out.append(c);
                }
            }
        }

        return out;
    };

    auto regionRankForEntry =
        [this, &normalizedRegionCode,
         &regionCodesFromText](const GameEntry &entry) -> int {
        QStringList codes;

        for (const auto &region : entry.romRegions) {
            QString c = normalizedRegionCode(region);
            if (!c.isEmpty() && !codes.contains(c)) {
                codes.append(c);
            }
        }

        if (codes.isEmpty()) {
            codes.append(regionCodesFromText(entry.path));
            codes.append(regionCodesFromText(entry.baseName));
            codes.append(regionCodesFromText(entry.title));
            codes.removeDuplicates();
        }

        int bestRank = 1000000;
        for (const auto &code : codes) {
            const int idx = config->regionPrios.indexOf(code);
            if (idx >= 0 && idx < bestRank) {
                bestRank = idx;
            }
        }

        return bestRank;
    };

    QStringList orderedKeys;
    QHash<QString, QList<GameEntry>> grouped;

    for (auto &entry : gameEntries) {
        if (entry.isFolder) {
            continue;
        }
        QString key;
        if (!entry.canonicalId.isEmpty()) {
            key = "id:" % entry.canonicalId;
        } else {
            key = "title:" % entry.title.toLower().trimmed();
        }
        if (!grouped.contains(key)) {
            orderedKeys.append(key);
        }
        grouped[key].append(entry);
    }

    finalOutput.append("<?xml version=\"1.0\"?>\n");
    finalOutput.append("<gameList>\n");

    int dots = -1;
    int dotMod = 1 + orderedKeys.length() * 0.1;

    for (const auto &key : orderedKeys) {
        if (++dots % dotMod == 0) {
            printf(".");
            fflush(stdout);
        }

        QList<GameEntry> group = grouped.value(key);
        if (group.isEmpty()) {
            continue;
        }

        int preferredIndex = 0;
        int bestRank = regionRankForEntry(group.at(0));
        for (int i = 1; i < group.length(); ++i) {
            const int rank = regionRankForEntry(group.at(i));
            if (rank < bestRank) {
                bestRank = rank;
                preferredIndex = i;
            }
        }

        QList<GameEntry> orderedGroup;
        orderedGroup.append(group.at(preferredIndex));
        for (int i = 0; i < group.length(); ++i) {
            if (i != preferredIndex) {
                orderedGroup.append(group.at(i));
            }
        }

        const int preferredOrderedIndex = 0;
        const GameEntry &preferred = orderedGroup.at(preferredOrderedIndex);

        QString gameName = preferred.title;
        if (gameName.isEmpty()) {
            gameName = mergeValue(group, &GameEntry::title);
        }
        QString desc = mergeValue(orderedGroup, &GameEntry::description);
        QString developer = mergeValue(orderedGroup, &GameEntry::developer);
        QString publisher = mergeValue(orderedGroup, &GameEntry::publisher);
        QString genre = mergeValue(orderedGroup, &GameEntry::tags);
        QString players = mergeValue(orderedGroup, &GameEntry::players);
        QString rating = mergeValue(orderedGroup, &GameEntry::rating);

        QString kidgame = preferred.getEsExtra(GameEntry::getTag(GameEntry::Elem::AGES));
        if (kidgame.isEmpty()) {
            bool ok = false;
            int ages = preferred.ages.toInt(&ok);
            if (ok && ages >= 1 && ages <= 10) {
                kidgame = "true";
            }
        }
        if (kidgame != "true") {
            kidgame = "false";
        }

        QString image = mergeValue(orderedGroup, &GameEntry::screenshotFile);
        QString marquee = mergeValue(orderedGroup, &GameEntry::marqueeFile);
        QString video = mergeValue(orderedGroup, &GameEntry::videoFile);
        QString thumbnail = mergeValue(orderedGroup, &GameEntry::coverFile);

        QString playcount = preferred.getEsExtra("playcount");
        QString lastplayed = preferred.getEsExtra("lastplayed");

        QStringList lines;
        lines.append(QString(INDENT % "<game>"));
        lines.append(textElem("name", gameName));
        lines.append(textElem("desc", desc));
        lines.append(textElem("developer", developer));
        lines.append(textElem("publisher", publisher));
        lines.append(textElem("genre", genre));
        lines.append(textElem("players", players));
        lines.append(textElem("rating", rating));
        lines.append(boolElem("kidgame", kidgame == "true"));

        lines.append(textElem("image", image, true));
        lines.append(textElem("marquee", marquee, true));
        lines.append(textElem("video", video, true));
        lines.append(textElem("thumbnail", thumbnail, true));

        lines.append(textElem("playcount", playcount));
        lines.append(textElem("lastplayed", lastplayed));

        lines.append(QString(INDENT % INDENT % "<roms>"));

        for (int i = 0; i < orderedGroup.length(); ++i) {
            const auto &rom = orderedGroup.at(i);
            const bool preferredRom = i == preferredOrderedIndex;

            QString romName = rom.romName;
            if (romName.isEmpty()) {
                romName = rom.baseName;
            }
            if (romName.isEmpty()) {
                romName = QFileInfo(rom.path).completeBaseName();
            }

            QString romReleaseDate = releaseDateValue(
                rom.romReleaseDate.isEmpty() ? rom.releaseDate : rom.romReleaseDate);

            QString romImage = (rom.screenshotFile == image) ? "" : rom.screenshotFile;
            QString romMarquee = (rom.marqueeFile == marquee) ? "" : rom.marqueeFile;
            QString romVideo = (rom.videoFile == video) ? "" : rom.videoFile;
            QString romThumbnail = (rom.coverFile == thumbnail) ? "" : rom.coverFile;

            lines.append(QString(INDENT % INDENT % INDENT %
                                 "<rom preferred=\"%1\">")
                             .arg(preferredRom ? "true" : "false"));
            lines.append(textElem("path", rom.path, true)
                             .replace(INDENT % INDENT, INDENT % INDENT % INDENT));
            lines.append(textElem("romname", romName)
                             .replace(INDENT % INDENT, INDENT % INDENT % INDENT));

            QStringList regions = listElems("regions", "region", rom.romRegions);
            if (regions.isEmpty()) {
                lines.append(QString(INDENT % INDENT % INDENT % INDENT % "<regions/>"));
            } else {
                for (const auto &r : regions) {
                    lines.append(r);
                }
            }

            QStringList languages =
                listElems("languages", "language", rom.romLanguages);
            if (languages.isEmpty()) {
                lines.append(QString(INDENT % INDENT % INDENT % INDENT % "<languages/>"));
            } else {
                for (const auto &l : languages) {
                    lines.append(l);
                }
            }

            lines.append(textElem("releasedate", romReleaseDate)
                             .replace(INDENT % INDENT, INDENT % INDENT % INDENT));
            lines.append(textElem("revision", rom.romRevision)
                             .replace(INDENT % INDENT, INDENT % INDENT % INDENT));
            lines.append(textElem("image", romImage, true)
                             .replace(INDENT % INDENT, INDENT % INDENT % INDENT));
            lines.append(textElem("video", romVideo, true)
                             .replace(INDENT % INDENT, INDENT % INDENT % INDENT));
            lines.append(textElem("thumbnail", romThumbnail, true)
                             .replace(INDENT % INDENT, INDENT % INDENT % INDENT));
            lines.append(textElem("marquee", romMarquee, true)
                             .replace(INDENT % INDENT, INDENT % INDENT % INDENT));
            lines.append(QString(INDENT % INDENT % INDENT % "</rom>"));
        }

        lines.append(QString(INDENT % INDENT % "</roms>"));
        lines.append(QString(INDENT % "</game>"));

        finalOutput.append(lines.join("\n") % "\n");
    }

    finalOutput.append("</gameList>\n");
}
