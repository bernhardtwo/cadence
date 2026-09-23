#include "trayicon.hpp"

#include <QByteArray>
#include <QFile>
#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QStyleHints>
#include <QSvgRenderer>

using namespace Qt::StringLiterals;

TrayController::TrayController(DayController& day, cadence::platform::Autostart& autostart, QObject* parent)
    : QObject(parent), day_(day), autostart_(autostart) {
    showAction_ = menu_.addAction(u"Show Cadence"_s, this, &TrayController::showRequested);
    toggleAction_ = menu_.addAction(u"Start"_s, this, [this] {
        if (day_.canPause()) {
            day_.pause();
        } else if (day_.canResume()) {
            day_.resume();
        } else {
            day_.start();
        }
    });
    finishAction_ = menu_.addAction(u"Finish block"_s, &day_, &DayController::finish);
    skipAction_ = menu_.addAction(u"Skip block"_s, &day_, &DayController::skip);
    menu_.addSeparator();
    autostartAction_ = menu_.addAction(u"Launch at login"_s);
    autostartAction_->setCheckable(true);
    autostartAction_->setChecked(autostart_.isEnabled());
    connect(autostartAction_, &QAction::toggled, this, &TrayController::toggleAutostart);
    menu_.addSeparator();
    quitAction_ = menu_.addAction(u"Quit"_s, this, &TrayController::quitRequested);

    tray_.setIcon(renderIcon());
    tray_.setContextMenu(&menu_);
    connect(&tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            emit showRequested();
        }
    });
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this] { tray_.setIcon(renderIcon()); });
    connect(&day_, &DayController::changed, this, &TrayController::refresh);

    refresh();
    tray_.show();
}

bool TrayController::isAvailable() {
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayController::showMessage(const QString& title, const QString& message, QSystemTrayIcon::MessageIcon icon) {
    tray_.showMessage(title, message, icon, 8000);
}

void TrayController::refresh() {
    tray_.setToolTip(u"Cadence · %1 · %2"_s.arg(day_.currentName(), day_.remainingText()));

    if (day_.canPause()) {
        toggleAction_->setText(u"Pause"_s);
        toggleAction_->setEnabled(true);
    } else if (day_.canResume()) {
        toggleAction_->setText(u"Resume"_s);
        toggleAction_->setEnabled(true);
    } else {
        toggleAction_->setText(u"Start"_s);
        toggleAction_->setEnabled(day_.canStart());
    }
    finishAction_->setEnabled(day_.canFinish());
    skipAction_->setEnabled(day_.canSkip());
}

void TrayController::syncAutostart() {
    const QSignalBlocker blocker(autostartAction_);
    autostartAction_->setChecked(autostart_.isEnabled());
}

void TrayController::toggleAutostart(bool enabled) {
    if (autostart_.setEnabled(enabled)) {
        emit autostartChanged();
        return;
    }
    showMessage(u"Launch at login"_s, u"Could not update %1"_s.arg(autostart_.location()), QSystemTrayIcon::Warning);
    const QSignalBlocker blocker(autostartAction_);
    autostartAction_->setChecked(autostart_.isEnabled());
}

// The SVG is monochrome through currentColor; the color follows the system theme so the glyph
// stays visible on both light and dark taskbars. macOS gets a template image instead.
QIcon TrayController::renderIcon() {
    QFile file(u":/icons/tray.svg"_s);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const bool dark = QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light;
    QByteArray svg = file.readAll();
    svg.replace("currentColor", dark ? "#FFFFFF" : "#000000");

    QSvgRenderer renderer(svg);
    QIcon icon;
    for (const int size : {16, 24, 32, 48, 64}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        icon.addPixmap(pixmap);
    }
#ifdef Q_OS_MACOS
    icon.setIsMask(true);
#endif
    return icon;
}
