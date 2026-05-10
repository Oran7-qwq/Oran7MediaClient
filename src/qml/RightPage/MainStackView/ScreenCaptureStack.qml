import QtQuick 2.15
import "../../Components"
import Client 1.0

Item {
    id:root_parent
    property string pageName: ""
    Item{
        id:root
        anchors.fill: parent
        Oran7ScreenCaptureComponent {
            anchors.fill: parent
            renderObject: Client.ScreenCaptureRender
        }
    }
}
