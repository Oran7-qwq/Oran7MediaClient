import QtQuick 2.15
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import "../../Basic"
import BilibiliAuth 1.0

Popup {
    id: bilibiliLoginPopup
    width: 380
    height: 480
    closePolicy: Popup.NoAutoClose
    padding: 0

    // 监听BasicConfig信号，自动打开弹窗
    Connections {
        target: BasicConfig
        function onOpenBilibiliLoginPopup() {
            bilibiliLoginPopup.open()
        }
    }

    background: Rectangle {
        id: popupBg
        anchors.fill: parent
        color: "#fef2e8"
        radius: 10
        border.color: "#3a3a3d"
        border.width: 2

        // 右上角关闭按钮
        Image {
            id: closeBtn
            source: "qrc:/image/close.png"
            scale: 1.5
            anchors.right: parent.right
            anchors.rightMargin: 30
            anchors.top: parent.top
            anchors.topMargin: 25
            layer.enabled: false
            layer.effect: ColorOverlay {
                source: closeBtn
                color: "white"
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    closeBtn.layer.enabled = true
                    cursorShape = Qt.PointingHandCursor
                }
                onExited: {
                    closeBtn.layer.enabled = false
                    cursorShape = Qt.ArrowCursor
                }
                onClicked: {
                    bilibiliLoginPopup.close()
                }
            }
        }

        // 标题
        Label {
            id: titleLabel
            anchors.top: parent.top
            anchors.topMargin: 50
            anchors.horizontalCenter: parent.horizontalCenter
            text: "B站扫码登录"
            font.pixelSize: 20
            font.family: "微软雅黑"
            color: "#2a1a22"
            font.bold: true
        }

        // QR码容器
        Rectangle {
            id: qrCodeContainer
            width: 200
            height: 200
            anchors.top: titleLabel.bottom
            anchors.topMargin: 30
            anchors.horizontalCenter: parent.horizontalCenter
            color: "white"
            radius: 8
            border.color: "#e0e0e0"
            border.width: 1

            // QR码图片
            Image {
                id: qrCodeImage
                anchors.fill: parent
                anchors.margins: 10
                fillMode: Image.PreserveAspectFit
                source: BilibiliAuthManager.qrCodeUrl
                visible: BilibiliAuthManager.loginState === BilibiliAuthManager.WaitingScan ||
                         BilibiliAuthManager.loginState === BilibiliAuthManager.Scanned

                onStatusChanged: {
                    if (status === Image.Error) {
                        console.warn("BilibiliLoginPopup: QR code image load failed")
                    }
                }
            }

            // 加载中动画
            Rectangle {
                anchors.fill: parent
                color: "white"
                visible: BilibiliAuthManager.loginState === BilibiliAuthManager.Generating
                Label {
                    anchors.centerIn: parent
                    text: "生成中..."
                    font.pixelSize: 14
                    color: "#888888"
                }
            }

            // 已登录覆盖层
            Rectangle {
                anchors.fill: parent
                color: "#80FFFFFF"
                visible: BilibiliAuthManager.loginState === BilibiliAuthManager.Success
                radius: 8

                Label {
                    anchors.centerIn: parent
                    text: "✓ 登录成功"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#4CAF50"
                }
            }

            // 过期覆盖层
            Rectangle {
                anchors.fill: parent
                color: "#80FFFFFF"
                visible: BilibiliAuthManager.loginState === BilibiliAuthManager.Expired
                radius: 8

                Label {
                    id: expiredLabel
                    anchors.centerIn: parent
                    text: "二维码已过期"
                    font.pixelSize: 16
                    color: "#FF5722"
                }
            }

            // 错误覆盖层
            Rectangle {
                anchors.fill: parent
                color: "#80FFFFFF"
                visible: BilibiliAuthManager.loginState === BilibiliAuthManager.Error
                radius: 8

                Label {
                    anchors.centerIn: parent
                    text: "网络错误"
                    font.pixelSize: 16
                    color: "#F44336"
                }
            }
        }

        // 状态文字
        Label {
            id: statusLabel
            anchors.top: qrCodeContainer.bottom
            anchors.topMargin: 20
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: 14
            font.family: "微软雅黑"
            color: {
                switch (BilibiliAuthManager.loginState) {
                case BilibiliAuthManager.Success: return "#4CAF50"
                case BilibiliAuthManager.Expired: return "#FF5722"
                case BilibiliAuthManager.Error: return "#F44336"
                case BilibiliAuthManager.Scanned: return "#2196F3"
                default: return "#888888"
                }
            }
            text: {
                switch (BilibiliAuthManager.loginState) {
                case BilibiliAuthManager.Idle: return ""
                case BilibiliAuthManager.Generating: return "正在生成二维码..."
                case BilibiliAuthManager.WaitingScan: return "请使用哔哩哔哩App扫描二维码"
                case BilibiliAuthManager.Scanned: return "扫描成功，请在手机上确认登录"
                case BilibiliAuthManager.Success: return "登录成功！欢迎 " + BilibiliAuthManager.userName
                case BilibiliAuthManager.Expired: return "二维码已过期，请点击刷新"
                case BilibiliAuthManager.Error: return "网络错误，请重试"
                }
            }
        }

        // 刷新按钮（过期或错误时显示）
        Rectangle {
            id: refreshBtn
            width: 140
            height: 36
            radius: 18
            color: refreshBtnArea.containsMouse ? "#fb7299" : "#FF6699"
            anchors.top: statusLabel.bottom
            anchors.topMargin: 15
            anchors.horizontalCenter: parent.horizontalCenter
            visible: BilibiliAuthManager.loginState === BilibiliAuthManager.Expired ||
                     BilibiliAuthManager.loginState === BilibiliAuthManager.Error

            Label {
                anchors.centerIn: parent
                text: "刷新二维码"
                font.pixelSize: 14
                font.family: "微软雅黑"
                color: "white"
            }

            MouseArea {
                id: refreshBtnArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    BilibiliAuthManager.startLogin()
                }
            }
        }

        // 底部说明
        Label {
            id: instructionLabel
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 40
            anchors.horizontalCenter: parent.horizontalCenter
            text: "使用哔哩哔哩客户端扫码登录"
            font.pixelSize: 13
            font.family: "微软雅黑"
            color: "#bbbbbe"
        }
    }

    // 登录成功自动关闭
    Connections {
        target: BilibiliAuthManager
        function onLoginStatusChanged() {
            if (BilibiliAuthManager.isLoggedIn) {
                // 延迟1秒关闭，让用户看到成功提示
                closeTimer.start()
            }
        }
    }

    Timer {
        id: closeTimer
        interval: 1200
        repeat: false
        onTriggered: {
            bilibiliLoginPopup.close()
        }
    }

    // 弹窗关闭时取消登录
    onClosed: {
        BilibiliAuthManager.cancelLogin()
    }

    // 弹窗打开时自动开始登录
    onOpened: {
        if (!BilibiliAuthManager.isLoggedIn) {
            BilibiliAuthManager.startLogin()
        }
    }
}
