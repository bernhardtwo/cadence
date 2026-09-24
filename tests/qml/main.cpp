#include <QCoreApplication>
#include <QList>
#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QString>
#include <QTranslator>
#include <QUrl>
#include <QtQuickTest/quicktest.h>

using namespace Qt::StringLiterals;

// Installs the real compiled catalogs of the app so a test can watch qsTr bindings retranslate.
class TestLanguage : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString current READ current WRITE setCurrent NOTIFY changed)

public:
    explicit TestLanguage(QQmlEngine* engine) : QObject(engine), engine_(engine) {}

    QString current() const { return current_; }
    void setCurrent(const QString& code) {
        if (code == current_) {
            return;
        }
        current_ = code;
        emit changed();
    }

    Q_INVOKABLE bool install(const QString& code) {
        QCoreApplication::removeTranslator(&translator_);
        const bool loaded =
            translator_.load(QString::fromUtf8(CADENCE_QM_DIR) + u"/cadence_"_s + code + u".qm"_s);
        if (loaded) {
            QCoreApplication::installTranslator(&translator_);
        }
        engine_->retranslate();
        setCurrent(code);
        return loaded;
    }

    Q_INVOKABLE QString nativeName(const QString& code) const { return code; }

signals:
    void changed();

private:
    QQmlEngine* engine_;
    QTranslator translator_;
    QString current_ = u"en"_s;
};

// Stand-ins for the app singletons QuoteBlock reads, so the component can be tested alone. The
// texts carry the id and the language, which is what the visibility and retranslation tests check.
class TestQuotes : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    using QObject::QObject;

    bool available() const { return true; }
    Q_INVOKABLE QString current(const QString&, const QString& phaseKey, const QString& context) {
        ++draws;
        return context + u"-"_s + phaseKey;
    }
    Q_INVOKABLE QString replace(const QString&, const QString&, const QString&) {
        ++replacements;
        return QString();
    }
    Q_INVOKABLE QString pick(const QString& context) { return context + u"-picked"_s; }
    Q_INVOKABLE QString sample(const QString& context, int index) {
        return context + u"-sample-"_s + QString::number(index);
    }
    Q_INVOKABLE int groupSize(const QString&) const { return 3; }
    Q_INVOKABLE QString text(const QString& id, const QString& language) const {
        return id.isEmpty() ? QString() : u"Quote %1 in %2"_s.arg(id, language);
    }
    Q_INVOKABLE QString attribution(const QString& id, const QString& language) const {
        return id.isEmpty() ? QString() : u"Author %1 · Work, 1"_s.arg(language);
    }
    Q_INVOKABLE QString original(const QString& id) const {
        return id.isEmpty() ? QString() : u"Originale"_s;
    }
    Q_INVOKABLE QString originalLanguage(const QString&) const { return u"la"_s; }

    Q_INVOKABLE int drawCount() const { return draws; }
    Q_INVOKABLE int replaceCount() const { return replacements; }

private:
    int draws = 0;
    int replacements = 0;
};

class TestSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool showQuotes READ showQuotes WRITE setShowQuotes NOTIFY changed)
    Q_PROPERTY(bool showQuoteOriginals READ showQuoteOriginals WRITE setShowQuoteOriginals NOTIFY changed)

public:
    using QObject::QObject;

    bool showQuotes() const { return showQuotes_; }
    void setShowQuotes(bool show) {
        showQuotes_ = show;
        emit changed();
    }
    bool showQuoteOriginals() const { return showOriginals_; }
    void setShowQuoteOriginals(bool show) {
        showOriginals_ = show;
        emit changed();
    }

signals:
    void changed();

private:
    bool showQuotes_ = true;
    bool showOriginals_ = false;
};

// Stand-in for the day controller: records the restores a component asks for.
class TestDayController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int restoreCount READ restoreCount NOTIFY changed)
    Q_PROPERTY(int lastRestored READ lastRestored NOTIFY changed)

public:
    using QObject::QObject;

    int restoreCount() const { return restores_.size(); }
    int lastRestored() const { return restores_.isEmpty() ? -1 : restores_.last(); }
    Q_INVOKABLE void restore(int blockIndex) {
        restores_.push_back(blockIndex);
        emit changed();
    }
    Q_INVOKABLE void reset() {
        restores_.clear();
        emit changed();
    }

signals:
    void changed();

private:
    QList<int> restores_;
};

class Setup : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine* engine) {
        // The Cadence module of the app is not linked here; these doubles answer the same names.
        auto* language = new TestLanguage(engine);
        auto* quotes = new TestQuotes(engine);
        auto* settings = new TestSettings(engine);
        auto* day = new TestDayController(engine);
        qmlRegisterModule("Cadence", 1, 0);
        qmlRegisterSingletonInstance("Cadence", 1, 0, "Language", language);
        qmlRegisterSingletonInstance("Cadence", 1, 0, "Quotes", quotes);
        qmlRegisterSingletonInstance("Cadence", 1, 0, "Settings", settings);
        qmlRegisterSingletonInstance("Cadence", 1, 0, "DayController", day);
        // The module's QML files live in the app's resources; the one component a row under test
        // instantiates is registered from its source instead.
        qmlRegisterType(QUrl::fromLocalFile(QString::fromUtf8(CADENCE_QML_SOURCE_DIR) + u"/SignalButton.qml"_s),
                        "Cadence", 1, 0, "SignalButton");
        engine->rootContext()->setContextProperty(u"testLanguage"_s, language);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(cadence_qml, Setup)

#include "main.moc"
