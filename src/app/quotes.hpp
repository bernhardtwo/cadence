#pragma once

#include <cadence/core/quotes.hpp>

#include <QObject>
#include <QQmlEngine>
#include <QString>

#include <map>
#include <string>

// The quotes catalog and one shuffle bag per context for QML. A surface asks for current() with a
// key that changes only when it appears anew or its phase changes, so re-created delegates and
// re-renders get the same id back; the texts are looked up by id and language so a language change
// swaps the wording without touching the selection. Bag state is written after every draw so a
// restart continues the round.
class QuoteProvider : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Quotes)
    QML_SINGLETON

    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QString error READ error CONSTANT)

public:
    // No default constructor on purpose: QML must call create() and get this instance.
    QuoteProvider(const QString& catalogJson, QString stateFile, QObject* parent = nullptr);
    ~QuoteProvider() override;

    static QuoteProvider* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static void setInstance(QuoteProvider* instance);

    bool available() const { return available_; }
    QString error() const { return error_; }

    // The quote shown on a surface for a phase key: drawn once when the key first appears, then
    // returned unchanged. Empty for an unknown group.
    Q_INVOKABLE QString current(const QString& surface, const QString& phaseKey, const QString& context);
    // Another quote of the group for the same surface and key, when the current one does not fit.
    // After one attempt per group member it settles on no quote at all.
    Q_INVOKABLE QString replace(const QString& surface, const QString& phaseKey, const QString& context);
    // The next quote of the group with no surface memory, or an empty id for an unknown group.
    Q_INVOKABLE QString pick(const QString& context);
    Q_INVOKABLE int groupSize(const QString& context) const;
    Q_INVOKABLE QString text(const QString& id, const QString& language) const;
    Q_INVOKABLE QString attribution(const QString& id, const QString& language) const;
    Q_INVOKABLE QString original(const QString& id) const;
    // "la" or "grc".
    Q_INVOKABLE QString originalLanguage(const QString& id) const;

private:
    struct Shown {
        QString phaseKey;
        QString id;
        int attempts = 0;
    };

    void saveStates();

    std::map<QString, Shown> shown_;

    cadence::core::QuoteCatalog catalog_;
    std::map<std::string, cadence::core::ShuffleBag> bags_;
    cadence::core::BagStates savedStates_;
    QString stateFile_;
    bool available_ = false;
    QString error_;
};
