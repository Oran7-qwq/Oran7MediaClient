import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import "../../Settings/GlobalSettings"

Item {
    id: root
    width: Oran7MainUiSetting.itemHeight * 0.833
    height: width

    property color selectedColor: "white"

    Rectangle {
        anchors.fill: parent
        color: root.selectedColor
        border.color: Oran7MainUiSetting.itemBackColor
        border.width: 1
        radius: 0

        MouseArea {
            anchors.fill: parent
            onClicked: {

            }
        }
    }
}
