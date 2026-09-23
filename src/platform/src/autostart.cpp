#include <cadence/platform/autostart.hpp>

#include <cadence/platform/autostart_files.hpp>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace cadence::platform {

namespace {

std::string toStd(const QString& text) {
    return text.toStdString();
}

// Linux and macOS both register through a file the user owns; only its path and contents differ.
class FileAutostart final : public Autostart {
public:
    FileAutostart(QString path, std::string contents) : path_(std::move(path)), contents_(std::move(contents)) {}

    bool isEnabled() const override { return QFileInfo::exists(path_); }

    bool setEnabled(bool enabled) override {
        if (!enabled) {
            return !QFileInfo::exists(path_) || QFile::remove(path_);
        }
        if (!QDir().mkpath(QFileInfo(path_).absolutePath())) {
            return false;
        }
        QSaveFile file(path_);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        file.write(QByteArray::fromStdString(contents_));
        return file.commit();
    }

    QString location() const override { return path_; }

private:
    QString path_;
    std::string contents_;
};

class RegistryAutostart final : public Autostart {
public:
    explicit RegistryAutostart(QString command) : command_(std::move(command)) {}

    bool isEnabled() const override { return settings().contains(valueName()); }

    bool setEnabled(bool enabled) override {
        QSettings run = settings();
        if (enabled) {
            run.setValue(valueName(), command_);
        } else {
            run.remove(valueName());
        }
        run.sync();
        return run.status() == QSettings::NoError;
    }

    QString location() const override { return keyPath() + u"\\"_s + valueName(); }

private:
    static QString keyPath() { return u"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"_s; }
    static QString valueName() { return u"Cadence"_s; }
    static QSettings settings() { return QSettings(keyPath(), QSettings::NativeFormat); }

    QString command_;
};

} // namespace

std::unique_ptr<Autostart> createAutostart(const QString& executablePath) {
    const QString native = QDir::toNativeSeparators(executablePath);
#if defined(Q_OS_WIN)
    return std::make_unique<RegistryAutostart>(QString::fromStdString(windowsRunCommand(toStd(native))));
#elif defined(Q_OS_MACOS)
    const QString path = QDir::homePath() + u"/Library/LaunchAgents/"_s +
                         QString::fromUtf8(launchAgentLabel.data(), static_cast<qsizetype>(launchAgentLabel.size())) +
                         u".plist"_s;
    return std::make_unique<FileAutostart>(path, launchAgentPlist(toStd(native)));
#else
    const QString path =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + u"/autostart/"_s +
        QString::fromUtf8(desktopEntryFileName.data(), static_cast<qsizetype>(desktopEntryFileName.size()));
    return std::make_unique<FileAutostart>(path, desktopEntry(toStd(native)));
#endif
}

} // namespace cadence::platform
