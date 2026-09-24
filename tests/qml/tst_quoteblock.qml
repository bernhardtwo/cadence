import QtQuick
import QtTest
import Cadence
import Cadence.Theme

// QuoteBlock against test doubles of Quotes, Language and Settings: the quote is drawn once per
// phase key, hides completely when quotes are off, shows the original only on the overlay variant
// and only when asked, and swaps its wording with the language while keeping its id.
TestCase {
    id: root

    name: "QuoteBlock"
    when: windowShown
    // A TestCase is invisible by default; visibility rules need a visible parent.
    visible: true
    width: 800
    height: 600

    property var blockComponent: null

    FontLoader {
        id: bodyFont

        source: Qt.resolvedUrl("../../src/app/fonts/Archivo/Archivo-Variable.ttf")
    }

    FontLoader {
        id: quoteFont

        source: Qt.resolvedUrl("../../src/app/fonts/NotoSerif/NotoSerif-Italic.ttf")
    }

    function init() {
        Settings.showQuotes = true
        Settings.showQuoteOriginals = false
        Language.current = "en"
        if (root.blockComponent === null) {
            root.blockComponent = Qt.createComponent(Qt.resolvedUrl("../../src/app/qml/QuoteBlock.qml"))
        }
        compare(root.blockComponent.status, Component.Ready, root.blockComponent.errorString())
    }

    function makeBlock(properties) {
        const defaults = { surface: "test", context: "break", phaseKey: "k1", width: 520 }
        const block = root.blockComponent.createObject(root, Object.assign(defaults, properties))
        verify(block !== null)
        waitForRendering(block)
        return block
    }

    function texts(block) {
        const out = []
        for (const child of block.children[0].children) {
            if (child.visible && child.text !== undefined) {
                out.push(child.text)
            }
        }
        return out
    }

    function test_draws_once_per_phase_key_and_keeps_the_id() {
        const before = Quotes.drawCount()
        const block = makeBlock({ variant: "card" })
        compare(block.quoteId, "break-k1")
        compare(Quotes.drawCount(), before + 1)
        block.width = 400
        waitForRendering(block)
        compare(Quotes.drawCount(), before + 1, "a re-layout draws nothing new")
        block.phaseKey = "k2"
        compare(block.quoteId, "break-k2")
        compare(Quotes.drawCount(), before + 2)
        block.phaseKey = ""
        compare(block.quoteId, "")
        verify(!block.visible)
        block.destroy()
    }

    function test_hidden_when_quotes_are_off() {
        const block = makeBlock({ variant: "focus" })
        verify(block.visible)
        Settings.showQuotes = false
        verify(!block.visible)
        Settings.showQuotes = true
        verify(block.visible)
        block.destroy()
    }

    function test_original_only_on_overlay_and_only_when_enabled_data() {
        return [
            { tag: "card off", variant: "card", enabled: false, expected: false },
            { tag: "card on", variant: "card", enabled: true, expected: false },
            { tag: "focus on", variant: "focus", enabled: true, expected: false },
            { tag: "overlay off", variant: "overlay", enabled: false, expected: false },
            { tag: "overlay on", variant: "overlay", enabled: true, expected: true }
        ]
    }

    function test_original_only_on_overlay_and_only_when_enabled(data) {
        Settings.showQuoteOriginals = data.enabled
        const block = makeBlock({ variant: data.variant, showOriginal: Settings.showQuoteOriginals })
        compare(texts(block).indexOf("Originale") >= 0, data.expected)
        block.destroy()
    }

    function test_language_change_swaps_words_not_the_quote() {
        const block = makeBlock({ variant: "overlay" })
        const id = block.quoteId
        compare(texts(block)[0], "Quote break-k1 in en")
        Language.current = "es"
        compare(block.quoteId, id)
        compare(texts(block)[0], "Quote break-k1 in es")
        compare(texts(block)[texts(block).length - 1], "Author es · Work, 1")
        block.destroy()
    }

    function test_never_takes_input() {
        const block = makeBlock({ variant: "card" })
        verify(!block.enabled)
        verify(!block.activeFocus)
        block.forceActiveFocus()
        verify(!block.activeFocus, "a disabled item cannot take focus")
        block.destroy()
    }

    function test_too_tall_a_slot_asks_for_a_replacement() {
        const before = Quotes.replaceCount()
        const block = makeBlock({ variant: "overlay", maxHeight: 10 })
        tryVerify(() => Quotes.replaceCount() > before)
        verify(!block.visible, "nothing fits, nothing shows")
        block.destroy()
    }
}
