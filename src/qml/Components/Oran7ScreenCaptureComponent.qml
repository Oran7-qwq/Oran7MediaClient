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
    property bool firstOpen: true
    property bool openScreenInfo: true  // 默认显示抓屏信息面板

    // ========= 抓屏信息面板 =========
    Connections{
        target:Client
        function onVideoRenderInfoUpdated(info)
        {
            if (info.sourceTag === "screen")
                root.screenInfo = info
        }
    }
    property var screenInfo: ({})

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 10
        width: sciText.contentWidth + 20
        height: sciText.contentHeight + 20
        color: "#80000000"
        radius: 5
        z: 100
        visible: root.openScreenInfo && root.screenInfo.srcWidth > 0

        Text {
            id: sciText
            anchors.centerIn: parent
            text:
                "抓屏尺寸: " + root.screenInfo.srcWidth + "x" + root.screenInfo.srcHeight + "\n" +
                "源帧格式: " + root.screenInfo.srcFormatName + "\n" +
                "DXGI格式:  " + root.screenInfo.dxgiFormatName  + "\n" +
                "抓屏帧率: " + root.screenInfo.fps
            color: "#00FF88"
            font.pixelSize: 12
        }
    }
    // =================================

    Rectangle{
        id:renderHost
        width: root.width
        height: root.height
        color:"transparent"
    }
    // Row{
    //     Button{
    //         text: "Preview"
    //         onClicked: {
    //             root.tryAttachDelayed()
    //             root.screenCapture.start()
    //         }
    //     }
    //     Button{
    //         text: "Stop"
    //         onClicked: {
    //             root.screenCapture.stop()
    //         }
    //     }
    // }

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
        console.log("tryAttachDelayed()")
    }
    Timer {
        id: attachTimer
        interval: 0
        onTriggered: {
            if (!tryAttach())
                retryTimer.start()
            else{
                if(root.firstOpen){
                    root.screenCapture.start()//自动打开预览Qt.callLater
                    root.firstOpen = false
                }
            }
        }
    }
    Timer {
        id: retryTimer
        interval: 1000
        repeat: true
        onTriggered: {
            if (tryAttach()){
                retryTimer.stop()
                if(root.firstOpen){
                    root.screenCapture.start()//自动打开预览Qt.callLater
                    root.firstOpen = false
                }
            }
        }
    }

    // ================== Component ===============//

    Component.onCompleted: {
        console.log("Oran7ScreenCaptureComponent onCompleted")
        if (!renderHost) return false
        root.screenCapture = Client.screenCapture()

        root.tryAttachDelayed()
    }
    Component.onDestruction: {
        if (attachTimer) attachTimer.stop()
        if (retryTimer) retryTimer.stop()
    }
    onVisibleChanged: {
        if (visible) root.tryAttachDelayed()
    }
}
