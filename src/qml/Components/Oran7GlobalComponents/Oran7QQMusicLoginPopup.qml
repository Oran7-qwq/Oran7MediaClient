import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../Basic"

// QQ 音乐账号弹窗。只呈现外部传入的账号/登录状态，不借用网易云会话。
Popup {
    id: root
    objectName: "qqMusicLoginPopup"
    property bool serviceConnected: false
    property bool loggedIn: false
    property bool loginBusy: false
    property string userName: ""
    property string statusText: ""
    property color accentColor: "#31c27c"
    property color surfaceColor: "#fef2e8"
    property color textColor: "#2a1a22"
    property color secondaryTextColor: "#716c68"
    readonly property var uiFont: typeof Oran7UiPreferences !== "undefined"
        ? Oran7UiPreferences.font : Qt.application.font
    signal loginRequested()
    signal loginCancelled()
    signal logoutRequested()

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(388, Overlay.overlay ? Math.max(0, Overlay.overlay.width - 32) : 388)
    height: Math.min(body.implicitHeight + topPadding + bottomPadding,
        Overlay.overlay ? Math.max(0, Overlay.overlay.height - 32) : 480)
    padding: 24
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180 }
            NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 240; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 140 }
    }
    onAboutToShow: {
        if (BasicConfig.activeMusicAccountPopup && BasicConfig.activeMusicAccountPopup !== root)
            BasicConfig.activeMusicAccountPopup.close()
        BasicConfig.activeMusicAccountPopup = root
    }
    onClosed: {
        if (BasicConfig.activeMusicAccountPopup === root) BasicConfig.activeMusicAccountPopup = null
        if (root.loginBusy) root.loginCancelled()
    }
    Component.onDestruction: {
        if (BasicConfig.activeMusicAccountPopup === root) BasicConfig.activeMusicAccountPopup = null
    }

    background: Rectangle {
        color: root.surfaceColor
        radius: 16
        border.color: Qt.alpha(root.textColor, 0.16)
    }
    contentItem: Flickable {
        clip: true
        contentHeight: body.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        ColumnLayout {
            id: body
            width: parent.width
            spacing: 16
            RowLayout {
                Layout.fillWidth: true
                Rectangle {
                    width: 40; height: 40; radius: 12
                    color: Qt.alpha(root.accentColor, 0.12)
                    Image {
                        anchors.centerIn: parent
                        width: 28; height: 28
                        source: "qrc:/image/oran7_qq_music.svg"
                        sourceSize: Qt.size(28, 28)
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTranslate("Oran7Ui", "QQ 音乐登录")
                    font.family: root.uiFont.family
                    font.pixelSize: 20
                    font.bold: true
                    color: root.textColor
                }
                Oran7StudioButton {
                    objectName: "qqMusicLoginClose"
                    implicitWidth: 30; implicitHeight: 30
                    leftPadding: 0; rightPadding: 0
                    text: "×"
                    font.pixelSize: 22
                    textColor: root.secondaryTextColor
                    surfaceColor: "transparent"
                    hoverColor: Qt.alpha(root.textColor, 0.07)
                    borderColor: "transparent"
                    onClicked: root.close()
                }
            }
            Label {
                Layout.fillWidth: true
                text: root.loggedIn ? root.userName : qsTranslate("Oran7Ui", "官方网页扫码登录")
                font.family: root.uiFont.family
                font.pixelSize: 16
                font.bold: true
                color: root.textColor
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                text: root.loggedIn ? qsTranslate("Oran7Ui", "QQ 音乐账号独立管理")
                    : qsTranslate("Oran7Ui", "在官方网页中使用手机扫码，确认登录后同步账号与喜欢的音乐。")
                font.family: root.uiFont.family
                font.pixelSize: 13
                color: root.secondaryTextColor
                wrapMode: Text.Wrap
                lineHeight: 1.35
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: steps.implicitHeight + 28
                radius: 10
                color: Qt.alpha(root.accentColor, 0.07)
                visible: !root.loggedIn
                Column {
                    id: steps
                    x: 14; y: 14; width: parent.width - 28
                    spacing: 12
                    Repeater {
                        model: [qsTranslate("Oran7Ui", "打开 QQ 音乐官方登录页"),
                            qsTranslate("Oran7Ui", "手机扫码并确认登录"),
                            qsTranslate("Oran7Ui", "返回播放器，同步喜欢的音乐")]
                        delegate: RowLayout {
                            required property int index
                            required property string modelData
                            width: steps.width
                            spacing: 10
                            Rectangle {
                                width: 20; height: 20; radius: 10
                                color: root.accentColor
                                Label {
                                    anchors.centerIn: parent
                                    text: index + 1
                                    font.pixelSize: 11
                                    color: root.surfaceColor
                                    font.bold: true
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: modelData
                                color: root.textColor
                                font.family: root.uiFont.family
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
            }
            Label {
                objectName: "qqMusicLoginStatus"
                Layout.fillWidth: true
                text: !root.serviceConnected ? qsTranslate("Oran7Ui", "登录服务暂未连接") : root.statusText
                visible: text.length > 0
                font.family: root.uiFont.family
                font.pixelSize: 12
                color: root.secondaryTextColor
                wrapMode: Text.Wrap
            }
            Oran7StudioButton {
                objectName: "qqMusicLoginStart"
                Layout.fillWidth: true
                implicitHeight: 42
                primary: true
                accentColor: root.accentColor
                cornerRadius: 12
                font.family: root.uiFont.family
                enabled: root.serviceConnected
                text: root.loggedIn ? qsTranslate("Oran7Ui", "退出登录")
                    : root.loginBusy ? qsTranslate("Oran7Ui", "取消登录")
                    : qsTranslate("Oran7Ui", "打开网页扫码登录")
                onClicked: {
                    if (root.loggedIn) root.logoutRequested()
                    else if (root.loginBusy) root.loginCancelled()
                    else root.loginRequested()
                }
            }
        }
    }
}
