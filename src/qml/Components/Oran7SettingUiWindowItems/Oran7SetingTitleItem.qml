import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

import "../../Settings/GlobalSettings"

import Oran7UI.Impl

Rectangle {
    id: root
    anchors.left: parent.left
    anchors.right: parent.right
    height: 30
    color: Oran7MainUiSetting.itemBackColor
    radius: 8

    property string icon: "qrc:/image/hugeicons_ai-setting.png"
    property string title: "Title"

    Image {
        id: topIcon
        source: root.icon
        sourceSize.height: root.height * 0.8
        sourceSize.width: root.height * 0.8
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        mipmap: true
        layer.enabled: true
        layer.effect: ColorOverlay {
            source: topIcon
            color: Oran7MainUiSetting.textColor
        }
    }
    Label {
        anchors.left: topIcon.right
        anchors.leftMargin: 4
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        text: root.title
        color: Oran7MainUiSetting.textColor
        font.pixelSize: Oran7MainUiSetting.textPixelSize
        font.family: Oran7MainUiSetting.fontFamily
        font.bold: true
    }
}
