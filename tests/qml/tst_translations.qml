import QtQuick
import QtTest

// The compiled catalogs of the app, installed at runtime: bindings on the app's own contexts
// follow the language switch at once, and switching back restores the source text.
TestCase {
    id: root

    name: "Translations"
    when: windowShown
    width: 300
    height: 100

    Text {
        id: today

        text: qsTranslate("TopBar", "Today")
    }

    Text {
        id: settings

        text: qsTranslate("TopBar", "Settings")
    }

    Text {
        id: pomodoros

        text: qsTranslate("TodayScreen", "%n pomodoro(s)", "", 3)
    }

    function cleanup() {
        testLanguage.install("en")
    }

    function test_spanish_and_french_retranslate_live() {
        compare(today.text, "Today")
        verify(testLanguage.install("es"))
        compare(today.text, "Hoy")
        compare(settings.text, "Ajustes")
        compare(pomodoros.text, "3 pomodoros")
        verify(testLanguage.install("fr"))
        compare(today.text, "Aujourd'hui")
        compare(settings.text, "Réglages")
        verify(testLanguage.install("en"))
        compare(today.text, "Today")
        compare(pomodoros.text, "3 pomodoros")
    }

    function test_english_plural_forms_come_from_the_source_catalog() {
        verify(testLanguage.install("en"))
        compare(pomodoros.text, "3 pomodoros")
    }
}
