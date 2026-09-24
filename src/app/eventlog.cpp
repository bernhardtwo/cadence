#include "eventlog.hpp"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <QtLogging>

#include <utility>

using namespace Qt::StringLiterals;

namespace {

EventLog* s_instance = nullptr;

} // namespace

EventLog::EventLog(QString directory, QObject* parent) : QObject(parent), directory_(std::move(directory)) {
    prune();
}

EventLog::~EventLog() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

EventLog* EventLog::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine) {
    Q_UNUSED(qmlEngine);
    Q_ASSERT(s_instance != nullptr);
    Q_ASSERT(jsEngine->thread() == s_instance->thread());
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void EventLog::setInstance(EventLog* instance) {
    s_instance = instance;
}

QString EventLog::todayPath() const {
    return u"%1/%2.log"_s.arg(directory_, QDate::currentDate().toString(Qt::ISODate));
}

void EventLog::write(const QString& line) {
    if (!QDir().mkpath(directory_)) {
        if (!warned_) {
            qWarning("cannot create %s", qPrintable(directory_));
            warned_ = true;
        }
        return;
    }
    QFile file(todayPath());
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        if (!warned_) {
            qWarning("cannot write %s: %s", qPrintable(file.fileName()), qPrintable(file.errorString()));
            warned_ = true;
        }
        return;
    }
    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString(u"HH:mm:ss"_s) << "  " << line << '\n';
}

int EventLog::prune() {
    static const QRegularExpression pattern(u"^(\\d{4}-\\d{2}-\\d{2})\\.log$"_s);
    const QDate oldest = QDate::currentDate().addDays(-(retentionDays - 1));
    int removed = 0;
    for (const QString& name : QDir(directory_).entryList(QDir::Files)) {
        const auto match = pattern.match(name);
        if (!match.hasMatch()) {
            continue;
        }
        const QDate date = QDate::fromString(match.captured(1), Qt::ISODate);
        if (date.isValid() && date < oldest && QFile::remove(directory_ + u"/"_s + name)) {
            ++removed;
        }
    }
    return removed;
}
