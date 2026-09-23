#pragma once

#include "progressstore.hpp"

// One JSON file per date under a directory, written atomically through QSaveFile.
class FileProgressStore final : public ProgressStore {
public:
    explicit FileProgressStore(QString directory);

    std::optional<cadence::core::DayProgress> load(cadence::core::Date date) override;
    bool save(cadence::core::Date date, const cadence::core::DayProgress& progress) override;
    QString lastError() const override { return lastError_; }

    QString pathFor(cadence::core::Date date) const;

private:
    QString directory_;
    QString lastError_;
};
