#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

// Exposes build information from the core library to QML as the AppInfo singleton.
class AppInfo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit AppInfo(QObject* parent = nullptr);

    QString version() const;
};
