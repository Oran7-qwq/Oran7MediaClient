import QtQuick

// 只负责灵动岛展示：歌词时间跟随播放时钟，空闲由有效换句计时。
QtObject {
    id: root
    property var model: null
    property string trackKey: ""
    property string songTitle: ""
    property string artist: ""
    property real positionSeconds: 0
    property int idleTimeoutMs: 10000
    property int minimumIntroMs: 3000
    readonly property bool introLocked: introductionTimer.running
    readonly property string titleText: songTitle.trim().length > 0
        ? songTitle.trim() + (artist.trim().length > 0 ? " - " + artist.trim() : "")
        : "Oran7MediaClient"
    property int displayedLine: -1
    property bool lyricsVisible: false
    property bool waitingForFirstLine: true
    signal lyricActivated()

    function reset() {
        idleTimer.stop()
        displayedLine = -1
        lyricsVisible = false
        waitingForFirstLine = true
    }
    function showLine(line) {
        if (introLocked) return
        var changed = displayedLine !== line || !lyricsVisible
        displayedLine = line
        lyricsVisible = true
        if (changed) {
            idleTimer.restart()
            lyricActivated()
        }
    }
    function synchronize() {
        if (!model || !model.hasLyrics) return
        model.updateTime(positionSeconds)
        if (introLocked) return
        if (model.currentLine >= 0 && waitingForFirstLine) {
            waitingForFirstLine = false
            showLine(model.currentLine)
        }
        if (model.currentLine < 0) {
            if (positionSeconds < Math.max(0, model.lineTriggerTime(0) - 3)) reset()
            else if (waitingForFirstLine) showLine(0)
        }
    }
    onPositionSecondsChanged: synchronize()
    onTrackKeyChanged: {
        reset()
        if (model) model.clearLyrics()
        introductionTimer.restart()
    }
    Component.onCompleted: {
        introductionTimer.restart()
        synchronize()
    }

    // 切歌介绍独立计时；歌词加载、换句和元数据补全均不能提前结束或重新计时。
    property QtObject introductionTimer: Timer {
        interval: root.minimumIntroMs
        onTriggered: Qt.callLater(root.synchronize)
    }

    property QtObject idleTimer: Timer {
        interval: root.idleTimeoutMs
        onTriggered: {
            root.lyricsVisible = false
            root.waitingForFirstLine = false
        }
    }
    property QtObject modelEvents: Connections {
        target: root.model
        function onCurrentLineChanged() {
            if (root.model.currentLine < 0) {
                root.reset()
                return
            }
            if (root.introLocked) return
            root.waitingForFirstLine = false
            root.showLine(root.model.currentLine)
        }
        function onCountChanged() {
            root.reset()
            root.synchronize()
        }
    }
}
