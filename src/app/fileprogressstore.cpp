#include "fileprogressstore.hpp"

#include <cadence/core/progress_json.hpp>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

using namespace Qt::StringLiterals;

FileProgressStore::FileProgressStore(QString directory) : directory_(std::move(directory)) {}

QString FileProgressStore::pathFor(cadence::core::Date date) const {
    const int year = static_cast<int>(date.year());
    const auto month = static_cast<unsigned>(date.month());
    const auto day = static_cast<unsigned>(date.day());
    return u"%1/%2-%3-%4.json"_s.arg(directory_)
        .arg(year, 4, 10, QChar(u'0'))
        .arg(month, 2, 10, QChar(u'0'))
        .arg(day, 2, 10, QChar(u'0'));
}

std::optional<cadence::core::DayProgress> FileProgressStore::load(cadence::core::Date date) {
    lastError_.clear();
    const QString path = pathFor(date);
    if (!QFileInfo::exists(path)) {
        return std::nullopt;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        lastError_ = u"cannot read %1: %2"_s.arg(path, file.errorString());
        return std::nullopt;
    }
    try {
        return cadence::core::parseDayProgress(file.readAll().toStdString());
    } catch (const cadence::core::ProgressError& error) {
        lastError_ = u"%1: %2"_s.arg(path, QString::fromUtf8(error.what()));
        return std::nullopt;
    }
}

bool FileProgressStore::save(cadence::core::Date date, const cadence::core::DayProgress& progress) {
    lastError_.clear();
    if (!QDir().mkpath(directory_)) {
        lastError_ = u"cannot create %1"_s.arg(directory_);
        return false;
    }
    const QString path = pathFor(date);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        lastError_ = u"cannot write %1: %2"_s.arg(path, file.errorString());
        return false;
    }
    file.write(QByteArray::fromStdString(cadence::core::serializeDayProgress(progress)));
    if (!file.commit()) {
        lastError_ = u"cannot write %1: %2"_s.arg(path, file.errorString());
        return false;
    }
    return true;
}
