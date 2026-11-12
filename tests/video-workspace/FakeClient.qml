pragma Singleton
import QtQuick
QtObject {
    id: root
    enum RenderObject { VideoPlayerRender, ScreenCaptureRender }
    enum ScaleMode { Fit, Fill }
    property int renderScale: -1
    property int renderAttachCount: 0
    property Item renderItem: Item {}
    function setScaleMode(key, mode) { renderScale = mode }
    function attachVideoItem(key, host) {
        renderAttachCount++
        renderItem.parent = host
        renderItem.width = host.width
        renderItem.height = host.height
        return true
    }
    function videoItem(key) { return renderItem }
    signal videoRenderInfoUpdated(var info)
    property int playCount: 0
    property int toggleCount: 0
    property int seekCount: 0
    property var videoHistory: videoHistoryFixture
    property QtObject playback: QtObject {
        property string source: ""
        property int state: 0
        property bool active: state === 1 || state === 2
        property bool playing: state === 2
        property int positionMs: 0
        property int durationMs: 300000
        property int volume: 80
        property string errorString: ""
    }
    function playVideo(source) { playCount++; playback.source = source; playback.state = 2; videoHistory.rememberLocal(source) }
    function mediaSource() { return playback.source }
    function stopPlayback() { playback.state = 0; playbackStopRequested() }
    function togglePlayback() { toggleCount++; playback.state = playback.playing ? 3 : 2 }
    function seekTo(seconds) { seekCount++; playback.positionMs = seconds * 1000 }
    function setVolume(volume) { playback.volume = volume }
    function renderBlackFrame(slot) {}
    signal playbackStopRequested()
}
