#include "wakewatcher.hpp"

#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

WakeWatcher::WakeWatcher(QObject* parent) : QObject(parent) {
    QCoreApplication::instance()->installNativeEventFilter(this);
}

WakeWatcher::~WakeWatcher() {
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
}

bool WakeWatcher::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    Q_UNUSED(result);
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        const MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_TIMECHANGE) {
            emit wakeDetected();
        } else if (msg->message == WM_POWERBROADCAST &&
                   (msg->wParam == PBT_APMRESUMEAUTOMATIC || msg->wParam == PBT_APMRESUMESUSPEND)) {
            emit wakeDetected();
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    return false;
}
