import QtQuick 2.15
import Qt5Compat.GraphicalEffects
import "../../Basic"

import Oran7UI.Impl
import Oran7NetEaseAuth 1.0

Item {
    id: root
    height: 28
    width: contentRow.implicitWidth + 8

    signal loginClicked

    Row {
        id: contentRow
        anchors.verticalCenter: parent.verticalCenter
        spacing: 5

        // 头像圆圈
        Rectangle {
            id: avatarCircle
            width: root.height
            height: root.height
            radius: width / 2
            color: "#fef2e8"
            border.color: "transparent"
            Image {
                id: avatarImage
                width: parent.width * 1.12
                height: parent.height * 1.12
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter

                source: "qrc:/image/thesvg_netease-cloud-music.png"
                layer.enabled: true
                layer.effect: ColorOverlay{
                    source: avatarImage
                    color:"#fc3d49"
                }
                sourceSize: Qt.size(512, 512)
                mipmap: true
                smooth: true
            }
        }

        // 登录状态文案
        Text {
            id: statusText
            text: NcmSession.isLoggedIn ? (NcmSession.userName + " ∨") : qsTranslate("Oran7Ui", "未登录∧")
            color: root.hovered ? Oran7Theme.Oran7MusicPlaylistView["textColorBase-4"] :
                                                       Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
            font.pixelSize: 17
            font.bold: true
            font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
            anchors.verticalCenter: avatarCircle.verticalCenter
        }
    }

    property bool hovered: false

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onEntered: root.hovered = true
        onExited: root.hovered = false
        onClicked: root.loginClicked()
    }
}
