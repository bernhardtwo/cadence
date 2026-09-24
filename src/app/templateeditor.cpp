#include "templateeditor.hpp"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QVariantMap>

#include <algorithm>
#include <array>
#include <set>
#include <utility>

using namespace Qt::StringLiterals;
using namespace cadence::core;

namespace {

TemplateEditor* s_instance = nullptr;

constexpr std::array<const char*, 7> weekdayKeys{"mon", "tue", "wed", "thu", "fri", "sat", "sun"};

// Colors handed to activities the user creates from the editor, in order of creation.
constexpr std::array<const char*, 7> palette{"#D6FF3F", "#FF6B3D", "#7FB8FF", "#C9A0FF",
                                             "#F2F0EA", "#5EE0A0", "#FFD166"};

QString kindName(BlockKind kind) {
    switch (kind) {
    case BlockKind::Anchored:
        return u"Anchored"_s;
    case BlockKind::Flexible:
        return u"Flexible"_s;
    case BlockKind::Soft:
        return u"Soft"_s;
    }
    return {};
}

QString kindLabel(BlockKind kind) {
    switch (kind) {
    case BlockKind::Anchored:
        return TemplateEditor::tr("Anchored");
    case BlockKind::Flexible:
        return TemplateEditor::tr("Flexible");
    case BlockKind::Soft:
        return TemplateEditor::tr("Soft");
    }
    return {};
}

std::optional<BlockKind> parseKind(const QString& text) {
    const QString lower = text.toLower();
    if (lower == u"anchored"_s) {
        return BlockKind::Anchored;
    }
    if (lower == u"flexible"_s) {
        return BlockKind::Flexible;
    }
    if (lower == u"soft"_s) {
        return BlockKind::Soft;
    }
    return std::nullopt;
}

QString timeText(std::optional<Minutes> value) {
    return value ? QString::fromStdString(formatTimeOfDay(*value)) : QString();
}

int minutesOf(Minutes value) {
    return static_cast<int>(value.count());
}

QString slugOf(const QString& name) {
    QString slug;
    for (const QChar c : name.toLower()) {
        if (c.isLetterOrNumber()) {
            slug += c;
        } else if (!slug.isEmpty() && !slug.endsWith(u'-')) {
            slug += u'-';
        }
    }
    while (slug.endsWith(u'-')) {
        slug.chop(1);
    }
    return slug.isEmpty() ? u"activity"_s : slug;
}

// "week.mon.blocks[2]" or "week.mon.dayStart" to a weekday index and a block index, -1 when absent.
std::pair<int, int> locate(const QString& location) {
    int day = -1;
    int block = -1;
    for (std::size_t i = 0; i < weekdayKeys.size(); ++i) {
        if (location.startsWith(u"week."_s + QLatin1StringView(weekdayKeys[i]))) {
            day = static_cast<int>(i);
            break;
        }
    }
    static const QRegularExpression blockPattern(u"blocks\\[(\\d+)\\]"_s);
    if (const auto match = blockPattern.match(location); match.hasMatch()) {
        block = match.captured(1).toInt();
    }
    return {day, block};
}

} // namespace

TemplateEditor::TemplateEditor(std::optional<TemplateDocument> document, QString path, QObject* parent)
    : QObject(parent), document_(document.value_or(TemplateDocument{})), saved_(document_),
      path_(std::move(path)) {
    if (const DayTemplate* day = currentDay(); day && !day->blocks.empty()) {
        block_ = 0;
    }
    revalidate();
}

TemplateEditor::~TemplateEditor() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

