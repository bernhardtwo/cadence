#pragma once

#include "daycontroller.hpp"

#include <cadence/core/settings_json.hpp>

#include <QObject>
#include <QSoundEffect>

// Two bundled tones: a chime for things that need the user now and a soft note for the rest.
class AlarmSounds : public QObject {
    Q_OBJECT

public:
    explicit AlarmSounds(QObject* parent = nullptr);

    void play(DayController::Alarm kind);
    void setMode(cadence::core::SoundMode mode) { mode_ = mode; }

private:
    QSoundEffect chime_;
    QSoundEffect soft_;
    cadence::core::SoundMode mode_ = cadence::core::SoundMode::Default;
};
