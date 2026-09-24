import QtQuick
import Cadence
import Cadence.Theme

// A philosopher's quote with its attribution and, on the overlay variant, the Latin or Greek
// original. Decorative only: it takes no focus, no clicks and no keys. The surface names itself
// and a phase key that changes only when it appears anew or its phase changes; the quote is drawn
// once per key and stays put while the wording follows the language, even when the surface is
// re-created. A quote that does not fit its slot gives way to the next one of its group, and after
// a whole round without a fit nothing is shown rather than a shrunken quote.
Item {
    id: root

    // The group to draw from: blockStart, focus, break, pushups or blockEnd.
    required property string context
    // Which surface this is: card, focus or overlay. The provider remembers one quote per surface.
    required property string surface
    // Empty means no quote; any other value identifies one appearance or phase of the surface.
    property string phaseKey: ""
    // card (Today), focus or overlay; sets the type sizes of the design.
    property string variant: "card"
    property color quoteColor: Theme.text
    property color attributionColor: Theme.textMuted
    property color originalColor: Theme.textMuted
    property bool showOriginal: false
    // Vertical room the slot offers; negative means unlimited.
    property real maxHeight: -1
    property string quoteId: ""
    // The surface's own say on visibility, on top of the setting and the fit.
    property bool shown: true
    property int topPadding: 0
    property int horizontalAlignment: Text.AlignLeft

    readonly property bool overlay: root.variant === "overlay"
    readonly property int quoteSize: root.variant === "card" ? Theme.quoteSizeCard
                                   : root.variant === "focus" ? Theme.quoteSizeFocus : Theme.quoteSizeOverlay
    readonly property bool fits: root.quoteId.length > 0 && !quoteText.truncated
                                 && (root.maxHeight < 0 || column.implicitHeight <= root.maxHeight)

    enabled: false
    visible: root.shown && Settings.showQuotes && root.fits
    implicitWidth: column.width
    implicitHeight: column.implicitHeight + root.topPadding

    function refresh() {
        root.quoteId = root.phaseKey.length > 0 ? Quotes.current(root.surface, root.phaseKey, root.context) : ""
    }

    // Another quote of the group when this one does not fit; the provider bounds the attempts.
    function retry() {
        if (root.quoteId.length === 0 || root.fits || !Settings.showQuotes) {
            return
        }
        root.quoteId = Quotes.replace(root.surface, root.phaseKey, root.context)
    }

    onPhaseKeyChanged: root.refresh()
    Component.onCompleted: root.refresh()
    onFitsChanged: Qt.callLater(root.retry)

    Column {
        id: column

        y: root.topPadding
        width: root.width
        spacing: Theme.spacing8

        Text {
            id: quoteText

            width: parent.width
            text: Quotes.text(root.quoteId, Language.current)
            color: root.quoteColor
            horizontalAlignment: root.horizontalAlignment
            wrapMode: Text.Wrap
            elide: Text.ElideRight
            maximumLineCount: root.overlay ? 4 : 3
            lineHeight: root.variant === "overlay" ? Theme.quoteLineHeightOverlay : Theme.quoteLineHeightCard
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightMedium
            font.pixelSize: root.quoteSize
        }

        Text {
            width: parent.width
            visible: root.overlay && root.showOriginal && text.length > 0
            text: Quotes.original(root.quoteId)
            color: root.originalColor
            horizontalAlignment: root.horizontalAlignment
            wrapMode: Text.Wrap
            font.family: Theme.quoteFamily
            font.italic: true
            font.pixelSize: Theme.quoteOriginalSize
        }

        Text {
            width: parent.width
            text: Quotes.attribution(root.quoteId, Language.current)
            color: root.attributionColor
            horizontalAlignment: root.horizontalAlignment
            wrapMode: Text.Wrap
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightSemiBold
            font.pixelSize: Theme.labelSize
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1
        }
    }
}
