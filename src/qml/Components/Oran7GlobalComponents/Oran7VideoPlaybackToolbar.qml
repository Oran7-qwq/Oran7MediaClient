import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects

// 沿用原视频页的透明底栏、白色播放图标、进度条与悬停音量控件。
Rectangle {
    id: root
    property bool live: false
    property bool playing: false
    property bool opening: false
    property bool fullscreen: false
    property bool controlsEnabled: false
    property real positionSeconds: 0
    property real durationSeconds: 0
    property string sourceKey: ""
    property string fontFamily: typeof Oran7UiPreferences !== "undefined"
        ? Oran7UiPreferences.font.family : Qt.application.font.family
    readonly property bool interacting: progress.isPressed || playArea.pressed || volumeControl.interacting || fullscreenArea.pressed
    signal toggleRequested()
    signal seekRequested(int seconds, string sourceKey)
    signal fullscreenRequested()
    signal activity()
    function closePopups() { volumeControl.closePopup() }
    color: "transparent"
    implicitHeight: 80
    onInteractingChanged: activity()

    Image {
        id: playImage
        objectName: "videoToggleButton"
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 7
        anchors.left: parent.left
        anchors.leftMargin: 15
        width: 40
        height: 40
        sourceSize: Qt.size(40, 40)
        source: root.playing ? "qrc:/image/ClearPause.png" : "qrc:/image/ClearPlay.png"
        scale: playArea.pressed ? 0.7 : 0.9
        Behavior on scale { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
        layer.enabled: true
        layer.effect: ColorOverlay { color: "white" }
        MouseArea {
            id: playArea
            anchors.fill: parent
            enabled: root.controlsEnabled && !root.opening
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.toggleRequested()
        }
    }
    Oran7ProgressSlider {
        id: progress
        objectName: "videoSeekSlider"
        width: parent.width * 0.98
        height: 12
        progressHandleWidth: 14
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        visible: !root.live
        enabled: root.controlsEnabled && !root.opening && root.durationSeconds > 0
        autoSeek: false
        allSecondTime: Math.max(0, Math.floor(root.durationSeconds))
        property string pressedSource: ""
        onIsPressedChanged: if (isPressed) pressedSource = root.sourceKey
        onPositionChanged: root.activity()
        onSeekCommitted: (seconds) => root.seekRequested(seconds, pressedSource)
        Binding {
            target: progress; property: "nowSecondTime"
            value: Math.max(0, Math.floor(root.positionSeconds))
            when: !progress.isPressed
        }
        Binding {
            target: progress; property: "progressHandleX"
            value: root.durationSeconds > 0 ? Math.min(1, root.positionSeconds / root.durationSeconds) * progress.width - progress.progressHandleWidth / 2
                : -progress.progressHandleWidth / 2
            when: !progress.isPressed
        }
        Binding {
            target: progress; property: "visibleProgressX"
            value: root.durationSeconds > 0 ? Math.min(1, root.positionSeconds / root.durationSeconds) * progress.width : 0
            when: !progress.isPressed
        }
    }
    Label {
        anchors.left: playImage.right
        anchors.leftMargin: 15
        anchors.verticalCenter: playImage.verticalCenter
        color: "white"
        font.family: root.fontFamily
        font.pixelSize: 20
        text: root.live ? qsTranslate("Oran7Ui", "直播") : progress.nowTimeText + " - " + progress.allTimeText
    }
    Oran7PlayerVolumeComponent {
        id: volumeControl
        objectName: "videoVolumeControl"
        anchors.right: fullscreenButton.left
        anchors.rightMargin: 20
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
    }
    Item {
        id: fullscreenButton
        objectName: "videoFullscreenButton"
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.verticalCenter: playImage.verticalCenter
        width: 26; height: 26
        Shape {
            anchors.centerIn: parent
            width: 18; height: 18
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: "white"
                strokeWidth: 1.5
                fillColor: "transparent"
                PathSvg {
                    path: root.fullscreen
                        ? "M 1 6 H 6 V 1 M 12 1 V 6 H 17 M 17 12 H 12 V 17 M 6 17 V 12 H 1"
                        : "M 1 6 V 1 H 6 M 12 1 H 17 V 6 M 17 12 V 17 H 12 M 6 17 H 1 V 12"
                }
            }
        }
        MouseArea {
            id: fullscreenArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.fullscreenRequested()
        }
        ToolTip.visible: fullscreenArea.containsMouse
        ToolTip.text: root.fullscreen ? qsTranslate("Oran7Ui", "退出全屏") : qsTranslate("Oran7Ui", "全屏")
    }
}
