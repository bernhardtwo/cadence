#pragma once

#include <QByteArray>
#include <QLocalServer>
#include <QObject>
#include <QString>

// One running copy per user. The first process listens on a local socket; a later launch sends it
// a command and exits.
class SingleInstance : public QObject {
    Q_OBJECT

public:
    explicit SingleInstance(const QString& key, QObject* parent = nullptr);

    bool isPrimary() const { return primary_; }

    // From a secondary process: hands the command to the primary. Returns false when it could
    // not be delivered.
    bool notifyPrimary(const QByteArray& command);

signals:
    void commandReceived(const QByteArray& command);

private:
    QString key_;
    QLocalServer server_;
    bool primary_ = false;
};
