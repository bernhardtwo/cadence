#pragma once

#include <cadence/core/template_json.hpp>

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <cstddef>
#include <map>
#include <optional>

// A working copy of the week template that the Templates screen edits. Every mutation re-validates
// through core; save refuses while issues remain and otherwise writes the file atomically and
// hands the new document to whoever listens to saved().
class TemplateEditor : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int selectedDay READ selectedDay WRITE selectDay NOTIFY changed)
    Q_PROPERTY(int selectedBlock READ selectedBlock WRITE selectBlock NOTIFY changed)
    Q_PROPERTY(QVariantList plannedDays READ plannedDays NOTIFY changed)
    Q_PROPERTY(bool dayPlanned READ dayPlanned NOTIFY changed)
    Q_PROPERTY(QString dayStartText READ dayStartText NOTIFY changed)
    Q_PROPERTY(QString dayCutoffText READ dayCutoffText NOTIFY changed)
    Q_PROPERTY(QVariantList blocks READ blocks NOTIFY changed)
    Q_PROPERTY(QVariantMap block READ block NOTIFY changed)
    Q_PROPERTY(QVariantList issues READ issues NOTIFY changed)
    Q_PROPERTY(bool valid READ valid NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QStringList activityNames READ activityNames NOTIFY changed)

public:
    TemplateEditor(std::optional<cadence::core::TemplateDocument> document, QString path,
                   QObject* parent = nullptr);
    ~TemplateEditor() override;

    static TemplateEditor* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static void setInstance(TemplateEditor* instance);

    int selectedDay() const { return day_; }
    int selectedBlock() const { return block_; }
    QVariantList plannedDays() const;
    bool dayPlanned() const;
    QString dayStartText() const;
    QString dayCutoffText() const;
    QVariantList blocks() const;
    QVariantMap block() const;
    QVariantList issues() const { return issues_; }
    bool valid() const { return issues_.isEmpty(); }
    bool dirty() const { return dirty_; }
    QString error() const { return error_; }
    QStringList activityNames() const;

    const cadence::core::TemplateDocument& document() const { return document_; }

    Q_INVOKABLE void selectDay(int day);
    Q_INVOKABLE void selectBlock(int index);
    Q_INVOKABLE void setDayPlanned(bool planned);
    Q_INVOKABLE void setDayStart(const QString& text);
    Q_INVOKABLE void setDayCutoff(const QString& text);
    Q_INVOKABLE void copyMondayToWeekdays();
    Q_INVOKABLE void addBlock();
    Q_INVOKABLE void deleteBlock(int index);
    Q_INVOKABLE void moveBlock(int from, int to);
    Q_INVOKABLE void setBlockName(int index, const QString& name);
    Q_INVOKABLE void setBlockKind(int index, const QString& kind);
    Q_INVOKABLE void setBlockStart(int index, const QString& text);
    Q_INVOKABLE void setBlockDuration(int index, int minutes);
    Q_INVOKABLE void setBlockPomodoroEnabled(int index, bool enabled);
    Q_INVOKABLE void setBlockPomodoroCount(int index, int count);
    Q_INVOKABLE void setBlockFocus(int index, int minutes);
    Q_INVOKABLE void setBlockShortBreak(int index, int minutes);
    Q_INVOKABLE void setBlockLongBreak(int index, int minutes);
    Q_INVOKABLE void setBlockLongBreakEvery(int index, int sessions);
    Q_INVOKABLE void setBlockPushups(int index, bool pushups);
    Q_INVOKABLE void setBlockFullscreenAlarm(int index, bool fullscreen);
    Q_INVOKABLE bool save();
    Q_INVOKABLE void revert();

signals:
    void changed();
    void saved(const cadence::core::TemplateDocument& document);

private:
    struct FieldError {
        int day;
        int block;
        QString message;
    };

    cadence::core::DayTemplate* currentDay();
    const cadence::core::DayTemplate* currentDay() const;
    cadence::core::BlockTemplate* blockAt(int index);
    void touch();
    void revalidate();
    cadence::core::ActivityId activityFor(const QString& name);
    QString activityName(const cadence::core::ActivityId& id) const;
    void pruneActivities();
    void setFieldError(int index, const QString& field, const QString& message);

    cadence::core::TemplateDocument document_;
    cadence::core::TemplateDocument saved_;
    QString path_;
    int day_ = 0;
    int block_ = -1;
    bool dirty_ = false;
    QString error_;
    QVariantList issues_;
    // Text that did not parse, keyed by "day:block:field". Kept so the panel can show it and so an
    // unparsable time never silently saves as "no start".
    std::map<QString, FieldError> fieldErrors_;
};
