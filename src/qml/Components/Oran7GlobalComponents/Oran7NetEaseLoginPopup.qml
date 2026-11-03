import QtQuick 2.15
import QtQuick.Controls
import "../../Basic"

import Oran7NetEaseAuth 1.0
import Oran7Network 1.0

// 网易云音乐登录弹窗
// 主登录方式：网页登录（NcmWebLogin）——启动本机 Edge/Chrome 打开网易云官方登录页，
// 用户在真实浏览器登录，经 CDP 读取 HttpOnly 的 MUSIC_U/__csrf 回填 CookieStore。
// 登录统一使用现行 NcmWebLogin 浏览器入口。
Popup {
    id: root
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose
    width: 380
    height: NcmSession.isLoggedIn ? 200 : 480
    Behavior on height {NumberAnimation{duration:200; easing.type: Easing.OutCubic }}
    anchors.centerIn: Overlay.overlay
    padding: 0

    // 错误提示（来自 loginFailed）
    property string statusMsg: ""
    property bool statusError: false
    // 无网络（网络监测器实时状态）：离线时登录页直接显示"无网络"
    property bool offline: !Oran7NetworkMonitor.online

    background: Rectangle {
        id: popupBg
        anchors.fill: parent
        color: "#fef2e8"
        radius: 10
        border.color: "#3a3a3d"
        border.width: 2

        // 右上角关闭
        Image {
            id: closeImag
            source: "qrc:/image/close.png"
            scale: 1.5
            anchors.right: parent.right
            anchors.rightMargin: 30
            anchors.top: parent.top
            anchors.topMargin: 25
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.close()
            }
        }

        // ========== 已登录态 ==========
        Column {
            // 离线时隐藏"已登录：用户名"与"退出登录"按钮，改由根层"无网络连接"覆盖层展示
            visible: NcmSession.isLoggedIn && !root.offline
            anchors.centerIn: parent
            spacing: 16
            Text {
                text: qsTranslate("Oran7Ui", "✓ 已登录：") + NcmSession.userName
                font.bold: true
                font.pixelSize: 16
                color: "#2ea44f"
            }
            // 退出登录按钮（与"在浏览器中打开登录"同款圆角渐变，改棕灰色）
            Rectangle {
                id: logoutBtn
                width: 220
                height: 44
                radius: height / 2
                anchors.horizontalCenter: parent.horizontalCenter
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { color: "#8d7a6a"; position: 0.0 }
                    GradientStop { color: "#5e5045"; position: 1.0 }
                }
                property bool pressed: false
                scale: pressed ? 0.97 : 1.0
                Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutQuad } }

                Label {
                    anchors.centerIn: parent
                    text: qsTranslate("Oran7Ui", "退出登录")
                    font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                    font.bold: true
                    font.pixelSize: 17
                    color: "white"
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onPressed: logoutBtn.pressed = true
                    onReleased: logoutBtn.pressed = false
                    onClicked: {
                        // 清空会话 + 配置文件里的登录信息（MUSIC_U/__csrf/nickname）
                        NcmSession.logout()
                        // 复位网页登录状态机，否则停留在 Success，重新登录按钮不会显示
                        NcmWebLogin.cancel()
                        root.statusMsg = ""
                        root.statusError = false
                    }
                }
            }
        }

        // ========== 未登录态 ==========
        Item {
            visible: !NcmSession.isLoggedIn
            anchors.fill: parent

            // 标题
            Label {
                id: titleLabel
                anchors.top: parent.top
                anchors.topMargin: 45
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTranslate("Oran7Ui", "网易云音乐登录")
                font.pixelSize: 20
                font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                color: "#2a1a22"
                font.bold: true
            }

            // 副标题
            Label {
                id: subtitleLabel
                anchors.top: titleLabel.bottom
                anchors.topMargin: 14
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTranslate("Oran7Ui", "网页登录 · 无需扫码接口，更稳定")
                font.pixelSize: 13
                font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                color: "#98989b"
            }

            // 主体：按钮 或 状态/进度（离线时隐藏，改显"无网络"提示）
            Column {
                anchors.top: subtitleLabel.bottom
                anchors.topMargin: 40
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 20
                width: 300
                visible: !root.offline

                // 开始按钮（Idle / Error 状态显示）
                Rectangle {
                    id: startBtn
                    width: 300
                    height: 44
                    radius: height / 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: NcmWebLogin.loginState === NcmWebLogin.Idle ||
                             NcmWebLogin.loginState === NcmWebLogin.Error
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { color: "#ff1168"; position: 0.0 }
                        GradientStop { color: "#fc3d49"; position: 1.0 }
                    }
                    property bool pressed: false
                    scale: pressed ? 0.97 : 1.0
                    Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutQuad } }

                    Label {
                        anchors.centerIn: parent
                        text: NcmWebLogin.loginState === NcmWebLogin.Error ? qsTranslate("Oran7Ui", "重新打开浏览器登录") : qsTranslate("Oran7Ui", "在浏览器中打开登录")
                        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                        font.bold: true
                        font.pixelSize: 17
                        color: "white"
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onPressed: startBtn.pressed = true
                        onReleased: startBtn.pressed = false
                        onClicked: {
                            root.statusMsg = ""
                            root.statusError = false
                            NcmWebLogin.start()
                        }
                    }
                }

                // 进行中状态（StartingBrowser / WaitingLogin / Captured）
                Column {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 16
                    visible: NcmWebLogin.loginState === NcmWebLogin.StartingBrowser ||
                             NcmWebLogin.loginState === NcmWebLogin.WaitingLogin ||
                             NcmWebLogin.loginState === NcmWebLogin.Captured

                    BusyIndicator {
                        anchors.horizontalCenter: parent.horizontalCenter
                        running: parent.visible
                        width: 44
                        height: 44
                    }

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 280
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        text: NcmWebLogin.statusText
                        font.pixelSize: 14
                        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                        color: "#2a1a22"
                    }

                    // 取消按钮
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTranslate("Oran7Ui", "取消登录")
                        font.pixelSize: 13
                        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                        color: "#5b9bd5"
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                NcmWebLogin.cancel()
                                root.statusMsg = ""
                                root.statusError = false
                            }
                        }
                    }
                }
            }

            // 状态/错误文案（错误或提示）
            Label {
                id: statusLabel
                anchors.top: subtitleLabel.bottom
                anchors.topMargin: 150
                anchors.horizontalCenter: parent.horizontalCenter
                width: 300
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                visible: (root.statusMsg.length > 0) &&
                         (NcmWebLogin.loginState === NcmWebLogin.Error)
                text: root.statusMsg
                font.pixelSize: 13
                font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                color: "#c42b1c"
            }

            // 底部说明
            Label {
                id: instructionLabel
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 40
                anchors.horizontalCenter: parent.horizontalCenter
                width: 320
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                visible: !root.offline
                text: qsTranslate("Oran7Ui", "将打开浏览器，请在浏览器内完成登录；")
                font.pixelSize: 13
                font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                color: "#bbbbbe"
            }
        }

        // 无网络覆盖提示：无论登录态，离线时显示在弹窗最上层（登录按钮/已登录文案都被盖住）
        Column {
            visible: root.offline
            anchors.centerIn: parent
            spacing: 14
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTranslate("Oran7Ui", "无网络连接")
                font.pixelSize: 18
                font.bold: true
                font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                color: "#c42b1c"
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTranslate("Oran7Ui", "请检查网络连接后重试")
                font.pixelSize: 13
                font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                color: "#98989b"
            }
        }
    }

    // 登录失败提示
    Connections {
        target: NcmWebLogin
        function onLoginFailed(message) {
            root.statusMsg = message
            root.statusError = true
        }
    }

    // 登录成功自动关闭
    Connections {
        target: NcmWebLogin
        function onLoginSucceeded(nickname) {
            root.statusMsg = ""
            root.statusError = false
            closeTimer.start()
        }
    }

    Timer {
        id: closeTimer
        interval: 1200
        repeat: false
        onTriggered: root.close()
    }

    // 关闭时取消登录（仅未登录态）
    onClosed: {
        if (!NcmSession.isLoggedIn)
            NcmWebLogin.cancel()
    }

    // 打开时清空状态
    onOpened: {
        root.statusMsg = ""
        root.statusError = false
    }
}
