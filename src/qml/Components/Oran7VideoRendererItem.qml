// VideoRendererItem.qml
import QtQuick
import QtQuick.Window
import Client 1.0
import D3D11VideoItem 1.0

Rectangle {
    id: root
    color: "black"
    property alias videoHost: videoHost
    property bool openVideoInfo: false

    property int renderObject: Client.VideoPlayerRender

    // 确保在窗口可见时才尝试连接
    visible: true
    clip: true

    Rectangle {
        id: videoHost
        width: root.width
        height: root.height
        color:"transparent"
        clip:true
    }

    Connections{
        target:Client
        function onUpdateQmlRenderedVideoInfo(info)
        {
            root.videoInfo = info
        }
    }
    property var videoInfo: ({})

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 10
        width: debugText.contentWidth + 20
        height: debugText.contentHeight + 20
        color: "#80000000"
        radius: 5
        visible: root.openVideoInfo

        Text {
            id: debugText
            anchors.centerIn: parent
            text:
                "源尺寸: " + (root.videoInfo.srcWidth || root.videoInfo.srcHeight  ? (root.videoInfo.srcWidth + "x" + root.videoInfo.srcHeight) : "N/A") + "\n" +
                "源帧格式: " + root.videoInfo.srcFormatName +"\n" +
                "渲染尺寸: " + (root.videoInfo.renderWidth || root.videoInfo.renderHeight ? (root.videoInfo.renderWidth + "x" +root.videoInfo.renderHeight ) : "N/A") + "\n" +
                "渲染帧格式: " + root.videoInfo.dxgiFormatName +"\n" +
                "是否启用硬解: " + root.videoInfo.isFromHardWare +"\n" +
                "解码设备: " + root.videoInfo.decodeDevice +"\n" +
                "渲染设备: " + root.videoInfo.renderDevice +"\n" +
                "填充模式: " + root.videoInfo.fillModeName +"\n" +
                "刷新帧率: " + root.videoInfo.fps
            color: "white"
            font.pixelSize: 12
        }
    }

    function tryAttach() {
        if (!videoHost) return false
        console.log("--->Client.attachVideoItem() : ",videoHost)
        Client.attachVideoItem(root.renderObject,videoHost)
        return true
    }

    //在onVisibleChanged或外部beagining of play video 时调用
    function tryAttachDelayed() {
        if (attachTimer)
            attachTimer.restart()
        else
            // 还没构建好就下一轮事件循环再试
            Qt.callLater(tryAttachDelayed)
    }

    Timer {
        id: attachTimer
        interval: 0
        onTriggered: {
            if (!tryAttach())
            {
                // 如果失败，每秒重试一次
                retryTimer.start()
            }
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

    Component.onCompleted: {
        //console.log("Component onCompleted")
        if (!videoHost) return false
        //console.log("--->Client.attachVideoItem() : ",videoHost)
        Client.attachVideoItem(root.renderObject,videoHost)
    }
    Component.onDestruction: {
        if (attachTimer) attachTimer.stop()
        if (retryTimer) retryTimer.stop()
    }
    onVisibleChanged: {
        if (visible) tryAttachDelayed()
    }
}
