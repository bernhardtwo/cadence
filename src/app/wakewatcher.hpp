#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>

// Re-evaluates the day when the machine resumes from sleep or the system clock jumps. On Windows
// this comes from WM_POWERBROADCAST and WM_TIMECHANGE; elsewhere the scheduler's wall clock
// comparisons already catch up on the next tick.
class WakeWatcher : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit WakeWatcher(QObject* parent = nullptr);
    ~WakeWatcher() override;

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void wakeDetected();
};
