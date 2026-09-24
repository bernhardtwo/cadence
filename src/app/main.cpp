#include "alarmsounds.hpp"
#include "apppaths.hpp"
#include "daycontroller.hpp"
#include "eventlog.hpp"
#include "fileprogressstore.hpp"
#include "language.hpp"
#include "settings.hpp"
#include "singleinstance.hpp"
#include "spotifyplayer.hpp"
#include "systemclock.hpp"
#include "templateeditor.hpp"
#include "trayicon.hpp"
#include "wakewatcher.hpp"

#include <cadence/core/version.hpp>
#include <cadence/platform/autostart.hpp>
#include <cadence/spotify/auth.hpp>
#include <cadence/spotify/client.hpp>

#include <QApplication>
#include <QCommandLineParser>
#include <QFont>
#include <QFontDatabase>
#include <QMetaEnum>
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

#ifdef QT_DEBUG
// A test session must never touch the real launch at login registration.
class NullAutostart final : public cadence::platform::Autostart {
public:
    bool isEnabled() const override { return enabled_; }
    bool setEnabled(bool enabled) override {
        enabled_ = enabled;
        return true;
    }
    QString location() const override { return u"test session (not registered)"_s; }

private:
    bool enabled_ = false;
};
#endif

bool isTestSession() {
#ifdef QT_DEBUG
    return !qgetenv("CADENCE_TEST_DATA_DIR").isEmpty();
#else
    return false;
#endif
}

QString instanceKey() {
    QString user = QString::fromLocal8Bit(qgetenv("USERNAME"));
    if (user.isEmpty()) {
        user = QString::fromLocal8Bit(qgetenv("USER"));
    }
    QString key = u"cadence-"_s + user;
    // A test session against its own data directory must not wake the real instance instead.
    if (isTestSession()) {
        key += u"-test"_s;
    }
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
    const QCommandLineOption enableAutostart(u"enable-autostart"_s,
                                             u"Register Cadence to launch at login and exit."_s);
    const QCommandLineOption disableAutostart(u"disable-autostart"_s,
                                              u"Remove the launch at login registration and exit."_s);
    parser.addOptions({minimized, enableAutostart, disableAutostart});
    parser.process(app);

    std::unique_ptr<cadence::platform::Autostart> autostart =
        cadence::platform::createAutostart(QCoreApplication::applicationFilePath());
#ifdef QT_DEBUG
    if (isTestSession()) {
        autostart = std::make_unique<NullAutostart>();
    }
#endif
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

    Settings settings(apppaths::configDir() + u"/settings.json"_s, *autostart);
    Settings::setInstance(&settings);

    EventLog eventLog(apppaths::dataDir() + u"/logs"_s);
    EventLog::setInstance(&eventLog);
    eventLog.write(u"start version %1"_s.arg(QString::fromUtf8(cadence::core::versionString())));

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

    QObject::connect(&controller, &DayController::alarmRaised, &eventLog,
                     [&eventLog](int kind, int blockIndex, const QString& title, const QString&) {
                         const QMetaEnum names = QMetaEnum::fromType<DayController::Alarm>();
                         eventLog.write(
                             u"alarm %1 block=%2 %3"_s.arg(QString::fromLatin1(names.valueToKey(kind)))
                                 .arg(blockIndex)
                                 .arg(title));
                     });
    QObject::connect(&controller, &DayController::pushupPrompt, &eventLog,
                     [&eventLog](int blockIndex, int setIndex) {
                         eventLog.write(u"prompt pushups block=%1 set=%2"_s.arg(blockIndex).arg(setIndex));
                     });
    QObject::connect(&controller, &DayController::pushupsLogged, &eventLog,
                     [&eventLog](int blockIndex, int reps) {
                         eventLog.write(u"pushups block=%1 reps=%2"_s.arg(blockIndex).arg(reps));
                     });
    QObject::connect(&controller, &DayController::sessionRestored, &eventLog,
                     [&eventLog](int blockIndex, const QString& phase, int remaining) {
                         eventLog.write(u"session restored block=%1 phase=%2 remaining=%3s"_s.arg(blockIndex)
                                            .arg(phase)
                                            .arg(remaining));
                     });

    // Settings drive the runtime directly; the screen only edits them.
    const auto applySettings = [&controller, &settings] {
        controller.setMaxSnoozes(settings.maxSnoozes());
        controller.setWarnDayNoLongerFits(settings.warnDayNoLongerFits());
    };
    applySettings();
    QObject::connect(&settings, &Settings::changed, &controller, applySettings);

    // Spotify: the auth and client live for the whole run; the player model faces QML.
    cadence::spotify::SpotifyAuth spotifyAuth;
    cadence::spotify::SpotifyClient spotifyClient(spotifyAuth);
    SpotifyPlayer spotify(settings, spotifyAuth, spotifyClient);
    SpotifyPlayer::setInstance(&spotify);
    QObject::connect(&spotifyAuth, &cadence::spotify::SpotifyAuth::logMessage, &eventLog, &EventLog::write);
    QObject::connect(&spotifyClient, &cadence::spotify::SpotifyClient::logMessage, &eventLog,
                     &EventLog::write);
    QObject::connect(&spotify, &SpotifyPlayer::logMessage, &eventLog, &EventLog::write);
    QObject::connect(&controller, &DayController::blockStarted, &spotify, &SpotifyPlayer::onBlockStarted);
    spotify.restore();

    WakeWatcher wakeWatcher;
    QObject::connect(&wakeWatcher, &WakeWatcher::wakeDetected, &controller, &DayController::evaluateNow);

    // With a tray the window only hides on close and Quit is the way out.
    const bool trayAvailable = TrayController::isAvailable();
    QApplication::setQuitOnLastWindowClosed(!trayAvailable);

    QQmlApplicationEngine engine;
    // The translator must be in place before the first QML loads; later changes retranslate live.
    LanguageManager language(engine);
    LanguageManager::setInstance(&language);
    language.apply(settings.language());
    QObject::connect(&settings, &Settings::changed, &language,
                     [&language, &settings] { language.apply(settings.language()); });
    QObject::connect(&language, &LanguageManager::changed, &controller, &DayController::evaluateNow);
    QObject::connect(&language, &LanguageManager::changed, &editor, &TemplateEditor::retranslate);
    QObject::connect(&language, &LanguageManager::changed, &spotify, &SpotifyPlayer::changed);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    const bool startHidden = (parser.isSet(minimized) || settings.startMinimized()) && trayAvailable;
    engine.setInitialProperties({
        {u"visible"_s, !startHidden},
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
        QObject::connect(tray.get(), &TrayController::showRequested, window,
                         [window] { showWindow(window); });
        QObject::connect(tray.get(), &TrayController::quitRequested, &app, &QCoreApplication::quit);
        QObject::connect(tray.get(), &TrayController::autostartChanged, &settings, &Settings::refresh);
        QObject::connect(&settings, &Settings::changed, tray.get(), &TrayController::syncAutostart);
        QObject::connect(&language, &LanguageManager::changed, tray.get(), &TrayController::retranslate);
    }
    QObject::connect(&instance, &SingleInstance::commandReceived, window,
                     [window](const QByteArray& command) {
                         if (command == "show") {
                             showWindow(window);
                         }
                     });

    AlarmSounds sounds;
    sounds.setMode(settings.soundMode());
    QObject::connect(&settings, &Settings::changed, &sounds,
                     [&sounds, &settings] { sounds.setMode(settings.soundMode()); });
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
