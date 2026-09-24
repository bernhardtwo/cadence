#pragma once

#include "daycontroller.hpp"

#include <cadence/platform/autostart.hpp>

#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QObject>
#include <QString>
#include <QSystemTrayIcon>

// The tray icon, its menu and its notifications. The tooltip and the menu labels follow the
// controller; the actions call straight back into it.
class TrayController : public QObject {
    Q_OBJECT

public:
    TrayController(DayController& day, cadence::platform::Autostart& autostart, QObject* parent = nullptr);

    static bool isAvailable();

    void showMessage(const QString& title, const QString& message,
                     QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);

    // The settings screen changed launch at login; the menu entry follows.
    void syncAutostart();

    // The UI language changed; every label is worded again.
    void retranslate();

signals:
    void showRequested();
    void quitRequested();
    void autostartChanged();

private:
    void refresh();
    void toggleAutostart(bool enabled);
    static QIcon renderIcon();

    DayController& day_;
    cadence::platform::Autostart& autostart_;
    QSystemTrayIcon tray_;
    QMenu menu_;
    QAction* showAction_ = nullptr;
    QAction* toggleAction_ = nullptr;
    QAction* finishAction_ = nullptr;
    QAction* skipAction_ = nullptr;
    QAction* autostartAction_ = nullptr;
    QAction* quitAction_ = nullptr;
};
