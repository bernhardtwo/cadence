import QtQuick
import QtTest

// The field is the only text entry of the app. The regression covered here: a field kept the
// text of the previous block when the selection moved while it had focus, and committed that
// stale text to the new block on blur.
TestCase {
    id: root

    name: "SignalField"
    when: windowShown
    width: 400
    height: 200

    property var fieldComponent: null

    Item {
        id: other

        // Somewhere for focus to go when the field is blurred.
        focus: false
    }

    function init() {
        if (root.fieldComponent === null) {
            root.fieldComponent = Qt.createComponent(Qt.resolvedUrl("../../src/app/qml/SignalField.qml"))
        }
        compare(root.fieldComponent.status, Component.Ready, root.fieldComponent.errorString())
    }

    function makeField(value) {
        const field = root.fieldComponent.createObject(root, { value: value, label: "Duration", width: 200 })
        verify(field !== null)
        return field
    }

    function test_value_change_replaces_text_even_while_focused() {
        const field = makeField("60")
        field.forceActiveFocus()
        verify(field.editing)
        compare(field.text, "60")
        field.value = "390"
        compare(field.text, "390")
        field.destroy()
    }

    function test_blur_after_selection_change_commits_nothing() {
        const field = makeField("60")
        const committed = createTemporaryObject(signalSpyComponent, root, { target: field, signalName: "committed" })
        field.forceActiveFocus()
        verify(field.editing)
        // The selection moves to another block: the model hands the field a new value.
        field.value = "390"
        other.forceActiveFocus()
        verify(!field.editing)
        compare(committed.count, 0)
        compare(field.text, "390")
        field.destroy()
    }

    function test_typing_then_return_commits_once() {
        const field = makeField("60")
        const committed = createTemporaryObject(signalSpyComponent, root, { target: field, signalName: "committed" })
        field.forceActiveFocus()
        verify(field.editing)
        keySequence(StandardKey.SelectAll)
        keyClick(Qt.Key_4)
        keyClick(Qt.Key_5)
        keyClick(Qt.Key_Return)
        compare(committed.count, 1)
        compare(committed.signalArguments[0][0], "45")
        field.destroy()
    }

    function test_unchanged_text_does_not_commit_on_blur() {
        const field = makeField("60")
        const committed = createTemporaryObject(signalSpyComponent, root, { target: field, signalName: "committed" })
        field.forceActiveFocus()
        other.forceActiveFocus()
        compare(committed.count, 0)
        field.destroy()
    }

    Component {
        id: signalSpyComponent

        SignalSpy {}
    }
}
