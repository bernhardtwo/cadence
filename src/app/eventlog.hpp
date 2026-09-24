#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

// One text file per day under the data directory, one line per alarm, overlay event or push-up
// set, so "did the prompt fire" can be answered after the fact. Files older than the retention
// window are removed at startup.
class EventLog : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static constexpr int retentionDays = 14;

    explicit EventLog(QString directory, QObject* parent = nullptr);
    ~EventLog() override;

    static EventLog* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static void setInstance(EventLog* instance);

    // Appends "HH:mm:ss  line" to today's file. Failures are reported once and never block the app.
    Q_INVOKABLE void write(const QString& line);

    QString directory() const { return directory_; }
    QString todayPath() const;

    // Deletes YYYY-MM-DD.log files older than the retention window. Returns how many were removed.
    int prune();

private:
    QString directory_;
    bool warned_ = false;
};
