#pragma once

#include "daycontroller.hpp"

#include <QObject>
#include <QSoundEffect>

// Two bundled tones: a chime for things that need the user now and a soft note for the rest.
class AlarmSounds : public QObject {
    Q_OBJECT

public:
    explicit AlarmSounds(QObject* parent = nullptr);

    void play(DayController::Alarm kind);

private:
    QSoundEffect chime_;
    QSoundEffect soft_;
};
