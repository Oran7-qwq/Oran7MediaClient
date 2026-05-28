import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

import "../../Settings/GlobalSettings"

Item {
    id: root

    property bool showTag: true
    property color backColor: Oran7MainUiSetting.backColor
    property real radius: 0

    property color textColor: Oran7MainUiSetting.textColor
    property string text: ""
    property bool fontBold: false
    property int textHAlign: Text.AlignLeft

    anchors.left: parent.left
    anchors.right: parent.right
    height: Oran7MainUiSetting.itemHeight

    Image{
        id:tagImage
        width: root.height * 0.75
        height: width
        anchors.right:parent.right
        anchors.verticalCenter: parent.verticalCenter
        source: "qrc:/image/hugeicons_more-horizontal-square-01.png"
        layer.enabled: true
        layer.effect: ColorOverlay{
            source: tagImage
            color:Oran7MainUiSetting.tagColor
        }
        visible: root.showTag

        mipmap: true
        antialiasing: true
    }

    Rectangle{
        color: root.backColor
        anchors.top:parent.top
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.right: tagImage.right
        anchors.rightMargin: root.showTag ? 4 + tagImage.width : 4
        anchors.bottom: parent.bottom
        radius: root.radius
        Label {
            anchors.fill: parent
            color: root.textColor
            text: root.text
            font.bold:root.fontBold
            font.pixelSize: Oran7MainUiSetting.textPixelSize
            font.family: Oran7MainUiSetting.fontFamily
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: root.textHAlign
        }
    }

    //header_ line
    // Rectangle{
    //     color: Oran7MainUiSetting.tagColor
    //     width:2
    //     height: Oran7MainUiSetting.itemHeight
    //     anchors.left: parent.left
    // }
}
