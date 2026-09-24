#include "language.hpp"

#include <cadence/core/language.hpp>

#include <QCoreApplication>
#include <QLocale>
#include <QtLogging>

#include <string>
#include <vector>

using namespace Qt::StringLiterals;

namespace {

LanguageManager* s_instance = nullptr;

std::vector<std::string> systemPreferences() {
    std::vector<std::string> out;
    for (const QString& tag : QLocale::system().uiLanguages()) {
        out.push_back(tag.toStdString());
    }
    return out;
}

} // namespace

LanguageManager::LanguageManager(QQmlEngine& engine, QObject* parent) : QObject(parent), engine_(engine) {}

LanguageManager::~LanguageManager() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

LanguageManager* LanguageManager::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine) {
    Q_UNUSED(qmlEngine);
    Q_ASSERT(s_instance != nullptr);
    Q_ASSERT(jsEngine->thread() == s_instance->thread());
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void LanguageManager::setInstance(LanguageManager* instance) {
    s_instance = instance;
}

QString LanguageManager::systemLanguage() const {
    return QString::fromStdString(cadence::core::resolveLanguage("system", systemPreferences()));
}

QStringList LanguageManager::codes() const {
    QStringList out;
    for (const std::string_view code : cadence::core::shippedLanguages) {
        out.push_back(QString::fromUtf8(code.data(), static_cast<qsizetype>(code.size())));
    }
    return out;
}

QStringList LanguageManager::shortDayNames() const {
    const QLocale locale(current_.isEmpty() ? u"en"_s : current_);
    QStringList out;
    for (int day = 1; day <= 7; ++day) {
        QString name = locale.standaloneDayName(day, QLocale::ShortFormat);
        // CLDR abbreviates with a trailing period in Spanish and French; the pills do not want it.
        if (name.endsWith(u'.')) {
            name.chop(1);
        }
        out.push_back(name);
    }
    return out;
}

QString LanguageManager::nativeName(const QString& code) const {
    // QLocale names the region too ("American English"); the shipped languages want the bare name.
    if (code == u"en"_s) {
        return u"English"_s;
    }
    if (code == u"es"_s) {
        return u"Español"_s;
    }
    if (code == u"fr"_s) {
        return u"Français"_s;
    }
    QString name = QLocale(code).nativeLanguageName();
    if (name.isEmpty()) {
        return code;
    }
    name[0] = name[0].toUpper();
    return name;
}

void LanguageManager::apply(const QString& setting) {
    const QString code =
        QString::fromStdString(cadence::core::resolveLanguage(setting.toStdString(), systemPreferences()));
    if (code == current_) {
        return;
    }
    if (installed_) {
        QCoreApplication::removeTranslator(&translator_);
        installed_ = false;
    }
    // English is the source language, but its catalog still carries the plural forms.
    if (translator_.load(u":/i18n/cadence_"_s + code + u".qm"_s)) {
        QCoreApplication::installTranslator(&translator_);
        installed_ = true;
    } else {
        qWarning("No translation bundled for %s", qPrintable(code));
    }
    QLocale::setDefault(QLocale(code));
    current_ = code;
    engine_.retranslate();
    emit changed();
}
