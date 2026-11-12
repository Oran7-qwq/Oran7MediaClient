import QtQuick
import QtQuick.Controls
import "../../Settings/GlobalSettings"
import "../Oran7SettingUiWindowItems"
import "../Oran7GlobalComponents"
import Oran7UI.Impl

Window {
    id: root
    property var effects
    property bool focusTracked: false
    property bool closeInProgress: false
    signal commitRequested()
    visible: false
    opacity: 0
    width: 760; height: 428
    color: "transparent"
    flags: Qt.Dialog | Qt.FramelessWindowHint
    modality: Qt.ApplicationModal
    readonly property color ink: Oran7MainUiSetting.textColor
    readonly property color accent: Oran7Theme.Oran7MainGUI.themeColor
    function presetIndex() {
        if (!effects) return -1
        for (let i = 0; i < effects.presets.length; ++i)
            if (effects.presets[i].id === effects.presetId) return i
        return -1
    }
    function customPresetIndices() {
        if (!effects) return []
        const indices = []
        const presets = effects.presets
        for (let i = 0; i < presets.length; ++i)
            if (presets[i].custom) indices.push(i)
        return indices
    }
    function presetNameOf(id) {
        if (!effects) return ""
        const presets = effects.presets
        for (let i = 0; i < presets.length; ++i)
            if (presets[i].id === id) return presets[i].label
        return ""
    }
    function releaseFocus() {
        if (!focusTracked) return
        focusTracked = false
        if (Oran7MainUiSetting.activeSettingsPopup === root) Oran7MainUiSetting.activeSettingsPopup = null
        Oran7MainUiSetting.activeOtherItemCount = Math.max(0, Oran7MainUiSetting.activeOtherItemCount - 1)
    }
    function showEditor() {
        if (closeInProgress) return
        if (Oran7MainUiSetting.activeSettingsPopup && Oran7MainUiSetting.activeSettingsPopup !== root)
            Oran7MainUiSetting.activeSettingsPopup.close()
        Oran7MainUiSetting.activeSettingsPopup = root
        if (!focusTracked) { focusTracked = true; Oran7MainUiSetting.activeOtherItemCount++ }
        const areas = Oran7Api.screenWorkAreas
        let selectedArea = areas.length ? areas[0] : null
        for (let i = 0; i < areas.length; ++i) {
            const area = areas[i]
            if (x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height) { selectedArea = area; break }
        }
        if (selectedArea) {
            x = Math.max(selectedArea.x, Math.min(x, selectedArea.x + Math.max(0, selectedArea.width - width)))
            y = Math.max(selectedArea.y, Math.min(y, selectedArea.y + Math.max(0, selectedArea.height - height)))
        }
        show(); requestActivate(); opening.start()
    }
    Component.onDestruction: releaseFocus()
    onClosing: function(event) {
        event.accepted = false
        if (closeInProgress) return
        closeInProgress = true
        presetChoice.popup.close()
        presetDialog.close()
        commitRequested()
        opening.stop(); closingAnimation.start()
    }
    Shortcut { sequence: "Escape"; enabled: !presetChoice.popup.visible && !presetDialog.visible; onActivated: root.close() }
    NumberAnimation { id: opening; target: root; property: "opacity"; to: 1; duration: 180; easing.type: Easing.OutCubic }
    NumberAnimation {
        id: closingAnimation; target: root; property: "opacity"; to: 0; duration: 130; easing.type: Easing.InCubic
        onFinished: { root.hide(); root.releaseFocus(); root.destroy() }
    }
    Rectangle {
        anchors.fill: parent; anchors.margins: 6
        radius: 14; color: Oran7MainUiSetting.backColor
        border.width: 1; border.color: Qt.alpha(root.ink, 0.22)
        Item {
            id: titleBar
            anchors.left: parent.left; anchors.right: parent.right; height: 50
            MouseArea { anchors.fill: parent; onPressed: root.startSystemMove() }
            Text {
                anchors.left: parent.left; anchors.leftMargin: 20; anchors.verticalCenter: parent.verticalCenter
                text: qsTranslate("Oran7Ui", "10 段均衡器"); color: root.ink; font.pixelSize: 18; font.bold: true
            }
            Oran7StudioButton { anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter
                text: "×"; width: 32; height: 30; leftPadding: 0; rightPadding: 0
                textColor: root.ink; surfaceColor: "transparent"; hoverColor: Qt.alpha(root.ink, 0.1)
                borderColor: Qt.alpha(root.ink, 0.1); onClicked: root.close() }
        }
        Row {
            id: actions
            anchors.top: titleBar.bottom; anchors.left: parent.left; anchors.leftMargin: 20
            spacing: 12; height: 40
            Item {
                width: 146; height: 38
                Oran7SwitchToggleItem {
                    id: effectsToggle
                    objectName: "equalizerEnabledToggle"
                    anchors.right: undefined; x: 0
                    checked: root.effects ? root.effects.enabled : false
                    onSwitchToggleChanged: function(checked) {
                        if (root.effects) root.effects.enabled = checked
                        effectsToggle.checked = Qt.binding(function() { return root.effects ? root.effects.enabled : false })
                    }
                }
                Text { x: effectsToggle.width + 10; anchors.verticalCenter: parent.verticalCenter
                    text: qsTranslate("Oran7Ui", "启用音效"); color: root.ink; font.pixelSize: 13 }
            }
            Oran7PreferenceChoice {
                id: presetChoice
                objectName: "equalizerPresetChoice"
                width: 200; height: 38
                badge: "EQ"; popupOwner: root
                popupTitle: qsTranslate("Oran7Ui", "预设")
                model: root.effects ? root.effects.presets.map(function(preset) { return qsTranslate("Oran7Ui", preset.label) }) : []
                itemActionIndices: root.effects ? root.customPresetIndices() : []
                displayText: root.effects && root.effects.presetId === "custom" ? qsTranslate("Oran7Ui", "自定义") : currentText
                currentIndex: root.presetIndex()
                onActivated: function(index) {
                    if (root.effects && index >= 0) root.effects.applyPreset(root.effects.presets[index].id)
                    currentIndex = Qt.binding(root.presetIndex)
                }
                onItemActionTriggered: function(index) {
                    if (!root.effects) return
                    const preset = root.effects.presets[index]
                    if (preset && preset.custom) presetDialog.openForEdit(preset.id)
                }
            }
            Oran7PreferenceAction { objectName: "equalizerResetButton"; text: qsTranslate("Oran7Ui", "归零"); badge: ""; width: 56; onClicked: {
                if (!root.effects) return
                for (let i = 0; i < 10; ++i) root.effects.equalizer.setGain(i, 0)
            } }
            Oran7PreferenceAction {
                objectName: "equalizerSavePresetButton"
                text: qsTranslate("Oran7Ui", "保存")
                badge: ""
                width: 56
                onClicked: presetDialog.openForSave()
            }
        }
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.margins: 20
            anchors.top: actions.bottom; anchors.topMargin: 10; height: 272
            radius: 10; color: Qt.alpha(root.ink, 0.035)
            Column {
                x: 10; y: 24; width: 42; height: 210; spacing: 82
                Repeater { model: ["+12 dB", "0 dB", "−12 dB"]
                    Text { required property string modelData; text: modelData; color: Qt.alpha(root.ink, 0.55); font.pixelSize: 10 } }
            }
            Row {
                anchors.left: parent.left; anchors.leftMargin: 58
                anchors.right: parent.right; anchors.rightMargin: 12; anchors.top: parent.top; anchors.topMargin: 10
                spacing: 4
                Repeater {
                    model: root.effects ? root.effects.equalizer : null
                    delegate: Oran7EqualizerBand {
                        required property int index
                        required property string label
                        required property real gainDb
                        objectName: "equalizerBand" + index
                        width: (parent.width - 36) / 10; height: 248
                        bandLabel: label; bandGain: gainDb; ink: root.ink; accent: root.accent
                        trackColor: Qt.alpha(root.ink, 0.18)
                        onGainEdited: function(value) { root.effects.equalizer.setGain(index, value) }
                    }
                }
            }
        }
        Text {
            anchors.left: parent.left; anchors.leftMargin: 22; anchors.bottom: parent.bottom; anchors.bottomMargin: 14
            text: qsTranslate("Oran7Ui", "向上增强，向下衰减 · 调整实时生效，自动保存")
            color: Qt.alpha(root.ink, 0.65); font.pixelSize: 12
        }

        Popup {
            id: presetDialog
            parent: Overlay.overlay
            modal: true
            focus: true
            width: 340
            height: 196
            padding: 18
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
            x: parent ? (parent.width - width) / 2 : 0
            y: parent ? (parent.height - height) / 2 : 0

            property string presetId: ""
            property bool editing: false

            function openForSave() {
                presetId = ""
                editing = false
                nameField.text = root.effects ? root.effects.suggestedPresetName() : ""
                open()
                nameField.forceActiveFocus()
                nameField.selectAll()
            }
            function openForEdit(id) {
                presetId = id
                editing = true
                nameField.text = root.presetNameOf(id)
                open()
                nameField.forceActiveFocus()
                nameField.selectAll()
            }

            background: Rectangle {
                radius: 12
                color: Oran7MainUiSetting.backColor
                border.width: 1
                border.color: Qt.alpha(root.ink, 0.22)
            }

            contentItem: Column {
                width: presetDialog.width - presetDialog.leftPadding - presetDialog.rightPadding
                spacing: 10

                Text {
                    width: parent.width
                    text: presetDialog.editing
                          ? qsTranslate("Oran7Ui", "重命名预设")
                          : qsTranslate("Oran7Ui", "保存当前设置为预设")
                    color: root.ink
                    font.pixelSize: 14
                    font.bold: true
                }

                TextField {
                    id: nameField
                    width: parent.width
                    height: 36
                    color: root.ink
                    placeholderText: qsTranslate("Oran7Ui", "预设名称")
                    placeholderTextColor: Qt.alpha(root.ink, 0.45)
                    selectByMouse: true
                    font.family: Oran7MainUiSetting.fontFamily
                    font.pixelSize: 13
                    background: Rectangle {
                        radius: 7
                        color: Oran7MainUiSetting.itemBackColor
                        border.width: 1
                        border.color: nameField.activeFocus
                                      ? root.accent
                                      : Qt.alpha(root.ink, 0.18)
                    }
                    onAccepted: saveButton.clicked()
                }

                Row {
                    width: parent.width
                    spacing: 8

                    Oran7StudioButton {
                        id: saveButton
                        text: qsTranslate("Oran7Ui", "保存")
                        primary: true
                        accentColor: root.accent
                        textColor: root.ink
                        surfaceColor: Oran7MainUiSetting.itemBackColor
                        hoverColor: Qt.alpha(root.ink, 0.10)
                        borderColor: Qt.alpha(root.ink, 0.18)
                        onClicked: {
                            if (!root.effects) return
                            if (presetDialog.editing) {
                                if (!root.effects.renamePreset(presetDialog.presetId, nameField.text))
                                    return
                            } else {
                                root.effects.saveCurrentAsPreset(nameField.text)
                            }
                            presetDialog.close()
                        }
                    }
                    Oran7StudioButton {
                        visible: presetDialog.editing
                        text: qsTranslate("Oran7Ui", "删除")
                        destructive: true
                        accentColor: root.accent
                        textColor: root.ink
                        surfaceColor: Oran7MainUiSetting.itemBackColor
                        hoverColor: Qt.alpha(root.ink, 0.10)
                        borderColor: Qt.alpha(root.ink, 0.18)
                        onClicked: {
                            if (root.effects) root.effects.deletePreset(presetDialog.presetId)
                            presetDialog.close()
                        }
                    }
                    Oran7StudioButton {
                        text: qsTranslate("Oran7Ui", "取消")
                        accentColor: root.accent
                        textColor: root.ink
                        surfaceColor: Oran7MainUiSetting.itemBackColor
                        hoverColor: Qt.alpha(root.ink, 0.10)
                        borderColor: Qt.alpha(root.ink, 0.18)
                        onClicked: presetDialog.close()
                    }
                }
            }
        }
    }
}
