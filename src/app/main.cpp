#include "apppaths.hpp"
#include "daycontroller.hpp"
#include "fileprogressstore.hpp"
#include "systemclock.hpp"
#include "wakewatcher.hpp"

#include <cadence/core/version.hpp>

#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStringList>
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

} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(u"Cadence"_s);
    QGuiApplication::setOrganizationName(u"Cadence"_s);
    QGuiApplication::setApplicationVersion(QString::fromUtf8(cadence::core::versionString()));

    // Basic is the only style whose controls we fully override; it must be set before any QML loads.
    QQuickStyle::setStyle(u"Basic"_s);

    loadBundledFonts();
    QFont bodyFont(u"Archivo"_s);
    bodyFont.setPixelSize(15);
    QGuiApplication::setFont(bodyFont);

    apppaths::TemplateLoad templateLoad = apppaths::loadWeekTemplate();
    if (!templateLoad.error.isEmpty()) {
        qWarning("%s", qPrintable(templateLoad.error));
    }

    DayController controller(std::make_unique<SystemClock>(),
                             std::make_unique<FileProgressStore>(apppaths::progressDir()),
                             std::move(templateLoad.document), templateLoad.error);
    DayController::setInstance(&controller);

    WakeWatcher wakeWatcher;
    QObject::connect(&wakeWatcher, &WakeWatcher::wakeDetected, &controller, &DayController::evaluateNow);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule(u"Cadence"_s, u"Main"_s);

    controller.startTicking();
    return QGuiApplication::exec();
}
