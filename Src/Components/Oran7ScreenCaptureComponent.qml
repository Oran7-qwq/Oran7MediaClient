//Oran7ScreenCaptureComponent.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Client 1.0

Rectangle {
    id:root
    color: "black"
    property var screenCapture: null
    property alias renderHost: renderHost
    property int renderObject: Client.ScreenCaptureRender
    Rectangle{
        id:renderHost
        width: root.width
        height: root.height
        color:"transparent"
    }
    Button{
        text: "Preview"
        onClicked: {
            root.tryAttachDelayed()
            root.screenCapture.start()
        }
    }
    // =============== ATTACH() ================
    function tryAttach() {
        if (!renderHost) return false
        console.log("--->Oran7ScreenCaptureComponent Client.attachVideoItem() : ",renderHost)
        Client.attachVideoItem(root.renderObject,renderHost)
        return true
    }
    //在onVisibleChanged或外部beagining of play video 时调用
    function tryAttachDelayed() {
        if (attachTimer)
            attachTimer.restart()
        else
            Qt.callLater(tryAttachDelayed)
    }
    Timer {
        id: attachTimer
        interval: 0
        onTriggered: {
            if (!tryAttach())
                retryTimer.start()
        }
    }
    Timer {
        id: retryTimer
        interval: 1000
        repeat: true
        onTriggered: {
            if (tryAttach())
                retryTimer.stop()
        }
    }

    // ================== Component ===============//

    Component.onCompleted: {
        console.log("Oran7ScreenCaptureComponent onCompleted")
        if (!renderHost) return false
        root.tryAttachDelayed()
        root.screenCapture = Client.screenCapture()
    }
    Component.onDestruction: {
        if (attachTimer) attachTimer.stop()
        if (retryTimer) retryTimer.stop()
    }
    onVisibleChanged: {
        if (visible) root.tryAttachDelayed()
    }
}