TemplateEditor* TemplateEditor::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine) {
    Q_UNUSED(qmlEngine);
    Q_ASSERT(s_instance != nullptr);
    Q_ASSERT(jsEngine->thread() == s_instance->thread());
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void TemplateEditor::setInstance(TemplateEditor* instance) {
    s_instance = instance;
}

DayTemplate* TemplateEditor::currentDay() {
    if (day_ < 0 || day_ >= 7) {
        return nullptr;
    }
    auto& day = document_.week.days[static_cast<std::size_t>(day_)];
    return day ? &*day : nullptr;
}

const DayTemplate* TemplateEditor::currentDay() const {
    if (day_ < 0 || day_ >= 7) {
        return nullptr;
    }
    const auto& day = document_.week.days[static_cast<std::size_t>(day_)];
    return day ? &*day : nullptr;
}

BlockTemplate* TemplateEditor::blockAt(int index) {
    DayTemplate* day = currentDay();
    if (day == nullptr || index < 0 || static_cast<std::size_t>(index) >= day->blocks.size()) {
        return nullptr;
    }
    return &day->blocks[static_cast<std::size_t>(index)];
}

void TemplateEditor::touch() {
    dirty_ = document_ != saved_ || !fieldErrors_.empty();
    revalidate();
}

// The user's wording of a core issue; core keeps the English message for the command line.
QString TemplateEditor::issueText(const ValidationIssue& issue) {
    const auto arg = [&issue](std::size_t index) {
        return index < issue.args.size() ? QString::fromStdString(issue.args[index]) : QString();
    };
    switch (issue.code) {
    case IssueCode::UnknownActivity:
        return tr("unknown activity id \"%1\"").arg(arg(0));
    case IssueCode::StartRequired: {
        const auto kind = parseKind(arg(0));
        return tr("%1 blocks need a start time").arg(kind ? kindLabel(*kind).toLower() : arg(0));
    }
    case IssueCode::StartNotAllowed:
        return tr("flexible blocks are placed in sequence and do not take a start time");
    case IssueCode::StartOutOfRange:
        return tr("start must lie between 00:00 and 23:59");
    case IssueCode::DurationNotPositive:
        return tr("duration must be greater than zero");
    case IssueCode::FocusNotPositive:
        return tr("pomodoro focus must be greater than zero");
    case IssueCode::BreakNegative:
        return tr("pomodoro breaks cannot be negative");
    case IssueCode::LongBreakEveryNegative:
        return tr("the long break interval cannot be negative");
    case IssueCode::CountNotPositive:
        return tr("pomodoro count must be greater than zero");
    case IssueCode::DurationMissing:
        return tr("a block needs a duration or a pomodoro plan with a count");
    case IssueCode::ZeroResolvedDuration:
        return tr("the pomodoro plan resolves to a zero duration");
    case IssueCode::CrossesMidnight:
        return tr("block crosses midnight (%1 plus %2 minutes)").arg(arg(0), arg(1));
    case IssueCode::DayStartOutOfRange:
        return tr("day start must lie between 00:00 and 23:59");
    case IssueCode::DayCutoffOutOfRange:
        return tr("cutoff must lie between 00:00 and 23:59");
    case IssueCode::CutoffBeforeStart:
        return tr("cutoff must be later than the day start");
    case IssueCode::AnchoredOverlap:
        return tr("anchored block overlaps block %1 (%2 to %3)").arg(arg(0).toInt() + 1).arg(arg(1), arg(2));
    case IssueCode::ActivityIdEmpty:
        return tr("activity id cannot be empty");
    case IssueCode::DuplicateActivity:
        return tr("duplicate activity id \"%1\"").arg(arg(0));
    case IssueCode::BadColor:
        return tr("expected a color formatted as #RRGGBB");
    }
    return QString::fromStdString(issue.message);
}

// Which field of the block panel an issue belongs to, so the panel can mark it.
QString TemplateEditor::issueField(IssueCode code) {
    switch (code) {
    case IssueCode::StartRequired:
    case IssueCode::StartNotAllowed:
    case IssueCode::StartOutOfRange:
        return u"start"_s;
    case IssueCode::DurationNotPositive:
    case IssueCode::DurationMissing:
    case IssueCode::ZeroResolvedDuration:
        return u"duration"_s;
    default:
        return {};
    }
}

void TemplateEditor::retranslate() {
    revalidate();
}

void TemplateEditor::revalidate() {
    issues_.clear();
    std::vector<ValidationIssue> found = validate(document_);
    for (const ValidationIssue& issue : found) {
        const auto [day, block] = locate(QString::fromStdString(issue.location));
        QVariantMap row;
        row[u"location"_s] = QString::fromStdString(issue.location);
        row[u"message"_s] = issueText(issue);
        row[u"field"_s] = issueField(issue.code);
        row[u"day"_s] = day;
        row[u"block"_s] = block;
        issues_.push_back(row);
    }
    // Core reports identical days once under the first of them; the screen wants them per day.
    const auto& days = document_.week.days;
    const qsizetype coreCount = issues_.size();
    for (std::size_t i = 1; i < days.size(); ++i) {
        if (!days[i]) {
            continue;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (days[j] && *days[j] == *days[i]) {
                for (qsizetype k = 0; k < coreCount; ++k) {
                    QVariantMap row = issues_[k].toMap();
                    if (row[u"day"_s].toInt() == static_cast<int>(j)) {
                        row[u"day"_s] = static_cast<int>(i);
                        issues_.push_back(row);
                    }
                }
                break;
            }
        }
    }
    for (const auto& [key, fieldError] : fieldErrors_) {
        QVariantMap row;
        row[u"location"_s] = key;
        row[u"message"_s] = fieldError.message;
        row[u"field"_s] = key.section(u':', 2);
        row[u"day"_s] = fieldError.day;
        row[u"block"_s] = fieldError.block;
        issues_.push_back(row);
    }
    emit changed();
}

void TemplateEditor::setFieldError(int index, const QString& field, const QString& message) {
    const QString key = u"%1:%2:%3"_s.arg(day_).arg(index).arg(field);
    if (message.isEmpty()) {
        fieldErrors_.erase(key);
    } else {
        fieldErrors_[key] = FieldError{day_, index, message};
    }
}

// Names map to activities: an existing one when the name matches, else a new activity with the
// next palette color. Existing activities are never renamed so other days keep their labels.
ActivityId TemplateEditor::activityFor(const QString& name) {
    const QString trimmed = name.trimmed();
    for (const Activity& activity : document_.activities) {
        if (QString::fromStdString(activity.name).compare(trimmed, Qt::CaseInsensitive) == 0) {
            return activity.id;
        }
    }
    std::set<ActivityId> ids;
    for (const Activity& activity : document_.activities) {
        ids.insert(activity.id);
    }
    const QString base = slugOf(trimmed);
    QString id = base;
    for (int n = 2; ids.contains(id.toStdString()); ++n) {
        id = u"%1-%2"_s.arg(base).arg(n);
    }
    Activity activity;
    activity.id = id.toStdString();
    activity.name = trimmed.isEmpty() ? id.toStdString() : trimmed.toStdString();
    activity.color = palette[document_.activities.size() % palette.size()];
    document_.activities.push_back(activity);
    return activity.id;
}

QString TemplateEditor::activityName(const ActivityId& id) const {
    for (const Activity& activity : document_.activities) {
        if (activity.id == id) {
            return QString::fromStdString(activity.name);
        }
    }
    return QString::fromStdString(id);
}

void TemplateEditor::pruneActivities() {
    std::set<ActivityId> used;
    for (const auto& day : document_.week.days) {
        if (!day) {
            continue;
        }
        for (const BlockTemplate& block : day->blocks) {
            used.insert(block.activityId);
        }
    }
    std::erase_if(document_.activities,
                  [&used](const Activity& activity) { return !used.contains(activity.id); });
}

QVariantList TemplateEditor::plannedDays() const {
    QVariantList list;
    for (const auto& day : document_.week.days) {
        list.push_back(day.has_value());
    }
    return list;
}

bool TemplateEditor::dayPlanned() const {
    return currentDay() != nullptr;
}

QString TemplateEditor::dayStartText() const {
    const DayTemplate* day = currentDay();
    return day ? timeText(day->dayStart) : QString();
}

QString TemplateEditor::dayCutoffText() const {
    const DayTemplate* day = currentDay();
    return day ? timeText(day->dayCutoff) : QString();
}

QStringList TemplateEditor::activityNames() const {
    QStringList names;
    for (const Activity& activity : document_.activities) {
        names.push_back(QString::fromStdString(activity.name));
    }
    return names;
}

QVariantList TemplateEditor::blocks() const {
    QVariantList list;
    const DayTemplate* day = currentDay();
    if (day == nullptr) {
        return list;
    }
    std::set<int> flagged;
    for (const QVariant& issue : issues_) {
        const QVariantMap row = issue.toMap();
        if (row[u"day"_s].toInt() == day_) {
            flagged.insert(row[u"block"_s].toInt());
        }
    }
    for (std::size_t i = 0; i < day->blocks.size(); ++i) {
        const BlockTemplate& block = day->blocks[i];
        QVariantMap row;
        row[u"index"_s] = static_cast<int>(i);
        row[u"name"_s] = activityName(block.activityId);
        row[u"kind"_s] = kindName(block.kind);
        row[u"kindText"_s] = kindLabel(block.kind);
        row[u"startText"_s] = timeText(block.start);
        row[u"durationMinutes"_s] = block.durationMinutes ? minutesOf(*block.durationMinutes) : 0;
        row[u"pomodoroEnabled"_s] = block.pomodoro.has_value();
        row[u"pomodoroCount"_s] = block.pomodoro && block.pomodoro->count ? *block.pomodoro->count : 0;
        row[u"resolvedCount"_s] = resolvedPomodoroCount(block).value_or(0);
        row[u"focus"_s] = block.pomodoro ? minutesOf(block.pomodoro->focus) : 25;
        row[u"shortBreak"_s] = block.pomodoro ? minutesOf(block.pomodoro->shortBreak) : 5;
        row[u"longBreak"_s] = block.pomodoro ? minutesOf(block.pomodoro->longBreak) : 15;
        row[u"longBreakEvery"_s] = block.pomodoro ? block.pomodoro->longBreakEvery : 4;
        row[u"pushups"_s] = block.pushupsOnBreak;
        row[u"fullscreenAlarm"_s] = block.fullscreenAlarm;
        row[u"hasIssue"_s] = flagged.contains(static_cast<int>(i));

        QStringList detail;
        if (const auto duration = resolvedDuration(block)) {
            detail.push_back(tr("%1 min").arg(minutesOf(*duration)));
        }
        if (const auto count = resolvedPomodoroCount(block); count && *count > 0) {
            detail.push_back(tr("%n pomodoro(s)", nullptr, *count));
        }
        if (block.start && block.kind != BlockKind::Flexible) {
            detail.push_back(block.kind == BlockKind::Anchored ? tr("at %1").arg(timeText(block.start))
                                                               : tr("from %1").arg(timeText(block.start)));
        }
        row[u"detail"_s] = detail.join(u" · "_s);
        list.push_back(row);
    }
    return list;
}

// Always fully keyed so bindings never see undefined while the selection moves.
QVariantMap TemplateEditor::block() const {
    const QVariantList all = blocks();
    if (block_ >= 0 && block_ < all.size()) {
        return all[block_].toMap();
    }
    QVariantMap empty;
    empty[u"index"_s] = -1;
    empty[u"name"_s] = QString();
    empty[u"kind"_s] = QString();
    empty[u"kindText"_s] = QString();
    empty[u"startText"_s] = QString();
    empty[u"durationMinutes"_s] = 0;
    empty[u"pomodoroEnabled"_s] = false;
    empty[u"pomodoroCount"_s] = 0;
    empty[u"resolvedCount"_s] = 0;
    empty[u"focus"_s] = 25;
    empty[u"shortBreak"_s] = 5;
    empty[u"longBreak"_s] = 15;
    empty[u"longBreakEvery"_s] = 4;
    empty[u"pushups"_s] = false;
    empty[u"fullscreenAlarm"_s] = true;
    empty[u"hasIssue"_s] = false;
    empty[u"detail"_s] = QString();
    return empty;
}

void TemplateEditor::selectDay(int day) {
    if (day < 0 || day >= 7 || day == day_) {
        return;
    }
    day_ = day;
    block_ = currentDay() && !currentDay()->blocks.empty() ? 0 : -1;
    emit changed();
}

void TemplateEditor::selectBlock(int index) {
    const DayTemplate* day = currentDay();
    const int count = day ? static_cast<int>(day->blocks.size()) : 0;
    block_ = index >= 0 && index < count ? index : -1;
    emit changed();
}

void TemplateEditor::setDayPlanned(bool planned) {
    auto& day = document_.week.days[static_cast<std::size_t>(day_)];
    if (planned == day.has_value()) {
        return;
    }
    if (planned) {
        day = DayTemplate{};
        block_ = -1;
    } else {
        day.reset();
        block_ = -1;
        std::erase_if(fieldErrors_, [this](const auto& entry) { return entry.second.day == day_; });
    }
    touch();
}

void TemplateEditor::setDayStart(const QString& text) {
    DayTemplate* day = currentDay();
    if (day == nullptr) {
        return;
    }
    const auto parsed = parseTimeOfDay(text.trimmed().toStdString());
    setFieldError(-1, u"dayStart"_s, parsed ? QString() : tr("day start must be a time like 08:30"));
    if (parsed) {
        day->dayStart = *parsed;
    }
    touch();
}

void TemplateEditor::setDayCutoff(const QString& text) {
    DayTemplate* day = currentDay();
    if (day == nullptr) {
        return;
    }
    const auto parsed = parseTimeOfDay(text.trimmed().toStdString());
    setFieldError(-1, u"dayCutoff"_s, parsed ? QString() : tr("day cutoff must be a time like 23:00"));
    if (parsed) {
        day->dayCutoff = *parsed;
    }
    touch();
}

void TemplateEditor::copyMondayToWeekdays() {
    auto& days = document_.week.days;
    for (std::size_t i = 1; i <= 4; ++i) {
        days[i] = days[0];
    }
    std::erase_if(fieldErrors_,
                  [](const auto& entry) { return entry.second.day >= 1 && entry.second.day <= 4; });
    if (day_ >= 1 && day_ <= 4) {
        selectBlock(block_);
    }
    touch();
}

void TemplateEditor::addBlock() {
    DayTemplate* day = currentDay();
    if (day == nullptr) {
        return;
    }
    BlockTemplate block;
    block.activityId = activityFor(tr("New block"));
    block.kind = BlockKind::Flexible;
    block.durationMinutes = Minutes{30};
    day->blocks.push_back(block);
    block_ = static_cast<int>(day->blocks.size()) - 1;
    touch();
}

void TemplateEditor::deleteBlock(int index) {
    DayTemplate* day = currentDay();
    if (blockAt(index) == nullptr) {
        return;
    }
    day->blocks.erase(day->blocks.begin() + index);
    std::erase_if(fieldErrors_, [this, index](const auto& entry) {
        return entry.second.day == day_ && entry.second.block == index;
    });
    // Field errors of the blocks that moved up keep their old index; re-key them.
    std::map<QString, FieldError> rekeyed;
    for (auto& [key, fieldError] : fieldErrors_) {
        if (fieldError.day == day_ && fieldError.block > index) {
            fieldError.block -= 1;
            const QString field = key.section(u':', 2);
            rekeyed[u"%1:%2:%3"_s.arg(day_).arg(fieldError.block).arg(field)] = fieldError;
        } else {
            rekeyed[key] = fieldError;
        }
    }
    fieldErrors_ = std::move(rekeyed);
    const int count = static_cast<int>(day->blocks.size());
    block_ = count == 0 ? -1 : std::min(index, count - 1);
    touch();
}

void TemplateEditor::moveBlock(int from, int to) {
    DayTemplate* day = currentDay();
    if (day == nullptr) {
        return;
    }
    const int count = static_cast<int>(day->blocks.size());
    if (from < 0 || from >= count || to < 0 || to >= count || from == to) {
        return;
    }
    BlockTemplate moved = day->blocks[static_cast<std::size_t>(from)];
    day->blocks.erase(day->blocks.begin() + from);
    day->blocks.insert(day->blocks.begin() + to, moved);
    // Field errors follow their block.
    std::map<QString, FieldError> rekeyed;
    for (auto& [key, fieldError] : fieldErrors_) {
        if (fieldError.day == day_ && fieldError.block >= 0) {
            int index = fieldError.block;
            if (index == from) {
                index = to;
            } else if (from < to && index > from && index <= to) {
                index -= 1;
            } else if (to < from && index >= to && index < from) {
                index += 1;
            }
            fieldError.block = index;
            rekeyed[u"%1:%2:%3"_s.arg(day_).arg(index).arg(key.section(u':', 2))] = fieldError;
        } else {
            rekeyed[key] = fieldError;
        }
    }
    fieldErrors_ = std::move(rekeyed);
    if (block_ == from) {
        block_ = to;
    }
    touch();
}

void TemplateEditor::setBlockName(int index, const QString& name) {
    if (BlockTemplate* block = blockAt(index)) {
        if (name.trimmed().isEmpty()) {
            return;
        }
        block->activityId = activityFor(name);
        touch();
    }
}

void TemplateEditor::setBlockKind(int index, const QString& kind) {
    BlockTemplate* block = blockAt(index);
    const auto parsed = parseKind(kind);
    if (block == nullptr || !parsed || block->kind == *parsed) {
        return;
    }
    block->kind = *parsed;
    if (*parsed == BlockKind::Flexible) {
        block->start.reset();
        setFieldError(index, u"start"_s, {});
    } else if (!block->start) {
        block->start = currentDay()->dayStart;
    }
    touch();
}

void TemplateEditor::setBlockStart(int index, const QString& text) {
    BlockTemplate* block = blockAt(index);
    if (block == nullptr) {
        return;
    }
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        block->start.reset();
        setFieldError(index, u"start"_s, {});
    } else if (const auto parsed = parseTimeOfDay(trimmed.toStdString())) {
        block->start = *parsed;
        setFieldError(index, u"start"_s, {});
    } else {
        setFieldError(index, u"start"_s, tr("start must be a time like 09:30"));
    }
    touch();
}

