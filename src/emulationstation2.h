/***************************************************************************
 *            emulationstation2.h
 ****************************************************************************/

#ifndef EMULATIONSTATION2_H
#define EMULATIONSTATION2_H

#include "emulationstation.h"

class EmulationStation2 : public EmulationStation {
    Q_OBJECT

public:
    EmulationStation2();

    void assembleList(QString &finalOutput,
                      QList<GameEntry> &gameEntries) override;
    bool canSkip() override;
    bool loadOldGameList(const QString &) override;
};

#endif // EMULATIONSTATION2_H
