import QtQuick 2.15
import "../../Components"

Item {
    id:root_parent
    property string pageName: ""
    Item{
        id:root
        anchors.fill: parent
        Oran7ScreenCapture {
            anchors.fill: parent
            targetFps: 15
            screenIndex: 0
        }
        // ELoader{
        //     size: 100
        //     color: "white"
        //     anchors.horizontalCenter: parent.horizontalCenter
        //     anchors.verticalCenter: parent.verticalCenter
        //     speed: 0.8
        // }
    }
}
