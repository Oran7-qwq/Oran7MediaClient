import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    required property var view
    required property var header
    anchors.top: header.bottom
    anchors.topMargin: 8
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.rightMargin: 20
    height: Math.max(28, view.playlistFontSize + 12)
    radius: 4
    // 装饰背景由列表门面的共享底板绘制，普通/多选模式只切换内容。
    color: "transparent"
    visible: !view.isMultiSelected

    Label {
        x: 12
        anchors.verticalCenter: parent.verticalCenter
        text: "#"
        color: root.view.playlistText
        font.pixelSize: root.view.playlistFontSize
        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
    }
    Label {
        objectName: "playlistColumnTitle"
        x: 104
        anchors.verticalCenter: parent.verticalCenter
        text: qsTranslate("Oran7Ui", "标题")
        color: root.view.playlistText
        font.pixelSize: root.view.playlistFontSize
        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
    }
    Label {
        objectName: "playlistColumnAlbum"
        x: Math.max(290, parent.width * 0.53)
        anchors.verticalCenter: parent.verticalCenter
        text: qsTranslate("Oran7Ui", "专辑")
        color: root.view.playlistText
        font.pixelSize: root.view.playlistFontSize
        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
        visible: parent.width >= 660
    }
    Label {
        objectName: "playlistColumnDuration"
        x: parent.width - 72
        anchors.verticalCenter: parent.verticalCenter
        text: qsTranslate("Oran7Ui", "时长")
        color: root.view.playlistText
        font.pixelSize: root.view.playlistFontSize
        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
        visible: root.view.showDuration
    }
}
