import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Oran7SettingUiWindowItems"

// 音频工具区可横向扩展；录制状态变化不改变工具栏与预览的占位。
Rectangle {
    id: root
    required property var pageState
    property var liveCommit: function() { return true }
    property var audioCommit: function() { return true }
    readonly property var liveSession: controller && controller.liveSession ? controller.liveSession : null
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
    property var audioOptions: ({})
    property var audioEdit: function(key,value) {}
    readonly property bool liveSupported: controller && typeof controller.previewAudioSettings === "function"
    readonly property bool audioEditable: !!controller && controller.previewBusy !== true && !pageState.applying
        && !(controller.recordingBusy && Number(pageState.outputSaved.mode) !== 0)
    readonly property real systemVolume: audioOptions.systemVolume === undefined ? 100 : audioOptions.systemVolume
    readonly property bool microphoneEnabled: !!audioOptions.microphoneEnabled
    readonly property real microphoneVolume: audioOptions.microphoneVolume === undefined ? 100 : audioOptions.microphoneVolume
    readonly property bool systemEnabled: liveSupported ? !!audioOptions.audioEnabled : !!pageState.outputDraft.audioEnabled
    readonly property var audioLevels: controller && controller.audioLevels ? controller.audioLevels : ({})
    property color meterNormalColor: "#59cf9c"
    property color meterWarningColor: "#edc265"
    property color meterPeakColor: "#ee7484"
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
        enabled: root.audioEditable
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

    component Oran7AudioLevelMeter: Item {
        id: meter
        property real level: 0
        property bool sourceEnabled: false
        property bool fullScale: false
        property string sourceCaption: ""
        readonly property bool available: !!root.audioLevels.active && sourceEnabled
        readonly property real decibels: level > 0 ? 20 * Math.log(level) / Math.LN10 : -60
        readonly property real targetLevel: available ? Math.max(0, Math.min(1, (decibels + 60) / 60)) : 0
        property real displayLevel: targetLevel
        property real heldPeak: 0
        property bool warningHeld: false
        readonly property string description: root.audioLevels.error ? qsTranslate("Oran7Ui",root.audioLevels.error)
            : !sourceEnabled ? qsTranslate("Oran7Ui", "已静音")
            : !root.audioLevels.active ? qsTranslate("Oran7Ui", "正在等待音频检测就绪")
            : sourceCaption + " · " + (level > 0 ? decibels.toFixed(1) : "−∞") + " dBFS\n"
                + qsTranslate("Oran7Ui", "音量调整后、混音前的峰值：绿色正常，黄色偏高，红色接近满刻度")
                + ((fullScale || warningHeld) ? "\n" + qsTranslate("Oran7Ui", "输入触及满刻度，请检查声源音量") : "")
        implicitHeight: 4
        implicitWidth: 48
        Accessible.role: Accessible.ProgressBar
        Accessible.name: sourceCaption + qsTranslate("Oran7Ui", "实时电平")
        Accessible.description: meter.description
        Behavior on displayLevel {
            NumberAnimation { duration: meter.targetLevel > meter.displayLevel ? 40 : 240; easing.type: Easing.OutCubic }
        }
        onTargetLevelChanged: {
            if (targetLevel >= heldPeak) { heldPeak = targetLevel; peakHold.restart() }
            else if (!peakHold.running) peakHold.start()
        }
        onFullScaleChanged: if (fullScale && available) { warningHeld = true; warningHold.restart() }
        onAvailableChanged: if (!available) {
            peakHold.stop(); warningHold.stop(); heldPeak = 0; warningHeld = false
        }
        Timer { id: peakHold; interval: 650; onTriggered: meter.heldPeak = meter.targetLevel }
        Timer { id: warningHold; interval: 1000; onTriggered: meter.warningHeld = false }
        Rectangle { anchors.fill: parent; radius: 2; color: Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.13) }
        Item {
            width: parent.width * meter.displayLevel; height: parent.height; clip: true
            Rectangle {
                width: meter.width; height: meter.height; radius: 2
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: root.meterNormalColor }
                    GradientStop { position: 0.79; color: root.meterNormalColor }
                    GradientStop { position: 0.80; color: root.meterWarningColor }
                    GradientStop { position: 0.94; color: root.meterWarningColor }
                    GradientStop { position: 0.95; color: root.meterPeakColor }
                    GradientStop { position: 1; color: root.meterPeakColor }
                }
            }
        }
        Rectangle {
            visible: meter.available && meter.heldPeak > 0
            x: Math.max(0, Math.min(parent.width - width, parent.width * meter.heldPeak - width))
            y: -1; width: 1; height: parent.height + 2
            color: meter.fullScale || meter.warningHeld ? root.meterPeakColor : root.textColor
            opacity: 0.9
        }
        HoverHandler { id: meterHover }
        ToolTip.visible: meterHover.hovered
        ToolTip.delay: 250
        ToolTip.text: meter.description
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
                        border.color: root.systemEnabled ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.28) : Qt.rgba(root.borderColor.r, root.borderColor.g, root.borderColor.b, 0.55)
                        Behavior on color { ColorAnimation { duration: 120 } }
                        HoverHandler { id: systemHover }
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 6; spacing: 4
                            Oran7RecordingIcon { Layout.preferredWidth: 16; Layout.preferredHeight: 16; glyph: "speaker"; ink: root.systemEnabled ? root.accentColor : root.mutedTextColor }
                            Label { text: qsTranslate("Oran7Ui", "系统声音"); color: root.textColor; font.family: root.fontFamily; font.pixelSize: 11 }
                            Item {
                                implicitWidth: root.toggleHeight * 2 + 4; implicitHeight: root.toggleHeight
                                Oran7AudioSwitch {
                                    id: audioToggle
                                    objectName: "recordingAudio"
                                    onSwitchToggleChanged: function(checked) {
                                        if (root.liveSupported) root.audioEdit("audioEnabled", checked)
                                        else root.pageState.editOutput("audioEnabled", checked)
                                    }
                                }
                                Binding { target: audioToggle; property: "checked"; value: root.systemEnabled }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                spacing: 1
                            Oran7VolumeSlider {
                                objectName: "recordingSystemVolume"
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                implicitHeight: 14
                                enabled: root.liveSupported && root.audioEditable && root.systemEnabled
                                value: root.systemVolume
                                onMoved: root.audioEdit("systemVolume", Math.round(value))
                                Accessible.name: qsTranslate("Oran7Ui", "系统声音音量")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTranslate("Oran7Ui", "调整录制与推流的系统声音音量，不改变 Windows 音量")
                            }
                                Oran7AudioLevelMeter {
                                    objectName: "recordingSystemLevelMeter"
                                    Layout.fillWidth: true; Layout.preferredHeight: 4
                                    level: Number(root.audioLevels.systemPeak || 0)
                                    fullScale: !!root.audioLevels.systemFullScale
                                    sourceEnabled: root.systemEnabled
                                    sourceCaption: qsTranslate("Oran7Ui", "系统声音")
                                }
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
                        ToolTip.visible: microphoneHover.hovered
                        ToolTip.delay: 600
                        ToolTip.timeout: 2500
                        ToolTip.text: qsTranslate("Oran7Ui", "采集 Windows 默认麦克风，与系统声音混音后录制或推流")
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 6; spacing: 4
                            Oran7RecordingIcon { Layout.preferredWidth: 16; Layout.preferredHeight: 16; glyph: "microphone"; ink: root.microphoneEnabled ? root.accentColor : root.mutedTextColor }
                            Label { text: qsTranslate("Oran7Ui", "麦克风"); color: root.textColor; font.family: root.fontFamily; font.pixelSize: 10 }
                            Item {
                                implicitWidth: root.toggleHeight * 2 + 4; implicitHeight: root.toggleHeight
                                Oran7AudioSwitch {
                                    id: microphoneToggle
                                    objectName: "recordingMicrophone"
                                    enabled: root.liveSupported && root.audioEditable
                                    onSwitchToggleChanged: function(checked) { root.audioEdit("microphoneEnabled", checked) }
                                }
                                Binding { target: microphoneToggle; property: "checked"; value: root.microphoneEnabled }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                spacing: 1
                            Oran7VolumeSlider {
                                objectName: "recordingMicrophoneVolume"
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                implicitHeight: 14
                                enabled: root.liveSupported && root.audioEditable && root.microphoneEnabled
                                value: root.microphoneVolume
                                onMoved: root.audioEdit("microphoneVolume", Math.round(value))
                                Accessible.name: qsTranslate("Oran7Ui", "麦克风音量")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTranslate("Oran7Ui", "采集 Windows 默认麦克风，与系统声音混音后录制或推流")
                            }
                                Oran7AudioLevelMeter {
                                    objectName: "recordingMicrophoneLevelMeter"
                                    Layout.fillWidth: true; Layout.preferredHeight: 4
                                    level: Number(root.audioLevels.microphonePeak || 0)
                                    fullScale: !!root.audioLevels.microphoneFullScale
                                    sourceEnabled: root.microphoneEnabled
                                    sourceCaption: qsTranslate("Oran7Ui", "麦克风")
                                }
                            }
                            Label { text: Math.round(root.microphoneVolume) + "%"; color: root.mutedTextColor; font.pixelSize: 10; Layout.preferredWidth: 28; horizontalAlignment: Text.AlignRight }
                        }
                    }
                }
            }
            Oran7StudioButton {
                objectName: "startLiveStreaming"
                visible: root.liveSession !== null
                text: root.liveSession && root.liveSession.busy
                    ? (root.liveSession.state === "Stopping" ? qsTranslate("Oran7Ui", "正在停止推流…")
                        : root.liveSession.state === "Connecting" ? qsTranslate("Oran7Ui", "取消连接") : qsTranslate("Oran7Ui", "停止推流"))
                    : qsTranslate("Oran7Ui", "开始推流")
                enabled: root.liveSession && (root.liveSession.busy ? root.liveSession.state !== "Stopping" : root.controller.previewBusy !== true && !root.pageState.applying && !(root.controller.recordingBusy && Number(root.pageState.outputSaved.mode) !== 0))
                implicitHeight: 32
                accentColor: root.accentColor; textColor: root.textColor
                surfaceColor: root.surfaceColor; hoverColor: root.hoverColor; borderColor: root.borderColor
                onClicked: {
                    if (root.liveSession.busy) {
                        if (root.liveSession.state === "Streaming") root.audioCommit()
                        root.controller.stopLive()
                    }
                    else { console.info("[RTMP] 用户请求开始推流"); root.forceActiveFocus(); if (root.liveCommit()) root.controller.startLive() }
                }
            }
            Oran7RecordingAction {
                objectName: "startLocalRecording"
                text: root.controller && root.controller.recordingState === "Preparing" ? qsTranslate("Oran7Ui", "正在准备…") : qsTranslate("Oran7Ui", "开始录制")
                primary: true; symbol: "●"
                Layout.preferredWidth: 108; Layout.minimumWidth: 108; Layout.maximumWidth: 108
                implicitHeight: 32
                enabled: !root.pageState.outputBusy
                onClicked: { root.forceActiveFocus(); if (root.audioCommit()) root.pageState.start() }
            }
            Oran7RecordingAction {
                objectName: "stopLocalRecording"
                text: root.controller && root.controller.recordingState === "Stopping" ? qsTranslate("Oran7Ui", "正在保存…") : qsTranslate("Oran7Ui", "停止")
                destructive: true; symbol: "■"
                Layout.preferredWidth: 92; Layout.minimumWidth: 92; Layout.maximumWidth: 92
                implicitHeight: 32
                enabled: root.controller && root.controller.recordingBusy && root.controller.recordingState !== "Stopping"
                onClicked: { root.audioCommit(); root.controller.stopRecording() }
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
                color: (root.controller && root.controller.recordingError) || root.audioLevels.error ? root.errorColor : root.textColor
                font.family: root.fontFamily; font.pixelSize: 11
                elide: Text.ElideRight
                text: (root.controller && root.controller.recordingBusy && root.liveSession && (root.liveSession.busy || root.liveSession.error) ? root.stateCaption + " · " : "") + (root.liveSession && (root.liveSession.busy || root.liveSession.error)
                    ? (root.liveSession.error ? qsTranslate("Oran7Ui",root.liveSession.error)
                        : root.liveSession.state === "Connecting" ? qsTranslate("Oran7Ui", "正在连接推流服务器…")
                        : root.liveSession.state === "Stopping" ? qsTranslate("Oran7Ui", "正在停止推流…")
                        : qsTranslate("Oran7Ui", "推流中") + " · " + Math.floor((root.liveSession.statistics.elapsedMs || 0)/1000) + "s · "
                            + Math.round(root.liveSession.statistics.bitrateKbps || 0) + " kbps · "
                            + Math.round(root.liveSession.statistics.fps || 0) + " FPS · Q=" + (root.liveSession.statistics.queuedPackets || 0)
                            + " · drop=" + (root.liveSession.statistics.droppedFrames || 0))
                    : root.audioLevels.error ? qsTranslate("Oran7Ui", "音频检测：%1").arg(qsTranslate("Oran7Ui",root.audioLevels.error))
                    : root.controller && root.controller.recordingBusy ? root.stateCaption
                    : qsTranslate("Oran7Ui", root.pageState.previewHint || root.pageState.message || (root.controller && root.controller.recordingError) || "") || root.stateCaption)
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
