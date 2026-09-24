#include "daycontroller.hpp"

#include <QDate>
#include <QStringList>
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

// The names above are keys: QML compares against them and the log records them. The labels
// below are what the user reads.
QString stateLabel(BlockState state) {
    switch (state) {
    case BlockState::Done:
        return DayController::tr("Done");
    case BlockState::Active:
        return DayController::tr("Active");
    case BlockState::Upcoming:
        return DayController::tr("Upcoming");
    case BlockState::Skipped:
        return DayController::tr("Skipped");
    case BlockState::DoesNotFit:
        return DayController::tr("Does not fit");
    case BlockState::Unconfirmed:
        return DayController::tr("Unconfirmed");
    }
    return {};
}

QString kindLabel(BlockKind kind) {
    switch (kind) {
    case BlockKind::Anchored:
        return DayController::tr("Anchored");
    case BlockKind::Flexible:
        return DayController::tr("Flexible");
    case BlockKind::Soft:
        return DayController::tr("Soft");
    }
    return {};
}

QString phaseLabel(PomodoroState state) {
    switch (state) {
    case PomodoroState::Idle:
        return DayController::tr("Idle");
    case PomodoroState::Focus:
        return DayController::tr("Focus");
    case PomodoroState::ShortBreak:
        return DayController::tr("Short break");
    case PomodoroState::LongBreak:
        return DayController::tr("Long break");
    case PomodoroState::Paused:
        return DayController::tr("Paused");
    case PomodoroState::Completed:
        return DayController::tr("Completed");
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
    evaluate(clock_->now(), false);
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
    if (restored_) {
        emit sessionRestored(restored_->blockIndex, restored_->phase, restored_->remainingSeconds);
        restored_.reset();
    }
    evaluateNow();
}

void DayController::setDocument(std::optional<TemplateDocument> document, QString templateError) {
    document_ = std::move(document);
    templateError_ = std::move(templateError);
    day_.reset();
    if (document_ && date_) {
        const Weekday weekday{std::chrono::local_days{*date_}};
        if (const auto& day = document_->week.day(weekday)) {
            day_ = *day;
        }
    }
    // A session tied to a block that no longer has a plan would tick against nothing.
    if (session_) {
        const BlockTemplate* tpl = block(sessionBlock_);
        if (tpl == nullptr || !tpl->pomodoro) {
            session_.reset();
        }
    }
    evaluate(clock_->now());
}

void DayController::setMaxSnoozes(int limit) {
    AlarmPolicy policy = scheduler_.policy();
    policy.maxSnoozes = std::max(0, limit);
    scheduler_.setPolicy(policy);
    emit changed();
}

void DayController::setWarnDayNoLongerFits(bool warn) {
    warnDayNoLongerFits_ = warn;
}

void DayController::evaluateNow() {
    evaluate(clock_->now());
}

void DayController::evaluate(Instant now, bool withAlarms) {
    now_ = now;
    const TimePoint point = toTimePoint(now);
    if (date_ != point.date) {
        loadDay(point.date, now);
    }

    PomodoroEvents events;
    if (session_) {
        events = session_->tick(now);
        applyPomodoroEvents(events, now);
    }

    if (day_) {
        plan_ = plan(*day_, progress_, point);
        current_ = pickCurrent();
        if (withAlarms) {
            const std::vector<cadence::core::Alarm> alarms =
                scheduler_.evaluate(*day_, plan_, progress_, events, now);
            for (const cadence::core::Alarm& alarm : alarms) {
                raise(alarm);
            }
        }
    } else {
        plan_ = DayPlan{};
        current_.reset();
    }
    emit changed();
}

void DayController::loadDay(Date date, Instant now) {
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
        restoreSession(now);
    }
}

