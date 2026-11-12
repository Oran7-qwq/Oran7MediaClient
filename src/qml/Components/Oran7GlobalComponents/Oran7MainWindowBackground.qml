import QtQuick
import QtMultimedia
import Oran7Startup 1.0

// 背景在启动遮罩内预热，首帧/图片终态才就绪；回退一旦选定便不再被晚到视频抢回。
Item {
    id: root
    property string mode: "Image"
    property bool dwmEnabled: false
    property bool nativeReady: false
    property bool prepareActive: false
    property bool playbackAllowed: true
    property url videoUrl: ""
    property url imageUrl: ""
    property color baseColor: "#171a22"
    property int imageWidth: 1920
    property int imageHeight: 1080
    property int videoTimeoutMs: 5000
    property bool videoFailed: false
    property bool initialized: false
    readonly property bool wantsVideo: !dwmEnabled && mode === "Video"
    readonly property string selectedLayer: dwmEnabled ? "Dwm" : wantsVideo && !videoFailed ? "Video" : "Image"
    readonly property bool ready: prepareActive && nativeReady && (selectedLayer === "Dwm"
        || (selectedLayer === "Video" ? frameMonitor.frameReady
            : still.status === Image.Ready || still.status === Image.Error || still.status === Image.Null))
    readonly property var videoSink: output.videoSink
    function play() { if (playbackAllowed && prepareActive && wantsVideo && !videoFailed && player.source.toString().length) player.play() }
    function pause() { player.pause() }
    function reloadVideo() {
        if (!initialized) return
        deadline.stop()
        player.stop()
        videoFailed = false
        player.source = wantsVideo ? videoUrl : ""
        frameMonitor.reset()
        play()
        if (playbackAllowed && prepareActive && wantsVideo && !videoFailed && !frameMonitor.frameReady) deadline.start()
    }
    function useImageFallback() {
        if (!wantsVideo || videoFailed) return
        videoFailed = true
        deadline.stop()
        player.stop()
        frameMonitor.reset()
    }
    onPlaybackAllowedChanged: {
        if (!initialized) return
        if (playbackAllowed) {
            play()
            if (prepareActive && wantsVideo && !videoFailed && !frameMonitor.frameReady) deadline.restart()
        } else { deadline.stop(); pause() }
    }
    onVideoUrlChanged: reloadVideo()
    onWantsVideoChanged: reloadVideo()
    onPrepareActiveChanged: {
        if (!initialized) return
        if (prepareActive) {
            play()
            if (playbackAllowed && wantsVideo && !videoFailed && !frameMonitor.frameReady) deadline.restart()
        } else { deadline.stop(); pause() }
    }
    Component.onCompleted: { initialized = true; reloadVideo() }
    Rectangle { anchors.fill: parent; color: root.baseColor; visible: !root.dwmEnabled }
    Image {
        id: still
        objectName: "mainBackgroundImage"
        anchors.fill: parent
        source: root.dwmEnabled ? "" : root.imageUrl
        sourceSize.width: root.imageWidth; sourceSize.height: root.imageHeight
        asynchronous: true
        cache: false; mipmap: false; smooth: false
        fillMode: Image.PreserveAspectCrop
        visible: root.selectedLayer === "Image" && status === Image.Ready
    }
    VideoOutput {
        id: output
        objectName: "mainBackgroundVideoOutput"
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectCrop
        visible: root.selectedLayer === "Video"
    }
    Oran7BackgroundFrameMonitor {
        id: frameMonitor
        sink: output.videoSink
        onFrameReadyChanged: if (frameReady) deadline.stop()
    }
    MediaPlayer {
        id: player
        objectName: "mainBackgroundPlayer"
        videoOutput: output
        loops: MediaPlayer.Infinite
        autoPlay: false
        // 没有 AudioOutput，背景视频保持静音。
        onErrorOccurred: root.useImageFallback()
    }
    Timer {
        id: deadline
        interval: root.videoTimeoutMs
        onTriggered: root.useImageFallback()
    }
}
