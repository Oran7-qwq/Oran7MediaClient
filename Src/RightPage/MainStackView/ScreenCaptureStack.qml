import QtQuick 2.15
import "../../Components"

Item {
    id:root_parent
    property string pageName: ""
    Item{
        id:root
        anchors.fill: parent
        ELoader{
            size: 100
            color: "white"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            speed: 0.8
        }
    }
}
