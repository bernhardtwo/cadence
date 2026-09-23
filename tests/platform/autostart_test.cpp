#include <cadence/platform/autostart_files.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace cadence::platform;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;

TEST_CASE("the Windows run command quotes the executable and starts minimized", "[autostart][windows]") {
    CHECK(windowsRunCommand(R"(C:\Program Files\Cadence\cadence.exe)") ==
          R"("C:\Program Files\Cadence\cadence.exe" --minimized)");
}

TEST_CASE("the desktop entry launches the executable minimized at login", "[autostart][linux]") {
    const std::string entry = desktopEntry("/opt/cadence/bin/cadence");

    CHECK_THAT(entry, StartsWith("[Desktop Entry]\n"));
    CHECK_THAT(entry, ContainsSubstring("Type=Application\n"));
    CHECK_THAT(entry, ContainsSubstring("Name=Cadence\n"));
    CHECK_THAT(entry, ContainsSubstring("Exec=\"/opt/cadence/bin/cadence\" --minimized\n"));
    CHECK_THAT(entry, ContainsSubstring("Terminal=false\n"));
    CHECK_THAT(entry, ContainsSubstring("X-GNOME-Autostart-enabled=true\n"));
}

TEST_CASE("the desktop entry escapes reserved characters in the executable path", "[autostart][linux]") {
    const std::string entry = desktopEntry("/home/me/my apps/$cad\"ence`");
    CHECK_THAT(entry, ContainsSubstring("Exec=\"/home/me/my apps/\\$cad\\\"ence\\`\" --minimized\n"));
}

TEST_CASE("the launch agent runs the executable minimized at load", "[autostart][macos]") {
    const std::string plist = launchAgentPlist("/Applications/Cadence.app/Contents/MacOS/cadence");

    CHECK_THAT(plist, StartsWith("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"));
    CHECK_THAT(plist, ContainsSubstring("<key>Label</key>\n\t<string>io.github.bernhardtwo.cadence</string>\n"));
    CHECK_THAT(plist, ContainsSubstring("<key>ProgramArguments</key>\n\t<array>\n"
                                        "\t\t<string>/Applications/Cadence.app/Contents/MacOS/cadence</string>\n"
                                        "\t\t<string>--minimized</string>\n\t</array>\n"));
    CHECK_THAT(plist, ContainsSubstring("<key>RunAtLoad</key>\n\t<true/>\n"));
    CHECK_THAT(plist, ContainsSubstring("</plist>\n"));
}

TEST_CASE("the launch agent escapes XML in the executable path", "[autostart][macos]") {
    const std::string plist = launchAgentPlist("/Users/me/A&B <x>/cadence");
    CHECK_THAT(plist, ContainsSubstring("<string>/Users/me/A&amp;B &lt;x&gt;/cadence</string>"));
}