void TemplateEditor::setBlockDuration(int index, int minutes) {
    if (BlockTemplate* block = blockAt(index)) {
        if (minutes > 0) {
            block->durationMinutes = Minutes{minutes};
        } else {
            block->durationMinutes.reset();
        }
        touch();
    }
}

void TemplateEditor::setBlockPomodoroEnabled(int index, bool enabled) {
    BlockTemplate* block = blockAt(index);
    if (block == nullptr || block->pomodoro.has_value() == enabled) {
        return;
    }
    if (enabled) {
        PomodoroPlan plan;
        if (!block->durationMinutes) {
            plan.count = 2;
        }
        block->pomodoro = plan;
    } else {
        block->pomodoro.reset();
        block->pushupsOnBreak = false;
    }
    touch();
}

void TemplateEditor::setBlockPomodoroCount(int index, int count) {
    if (BlockTemplate* block = blockAt(index); block && block->pomodoro) {
        if (count > 0) {
            block->pomodoro->count = count;
        } else {
            block->pomodoro->count.reset();
        }
        touch();
    }
}

void TemplateEditor::setBlockFocus(int index, int minutes) {
    if (BlockTemplate* block = blockAt(index); block && block->pomodoro) {
        block->pomodoro->focus = Minutes{minutes};
        touch();
    }
}

