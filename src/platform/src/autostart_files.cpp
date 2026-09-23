#include <cadence/platform/autostart_files.hpp>

namespace cadence::platform {

namespace {

// Desktop entry Exec values are quoted with double quotes; inside them the spec reserves
// backslash, double quote, dollar and backtick.
std::string quoteExecArgument(std::string_view text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '\\' || c == '"' || c == '$' || c == '`') {
            out += '\\';
        }
        out += c;
    }
    out += '"';
    return out;
}

std::string escapeXml(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        default:
            out += c;
        }
    }
    return out;
}

} // namespace

std::string windowsRunCommand(std::string_view executablePath) {
    return "\"" + std::string(executablePath) + "\" " + std::string(minimizedFlag);
}

std::string desktopEntry(std::string_view executablePath) {
    std::string out;
    out += "[Desktop Entry]\n";
    out += "Type=Application\n";
    out += "Name=Cadence\n";
    out += "Comment=Structure your day into blocks with alarms, pomodoros and push-ups\n";
    out += "Exec=" + quoteExecArgument(executablePath) + " " + std::string(minimizedFlag) + "\n";
    out += "Terminal=false\n";
    out += "X-GNOME-Autostart-enabled=true\n";
    return out;
}

std::string launchAgentPlist(std::string_view executablePath) {
    std::string out;
    out += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out += "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
           "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    out += "<plist version=\"1.0\">\n";
    out += "<dict>\n";
    out += "\t<key>Label</key>\n";
    out += "\t<string>" + std::string(launchAgentLabel) + "</string>\n";
    out += "\t<key>ProgramArguments</key>\n";
    out += "\t<array>\n";
    out += "\t\t<string>" + escapeXml(executablePath) + "</string>\n";
    out += "\t\t<string>" + std::string(minimizedFlag) + "</string>\n";
    out += "\t</array>\n";
    out += "\t<key>RunAtLoad</key>\n";
    out += "\t<true/>\n";
    out += "</dict>\n";
    out += "</plist>\n";
    return out;
}

} // namespace cadence::platform
