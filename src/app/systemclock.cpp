#include "systemclock.hpp"

#include <QDate>
#include <QDateTime>
#include <QTime>

cadence::core::Instant SystemClock::now() const {
    const QDateTime local = QDateTime::currentDateTime();
    const QDate date = local.date();
    const QTime time = local.time();
    const std::chrono::year_month_day day{std::chrono::year{date.year()},
                                          std::chrono::month{static_cast<unsigned>(date.month())},
                                          std::chrono::day{static_cast<unsigned>(date.day())}};
    return std::chrono::local_days{day} +
           std::chrono::seconds{time.hour() * 3600 + time.minute() * 60 + time.second()};
}
