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
    property Item videoTextureItem: null

    // 确保在窗口可见时才尝试连接
    visible: true
    clip: true

    Item {
        id: videoHost
        width: parent.width
        height: parent.height
        clip: true
    }

    Item {
        id: overlayLayer
        anchors.fill: parent
        clip: false

        Oran7BlurCard{
            id: blurCard
            // 直接跟随 D3D11VideoItem 的位置和尺寸
            x: root.videoTextureItem ? root.videoTextureItem.x : 0
            y: root.videoTextureItem ? root.videoTextureItem.y : 0
            width: root.videoTextureItem ? root.videoTextureItem.width : 0
            height: root.videoTextureItem ? root.videoTextureItem.height : 0
            blurSource: root.videoTextureItem
            blurEnabled: false
            // blurSource 是 C++ QSGRenderNode 视频 item，mapToItem() 会混入
            // LeftPage/TopBar 等窗口级偏移到 sourceRect。强制使用 local 坐标。
            forceLocalSourceRect: true
            z: 10
            themeColor: "#04FFFFFF"
            visible: root.videoTextureItem !== null
        }
    }
    // ========= 诊断：裸 ShaderEffectSource（不经过 MultiEffect）=========
    // 验证 QSGRenderNode offscreen 捕获是否正确。
    // 如果这个小窗里的视频被压缩，说明问题在 C++ render node 的 offscreen 路径。
    // 如果正常，说明问题在 Oran7BlurCard 的 sourceRect / padding / MultiEffect。
    //
    // 测试完毕后删除此块。
    ShaderEffectSource {
        id: debugCapture

        sourceItem: root.videoTextureItem
        sourceRect: root.videoTextureItem
            ? Qt.rect(0, 0, root.videoTextureItem.width, root.videoTextureItem.height)
            : Qt.rect(0, 0, 0, 0)

        width: 320
        height: 180

        live: true
        recursive: false
        z: 100
        visible: false/*root.videoTextureItem !== null*/
        opacity: 0.8

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: "red"
            border.width: 2
        }

        Text {
            text: "DEBUG: raw ShaderEffectSource\nsourceRect=(0,0," +
                  (root.videoTextureItem ? root.videoTextureItem.width : 0) + "," +
                  (root.videoTextureItem ? root.videoTextureItem.height : 0) + ")"
            color: "red"
            font.pixelSize: 11
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 4
        }
    }
    // ========= END 诊断块 =========

    Connections{
        target:Client
        function onVideoRenderInfoUpdated(info)
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
        // 获取 C++ 创建的 D3D11VideoItem 引用，供 Oran7BlurCard 等组件采集
        root.videoTextureItem = Client.getVideoItem(root.renderObject)
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
        root.videoTextureItem = Client.getVideoItem(root.renderObject)
    }
    Component.onDestruction: {
        if (attachTimer) attachTimer.stop()
        if (retryTimer) retryTimer.stop()
    }
    onVisibleChanged: {
        if (visible) tryAttachDelayed()
    }
}
