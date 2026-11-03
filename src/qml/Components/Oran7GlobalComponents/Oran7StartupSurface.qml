import QtQuick
import QtQuick.Controls
import Oran7Startup 1.0

// 启动专用轻量画面，不加载主题插件、视频或网络资源。
Rectangle {
    id: root
    objectName: "oran7StartupSurface"
    property string stageText: ""
    property string errorText: ""
    property bool failed: false
    property bool animate: true
    property color accentColor: "#beb7ff"
    property color backgroundColor: "#1c1f29"
    property string fontFamily: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
    signal cancelRequested()
    implicitWidth: 460
    implicitHeight: 300
    radius: 22
    color: backgroundColor
    border.color: "#343949"
    Rectangle { x: 32; y: 0; width: 56; height: 3; radius: 1; color: root.accentColor }
    Button {
        id: closeButton
        objectName: "startupClose"
        anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 14
        width: 28; height: 28
        text: "×"
        Accessible.name: qsTranslate("Oran7Ui", "退出")
        onClicked: root.cancelRequested()
        contentItem: Text { text: closeButton.text; color: "#969cad"; font.pixelSize: 21; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 8; color: closeButton.hovered ? "#18ffffff" : "transparent"; border.color: closeButton.activeFocus ? root.accentColor : "transparent" }
    }
    Column {
        x: 32; y: 44; width: parent.width - 64
        spacing: 5
        Text { text: "Oran柒"; color: "#f1f1f7"; font.family: "华文琥珀"; font.pixelSize: 38 }
        Text { text: "MediaClient"; color: root.accentColor; font.family: root.fontFamily; font.pixelSize: 15; font.letterSpacing: 1.5 }
    }
    Text {
        id: status
        objectName: "startupStatus"
        x: 32; y: 160; width: parent.width - 64
        text: qsTranslate("Oran7Ui", root.stageText)
        color: root.failed ? "#f29b9b" : "#e0e3ed"
        font.family: root.fontFamily; font.pixelSize: 14
        elide: Text.ElideRight
    }
    Rectangle {
        id: track
        objectName: "startupActivityTrack"
        x: 32; y: 195; width: parent.width - 64; height: 3; radius: 2
        color: "#323746"
        clip: true
        visible: !root.failed
        property real flowPosition: 0
        // 归一化位移配合左侧副本无缝绕回，始终向右，不表示完成百分比。
        NumberAnimation on flowPosition {
            from: 0; to: 1; duration: 1600
            loops: Animation.Infinite
            // 原生提交成功不等于桌面可见；底层活动条始终保留，避免透明窗合成差异导致空白。
            running: root.animate && !root.failed
            easing.type: Easing.Linear
        }
        Rectangle {
            id: pulse
            objectName: "startupActivityPulse"
            x: track.flowPosition * track.width
            width: parent.width * 0.3; height: parent.height; radius: 2
            color: root.accentColor
        }
        Rectangle {
            objectName: "startupActivityWrap"
            x: pulse.x - track.width
            width: pulse.width; height: pulse.height; radius: pulse.radius
            color: root.accentColor
        }
        Oran7StartupActivityItem {
            id: nativeActivity
            objectName: "startupNativeActivity"
            anchors.fill: parent
            running: root.animate && !root.failed
            accent: root.accentColor
            trackColor: track.color
        }
    }
    Text {
        objectName: "startupDetail"
        x: 32; y: 216; width: parent.width - 64; height: 60
        text: root.failed ? qsTranslate("Oran7Ui", root.errorText) : qsTranslate("Oran7Ui", "界面就绪后自动进入")
        color: "#9299ad"
        font.family: root.fontFamily; font.pixelSize: 11
        wrapMode: Text.Wrap
        maximumLineCount: 3
        elide: Text.ElideRight
    }
}