// A running block with recorded phases gets its session back where it was; the next evaluation
// catches up on whatever elapsed while the app was down. Blocks recorded before phases existed
// keep running without a session, as they did before.
void DayController::restoreSession(Instant now) {
    const std::optional<std::size_t> running = runningIndex();
    if (!running) {
        return;
    }
    const BlockTemplate* tpl = block(*running);
    const BlockProgress* prog = progress_.find(*running);
    if (tpl == nullptr || prog == nullptr || prog->phases.empty() || !date_) {
        return;
    }
    if (auto session =
            PomodoroSession::restore(*tpl, prog->phases, std::chrono::local_days{*date_}, prog->pomodoroCount)) {
        session_ = std::move(session);
        sessionBlock_ = *running;
        restored_ = Restored{static_cast<int>(*running), phaseName(session_->activePhase()),
                             static_cast<int>(session_->remaining(now).count())};
        if (timer_.isActive()) {
            emit sessionRestored(restored_->blockIndex, restored_->phase, restored_->remainingSeconds);
            restored_.reset();
        }
    }
}

Seconds DayController::secondOfDay(Instant instant) const {
    if (!date_) {
        return Seconds{0};
    }
    return std::chrono::duration_cast<Seconds>(instant - std::chrono::local_days{*date_});
}

