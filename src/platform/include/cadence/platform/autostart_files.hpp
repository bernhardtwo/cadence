#pragma once

#include <string>
#include <string_view>

namespace cadence::platform {

inline constexpr std::string_view minimizedFlag = "--minimized";
inline constexpr std::string_view launchAgentLabel = "io.github.bernhardtwo.cadence";
inline constexpr std::string_view desktopEntryFileName = "cadence.desktop";

// Value written under HKCU\Software\Microsoft\Windows\CurrentVersion\Run.
std::string windowsRunCommand(std::string_view executablePath);

// XDG desktop entry for ~/.config/autostart.
std::string desktopEntry(std::string_view executablePath);

// launchd agent for ~/Library/LaunchAgents/<launchAgentLabel>.plist.
std::string launchAgentPlist(std::string_view executablePath);

} // namespace cadence::platform
