import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Oran7SettingUiWindowItems"

// 音频工具区可横向扩展；录制状态变化不改变工具栏与预览的占位。
Rectangle {
    id: root
    required property var pageState
    required property var controller
    property color panelColor: "#242020"
    property color textColor: "#dadada"
    property color errorColor: "#e36a75"
    property color accentColor: "#beb7ff"
    property color mutedTextColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.58)
    property color surfaceColor: "#292d36"
    property color hoverColor: "#363b47"
    property color borderColor: "#424650"
    property color toggleOffColor: root.panelColor
    property color toggleBorderColor: root.accentColor
    property color toggleThumbColor: "white"
    property real toggleHeight: 20
    property string fontFamily: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
    property real textPixelSize: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).pixelSize > 0 ? (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).pixelSize : 16
    // 新增音量与麦克风仅为 UI 草稿，不写入录制配置，也不宣称后端已生效。
    property real systemVolume: 100
    property bool microphoneEnabled: false
    property real microphoneVolume: 100
    readonly property string stateCaption: root.controller
        ? ({Idle: qsTranslate("Oran7Ui", "就绪"), Preparing: qsTranslate("Oran7Ui", "正在准备编码器与输出…"), Recording: qsTranslate("Oran7Ui", "正在录制"), Stopping: qsTranslate("Oran7Ui", "正在保存文件，请稍候…"), Error: qsTranslate("Oran7Ui", "录制失败")})[root.controller.recordingState] || qsTranslate("Oran7Ui", "就绪")
        : qsTranslate("Oran7Ui", "录制控制器尚未就绪")
    implicitHeight: 86
    color: root.panelColor
    radius: 12
    border.color: root.borderColor
    border.width: 1

    Rectangle {
        anchors.fill: parent; anchors.margins: 1; radius: 11
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.tint(root.panelColor, "#05ffffff") }
            GradientStop { position: 1; color: root.panelColor }
        }
    }
    Rectangle { anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; anchors.margins: 12; height: 1; color: "#0affffff" }

    component Oran7RecordingAction: Oran7StudioButton {
        id: action
        scale: down ? 0.98 : 1
        Behavior on scale { NumberAnimation { duration: 90 } }
        accentColor: root.accentColor
        textColor: root.textColor
        surfaceColor: root.surfaceColor
        hoverColor: root.hoverColor
        borderColor: root.borderColor
        dangerColor: root.errorColor
        font.family: root.fontFamily
        contentItem: RowLayout {
            spacing: 6
            Oran7RecordingIcon {
                Layout.preferredWidth: 14; Layout.preferredHeight: 14
                glyph: action.primary ? "record" : "stop"
                ink: action.primary ? "#171a20" : root.errorColor
            }
            Label {
                Layout.fillWidth: true
                text: action.text; font: action.font
                color: action.primary ? "#171a20" : root.errorColor
                elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter
            }
        }
        background: Rectangle {
            radius: 8
            color: action.primary ? (action.hovered ? Qt.lighter(root.accentColor, 1.08) : root.accentColor)
                : action.hovered ? Qt.rgba(root.errorColor.r, root.errorColor.g, root.errorColor.b, 0.15) : root.surfaceColor
            border.color: action.activeFocus ? root.accentColor : action.primary ? "#30ffffff" : Qt.rgba(root.errorColor.r, root.errorColor.g, root.errorColor.b, 0.28)
            Behavior on color { ColorAnimation { duration: 120 } }
        }
    }
    component Oran7AudioSwitch: Oran7SwitchToggleItem {
        enabled: !root.pageState.busy
        onColor: root.accentColor
        offColor: root.toggleOffColor
        borderColor: root.toggleBorderColor
        thumbColor: root.toggleThumbColor
        controlHeight: root.toggleHeight
    }
    component Oran7VolumeSlider: Slider {
        id: slider
        from: 0; to: 100; stepSize: 1
        implicitHeight: 22
        leftPadding: 6; rightPadding: 6
        opacity: enabled ? 1 : 0.4
        background: Rectangle {
            x: slider.leftPadding; y: (slider.height - height) / 2
            width: slider.availableWidth; height: 3; radius: 2
            color: Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.14)
            Rectangle { width: slider.visualPosition * parent.width; height: parent.height; radius: 2; color: root.accentColor }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: (slider.height - height) / 2
            width: slider.pressed ? 12 : 10; height: width; radius: width / 2
            color: slider.hovered || slider.pressed ? root.textColor : root.accentColor
            Behavior on width { NumberAnimation { duration: 90 } }
            Behavior on color { ColorAnimation { duration: 90 } }
            border.width: slider.activeFocus ? 2 : 0
            border.color: root.textColor
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 5
        z: 1
        RowLayout {
            objectName: "recordingControlRow"
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            Layout.minimumHeight: 40
            Layout.maximumHeight: 40
            spacing: 8
            ScrollView {
                id: audioTools
                objectName: "recordingAudioTools"
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Layout.preferredHeight: 40
                clip: true
                contentWidth: audioRow.width
                contentHeight: 40
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded; implicitHeight: 4 }
                ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                Row {
                    id: audioRow
                    y: 4
                    width: implicitWidth; height: 32; spacing: 6
                    Rectangle {
                        objectName: "recordingSystemAudioTool"
                        width: 220; height: 32; radius: 8
                        color: systemHover.hovered ? Qt.tint(root.surfaceColor, "#08ffffff") : root.surfaceColor
                        border.color: !!root.pageState.outputDraft.audioEnabled ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.28) : Qt.rgba(root.borderColor.r, root.borderColor.g, root.borderColor.b, 0.55)
                        Behavior on color { ColorAnimation { duration: 120 } }
                        HoverHandler { id: systemHover }
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 6; spacing: 4
                            Oran7RecordingIcon { Layout.preferredWidth: 16; Layout.preferredHeight: 16; glyph: "speaker"; ink: !!root.pageState.outputDraft.audioEnabled ? root.accentColor : root.mutedTextColor }
                            Label { text: qsTranslate("Oran7Ui", "系统声音"); color: root.textColor; font.family: root.fontFamily; font.pixelSize: 11 }
                            Item {
                                implicitWidth: root.toggleHeight * 2 + 4; implicitHeight: root.toggleHeight
                                Oran7AudioSwitch {
                                    id: audioToggle
                                    objectName: "recordingAudio"
                                    onSwitchToggleChanged: function(checked) { root.pageState.editOutput("audioEnabled", checked) }
                                }
                                Binding { target: audioToggle; property: "checked"; value: !!root.pageState.outputDraft.audioEnabled }
                            }
                            Oran7VolumeSlider {
                                objectName: "recordingSystemVolume"
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                enabled: !root.pageState.busy && !!root.pageState.outputDraft.audioEnabled
                                value: root.systemVolume
                                onMoved: root.systemVolume = value
                                Accessible.name: qsTranslate("Oran7Ui", "系统声音录制音量（界面预览）")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTranslate("Oran7Ui", "系统声音音量调节尚未接入录制")
                            }
                            Label { text: Math.round(root.systemVolume) + "%"; color: root.mutedTextColor; font.pixelSize: 10; Layout.preferredWidth: 28; horizontalAlignment: Text.AlignRight }
                        }
                    }
                    Rectangle {
                        objectName: "recordingMicrophoneTool"
                        width: 220; height: 32; radius: 8
                        color: microphoneHover.hovered ? Qt.tint(root.surfaceColor, "#08ffffff") : root.surfaceColor
                        border.color: Qt.rgba(root.borderColor.r, root.borderColor.g, root.borderColor.b, 0.55)
                        Behavior on color { ColorAnimation { duration: 120 } }
                        HoverHandler { id: microphoneHover }
                        ToolTip.visible: microphoneHover.hovered && !root.pageState.busy
                        ToolTip.delay: 600
                        ToolTip.timeout: 2500
                        ToolTip.text: qsTranslate("Oran7Ui", "麦克风录音与音量调节尚未接入后端")
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 6; spacing: 4
                            Oran7RecordingIcon { Layout.preferredWidth: 16; Layout.preferredHeight: 16; glyph: "microphone"; ink: root.mutedTextColor }
                            Label { text: qsTranslate("Oran7Ui", "麦克风·预览"); color: root.textColor; font.family: root.fontFamily; font.pixelSize: 10 }
                            Item {
                                implicitWidth: root.toggleHeight * 2 + 4; implicitHeight: root.toggleHeight
                                Oran7AudioSwitch {
                                    id: microphoneToggle
                                    objectName: "recordingMicrophone"
                                    checked: root.microphoneEnabled
                                    onSwitchToggleChanged: function(checked) { root.microphoneEnabled = checked }
                                }
                            }
                            Oran7VolumeSlider {
                                objectName: "recordingMicrophoneVolume"
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                enabled: !root.pageState.busy && root.microphoneEnabled
                                value: root.microphoneVolume
                                onMoved: root.microphoneVolume = value
                                Accessible.name: qsTranslate("Oran7Ui", "麦克风录制音量（界面预览）")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTranslate("Oran7Ui", "麦克风录音与音量调节尚未接入后端")
                            }
                            Label { text: Math.round(root.microphoneVolume) + "%"; color: root.mutedTextColor; font.pixelSize: 10; Layout.preferredWidth: 28; horizontalAlignment: Text.AlignRight }
                        }
                    }
                }
            }
            Oran7RecordingAction {
                objectName: "startLocalRecording"
                text: root.controller && root.controller.recordingState === "Preparing" ? qsTranslate("Oran7Ui", "正在准备…") : qsTranslate("Oran7Ui", "开始录制")
                primary: true; symbol: "●"
                Layout.preferredWidth: 108; Layout.minimumWidth: 108; Layout.maximumWidth: 108
                implicitHeight: 32
                enabled: !root.pageState.busy
                onClicked: { root.forceActiveFocus(); root.pageState.start() }
            }
            Oran7RecordingAction {
                objectName: "stopLocalRecording"
                text: root.controller && root.controller.recordingState === "Stopping" ? qsTranslate("Oran7Ui", "正在保存…") : qsTranslate("Oran7Ui", "停止")
                destructive: true; symbol: "■"
                Layout.preferredWidth: 92; Layout.minimumWidth: 92; Layout.maximumWidth: 92
                implicitHeight: 32
                enabled: root.controller && root.controller.recordingBusy && root.controller.recordingState !== "Stopping"
                onClicked: root.controller.stopRecording()
            }
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: root.borderColor }
        RowLayout {
            objectName: "recordingStatusRow"
            Layout.fillWidth: true
            Layout.preferredHeight: 18; Layout.minimumHeight: 18; Layout.maximumHeight: 18
            spacing: 8
            Rectangle {
                implicitWidth: 10; implicitHeight: 10; radius: 5
                border.width: 2; border.color: root.panelColor
                color: root.controller && (root.controller.recordingError || root.controller.recordingState === "Recording")
                    ? root.errorColor : root.pageState.busy ? root.accentColor : root.mutedTextColor
            }
            Label {
                objectName: "recordingStatus"
                Layout.preferredWidth: Math.min(implicitWidth, root.width * 0.55)
                Layout.minimumWidth: 0
                color: root.controller && root.controller.recordingError ? root.errorColor : root.textColor
                font.family: root.fontFamily; font.pixelSize: 11
                elide: Text.ElideRight
                text: root.controller && root.controller.recordingBusy ? root.stateCaption
                    : qsTranslate("Oran7Ui", root.pageState.previewHint || root.pageState.message || (root.controller && root.controller.recordingError) || "") || root.stateCaption
                ToolTip.visible: statusHover.hovered
                ToolTip.text: text
                HoverHandler { id: statusHover }
            }
            Oran7RecordingIcon { Layout.preferredWidth: 13; Layout.preferredHeight: 13; glyph: "folder"; ink: root.mutedTextColor }
            Label {
                objectName: "recordingOutputLocation"
                Layout.fillWidth: true; Layout.minimumWidth: 0
                text: root.controller && root.controller.recordingOutputPath ? root.controller.recordingOutputPath : qsTranslate("Oran7Ui", "录制后显示文件保存位置")
                color: root.mutedTextColor
                font.family: root.fontFamily; font.pixelSize: 10
                elide: Text.ElideMiddle
                horizontalAlignment: Text.AlignRight
                ToolTip.visible: pathHover.hovered
                ToolTip.text: text
                HoverHandler { id: pathHover }
            }
        }
    }
}