void DayController::closeOpenPhase(Seconds at) {
    cadence::core::closeOpenPhase(progress_.at(sessionBlock_).phases, at);
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

// Every event lands in the block's progress so a restart can rebuild the session and the day can
// be audited afterwards.
void DayController::applyPomodoroEvents(const PomodoroEvents& events, Instant now) {
    for (const PomodoroEvent& event : events) {
        if (const auto* started = std::get_if<PhaseStarted>(&event)) {
            closeOpenPhase(secondOfDay(started->at));
            progress_.at(sessionBlock_)
                .phases.push_back(
                    PhaseRecord{phaseKindOf(started->phase), started->index, secondOfDay(started->at)});
        } else if (const auto* prompt = std::get_if<PushupPrompt>(&event)) {
            progress_.at(sessionBlock_).prompts.push_back(PromptRecord{prompt->setIndex, secondOfDay(now)});
            emit pushupPrompt(static_cast<int>(sessionBlock_), prompt->setIndex);
        } else if (const auto* completed = std::get_if<SessionCompleted>(&event)) {
            closeOpenPhase(secondOfDay(completed->at));
            // The block's pomodoros are done, so the block is done.
            BlockProgress& prog = progress_.at(sessionBlock_);
            if (prog.actualStart && !prog.actualEnd) {
                closeOpenPause(toTimePoint(now).minuteOfDay);
                prog.actualEnd = toTimePoint(now).minuteOfDay;
            }
        }
    }
    if (!events.empty()) {
        persist();
    }
}

void DayController::raise(const cadence::core::Alarm& alarm) {
    if (alarm.kind == AlarmKind::DayNoLongerFits && !warnDayNoLongerFits_) {
        return;
    }
    const int index = alarm.blockIndex ? static_cast<int>(*alarm.blockIndex) : -1;
    const QString name = index >= 0 ? blockName(index) : QString();
    QString title;
    QString message;
    switch (alarm.kind) {
    case AlarmKind::BlockStart:
        title = name;
        message = tr("Block starts now");
        break;
    case AlarmKind::BlockEndingSoon:
        title = name;
        message =
            tr("Ends in %n minute(s)", nullptr, static_cast<int>(scheduler_.policy().endingSoonLead.count()));
        break;
    case AlarmKind::SoftReminder:
        title = name;
        message = tr("Whenever you are ready");
        break;
    case AlarmKind::PomodoroFocusEnd:
        title = tr("Focus done");
        message = session_ && session_->state() != PomodoroState::Completed ? tr("Take a break")
                                                                            : tr("Last pomodoro finished");
        break;
    case AlarmKind::BreakEnd:
        title = tr("Break over");
        message = tr("Back to focus");
        break;
    case AlarmKind::DayNoLongerFits:
        title = tr("The day no longer fits");
        message = tr("Some blocks end after the cutoff");
        break;
    case AlarmKind::UnconfirmedPending:
        title = name;
        message = tr("Did you do this?");
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

QString DayController::activityColor(const ActivityId& id) const {
    if (document_) {
        for (const Activity& activity : document_->activities) {
            if (activity.id == id) {
                return QString::fromStdString(activity.color);
            }
        }
    }
    return {};
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

QString DayController::formatClock(int minutes) {
    const int clamped = std::max(0, minutes);
    return u"%1:%2"_s.arg(clamped / 60).arg(clamped % 60, 2, 10, QChar(u'0'));
}

QString DayController::formatMinutes(int minutes) const {
    return formatClock(minutes);
}

// The right hand text of a plan row: length, pomodoro count and whether breaks ask for push-ups.
QString DayController::blockDetail(std::size_t index, const PlannedBlock& planned) const {
    const BlockTemplate* tpl = block(index);
    if (tpl == nullptr) {
        return {};
    }
    QStringList parts;
    const int minutes = static_cast<int>((planned.end - planned.start).count());
    parts.push_back(minutes >= 60 ? tr("%1 h").arg(formatClock(minutes)) : tr("%1 min").arg(minutes));
    if (const auto count = resolvedPomodoroCount(*tpl); count && *count > 0) {
        parts.push_back(tr("%n pomodoro(s)", nullptr, *count));
    }
    if (tpl->pushupsOnBreak) {
        parts.push_back(tr("push-ups"));
    }
    return parts.join(u" · "_s);
}

QString DayController::skippedAtText(std::size_t index) const {
    const BlockProgress* prog = progress_.find(index);
    if (prog == nullptr || !prog->skipped || prog->skips.empty()) {
        return {};
    }
    return timeText(prog->skips.back().at);
}

// The caption of a skipped row: what restoring would do, worked out by rehearsing the restore on a
// copy of the progress, or why it is no longer possible.
QString DayController::restoreCaption(std::size_t index) const {
    const BlockTemplate* tpl = block(index);
    const BlockProgress* prog = progress_.find(index);
    if (tpl == nullptr || prog == nullptr || !day_ || !(prog->skipped || prog->confirmed == false)) {
        return {};
    }
    DayProgress trial = progress_;
    const std::optional<RestoreResult> result = restoreBlock(*day_, trial, index, now_);
    if (!result) {
        return tr("window closed");
    }
    const DayPlan planned = plan(*day_, trial, toTimePoint(now_));
    const PlannedBlock& mine = planned.at(index);
    const QString skippedAt = skippedAtText(index);
    const Minutes now = nowMinute();

    if (tpl->kind == BlockKind::Anchored) {
        const QString start = timeText(mine.start);
        const QString end = timeText(mine.end);
        if (now < mine.start) {
            return tr("%1 to %2 · restoring puts it back at its time.").arg(start, end);
        }
        const QString done = formatClock(static_cast<int>(workedMinutes(mine, trial.find(index), now).count()));
        const QString left = formatClock(static_cast<int>(std::max(Minutes{0}, mine.end - now).count()));
        const int count = result->pomodoroCount.value_or(0);
        if (count > 0) {
            return tr("%1 to %2 · %3 done · restoring continues at the current time with %4 left, %n pomodoro(s).",
                      nullptr, count)
                .arg(start, end, done, left);
        }
        return tr("%1 to %2 · %3 done · restoring continues at the current time with %4 left.")
            .arg(start, end, done, left);
    }

    if (tpl->kind == BlockKind::Soft) {
        return skippedAt.isEmpty() ? tr("Soft, restoring re-arms the reminder.")
                                   : tr("Skipped at %1 · soft, restoring re-arms the reminder.").arg(skippedAt);
    }

    // The block scheduled right before it in the queue, if any.
    std::optional<std::size_t> before;
    for (const PlannedBlock& other : planned.blocks) {
        const BlockTemplate* otherTpl = block(other.templateIndex);
        if (other.templateIndex == index || otherTpl == nullptr || otherTpl->kind == BlockKind::Soft ||
            other.state == BlockState::Skipped || other.end > mine.start) {
            continue;
        }
        if (!before || other.end > planned.at(*before).end ||
            (other.end == planned.at(*before).end && other.start > planned.at(*before).start)) {
            before = other.templateIndex;
        }
    }
    if (before) {
        const QString name = blockName(static_cast<int>(*before));
        return skippedAt.isEmpty()
                   ? tr("Flexible, restoring puts it back in the queue after %1.").arg(name)
                   : tr("Skipped at %1 · flexible, restoring puts it back in the queue after %2.").arg(skippedAt, name);
    }
    return skippedAt.isEmpty()
               ? tr("Flexible, restoring puts it back at the front of the queue.")
               : tr("Skipped at %1 · flexible, restoring puts it back at the front of the queue.").arg(skippedAt);
}

QString DayController::dateText() const {
    if (!date_) {
        return {};
    }
    const QDate date(static_cast<int>(date_->year()), static_cast<int>(static_cast<unsigned>(date_->month())),
                     static_cast<int>(static_cast<unsigned>(date_->day())));
    // The default locale follows the UI language, so the weekday and month come out in it.
    return QLocale().toString(date, u"dddd d MMMM"_s);
}

int DayController::currentIndex() const {
    return current_ ? static_cast<int>(*current_) : -1;
}

QString DayController::currentName() const {
    if (!day_) {
        return tr("Free day");
    }
    if (!current_) {
        return tr("Day complete");
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

QString DayController::currentActivityId() const {
    const BlockTemplate* tpl = current_ ? block(*current_) : nullptr;
    return tpl ? QString::fromStdString(tpl->activityId) : QString();
}

QVariantList DayController::activities() const {
    QVariantList list;
    if (!document_) {
        return list;
    }
    for (const Activity& activity : document_->activities) {
        QVariantMap row;
        row[u"id"_s] = QString::fromStdString(activity.id);
        row[u"name"_s] = QString::fromStdString(activity.name);
        list.push_back(row);
    }
    return list;
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

QString DayController::currentStartText() const {
    return current_ ? timeText(plan_.at(*current_).start) : QString();
}

QString DayController::currentEndText() const {
    return current_ ? timeText(plan_.at(*current_).end) : QString();
}

bool DayController::currentActive() const {
    return current_ && plan_.at(*current_).state == BlockState::Active;
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

QString DayController::pomodoroPhaseText() const {
    return hasPomodoro() ? phaseLabel(session_->state()) : QString();
}

int DayController::pomodoroRemainingSeconds() const {
    return hasPomodoro() ? static_cast<int>(session_->remaining(now_).count()) : 0;
}

int DayController::pomodorosDone() const {
    return hasPomodoro() ? session_->completedSessions() : 0;
}

// Length of the running phase, for progress bars.
int DayController::pomodoroPhaseSeconds() const {
    if (!hasPomodoro()) {
        return 0;
    }
    const PomodoroPlan& plan = session_->plan();
    const auto seconds = [](Minutes minutes) {
        return static_cast<int>(std::chrono::duration_cast<Seconds>(minutes).count());
    };
    switch (session_->activePhase()) {
    case PomodoroState::Idle:
    case PomodoroState::Focus:
        return seconds(plan.focus);
    case PomodoroState::ShortBreak:
        return seconds(plan.shortBreak);
    case PomodoroState::LongBreak:
        return seconds(plan.longBreak);
    case PomodoroState::Paused:
    case PomodoroState::Completed:
        break;
    }
    return 0;
}

int DayController::currentDurationMinutes() const {
    if (!current_) {
        return 0;
    }
    const PlannedBlock& planned = plan_.at(*current_);
    return static_cast<int>(std::max(Minutes{0}, planned.end - planned.start).count());
}

bool DayController::pomodoroOnBreak() const {
    if (!hasPomodoro()) {
        return false;
    }
    const PomodoroState state = session_->state();
    return state == PomodoroState::ShortBreak || state == PomodoroState::LongBreak;
}

QString DayController::nextBreakText() const {
    if (!hasPomodoro()) {
        return {};
    }
    const PomodoroPlan& plan = session_->plan();
    const int done = session_->completedSessions();
    const PomodoroState state = session_->state();
    if (state == PomodoroState::Completed) {
        return tr("all done");
    }
    if (state == PomodoroState::ShortBreak || state == PomodoroState::LongBreak) {
        return tr("on a break");
    }
    // The break after the running session; the last session ends the block instead.
    const int session = done + 1;
    if (session >= session_->totalCount()) {
        return tr("last one");
    }
    const bool longBreak = plan.longBreakEvery > 0 && session % plan.longBreakEvery == 0;
    const Minutes length = longBreak ? plan.longBreak : plan.shortBreak;
    const QString text = tr("next break: %1 min").arg(length.count());
    if (session_->pushupsOnBreak()) {
        return tr("%1 + push-ups").arg(text);
    }
    return text;
}

// The day summary follows one activity, work when the template has it, and measures it against
// its template length so an early finish never shrinks the target.
ActivitySummary DayController::summary() const {
    if (!day_) {
        return {};
    }
    const std::optional<ActivityId> activity =
        summaryActivity(*day_, document_ ? document_->activities : std::vector<Activity>{});
    if (!activity) {
        return {};
    }
    return summarizeActivity(*day_, plan_, progress_, *activity, nowMinute());
}

QString DayController::summaryName() const {
    if (!day_) {
        return {};
    }
    const std::optional<ActivityId> activity =
        summaryActivity(*day_, document_ ? document_->activities : std::vector<Activity>{});
    return activity ? activityName(*activity) : QString();
}

int DayController::doneMinutes() const {
    return static_cast<int>(summary().done.count());
}

int DayController::targetMinutes() const {
    return static_cast<int>(summary().target.count());
}

int DayController::pushupsToday() const {
    int total = 0;
    for (const PushupSet& set : progress_.pushups) {
        total += set.reps;
    }
    return total;
}

QString DayController::summaryText() const {
    if (!day_) {
        return {};
    }
    const ActivitySummary current = summary();
    const QString name = summaryName();
    if (name.isEmpty()) {
        return tr("Push-ups %1").arg(pushupsToday());
    }
    return tr("%1 %2 / %3 · Push-ups %4")
        .arg(name, formatClock(static_cast<int>(current.done.count())),
             formatClock(static_cast<int>(current.target.count())))
        .arg(pushupsToday());
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
        row[u"kindText"_s] = kindLabel(tpl->kind);
        row[u"start"_s] = timeText(planned.start);
        row[u"end"_s] = timeText(planned.end);
        row[u"state"_s] = stateName(planned.state);
        row[u"stateText"_s] = stateLabel(planned.state);
        row[u"overrunsCutoff"_s] = planned.overrunsCutoff;
        row[u"current"_s] = current_ && *current_ == planned.templateIndex;
        row[u"durationMinutes"_s] =
            static_cast<int>(std::max(Minutes{0}, planned.end - planned.start).count());
        row[u"pomodoros"_s] = resolvedPomodoroCount(*tpl).value_or(0);
        row[u"pushups"_s] = tpl->pushupsOnBreak;
        row[u"detail"_s] = blockDetail(planned.templateIndex, planned);
        row[u"color"_s] = activityColor(tpl->activityId);
        row[u"restorable"_s] = canRestore(static_cast<int>(planned.templateIndex));
        row[u"skippedAt"_s] = skippedAtText(planned.templateIndex);
        row[u"restoreCaption"_s] = restoreCaption(planned.templateIndex);
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

bool DayController::canExtend() const {
    if (!current_) {
        return false;
    }
    const BlockState state = plan_.at(*current_).state;
    return state == BlockState::Active || state == BlockState::Upcoming;
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
    if (const BlockTemplate* tpl = block(index)) {
        emit blockStarted(static_cast<int>(index), QString::fromStdString(tpl->activityId));
    }
}

void DayController::pause() {
    if (!canPause()) {
        return;
    }
    progress_.at(*runningIndex()).pauses.push_back(PauseInterval{nowMinute(), std::nullopt});
    if (session_) {
        applyPomodoroEvents(session_->pause(now_), now_);
        if (session_->state() == PomodoroState::Paused) {
            BlockProgress& prog = progress_.at(sessionBlock_);
            if (!prog.phases.empty() && !prog.phases.back().end) {
                prog.phases.back().pauses.push_back(PhasePause{secondOfDay(now_), std::nullopt});
            }
        }
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
        BlockProgress& prog = progress_.at(sessionBlock_);
        if (!prog.phases.empty() && !prog.phases.back().pauses.empty() &&
            !prog.phases.back().pauses.back().end) {
            prog.phases.back().pauses.back().end = secondOfDay(now_);
        }
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
    if (session_ && sessionBlock_ == index) {
        session_.reset();
    }
    skipBlock(progress_.at(index), nowMinute(), secondOfDay(now_));
    scheduler_.dismiss(index);
    persist();
    evaluateNow();
}

void DayController::skipPhase() {
    if (session_) {
        BlockProgress& prog = progress_.at(sessionBlock_);
        if (!prog.phases.empty() && !prog.phases.back().end) {
            prog.phases.back().skipped = true;
            prog.phases.back().end = secondOfDay(now_);
        }
        applyPomodoroEvents(session_->skip(now_), now_);
        persist();
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
    const std::size_t index = *runningIndex();
    closeOpenPause(minute);
    // A block finished by hand ends its phase record now, so no record is ever left open.
    cadence::core::closeOpenPhase(progress_.at(index).phases, secondOfDay(now_));
    progress_.at(index).actualEnd = minute;
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

bool DayController::canRestore(int blockIndex) const {
    if (blockIndex < 0 || !day_) {
        return false;
    }
    return !restoreRefusal(*day_, progress_, static_cast<std::size_t>(blockIndex), toTimePoint(now_));
}

void DayController::restore(int blockIndex) {
    if (!canRestore(blockIndex)) {
        return;
    }
    const auto index = static_cast<std::size_t>(blockIndex);
    const BlockTemplate* tpl = block(index);
    const std::optional<RestoreResult> result = restoreBlock(*day_, progress_, index, now_);
    if (tpl == nullptr || !result) {
        return;
    }
    if (session_ && (sessionBlock_ == index || result->displaced == sessionBlock_)) {
        session_.reset();
    }
    scheduler_.rearm(index);
    if (result->displaced) {
        // Back in the queue, it deserves its start alarm again when the chain reaches it.
        scheduler_.rearm(*result->displaced);
    }
    if (result->pomodoroCount && tpl->pomodoro) {
        session_ = PomodoroSession(*tpl->pomodoro, *result->pomodoroCount, tpl->pushupsOnBreak);
        sessionBlock_ = index;
        applyPomodoroEvents(session_->start(now_), now_);
    }
    persist();
    evaluateNow();
    emit blockRestored(blockIndex, QString::fromStdString(tpl->activityId));
}

void DayController::logPushups(int reps) {
    if (reps <= 0) {
        return;
    }
    const std::size_t index = session_ ? sessionBlock_ : current_.value_or(0);
    progress_.pushups.push_back(PushupSet{index, nowMinute(), reps});
    for (auto it = progress_.at(index).prompts.rbegin(); it != progress_.at(index).prompts.rend(); ++it) {
        if (it->answer == PromptAnswer::Pending) {
            it->answer = PromptAnswer::Logged;
            break;
        }
    }
    persist();
    emit pushupsLogged(static_cast<int>(index), reps);
    emit changed();
}

void DayController::dismissPrompt() {
    const std::size_t index = session_ ? sessionBlock_ : current_.value_or(0);
    const auto found = progress_.blocks.find(index);
    if (found == progress_.blocks.end()) {
        return;
    }
    BlockProgress* prog = &found->second;
    for (auto it = prog->prompts.rbegin(); it != prog->prompts.rend(); ++it) {
        if (it->answer == PromptAnswer::Pending) {
            it->answer = PromptAnswer::Skipped;
            persist();
            return;
        }
    }
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

bool DayController::blockFullscreenAlarm(int blockIndex) const {
    const BlockTemplate* tpl = blockIndex >= 0 ? block(static_cast<std::size_t>(blockIndex)) : nullptr;
    return tpl && tpl->fullscreenAlarm;
}
