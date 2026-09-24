#include "quotes.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QtLogging>

#include <random>
#include <utility>

using namespace Qt::StringLiterals;
using namespace cadence::core;

namespace {

QuoteProvider* s_instance = nullptr;

} // namespace

QuoteProvider::QuoteProvider(const QString& catalogJson, QString stateFile, QObject* parent)
    : QObject(parent), stateFile_(std::move(stateFile)) {
    try {
        catalog_ = parseQuoteCatalog(catalogJson.toStdString());
        available_ = true;
    } catch (const QuoteError& error) {
        error_ = QString::fromUtf8(error.what());
        qWarning("quotes: %s", qPrintable(error_));
        return;
    }
    QFile file(stateFile_);
    if (file.open(QIODevice::ReadOnly)) {
        savedStates_ = parseBagStates(file.readAll().toStdString());
    }
}

QuoteProvider::~QuoteProvider() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

QuoteProvider* QuoteProvider::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine) {
    Q_UNUSED(qmlEngine);
    Q_ASSERT(s_instance != nullptr);
    Q_ASSERT(jsEngine->thread() == s_instance->thread());
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void QuoteProvider::setInstance(QuoteProvider* instance) {
    s_instance = instance;
}

QString QuoteProvider::pick(const QString& context) {
    const auto parsed = parseQuoteContext(context.toStdString());
    if (!available_ || !parsed) {
        return {};
    }
    const std::string key = context.toStdString();
    auto bag = bags_.find(key);
    if (bag == bags_.end()) {
        const auto saved = savedStates_.find(key);
        const ShuffleBag::State state = saved == savedStates_.end() ? ShuffleBag::State{} : saved->second;
        std::random_device device;
        bag = bags_.emplace(key, ShuffleBag(catalog_.idsIn(*parsed), state, device())).first;
    }
    const QString id = QString::fromStdString(bag->second.next());
    saveStates();
    return id;
}

QString QuoteProvider::current(const QString& surface, const QString& phaseKey, const QString& context) {
    auto shown = shown_.find(surface);
    if (shown != shown_.end() && shown->second.phaseKey == phaseKey) {
        return shown->second.id;
    }
    const QString id = pick(context);
    shown_[surface] = Shown{phaseKey, id, 0};
    return id;
}

QString QuoteProvider::replace(const QString& surface, const QString& phaseKey, const QString& context) {
    Shown& shown = shown_[surface];
    if (shown.phaseKey != phaseKey) {
        shown = Shown{phaseKey, QString(), 0};
    }
    if (shown.attempts >= groupSize(context)) {
        shown.id.clear();
        return {};
    }
    shown.attempts += 1;
    shown.id = pick(context);
    return shown.id;
}

int QuoteProvider::groupSize(const QString& context) const {
    const auto parsed = parseQuoteContext(context.toStdString());
    return available_ && parsed ? static_cast<int>(catalog_.idsIn(*parsed).size()) : 0;
}

QString QuoteProvider::text(const QString& id, const QString& language) const {
    const Quote* quote = catalog_.find(id.toStdString());
    return quote ? QString::fromStdString(quote->text.in(language.toStdString())) : QString();
}

QString QuoteProvider::attribution(const QString& id, const QString& language) const {
    const Quote* quote = catalog_.find(id.toStdString());
    return quote ? QString::fromStdString(catalog_.attribution(*quote, language.toStdString())) : QString();
}

QString QuoteProvider::original(const QString& id) const {
    const Quote* quote = catalog_.find(id.toStdString());
    return quote ? QString::fromStdString(quote->original) : QString();
}

QString QuoteProvider::originalLanguage(const QString& id) const {
    const Quote* quote = catalog_.find(id.toStdString());
    return quote ? QString::fromStdString(quote->language) : QString();
}

void QuoteProvider::saveStates() {
    BagStates states = savedStates_;
    for (const auto& [context, bag] : bags_) {
        states[context] = bag.state();
    }
    if (!QDir().mkpath(QFileInfo(stateFile_).absolutePath())) {
        qWarning("quotes: cannot create %s", qPrintable(QFileInfo(stateFile_).absolutePath()));
        return;
    }
    QSaveFile file(stateFile_);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("quotes: cannot write %s", qPrintable(stateFile_));
        return;
    }
    file.write(QByteArray::fromStdString(serializeBagStates(states)));
    if (!file.commit()) {
        qWarning("quotes: cannot write %s", qPrintable(stateFile_));
    }
}
