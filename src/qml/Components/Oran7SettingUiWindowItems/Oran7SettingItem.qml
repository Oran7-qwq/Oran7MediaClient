import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

import "../../Settings/GlobalSettings"

Item {
    id: root

    // 属性定义
    property string text: ""

    anchors.left: parent.left
    anchors.right: parent.right
    height: Oran7MainUiSetting.itemHeight

    Image{
        id:tagImage
        width: root.height * 0.75
        height: width
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        source: "qrc:/image/streamline_star-2-solid.png"
        layer.enabled: true
        layer.effect: ColorOverlay{
            source: tagImage
            color:Oran7MainUiSetting.tagColor
        }

        mipmap: true
        antialiasing: true
    }

    Label {
        anchors.top:parent.top
        anchors.left: tagImage.right
        anchors.leftMargin: 4
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.bottom: parent.bottom
        color: Oran7MainUiSetting.textColor
        text: root.text
        font.pixelSize: Oran7MainUiSetting.textPixelSize
        font.family: Oran7MainUiSetting.fontFamily
        verticalAlignment: Text.AlignVCenter
    }

    //header_ line
    // Rectangle{
    //     color: Oran7MainUiSetting.tagColor
    //     width:2
    //     height: Oran7MainUiSetting.itemHeight
    //     anchors.left: parent.left
    // }
}
