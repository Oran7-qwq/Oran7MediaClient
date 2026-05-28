import QtQuick

import "../../Settings/GlobalSettings"

Rectangle{
    id:root

    property real scale: 1
    property bool checked: false

    width: Oran7MainUiSetting.itemHeight * 0.7 * root.scale
    height: width
    radius: 4
    anchors.left: parent.right
    anchors.verticalCenter: parent.verticalCenter
    border.width: 1
    border.color: Oran7MainUiSetting.textColor
    color: "#ffffff"
    //Behavior on color{PropertyAnimation{duration:Oran7Theme.Primary.durationMid}}
    Behavior on scale{NumberAnimation{duration: 50}}

    MouseArea{
        anchors.fill:parent
        hoverEnabled: true
        onEntered:root.scale = 1.05
        onExited: root.scale = 1
        onPressed: root.scale = 0.95
        onReleased: root.scale = 1
        onClicked: root.checked = !root.checked
    }
}
