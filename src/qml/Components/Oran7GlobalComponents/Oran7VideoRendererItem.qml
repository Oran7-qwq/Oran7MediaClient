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
    property int scaleMode: Client.Fit
    onScaleModeChanged: Client.setScaleMode(root.renderObject, root.scaleMode)
    property Item videoTextureItem: null

    // 弹幕覆盖层：直播播放时置 true，外部通过 danmakuLayer.spawn/showSuperChat 投递内容
    property bool danmakuEnabled: false
    property alias danmakuLayer: danmakuLayer

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

        Oran7DanmakuLayer {
            id: danmakuLayer
            // 跟随 D3D11VideoItem（其几何永远铺满 host，即整个视频渲染区域）
            x: root.videoTextureItem ? root.videoTextureItem.x : 0
            y: root.videoTextureItem ? root.videoTextureItem.y : 0
            width: root.videoTextureItem ? root.videoTextureItem.width : 0
            height: root.videoTextureItem ? root.videoTextureItem.height : 0
            z: 20
            active: root.danmakuEnabled
        }
    }
    Connections{
        target:Client
        function onVideoRenderInfoUpdated(info)
        {
            // 只接受自己来源的帧信息：screen → "screen"，player → "player"
            var expected = (root.renderObject === Client.ScreenCaptureRender) ? "screen" : "player";
            if (info.sourceTag === expected)
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
                qsTranslate("Oran7Ui", "源尺寸: ") + (root.videoInfo.srcWidth || root.videoInfo.srcHeight  ? (root.videoInfo.srcWidth + "x" + root.videoInfo.srcHeight) : "N/A") + "\n" +
                qsTranslate("Oran7Ui", "源帧格式: ") + root.videoInfo.srcFormatName +"\n" +
                qsTranslate("Oran7Ui", "渲染尺寸: ") + (root.videoInfo.renderWidth || root.videoInfo.renderHeight ? (root.videoInfo.renderWidth + "x" +root.videoInfo.renderHeight ) : "N/A") + "\n" +
                qsTranslate("Oran7Ui", "渲染帧格式: ") + root.videoInfo.dxgiFormatName +"\n" +
                qsTranslate("Oran7Ui", "色彩信息: ") + (root.videoInfo.bitDepth || 8) + "-bit / " +
                    (root.videoInfo.colorPrimaries || "unknown") + " / " + (root.videoInfo.colorTransfer || "unknown") + "\n" +
                qsTranslate("Oran7Ui", "输出色彩: ") + (root.videoInfo.outputColorMode || "SDR") + "\n" +
                qsTranslate("Oran7Ui", "是否启用硬解: ") + root.videoInfo.isFromHardWare +"\n" +
                qsTranslate("Oran7Ui", "解码设备: ") + root.videoInfo.decodeDevice +"\n" +
                qsTranslate("Oran7Ui", "渲染设备: ") + root.videoInfo.renderDevice +"\n" +
                qsTranslate("Oran7Ui", "填充模式: ") + root.videoInfo.fillModeName +"\n" +
                qsTranslate("Oran7Ui", "刷新帧率: ") + root.videoInfo.fps
            color: "white"
            font.pixelSize: 12
        }
    }

    function tryAttach() {
        if (!videoHost) return false
        // 挂载/重挂载先恢复绘制模式，不能依赖设置面板是否已经创建。
        Client.setScaleMode(root.renderObject, root.scaleMode)
        console.log("--->Client.attachVideoItem() : ",videoHost)
        var attached = Client.attachVideoItem(root.renderObject,videoHost)
        // 借用 C++ 渲染项，供覆盖层同步画布几何
        root.videoTextureItem = Client.videoItem(root.renderObject)
        return attached
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
        if (!tryAttach()) retryTimer.start()
    }
    Component.onDestruction: {
        if (attachTimer) attachTimer.stop()
        if (retryTimer) retryTimer.stop()
    }
    onVisibleChanged: {
        if (visible) tryAttachDelayed()
    }
}
