#include "apppaths.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtGlobal>

using namespace Qt::StringLiterals;

namespace apppaths {

namespace {

QString testOverride(const char* name) {
#ifdef QT_DEBUG
    return QString::fromLocal8Bit(qgetenv(name));
#else
    Q_UNUSED(name);
    return {};
#endif
}

QString readAll(const QString& path, QString& error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = u"cannot read %1: %2"_s.arg(path, file.errorString());
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

bool copyBundledTemplate(const QString& target, QString& error) {
    if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
        error = u"cannot create %1"_s.arg(QFileInfo(target).absolutePath());
        return false;
    }
    QFile bundled(u":/templates/default.json"_s);
    if (!bundled.open(QIODevice::ReadOnly)) {
        error = u"bundled default template is missing"_s;
        return false;
    }
    QSaveFile out(target);
    if (!out.open(QIODevice::WriteOnly)) {
        error = u"cannot write %1: %2"_s.arg(target, out.errorString());
        return false;
    }
    out.write(bundled.readAll());
    if (!out.commit()) {
        error = u"cannot write %1: %2"_s.arg(target, out.errorString());
        return false;
    }
    return true;
}

} // namespace

QString configDir() {
    const QString override = testOverride("CADENCE_TEST_DATA_DIR");
    if (!override.isEmpty()) {
        return override + u"/config"_s;
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QString dataDir() {
    const QString override = testOverride("CADENCE_TEST_DATA_DIR");
    if (!override.isEmpty()) {
        return override + u"/data"_s;
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString weekTemplatePath() {
    const QString override = testOverride("CADENCE_TEST_TEMPLATE");
    if (!override.isEmpty()) {
        return override;
    }
    return configDir() + u"/templates/week.json"_s;
}

QString progressDir() {
    return dataDir() + u"/progress"_s;
}

TemplateLoad loadWeekTemplate() {
    TemplateLoad result;
    result.path = weekTemplatePath();

    if (!QFileInfo::exists(result.path)) {
        if (!copyBundledTemplate(result.path, result.error)) {
            return result;
        }
    }

    const QString text = readAll(result.path, result.error);
    if (!result.error.isEmpty()) {
        return result;
    }
    try {
        result.document = cadence::core::loadTemplateDocument(text.toStdString());
    } catch (const cadence::core::TemplateError& error) {
        result.error = u"%1: %2"_s.arg(result.path, QString::fromUtf8(error.what()));
    }
    return result;
}

} // namespace apppaths
