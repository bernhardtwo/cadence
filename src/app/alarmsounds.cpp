#include "alarmsounds.hpp"

#include <QUrl>

using namespace Qt::StringLiterals;

AlarmSounds::AlarmSounds(QObject* parent) : QObject(parent) {
    chime_.setSource(QUrl(u"qrc:/sounds/chime.wav"_s));
    soft_.setSource(QUrl(u"qrc:/sounds/soft.wav"_s));
    chime_.setVolume(0.8);
    soft_.setVolume(0.6);
}

void AlarmSounds::play(DayController::Alarm kind) {
    switch (kind) {
    case DayController::BlockStart:
    case DayController::PomodoroFocusEnd:
    case DayController::DayNoLongerFits:
    case DayController::UnconfirmedPending:
        chime_.play();
        break;
    case DayController::BlockEndingSoon:
    case DayController::SoftReminder:
    case DayController::BreakEnd:
        soft_.play();
        break;
    }
}
