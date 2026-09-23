#pragma once

#include "progressstore.hpp"

#include <cadence/core/alarm.hpp>
#include <cadence/core/clock.hpp>
#include <cadence/core/planner.hpp>
#include <cadence/core/pomodoro.hpp>
#include <cadence/core/template_json.hpp>

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <cstddef>
#include <memory>
#include <optional>

// Owns today's template, progress, plan, pomodoro machine and alarm scheduler. One evaluate(now)
// per second derives everything else; actions mutate progress, persist it and re-evaluate.
class DayController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString templateError READ templateError NOTIFY changed)
    Q_PROPERTY(QString storageError READ storageError NOTIFY changed)
    Q_PROPERTY(QString dateText READ dateText NOTIFY changed)
    Q_PROPERTY(bool freeDay READ freeDay NOTIFY changed)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY changed)
    Q_PROPERTY(QString currentName READ currentName NOTIFY changed)
    Q_PROPERTY(QString currentKind READ currentKind NOTIFY changed)
    Q_PROPERTY(QString currentState READ currentState NOTIFY changed)
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(bool paused READ paused NOTIFY changed)
    Q_PROPERTY(int remainingSeconds READ remainingSeconds NOTIFY changed)
    Q_PROPERTY(QString remainingText READ remainingText NOTIFY changed)
    Q_PROPERTY(bool hasPomodoro READ hasPomodoro NOTIFY changed)
    Q_PROPERTY(int pomodoroIndex READ pomodoroIndex NOTIFY changed)
    Q_PROPERTY(int pomodoroTotal READ pomodoroTotal NOTIFY changed)
    Q_PROPERTY(QString pomodoroPhase READ pomodoroPhase NOTIFY changed)
    Q_PROPERTY(int pomodoroRemainingSeconds READ pomodoroRemainingSeconds NOTIFY changed)
    Q_PROPERTY(QVariantList plan READ planList NOTIFY changed)
    Q_PROPERTY(bool doesNotFit READ doesNotFit NOTIFY changed)
    Q_PROPERTY(QVariantList overrunsCutoff READ overrunsCutoff NOTIFY changed)
    Q_PROPERTY(QVariantList unconfirmed READ unconfirmed NOTIFY changed)
    Q_PROPERTY(bool canStart READ canStart NOTIFY changed)
    Q_PROPERTY(bool canPause READ canPause NOTIFY changed)
    Q_PROPERTY(bool canResume READ canResume NOTIFY changed)
    Q_PROPERTY(bool canSkip READ canSkip NOTIFY changed)
    Q_PROPERTY(bool canPostpone READ canPostpone NOTIFY changed)
    Q_PROPERTY(bool canFinish READ canFinish NOTIFY changed)

public:
    enum Alarm {
        BlockStart,
        BlockEndingSoon,
        SoftReminder,
        PomodoroFocusEnd,
        BreakEnd,
        DayNoLongerFits,
        UnconfirmedPending,
    };
    Q_ENUM(Alarm)

    DayController(std::unique_ptr<cadence::core::IClock> clock, std::unique_ptr<ProgressStore> store,
                  std::optional<cadence::core::TemplateDocument> document, QString templateError,
                  QObject* parent = nullptr);
    ~DayController() override;

    // QML sees the single instance main creates; it is never constructed from QML.
    static DayController* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static void setInstance(DayController* instance);

    void startTicking();

    QString templateError() const { return templateError_; }
    QString storageError() const { return storageError_; }
    QString dateText() const;
    bool freeDay() const { return !day_.has_value(); }
    int currentIndex() const;
    QString currentName() const;
    QString currentKind() const;
    QString currentState() const;
    bool running() const;
    bool paused() const;
    int remainingSeconds() const;
    QString remainingText() const;
    bool hasPomodoro() const;
    int pomodoroIndex() const;
    int pomodoroTotal() const;
    QString pomodoroPhase() const;
    int pomodoroRemainingSeconds() const;
    QVariantList planList() const;
    bool doesNotFit() const { return plan_.doesNotFit(); }
    QVariantList overrunsCutoff() const;
    QVariantList unconfirmed() const;
    bool canStart() const;
    bool canPause() const;
    bool canResume() const;
    bool canSkip() const;
    bool canPostpone() const;
    bool canFinish() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void skip();
    Q_INVOKABLE void skipPhase();
    Q_INVOKABLE void postpone();
    Q_INVOKABLE void extend(int minutes);
    Q_INVOKABLE void finish();
    Q_INVOKABLE void confirm(int blockIndex, bool happened);
    Q_INVOKABLE void logPushups(int reps);
    Q_INVOKABLE bool snooze(int blockIndex);
    Q_INVOKABLE bool canSnooze(int blockIndex) const;
    Q_INVOKABLE QString blockName(int blockIndex) const;
    Q_INVOKABLE bool blockWantsPushups(int blockIndex) const;
    Q_INVOKABLE void evaluateNow();

signals:
    void changed();
    void alarmRaised(int kind, int blockIndex, const QString& title, const QString& message);
    void pushupPrompt(int blockIndex, int setIndex);

private:
    void evaluate(cadence::core::Instant now);
    void loadDay(cadence::core::Date date);
    void persist();
    void applyPomodoroEvents(const cadence::core::PomodoroEvents& events, cadence::core::Instant now);
    void raise(const cadence::core::Alarm& alarm);
    void closeOpenPause(cadence::core::Minutes nowMinute);
    std::optional<std::size_t> runningIndex() const;
    std::optional<std::size_t> pickCurrent() const;
    const cadence::core::BlockTemplate* block(std::size_t index) const;
    QString activityName(const cadence::core::ActivityId& id) const;
    cadence::core::Minutes nowMinute() const;
    static QString formatDuration(int seconds);

    std::unique_ptr<cadence::core::IClock> clock_;
    std::unique_ptr<ProgressStore> store_;
    std::optional<cadence::core::TemplateDocument> document_;
    QString templateError_;
    QString storageError_;
    QTimer timer_;

    std::optional<cadence::core::Date> date_;
    std::optional<cadence::core::DayTemplate> day_;
    cadence::core::DayProgress progress_;
    cadence::core::DayPlan plan_;
    cadence::core::AlarmScheduler scheduler_;
    std::optional<cadence::core::PomodoroSession> session_;
    std::size_t sessionBlock_ = 0;
    std::optional<std::size_t> current_;
    cadence::core::Instant now_{};
};
