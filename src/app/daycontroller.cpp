#include "daycontroller.hpp"

#include <QDate>
#include <QVariantMap>
#include <QtLogging>

#include <algorithm>
#include <variant>

using namespace Qt::StringLiterals;
using namespace cadence::core;

namespace {

DayController* s_instance = nullptr;

QString stateName(BlockState state) {
    switch (state) {
    case BlockState::Done:
        return u"Done"_s;
    case BlockState::Active:
        return u"Active"_s;
    case BlockState::Upcoming:
        return u"Upcoming"_s;
    case BlockState::Skipped:
        return u"Skipped"_s;
    case BlockState::DoesNotFit:
        return u"Does not fit"_s;
    case BlockState::Unconfirmed:
        return u"Unconfirmed"_s;
    }
    return {};
}

QString kindName(BlockKind kind) {
    switch (kind) {
    case BlockKind::Anchored:
        return u"Anchored"_s;
    case BlockKind::Flexible:
        return u"Flexible"_s;
    case BlockKind::Soft:
        return u"Soft"_s;
    }
    return {};
}

QString phaseName(PomodoroState state) {
    switch (state) {
    case PomodoroState::Idle:
        return u"Idle"_s;
    case PomodoroState::Focus:
        return u"Focus"_s;
    case PomodoroState::ShortBreak:
        return u"Short break"_s;
    case PomodoroState::LongBreak:
        return u"Long break"_s;
    case PomodoroState::Paused:
        return u"Paused"_s;
    case PomodoroState::Completed:
        return u"Completed"_s;
    }
    return {};
}

DayController::Alarm toAlarm(AlarmKind kind) {
    switch (kind) {
    case AlarmKind::BlockStart:
        return DayController::BlockStart;
    case AlarmKind::BlockEndingSoon:
        return DayController::BlockEndingSoon;
    case AlarmKind::SoftReminder:
        return DayController::SoftReminder;
    case AlarmKind::PomodoroFocusEnd:
        return DayController::PomodoroFocusEnd;
    case AlarmKind::BreakEnd:
        return DayController::BreakEnd;
    case AlarmKind::DayNoLongerFits:
        return DayController::DayNoLongerFits;
    case AlarmKind::UnconfirmedPending:
        return DayController::UnconfirmedPending;
    }
    return DayController::BlockStart;
}

QString timeText(Minutes minuteOfDay) {
    return QString::fromStdString(formatTimeOfDay(minuteOfDay));
}

} // namespace

DayController::DayController(std::unique_ptr<IClock> clock, std::unique_ptr<ProgressStore> store,
                             std::optional<TemplateDocument> document, QString templateError, QObject* parent)
    : QObject(parent), clock_(std::move(clock)), store_(std::move(store)), document_(std::move(document)),
      templateError_(std::move(templateError)) {
    timer_.setInterval(1000);
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &DayController::evaluateNow);
    evaluate(clock_->now());
}

DayController::~DayController() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

