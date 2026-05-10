import QtQuick
import QtQuick.Controls

import "../../Settings/GlobalSettings"

Item {
    id:root

    height: Oran7MainUiSetting.itemHeight
    anchors.left: parent.left
    anchors.right: parent.right

    Rectangle{
        id:diveLineUp
        height: 4
        anchors.top: parent.top
        width: parent.width
        color:Oran7MainUiSetting.textColor
    }
}
