import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

import "../../Settings/GlobalSettings"
import "../"

import Oran7UI.Impl

Rectangle {
    id: root
    anchors.left: parent.left
    anchors.right: parent.right
    height: Oran7MainUiSetting.itemHeight

    color: "transparent"

    // 悬停背景层 —— 独立子项，opacity 只影响自己，不影响其他子组件
    Rectangle {
        anchors.fill: parent
        color: Oran7MainUiSetting.itemHoverdColor
        opacity: enableHoverHandler && hoverHandler.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Oran7Theme.Primary.durationVerySlow; easing.type: Easing.OutQuint } }
    }

    property bool showTag: true
    property color backColor:"transparent"
    property real radius: 0

    property color textColor: Oran7MainUiSetting.textColor
    property string text: ""
    property bool fontBold:false
    property bool fontItalic:false
    property bool gradientMaskEnabled: false
    property int textHAlign: Text.AlignLeft

    property int index: 0
    property bool enableMouseArea: false
    property bool enableHoverHandler: false
    readonly property bool hovered:hoverHandler.hovered

    signal rightClicked()
    signal leftClicked()

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
        Oran7GradientMask{
            anchors.fill: parent
            _dynamicIndex:root.index
            gradientMaskEnabled:root.gradientMaskEnabled
            dynamicGradient:true
            transitionDuration:Oran7Theme.Primary.durationVerySlow
            dynamicInterval:Oran7Theme.Primary.durationVerySlow
            Label {
                id:textLabel
                anchors.fill: parent
                color: root.textColor
                text: root.text
                font.bold:root.fontBold
                font.italic: root.fontItalic
                font.pixelSize: Oran7MainUiSetting.textPixelSize
                font.family: Oran7MainUiSetting.fontFamily
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: root.textHAlign
            }
        }
    }

    MouseArea{
        anchors.fill: parent
        enabled:root.enableMouseArea
        acceptedButtons: Qt.RightButton | Qt.LeftButton
        onClicked:mouse => {
            if(mouse.button === Qt.RightButton)
                root.rightClicked()
            if(mouse.button === Qt.LeftButton)
                root.leftClicked()
        }
    }
    HoverHandler{
        id:hoverHandler
        enabled: root.enableHoverHandler
        acceptedDevices: PointerDevice.Mouse
    }
}
