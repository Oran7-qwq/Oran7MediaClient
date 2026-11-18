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
    property color accentColor: "#6979bc"
    property color backgroundColor: "#fafbfe"
    property string fontFamily: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
    signal cancelRequested()
    implicitWidth: 460
    implicitHeight: 300
    radius: 22
    color: backgroundColor
    border.color: "#dfe4ed"

    // 品牌区与状态区分开，保持启动画面轻量。
    Row {
        x: 32; y: 29; spacing: 8
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 6; height: 6; radius: 3
            color: root.accentColor
        }
        Text {
            text: "ORAN7 / MEDIA"
            color: "#7b8498"
            font.family: root.fontFamily; font.pixelSize: 10; font.letterSpacing: 1.4
        }
    }
    Button {
        id: closeButton
        objectName: "startupClose"
        anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 14
        width: 30; height: 30
        Accessible.name: qsTranslate("Oran7Ui", "退出")
        onClicked: root.cancelRequested()
        contentItem: Item {
            Repeater {
                model: [45, -45]
                Rectangle {
                    required property int modelData
                    anchors.centerIn: parent
                    width: 12; height: 1.4; radius: 0.7
                    rotation: modelData
                    antialiasing: true
                    color: closeButton.hovered ? "#a44956" : "#7b8498"
                }
            }
        }
        background: Rectangle {
            radius: 9
            color: closeButton.down ? "#f2dfe3" : closeButton.hovered ? "#f7ebee" : "transparent"
            border.color: closeButton.activeFocus ? root.accentColor : "transparent"
            Behavior on color { ColorAnimation { duration: 130 } }
        }
    }
    Column {
        x: 32; y: 73; width: parent.width - 164
        spacing: 3
        Text { text: "Oran柒"; color: "#29334b"; font.family: "华文琥珀"; font.pixelSize: 38 }
        Text { text: "MediaClient"; color: root.accentColor; font.family: root.fontFamily; font.pixelSize: 14; font.letterSpacing: 2 }
    }
    Rectangle {
        x: 20; y: 176
        width: parent.width - 40; height: parent.height - y - 20
        radius: 15
        color: root.failed ? "#fcf0f1" : "#f0f3f9"
        border.color: root.failed ? "#f0dce0" : "#e8ecf3"
    }
    Text {
        id: status
        objectName: "startupStatus"
        x: 36; y: 190; width: parent.width - 72
        text: qsTranslate("Oran7Ui", root.stageText)
        color: root.failed ? "#a44956" : "#35415b"
        font.family: root.fontFamily; font.pixelSize: 14
        elide: Text.ElideRight
    }
    Rectangle {
        id: track
        objectName: "startupActivityTrack"
        x: 36; y: 222; width: parent.width - 72; height: 3; radius: 2
        color: "#dde3ef"
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
        x: 36; y: root.failed ? 219 : 241
        width: parent.width - 72; height: root.failed ? 48 : 30
        text: root.failed ? qsTranslate("Oran7Ui", root.errorText) : qsTranslate("Oran7Ui", "界面就绪后自动进入")
        color: root.failed ? "#925e67" : "#6d7890"
        font.family: root.fontFamily; font.pixelSize: 11
        wrapMode: Text.Wrap
        maximumLineCount: 3
        elide: Text.ElideRight
    }
}