DayController* DayController::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine) {
    Q_UNUSED(qmlEngine);
    Q_ASSERT(s_instance != nullptr);
    Q_ASSERT(jsEngine->thread() == s_instance->thread());
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void DayController::setInstance(DayController* instance) {
    s_instance = instance;
}

void DayController::startTicking() {
    timer_.start();
}

void DayController::evaluateNow() {
    evaluate(clock_->now());
}

void DayController::evaluate(Instant now) {
    now_ = now;
    const TimePoint point = toTimePoint(now);
    if (date_ != point.date) {
        loadDay(point.date);
    }

    PomodoroEvents events;
    if (session_) {
        events = session_->tick(now);
        applyPomodoroEvents(events, now);
    }

    if (day_) {
        plan_ = plan(*day_, progress_, point);
        const std::vector<cadence::core::Alarm> alarms = scheduler_.evaluate(*day_, plan_, progress_, events, now);
        current_ = pickCurrent();
        for (const cadence::core::Alarm& alarm : alarms) {
            raise(alarm);
        }
    } else {
        plan_ = DayPlan{};
        current_.reset();
    }
    emit changed();
}

void DayController::loadDay(Date date) {
    if (date_ && date_ != date) {
        persist();
    }
    date_ = date;
    session_.reset();
    scheduler_.reset();
    progress_ = DayProgress{};
    day_.reset();

    if (document_) {
        const Weekday weekday{std::chrono::local_days{date}};
        if (const auto& day = document_->week.day(weekday)) {
            day_ = *day;
        }
    }
    if (day_) {
        if (auto stored = store_->load(date)) {
            progress_ = std::move(*stored);
        }
        storageError_ = store_->lastError();
        if (!storageError_.isEmpty()) {
            qWarning("%s", qPrintable(storageError_));
        }
    }
}

void DayController::persist() {
    if (!date_ || !day_) {
        return;
    }
    if (!store_->save(*date_, progress_)) {
        storageError_ = store_->lastError();
        qWarning("%s", qPrintable(storageError_));
    } else {
        storageError_.clear();
    }
}

void DayController::applyPomodoroEvents(const PomodoroEvents& events, Instant now) {
    for (const PomodoroEvent& event : events) {
        if (const auto* prompt = std::get_if<PushupPrompt>(&event)) {
            emit pushupPrompt(static_cast<int>(sessionBlock_), prompt->setIndex);
        } else if (std::holds_alternative<SessionCompleted>(event)) {
            // The block's pomodoros are done, so the block is done.
            BlockProgress& prog = progress_.at(sessionBlock_);
            if (prog.actualStart && !prog.actualEnd) {
                closeOpenPause(toTimePoint(now).minuteOfDay);
                prog.actualEnd = toTimePoint(now).minuteOfDay;
                persist();
            }
        }
    }
}

void DayController::raise(const cadence::core::Alarm& alarm) {
    const int index = alarm.blockIndex ? static_cast<int>(*alarm.blockIndex) : -1;
    const QString name = index >= 0 ? blockName(index) : QString();
    QString title;
    QString message;
    switch (alarm.kind) {
    case AlarmKind::BlockStart:
        title = name;
        message = u"Block starts now"_s;
        break;
    case AlarmKind::BlockEndingSoon:
        title = name;
        message = u"Ends in %1 minutes"_s.arg(scheduler_.policy().endingSoonLead.count());
        break;
    case AlarmKind::SoftReminder:
        title = name;
        message = u"Whenever you are ready"_s;
        break;
    case AlarmKind::PomodoroFocusEnd:
        title = u"Focus done"_s;
        message = session_ && session_->state() != PomodoroState::Completed ? u"Take a break"_s
                                                                            : u"Last pomodoro finished"_s;
        break;
    case AlarmKind::BreakEnd:
        title = u"Break over"_s;
        message = u"Back to focus"_s;
        break;
    case AlarmKind::DayNoLongerFits:
        title = u"The day no longer fits"_s;
        message = u"Some blocks end after the cutoff"_s;
        break;
    case AlarmKind::UnconfirmedPending:
        title = name;
        message = u"Did you do this?"_s;
        break;
    }
    emit alarmRaised(toAlarm(alarm.kind), index, title, message);
}

void DayController::closeOpenPause(Minutes minute) {
    if (const auto running = runningIndex()) {
        BlockProgress& prog = progress_.at(*running);
        if (!prog.pauses.empty() && !prog.pauses.back().end) {
            prog.pauses.back().end = minute;
        }
    }
}

std::optional<std::size_t> DayController::runningIndex() const {
    for (const auto& [index, prog] : progress_.blocks) {
        if (prog.actualStart && !prog.actualEnd && !prog.skipped) {
            return index;
        }
    }
    return std::nullopt;
}

// The block the user is in, else the anchored block the clock is in, else the next one to come.
std::optional<std::size_t> DayController::pickCurrent() const {
    if (const auto running = runningIndex()) {
        return running;
    }
    std::optional<std::size_t> next;
    for (const PlannedBlock& planned : plan_.blocks) {
        if (planned.state == BlockState::Active) {
            return planned.templateIndex;
        }
        const bool pending = planned.state == BlockState::Upcoming || planned.state == BlockState::DoesNotFit;
        if (pending && (!next || planned.start < plan_.at(*next).start)) {
            next = planned.templateIndex;
        }
    }
    return next;
}

const BlockTemplate* DayController::block(std::size_t index) const {
    if (!day_ || index >= day_->blocks.size()) {
        return nullptr;
    }
    return &day_->blocks[index];
}

QString DayController::activityName(const ActivityId& id) const {
    if (document_) {
        for (const Activity& activity : document_->activities) {
            if (activity.id == id) {
                return QString::fromStdString(activity.name);
            }
        }
    }
    return QString::fromStdString(id);
}

Minutes DayController::nowMinute() const {
    return toTimePoint(now_).minuteOfDay;
}

QString DayController::formatDuration(int seconds) {
    const int clamped = std::max(0, seconds);
    const int hours = clamped / 3600;
    const int minutes = (clamped / 60) % 60;
    const int secs = clamped % 60;
    if (hours > 0) {
        return u"%1:%2:%3"_s.arg(hours).arg(minutes, 2, 10, QChar(u'0')).arg(secs, 2, 10, QChar(u'0'));
    }
    return u"%1:%2"_s.arg(minutes, 2, 10, QChar(u'0')).arg(secs, 2, 10, QChar(u'0'));
}

QString DayController::dateText() const {
    if (!date_) {
        return {};
    }
    const QDate date(static_cast<int>(date_->year()), static_cast<int>(static_cast<unsigned>(date_->month())),
                     static_cast<int>(static_cast<unsigned>(date_->day())));
    return date.toString(u"dddd d MMMM"_s);
}

int DayController::currentIndex() const {
    return current_ ? static_cast<int>(*current_) : -1;
}

QString DayController::currentName() const {
    if (!day_) {
        return u"Free day"_s;
    }
    if (!current_) {
        return u"Day complete"_s;
    }
    return blockName(static_cast<int>(*current_));
}

QString DayController::currentKind() const {
    const BlockTemplate* tpl = current_ ? block(*current_) : nullptr;
    return tpl ? kindName(tpl->kind) : QString();
}

QString DayController::currentState() const {
    return current_ ? stateName(plan_.at(*current_).state) : QString();
}

bool DayController::running() const {
    return runningIndex().has_value();
}

bool DayController::paused() const {
    const auto running = runningIndex();
    if (!running) {
        return false;
    }
    const BlockProgress* prog = progress_.find(*running);
    return prog && !prog->pauses.empty() && !prog->pauses.back().end;
}

int DayController::remainingSeconds() const {
    if (!current_ || !date_) {
        return 0;
    }
    const PlannedBlock& planned = plan_.at(*current_);
    const bool inProgress = planned.state == BlockState::Active;
    const Minutes edge = inProgress ? planned.end : planned.start;
    const Instant target = std::chrono::local_days{*date_} + edge;
    return static_cast<int>(std::chrono::duration_cast<Seconds>(target - now_).count());
}

QString DayController::remainingText() const {
    return formatDuration(remainingSeconds());
}

bool DayController::hasPomodoro() const {
    return session_.has_value() && current_ && sessionBlock_ == *current_;
}

int DayController::pomodoroIndex() const {
    return hasPomodoro() ? session_->currentIndex() + 1 : 0;
}

int DayController::pomodoroTotal() const {
    if (hasPomodoro()) {
        return session_->totalCount();
    }
    const BlockTemplate* tpl = current_ ? block(*current_) : nullptr;
    return tpl ? resolvedPomodoroCount(*tpl).value_or(0) : 0;
}

QString DayController::pomodoroPhase() const {
    return hasPomodoro() ? phaseName(session_->state()) : QString();
}

int DayController::pomodoroRemainingSeconds() const {
    return hasPomodoro() ? static_cast<int>(session_->remaining(now_).count()) : 0;
}

QVariantList DayController::planList() const {
    QVariantList list;
    for (const PlannedBlock& planned : plan_.blocks) {
        const BlockTemplate* tpl = block(planned.templateIndex);
        if (tpl == nullptr) {
            continue;
        }
        QVariantMap row;
        row[u"index"_s] = static_cast<int>(planned.templateIndex);
        row[u"name"_s] = activityName(tpl->activityId);
        row[u"kind"_s] = kindName(tpl->kind);
        row[u"start"_s] = timeText(planned.start);
        row[u"end"_s] = timeText(planned.end);
        row[u"state"_s] = stateName(planned.state);
        row[u"overrunsCutoff"_s] = planned.overrunsCutoff;
        row[u"current"_s] = current_ && *current_ == planned.templateIndex;
        list.push_back(row);
    }
    return list;
}

QVariantList DayController::overrunsCutoff() const {
    QVariantList list;
    for (const std::size_t index : plan_.overrunsCutoff()) {
        list.push_back(static_cast<int>(index));
    }
    return list;
}

QVariantList DayController::unconfirmed() const {
    QVariantList list;
    for (const std::size_t index : plan_.unconfirmedBlocks()) {
        list.push_back(static_cast<int>(index));
    }
    return list;
}

bool DayController::canStart() const {
    return current_.has_value() && !running();
}

bool DayController::canPause() const {
    return running() && !paused();
}

bool DayController::canResume() const {
    return paused();
}

bool DayController::canSkip() const {
    return current_.has_value();
}

bool DayController::canPostpone() const {
    const BlockTemplate* tpl = current_ ? block(*current_) : nullptr;
    return tpl && tpl->kind == BlockKind::Flexible && !running();
}

bool DayController::canFinish() const {
    return running();
}

void DayController::start() {
    if (!canStart()) {
        return;
    }
    const std::size_t index = *current_;
    const Minutes minute = nowMinute();
    if (!progress_.actualDayStart && day_ && minute > day_->dayStart) {
        progress_.actualDayStart = minute;
    }
    progress_.at(index).actualStart = minute;
    scheduler_.dismiss(index);

    session_.reset();
    if (const BlockTemplate* tpl = block(index)) {
        if (auto session = PomodoroSession::fromTemplate(*tpl)) {
            session_ = std::move(session);
            sessionBlock_ = index;
            applyPomodoroEvents(session_->start(now_), now_);
        }
    }
    persist();
    evaluateNow();
}

void DayController::pause() {
    if (!canPause()) {
        return;
    }
    progress_.at(*runningIndex()).pauses.push_back(PauseInterval{nowMinute(), std::nullopt});
    if (session_) {
        session_->pause(now_);
    }
    persist();
    evaluateNow();
}

void DayController::resume() {
    if (!canResume()) {
        return;
    }
    closeOpenPause(nowMinute());
    if (session_) {
        applyPomodoroEvents(session_->resume(now_), now_);
    }
    persist();
    evaluateNow();
}

void DayController::skip() {
    if (!canSkip()) {
        return;
    }
    const std::size_t index = *current_;
    if (runningIndex() == index) {
        closeOpenPause(nowMinute());
        session_.reset();
    }
    progress_.at(index).skipped = true;
    scheduler_.dismiss(index);
    persist();
    evaluateNow();
}

void DayController::skipPhase() {
    if (session_) {
        applyPomodoroEvents(session_->skip(now_), now_);
        evaluateNow();
    }
}

void DayController::postpone() {
    if (!canPostpone()) {
        return;
    }
    progress_.at(*current_).postponed = true;
    scheduler_.dismiss(*current_);
    persist();
    evaluateNow();
}

void DayController::extend(int minutes) {
    if (!current_ || minutes <= 0) {
        return;
    }
    progress_.at(*current_).extended += Minutes{minutes};
    persist();
    evaluateNow();
}

void DayController::finish() {
    if (!canFinish()) {
        return;
    }
    const Minutes minute = nowMinute();
    closeOpenPause(minute);
    progress_.at(*runningIndex()).actualEnd = minute;
    session_.reset();
    persist();
    evaluateNow();
}

void DayController::confirm(int blockIndex, bool happened) {
    if (blockIndex < 0 || block(static_cast<std::size_t>(blockIndex)) == nullptr) {
        return;
    }
    progress_.at(static_cast<std::size_t>(blockIndex)).confirmed = happened;
    persist();
    evaluateNow();
}

void DayController::logPushups(int reps) {
    if (reps <= 0) {
        return;
    }
    const std::size_t index = session_ ? sessionBlock_ : current_.value_or(0);
    progress_.pushups.push_back(PushupSet{index, nowMinute(), reps});
    persist();
    emit changed();
}

bool DayController::snooze(int blockIndex) {
    if (blockIndex < 0) {
        return false;
    }
    return scheduler_.snooze(static_cast<std::size_t>(blockIndex), now_);
}

bool DayController::canSnooze(int blockIndex) const {
    return blockIndex >= 0 && scheduler_.canSnooze(static_cast<std::size_t>(blockIndex));
}

QString DayController::blockName(int blockIndex) const {
    const BlockTemplate* tpl = blockIndex >= 0 ? block(static_cast<std::size_t>(blockIndex)) : nullptr;
    return tpl ? activityName(tpl->activityId) : QString();
}

bool DayController::blockWantsPushups(int blockIndex) const {
    const BlockTemplate* tpl = blockIndex >= 0 ? block(static_cast<std::size_t>(blockIndex)) : nullptr;
    return tpl && tpl->pushupsOnBreak;
}
