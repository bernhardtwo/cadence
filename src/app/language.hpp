#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QTranslator>

// Installs the UI translation for the language setting and retranslates the running QML at once.
// English is the source language, so it installs no translator. The default QLocale follows the
// language so dates and weekday names come out in it too.
class LanguageManager : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Language)
    QML_SINGLETON

    // The language in use: en, es or fr.
    Q_PROPERTY(QString current READ current NOTIFY changed)
    // What "system" resolves to on this machine.
    Q_PROPERTY(QString systemLanguage READ systemLanguage CONSTANT)
    Q_PROPERTY(QStringList codes READ codes CONSTANT)

public:
    explicit LanguageManager(QObject* parent = nullptr);
    ~LanguageManager() override;

    static LanguageManager* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static void setInstance(LanguageManager* instance);

    // The engine whose bindings are retranslated on a change; set before the first apply.
    void setEngine(QQmlEngine* engine);

    // Resolves the setting ("system", "en", "es", "fr") and switches to it. Does nothing when the
    // resolved language is already in use.
    void apply(const QString& setting);

    QString current() const { return current_; }
    QString systemLanguage() const;
    QStringList codes() const;

    // The language's own name, capitalized: English, Español, Français.
    Q_INVOKABLE QString nativeName(const QString& code) const;

signals:
    void changed();

private:
    QQmlEngine* engine_ = nullptr;
    QTranslator translator_;
    bool installed_ = false;
    QString current_;
};
