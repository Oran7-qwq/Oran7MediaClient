import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var controller
    required property var pageState
    property bool live: false
    property var commitAudio: function() { return true }
    property var commitLive: function() { return true }
    property bool violetSkin: false
    property real materialStrength: 0.025
    property color accentColor: "#80a9db"
    property color paperColor: "#d5dfec"
    property color paperInkColor: "#263447"
    property color textColor: "#e7edf5"
    property color mutedTextColor: "#a3afbf"
    property color surfaceColor: "#161e29"
    property color actionColor: root.surfaceColor
    property color hoverColor: "#2b394c"
    property color borderColor: "#334154"
    property color errorColor: "#ed8795"
    readonly property var session: controller && controller.liveSession ? controller.liveSession : null
    readonly property bool busy: live ? !!session && session.busy : !!controller && controller.recordingBusy
    readonly property string sessionState: live ? (session ? session.state : "Idle") : (controller ? controller.recordingState : "Idle")
    readonly property string error: live ? (session ? session.error : "") : (controller ? controller.recordingError : "")
    implicitHeight: 96
    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop {
            position: 0
            color: Qt.tint(root.surfaceColor, Qt.alpha(root.accentColor, root.busy ? 0.16 : root.violetSkin ? 0.06 : 0))
        }
        GradientStop { position: 1; color: root.surfaceColor }
    }
    radius: 8
    border.color: root.busy ? Qt.rgba(accentColor.r,accentColor.g,accentColor.b,.55) : root.borderColor
    // 信号从卡片底部接入；亮度仅跟随真实会话状态。
    Rectangle {
        x: 10; y: parent.height - 4; width: parent.width - 20; height: 2; radius: 1
        color: root.error ? root.errorColor : root.busy ? root.accentColor : root.borderColor
        opacity: root.busy || root.error ? 0.85 : 0.35
        Behavior on color { ColorAnimation { duration: 220 } }
        Behavior on opacity { NumberAnimation { duration: 220 } }
    }
    Item {
        anchors.fill: parent; anchors.margins: 2; clip: true
        Oran7StudioMaterial { anchors.fill: parent; grainOpacity: root.materialStrength }
        Oran7StudioMotif {
            visible: !root.violetSkin
            width: 100; height: 100; x: parent.width - 82; y: -16
            ink: root.accentColor; opacity: root.busy ? 0.10 : 0.025
            energized: root.busy; signalColor: root.accentColor
        }
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 10; spacing: 5
        RowLayout {
            Layout.fillWidth: true; spacing: 6
            Rectangle {
                visible: root.violetSkin
                width: 12; height: 12; radius: 6; color: root.surfaceColor; border.color: root.borderColor
                Rectangle { anchors.centerIn: parent; width: 6; height: 6; radius: 3; color: root.error ? root.errorColor : root.accentColor; opacity: root.busy ? 1 : 0.55 }
            }
            Label {
                text: root.live ? "LIVE" : "REC"; font.pixelSize: 10; font.bold: true
                color: root.busy ? (root.violetSkin ? "#ffffff" : root.surfaceColor) : root.paperInkColor
                leftPadding: 5; rightPadding: 5; topPadding: 1; bottomPadding: 1
                background: Rectangle { radius: 3; color: root.busy ? root.accentColor : root.paperColor }
            }
            Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: root.live ? qsTranslate("Oran7Ui", "直播推流") : qsTranslate("Oran7Ui", "本地录制"); font.pixelSize: 12; font.bold: true; color: root.textColor; elide: Text.ElideRight }
            Label {
                objectName: root.live ? "liveFormatBadge" : "recordingFormatBadge"
                visible: root.violetSkin
                text: root.live ? "RTMP" : String(root.pageState.outputDraft.format || "mp4").toUpperCase()
                font.pixelSize: 9; color: root.textColor; leftPadding: 5; rightPadding: 5; topPadding: 1; bottomPadding: 1
                background: Rectangle { radius: 3; color: "#0affffff"; border.color: root.borderColor }
            }
            Rectangle { visible: !root.violetSkin; width: 5; height: 5; radius: 2.5; color: root.error ? root.errorColor : root.busy ? root.accentColor : root.mutedTextColor }
        }
        Label {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            text: root.error ? qsTranslate("Oran7Ui",root.error)
                : root.live ? (root.sessionState === "Connecting" ? qsTranslate("Oran7Ui", "正在连接推流服务器…")
                    : root.sessionState === "Streaming" ? Math.floor((root.session.statistics.elapsedMs || 0)/1000) + "s  /  " + Math.round(root.session.statistics.bitrateKbps || 0) + " kbps"
                    : root.sessionState === "Stopping" ? qsTranslate("Oran7Ui", "正在停止推流…") : "H.264 / AAC · RTMP")
                : root.sessionState === "Preparing" ? qsTranslate("Oran7Ui", "正在准备编码器与输出…")
                : root.sessionState === "Recording" ? qsTranslate("Oran7Ui", "正在录制")
                : root.sessionState === "Stopping" ? qsTranslate("Oran7Ui", "正在保存文件，请稍候…")
                : String(root.pageState.outputDraft.format || "mp4").toUpperCase() + "  /  " + root.pageState.outputDraft.fps + " FPS"
            font.pixelSize: 10; color: root.error ? root.errorColor : root.mutedTextColor; elide: Text.ElideRight
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 6
            Oran7StudioButton {
                polished: root.violetSkin
                objectName: root.live ? "startLiveStreaming" : "startLocalRecording"
                Layout.fillWidth: true; Layout.minimumWidth: 0
                implicitHeight: 30; font.pixelSize: 12; font.bold: true; cornerRadius: 5
                primary: root.violetSkin
                destructive: !root.violetSkin && root.live && root.busy
                accentColor: root.accentColor; textColor: root.textColor; dangerColor: root.errorColor
                surfaceColor: root.actionColor; hoverColor: root.hoverColor; borderColor: root.borderColor
                text: root.live ? (root.busy ? (root.sessionState === "Stopping" ? qsTranslate("Oran7Ui", "正在停止推流…") : root.sessionState === "Connecting" ? qsTranslate("Oran7Ui", "取消连接") : qsTranslate("Oran7Ui", "停止推流")) : qsTranslate("Oran7Ui", "开始推流"))
                    : root.sessionState === "Preparing" ? qsTranslate("Oran7Ui", "正在准备…") : qsTranslate("Oran7Ui", "开始录制")
                enabled: root.live ? root.session && (root.busy ? root.sessionState !== "Stopping" : root.controller.previewBusy !== true && !root.pageState.applying && !(root.controller.recordingBusy && Number(root.pageState.outputSaved.mode) !== 0)) : !root.pageState.outputBusy
                onClicked: {
                    root.forceActiveFocus()
                    if (root.live) {
                        if (root.busy) { root.commitAudio(); root.controller.stopLive() }
                        else if (root.commitLive()) { console.info("[RTMP] 用户请求开始推流"); root.controller.startLive() }
                    } else if (root.commitAudio()) root.pageState.start()
                }
            }
            Oran7StudioButton {
                polished: root.violetSkin
                objectName: root.live ? "" : "stopLocalRecording"
                visible: !root.live
                Layout.minimumWidth: 60; Layout.preferredWidth: 65; Layout.maximumWidth: 72
                leftPadding: 8; rightPadding: 8; implicitHeight: 30; font.pixelSize: 12; cornerRadius: 5
                text: qsTranslate("Oran7Ui", "停止"); destructive: true
                dangerColor: root.errorColor; surfaceColor: root.surfaceColor; hoverColor: root.hoverColor; borderColor: root.borderColor
                enabled: !!root.controller && root.controller.recordingBusy && root.sessionState !== "Stopping"
                onClicked: { root.commitAudio(); root.controller.stopRecording() }
            }
        }
    }
}
