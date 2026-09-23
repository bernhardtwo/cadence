#include "alarmsounds.hpp"
#include "apppaths.hpp"
#include "daycontroller.hpp"
#include "fileprogressstore.hpp"
#include "singleinstance.hpp"
#include "systemclock.hpp"
#include "templateeditor.hpp"
#include "trayicon.hpp"
#include "wakewatcher.hpp"

#include <cadence/core/version.hpp>
#include <cadence/platform/autostart.hpp>

#include <QApplication>
#include <QCommandLineParser>
#include <QFont>
#include <QFontDatabase>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStringList>
#include <QTextStream>
#include <QtLogging>

#include <cstdlib>
#include <memory>

using namespace Qt::StringLiterals;

namespace {

// The bundled fonts are the only ones the UI is allowed to render with.
void loadBundledFonts() {
    const QStringList files = {
        u":/fonts/BigShouldersDisplay/BigShouldersDisplay-Variable.ttf"_s,
        u":/fonts/Archivo/Archivo-Variable.ttf"_s,
    };
    for (const QString& file : files) {
        if (QFontDatabase::addApplicationFont(file) < 0) {
            qWarning("Failed to load bundled font %s", qPrintable(file));
        }
    }
}

QString instanceKey() {
    QString user = QString::fromLocal8Bit(qgetenv("USERNAME"));
    if (user.isEmpty()) {
        user = QString::fromLocal8Bit(qgetenv("USER"));
    }
    QString key = u"cadence-"_s + user;
#ifdef QT_DEBUG
    // A test session against its own data directory must not wake the real instance instead.
    if (!qgetenv("CADENCE_TEST_DATA_DIR").isEmpty()) {
        key += u"-test"_s;
    }
#endif
    return key;
}

void showWindow(QQuickWindow* window) {
    if (window == nullptr) {
        return;
    }
    window->show();
    window->raise();
    window->requestActivate();
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"Cadence"_s);
    QApplication::setOrganizationName(u"Cadence"_s);
    QApplication::setApplicationVersion(QString::fromUtf8(cadence::core::versionString()));

    QCommandLineParser parser;
    parser.setApplicationDescription(u"Structure your day into blocks with alarms, pomodoros and push-ups"_s);
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption minimized(u"minimized"_s, u"Start hidden in the tray."_s);
    const QCommandLineOption enableAutostart(u"enable-autostart"_s, u"Register Cadence to launch at login and exit."_s);
    const QCommandLineOption disableAutostart(u"disable-autostart"_s, u"Remove the launch at login registration and exit."_s);
    parser.addOptions({minimized, enableAutostart, disableAutostart});
    parser.process(app);

    const std::unique_ptr<cadence::platform::Autostart> autostart =
        cadence::platform::createAutostart(QCoreApplication::applicationFilePath());
    if (parser.isSet(enableAutostart) || parser.isSet(disableAutostart)) {
        const bool enable = parser.isSet(enableAutostart);
        const bool ok = autostart->setEnabled(enable);
        QTextStream out(stdout);
        out << (ok ? (enable ? u"Launch at login enabled: "_s : u"Launch at login disabled: "_s)
                   : u"Could not update launch at login: "_s)
            << autostart->location() << Qt::endl;
        return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    SingleInstance instance(instanceKey());
    if (!instance.isPrimary()) {
        instance.notifyPrimary("show");
        return EXIT_SUCCESS;
    }

    // Basic is the only style whose controls we fully override; it must be set before any QML loads.
    QQuickStyle::setStyle(u"Basic"_s);

    loadBundledFonts();
    QFont bodyFont(u"Archivo"_s);
    bodyFont.setPixelSize(15);
    QApplication::setFont(bodyFont);

    apppaths::TemplateLoad templateLoad = apppaths::loadWeekTemplate();
    if (!templateLoad.error.isEmpty()) {
        qWarning("%s", qPrintable(templateLoad.error));
    }

    // The editor keeps its own copy; the controller takes the document it will run the day from.
    TemplateEditor editor(templateLoad.document, templateLoad.path);
    TemplateEditor::setInstance(&editor);

    DayController controller(std::make_unique<SystemClock>(),
                             std::make_unique<FileProgressStore>(apppaths::progressDir()),
                             std::move(templateLoad.document), templateLoad.error);
    DayController::setInstance(&controller);
    QObject::connect(&editor, &TemplateEditor::saved, &controller,
                     [&controller](const cadence::core::TemplateDocument& document) {
                         controller.setDocument(document, {});
                     });

    WakeWatcher wakeWatcher;
    QObject::connect(&wakeWatcher, &WakeWatcher::wakeDetected, &controller, &DayController::evaluateNow);

    // With a tray the window only hides on close and Quit is the way out.
    const bool trayAvailable = TrayController::isAvailable();
    QApplication::setQuitOnLastWindowClosed(!trayAvailable);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.setInitialProperties({
        {u"visible"_s, !(parser.isSet(minimized) && trayAvailable)},
        {u"trayAvailable"_s, trayAvailable},
    });
    engine.loadFromModule(u"Cadence"_s, u"Main"_s);
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());

    std::unique_ptr<TrayController> tray;
    if (trayAvailable) {
        tray = std::make_unique<TrayController>(controller, *autostart);
        QObject::connect(tray.get(), &TrayController::showRequested, window, [window] { showWindow(window); });
        QObject::connect(tray.get(), &TrayController::quitRequested, &app, &QCoreApplication::quit);
    }
    QObject::connect(&instance, &SingleInstance::commandReceived, window, [window](const QByteArray& command) {
        if (command == "show") {
            showWindow(window);
        }
    });

    AlarmSounds sounds;
    QObject::connect(&controller, &DayController::alarmRaised, &sounds,
                     [&sounds, &tray](int kind, int, const QString& title, const QString& message) {
                         sounds.play(static_cast<DayController::Alarm>(kind));
                         if (tray) {
                             tray->showMessage(title, message);
                         }
                     });

    controller.startTicking();
    return QApplication::exec();
}