void TemplateEditor::setBlockShortBreak(int index, int minutes) {
    if (BlockTemplate* block = blockAt(index); block && block->pomodoro) {
        block->pomodoro->shortBreak = Minutes{minutes};
        touch();
    }
}

void TemplateEditor::setBlockLongBreak(int index, int minutes) {
    if (BlockTemplate* block = blockAt(index); block && block->pomodoro) {
        block->pomodoro->longBreak = Minutes{minutes};
        touch();
    }
}

void TemplateEditor::setBlockLongBreakEvery(int index, int sessions) {
    if (BlockTemplate* block = blockAt(index); block && block->pomodoro) {
        block->pomodoro->longBreakEvery = sessions;
        touch();
    }
}

void TemplateEditor::setBlockPushups(int index, bool pushups) {
    if (BlockTemplate* block = blockAt(index)) {
        block->pushupsOnBreak = pushups;
        touch();
    }
}

void TemplateEditor::setBlockFullscreenAlarm(int index, bool fullscreen) {
    if (BlockTemplate* block = blockAt(index)) {
        block->fullscreenAlarm = fullscreen;
        touch();
    }
}

bool TemplateEditor::save() {
    error_.clear();
    revalidate();
    if (!issues_.isEmpty()) {
        error_ = tr("Fix the problems above before saving");
        emit changed();
        return false;
    }
    pruneActivities();
    if (!QDir().mkpath(QFileInfo(path_).absolutePath())) {
        error_ = u"cannot create %1"_s.arg(QFileInfo(path_).absolutePath());
        emit changed();
        return false;
    }
    QSaveFile file(path_);
    if (!file.open(QIODevice::WriteOnly)) {
        error_ = u"cannot write %1: %2"_s.arg(path_, file.errorString());
        emit changed();
        return false;
    }
    file.write(QByteArray::fromStdString(serializeTemplateDocument(document_)));
    if (!file.commit()) {
        error_ = u"cannot write %1: %2"_s.arg(path_, file.errorString());
        emit changed();
        return false;
    }
    saved_ = document_;
    dirty_ = false;
    emit saved(document_);
    emit changed();
    return true;
}

void TemplateEditor::revert() {
    document_ = saved_;
    fieldErrors_.clear();
    error_.clear();
    dirty_ = false;
    selectBlock(block_);
    revalidate();
}
