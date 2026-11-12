import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import "../../Basic"
import "../../Components/Oran7GlobalComponents"
import Client 1.0
import Oran7UI.Impl
import BilibiliRoomAddressCatch 1.0
import BilibiliDanmakuClient 1.0
import BilibiliAuth 1.0

Item {
    id: root
    property string pageName: ""
    property var stackView_rootItemContainer: root
    property var mainWindow: BasicConfig.mainWindow
    property bool isActive: false
    property bool playbackView: false
    property bool fullscreen: false
    property int previousVisibility: Window.Windowed
    property rect previousGeometry: Qt.rect(0, 0, 990, 564)
    property color surfaceColor: "#dd1e222a"
    property color textColor: "#f1f2f5"
    property color secondaryTextColor: "#a8aebb"
    property color accentColor: "#ebc286"
    property bool live: false
    property string openedSource: ""
    property string localPath: ""
    property string roomNumber: ""
    property string displayTitle: ""
    property string statusText: ""
    property string streamDescription: ""
    property string danmakuErrorText: ""
    property bool danmakuEnabled: true
    property bool showInfo: false
    property bool controlsVisible: true
    property bool windowButtonsHovered: false
    // 主窗口直接沿用视频标题栏的实际动画进度，不另外维护显隐定时器。
    readonly property real windowControlsOpacity: isActive && playbackView ? header.opacity : 1
    // 本地视频仅在画布内静止时跟随标题栏隐藏；直播和画布外始终显示网络延迟。
    readonly property real networkLatencyOpacity: live || !playerHover.hovered ? 1 : windowControlsOpacity
    property bool mouseHidden: false
    property int controlsIdleDelay: 1500
    property int cursorIdleDelay: 2000
    readonly property bool controlsFullyHidden: !controlsVisible && header.opacity === 0 && toolbar.opacity === 0
    onControlsFullyHiddenChanged: {
        if (controlsFullyHidden && isActive && playbackView) hideCursor.restart()
        else hideCursor.stop()
    }
    property int requestGeneration: 0
    property var activeRequest: null
    readonly property bool resolving: activeRequest !== null
    readonly property bool ownsPlayback: openedSource.length > 0 && Client.playback.source === openedSource
        && (BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_VideoPlayerIndex
            || BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_LivePlayerIndex)
    readonly property bool opening: resolving || (ownsPlayback && Client.playback.state === 1)
    readonly property bool failed: ownsPlayback && Client.playback.state === 6

    anchors.fill: parent
    StackView.onActivated: isActive = true
    StackView.onDeactivated: {
        isActive = false
        hideControls.stop()
        hideCursor.stop()
        mouseHidden = false
        singleClick.stop()
        toolbar.closePopups()
        settings.close()
        cancelResolve()
        exitFullscreen()
        if (ownsPlayback) {
            if (Client.playback.state === 1) Client.stopPlayback()
            else if (Client.playback.active) Client.togglePlayback()
        }
        danmaku.stop()
        BasicConfig.clearAllUi_inVIdeoRenderArea(false)
    }
    Component.onDestruction: {
        cancelResolve()
        exitFullscreen()
    }

    function cancelResolve() {
        ++requestGeneration
        var previous = activeRequest
        activeRequest = null
        if (previous) previous.destroy()
    }
    function showPlayer() {
        playbackView = true
        wakeControls()
        playerSurface.forceActiveFocus()
    }
    function showLibrary() {
        exitFullscreen()
        playbackView = false
        hideControls.stop()
        hideCursor.stop()
        mouseHidden = false
        singleClick.stop()
        toolbar.closePopups()
        settings.close()
        BasicConfig.clearAllUi_inVIdeoRenderArea(false)
    }
    function wakeControls() {
        controlsVisible = true
        mouseHidden = false
        hideCursor.stop()
        if (!isActive || !playbackView) return
        BasicConfig.clearAllUi_inVIdeoRenderArea(false)
        hideControls.restart()
    }
    function idleBlocked() {
        return toolbar.interacting || headerHover.hovered || root.windowButtonsHovered
            || settings.visible || fileDialog.visible || root.opening || root.failed || !root.ownsPlayback
    }
    function playLocal(path) {
        var value = String(path).trim()
        if (value.startsWith('"') && value.endsWith('"')) value = value.slice(1, -1)
        if (value.startsWith("file:")) {
            try { value = decodeURIComponent(value.replace(/^file:\/\/\//, "").replace(/^file:\/\//, "//")) }
            catch (error) { statusText = qsTranslate("Oran7Ui", "文件路径无效"); return }
        }
        if (!fileHelper.fileExists(value)) {
            statusText = qsTranslate("Oran7Ui", "文件不存在，请重新选择")
            return
        }
        cancelResolve()
        danmaku.stop()
        danmakuErrorText = ""
        localPath = value
        live = false
        displayTitle = value.replace(/\\/g, "/").split("/").pop()
        statusText = ""
        streamDescription = ""
        var sameSource = ownsPlayback && openedSource === value
        openedSource = value
        BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_VideoPlayerIndex
        showPlayer()
        videoRenderItem.tryAttachDelayed()
        if (sameSource && (Client.playback.state === 5 || Client.playback.state === 6))
            Client.stopPlayback()
        if (!sameSource || !Client.playback.active) Client.playVideo(value)
    }
    function playRoom(number) {
        if (!/^[0-9]{1,12}$/.test(number)) {
            statusText = qsTranslate("Oran7Ui", "请输入有效的房间号")
            return
        }
        number = String(Number(number))
        cancelResolve()
        if (ownsPlayback) Client.stopPlayback()
        danmaku.stop()
        danmakuErrorText = ""
        openedSource = ""
        live = true
        roomNumber = number
        displayTitle = qsTranslate("Oran7Ui", "直播间") + " " + number
        statusText = qsTranslate("Oran7Ui", "正在连接直播间…")
        showPlayer()
        activeRequest = roomRequest.createObject(root, {generation: requestGeneration})
        activeRequest.getRoomInfo(number)
    }
    function acceptRoom(request) {
        if (activeRequest !== request || request.generation !== requestGeneration) return
        var urls = request.avliStrAdr
        if (!urls.length) { rejectRoom(request); return }
        var url = String(urls[0])
        streamDescription = request.selectedCodecName.toUpperCase() + " / " + request.selectedFormatName.toUpperCase()
        activeRequest = null
        request.destroy()
        openedSource = url
        statusText = ""
        BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_LivePlayerIndex
        videoRenderItem.tryAttachDelayed()
        Client.playVideo(url)
        syncDanmaku()
    }
    function rejectRoom(request) {
        if (activeRequest !== request || request.generation !== requestGeneration) return
        activeRequest = null
        request.destroy()
        statusText = qsTranslate("Oran7Ui", "直播连接失败，房间可能未开播，请重试")
        wakeControls()
    }
    function retry() {
        if (live) playRoom(roomNumber)
        else if (localPath.length) playLocal(localPath)
    }
    function togglePlayback() {
        if (opening) return
        if (!ownsPlayback || Client.playback.state === 0 || Client.playback.state === 5 || failed) retry()
        else Client.togglePlayback()
        wakeControls()
    }
    function syncDanmaku() {
        var enabled = isActive && ownsPlayback && live && danmakuEnabled && Client.playback.playing
        if (enabled && !danmaku.active) danmaku.start(roomNumber)
        else if (!enabled && danmaku.active) danmaku.stop()
    }
    function rememberViewing() {
        if (!ownsPlayback || !Client.playback.playing) return
        if (live) Client.videoHistory.rememberRoom(roomNumber)
    }
    function toggleFullscreen() {
        if (!mainWindow) return
        if (fullscreen) { exitFullscreen(); return }
        previousVisibility = mainWindow.visibility
        if (typeof mainWindow.captureNormalGeometry === "function") mainWindow.captureNormalGeometry()
        previousGeometry = mainWindow.normalWindowGeometry || Qt.rect(mainWindow.x, mainWindow.y, mainWindow.width, mainWindow.height)
        fullscreen = true
        mainWindow.showFullScreen()
        wakeControls()
    }
    function exitFullscreen() {
        if (!fullscreen || !mainWindow) return
        fullscreen = false
        if (previousVisibility === Window.Maximized) mainWindow.showMaximized()
        else {
            mainWindow.showNormal()
            mainWindow.x = previousGeometry.x
            mainWindow.y = previousGeometry.y
            mainWindow.width = previousGeometry.width
            mainWindow.height = previousGeometry.height
        }
    }
    function login() {
        settings.close()
        exitFullscreen()
        BasicConfig.openBilibiliLoginPopup()
    }

    Component {
        id: roomRequest
        BilibiliRoomAddressCatch {
            id: request
            property int generation: 0
            onUrlsReady: root.acceptRoom(request)
            onUrlsError: root.rejectRoom(request)
        }
    }
    Oran7FileHelper { id: fileHelper }
    Oran7FileDialog {
        id: fileDialog
        nameFilters: [qsTranslate("Oran7Ui", "视频文件 (*.mp4 *.mkv *.mov *.avi *.flv *.webm *.ts *.m4v)"),
            qsTranslate("Oran7Ui", "所有文件 (*)")]
        onReady: root.playLocal(String(selectedFile))
    }
    BilibiliDanmakuClient {
        id: danmaku
        onDanmakuReceived: (text, uname, color, timestamp) => {
            if (videoRenderItem.danmakuEnabled) videoRenderItem.danmakuLayer.spawn(text, color)
        }
        onSuperChatReceived: (text, uname, price, color) => {
            if (videoRenderItem.danmakuEnabled) videoRenderItem.danmakuLayer.showSuperChat(text, uname, price, color)
        }
        onErrorOccurred: (message) => { root.danmakuErrorText = message; root.wakeControls() }
        onConnectionChanged: if (isConnected) root.danmakuErrorText = ""
    }
    Connections {
        target: Client.videoHistory
        function onRecordsChanged() {
            var recent = Client.videoHistory.latest
            if (root.live && recent.mediaKind === "live" && recent.target === root.roomNumber
                && recent.roomTitle) root.displayTitle = recent.roomTitle
        }
    }
    Connections {
        target: Client.playback
        function onStateChanged() {
            root.syncDanmaku()
            root.rememberViewing()
            if (root.failed) root.wakeControls()
        }
        function onSourceChanged() {
            if (root.resolving) { root.cancelResolve(); root.statusText = "" }
            root.syncDanmaku()
            if (!root.ownsPlayback && !root.resolving) root.exitFullscreen()
        }
    }
    Connections {
        target: Client
        function onPlaybackStopRequested() {
            danmaku.stop()
            Client.renderBlackFrame(videoRenderItem.renderObject)
        }
    }
    Connections {
        target: root.mainWindow
        function onVisibilityChanged() {
            if (root.fullscreen && root.mainWindow.visibility !== Window.FullScreen) root.fullscreen = false
        }
    }
    Timer {
        id: hideControls
        interval: root.controlsIdleDelay
        onTriggered: {
            if (root.idleBlocked()) { root.wakeControls(); return }
            root.controlsVisible = false
            BasicConfig.clearAllUi_inVIdeoRenderArea(true)
        }
    }
    Timer {
        id: hideCursor
        interval: root.cursorIdleDelay
        onTriggered: {
            if (root.idleBlocked()) { root.wakeControls(); return }
            if (!root.controlsFullyHidden || !root.isActive || !root.playbackView) return
            root.mouseHidden = true
        }
    }

    Oran7VideoLibraryPage {
        anchors.fill: parent
        visible: !root.playbackView
        title: root.displayTitle
        hasSession: root.displayTitle.length > 0
        roomNumber: root.roomNumber
        resolving: root.resolving
        statusText: root.statusText
        loggedIn: BilibiliAuthManager.isLoggedIn
        historyModel: Client.videoHistory
        latestRecord: Client.videoHistory.latest
        historyCount: selectedTab === 0 ? Client.videoHistory.localCount : Client.videoHistory.liveCount
        surfaceColor: root.surfaceColor
        textColor: root.textColor
        secondaryTextColor: root.secondaryTextColor
        accentColor: root.accentColor
        onOpenFileRequested: fileDialog.open()
        onPlayFileRequested: (path) => root.playLocal(path)
        onPlayRoomRequested: (number) => root.playRoom(number)
        onResumeRequested: root.showPlayer()
        onLoginRequested: root.login()
        onHistoryRequested: (kind, target) => {
            if (kind === "local") root.playLocal(target)
            else root.playRoom(target)
        }
        onRemoveHistoryRequested: (kind, target) => Client.videoHistory.remove(kind, target)
    }

    Item {
        id: playerSurface
        objectName: "videoPlaybackSurface"
        parent: root.fullscreen && root.mainWindow ? root.mainWindow.contentItem : root
        anchors.fill: parent
        z: root.fullscreen ? 100000 : 1
        visible: root.playbackView && root.isActive
        focus: visible
        Keys.onSpacePressed: (event) => { if (!event.isAutoRepeat) root.togglePlayback(); event.accepted = true }
        Rectangle { anchors.fill: parent; color: "#080a0e" }
        Oran7VideoRendererItem {
            id: videoRenderItem
            objectName: "videoRenderer"
            anchors.fill: parent
            renderObject: Client.VideoPlayerRender
            scaleMode: Oran7Theme.Oran7VideoPlayer.ScaleMode === "Fill" ? Client.Fill : Client.Fit
            openVideoInfo: root.showInfo
            danmakuEnabled: root.ownsPlayback && root.live && root.danmakuEnabled && danmaku.isConnected
        }
        MouseArea {
            id: canvasMouseArea
            anchors.fill: parent
            hoverEnabled: true
            // 点击层也参与光标选择，必须与 HoverHandler 的隐藏状态一致。
            cursorShape: root.mouseHidden ? Qt.BlankCursor : Qt.ArrowCursor
            onClicked: singleClick.restart()
            onDoubleClicked: { singleClick.stop(); root.toggleFullscreen() }
        }
        Timer {
            id: singleClick
            interval: 230
            onTriggered: {
                if (root.controlsVisible) {
                    root.controlsVisible = false
                    root.mouseHidden = false
                    hideControls.stop()
                } else root.wakeControls()
                playerSurface.forceActiveFocus()
            }
        }
        HoverHandler {
            id: playerHover
            acceptedDevices: PointerDevice.Mouse
            cursorShape: root.mouseHidden ? Qt.BlankCursor : Qt.ArrowCursor
            property point lastPosition: Qt.point(0, 0)
            property bool hasPosition: false
            onPointChanged: {
                if (!hovered) return
                var position = point.scenePosition
                if (!hasPosition) {
                    lastPosition = position
                    hasPosition = true
                    return
                }
                var dx = position.x - lastPosition.x
                var dy = position.y - lastPosition.y
                // 累计移动至少 2px 才视为活动，避免静止指针被合成事件持续唤醒。
                if (dx * dx + dy * dy >= 4) {
                    lastPosition = position
                    root.wakeControls()
                }
            }
            onHoveredChanged: {
                hasPosition = false
                if (hovered) root.wakeControls()
            }
        }
        Item {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 48
            opacity: root.controlsVisible ? 1 : 0
            enabled: opacity > 0.1
            Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
            HoverHandler { id: headerHover }
            // 顶部仅保留渐隐遮罩，浅色画面中也能读清标题，不再覆盖整块工具面板。
            Rectangle {
                width: parent.width
                height: 80
                gradient: Gradient {
                    GradientStop { position: 0; color: "#99000000" }
                    GradientStop { position: 0.6; color: "#3d000000" }
                    GradientStop { position: 1; color: "transparent" }
                }
            }
            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8
                ToolButton {
                    id: backButton
                    objectName: "videoBackButton"
                    implicitWidth: 34
                    implicitHeight: 34
                    hoverEnabled: true
                    Accessible.name: qsTranslate("Oran7Ui", "视频库")
                    ToolTip.visible: hovered
                    ToolTip.delay: 600
                    ToolTip.text: Accessible.name
                    background: Rectangle {
                        radius: 7
                        color: backButton.down ? "#3dffffff" : backButton.hovered ? "#20ffffff" : "transparent"
                        Behavior on color { ColorAnimation { duration: 140 } }
                    }
                    contentItem: Item {
                        Shape {
                            anchors.centerIn: parent
                            width: 16; height: 16
                            antialiasing: true
                            ShapePath {
                                fillColor: "transparent"
                                strokeColor: "#f2f2f2"
                                strokeWidth: 1.7
                                capStyle: ShapePath.RoundCap
                                joinStyle: ShapePath.RoundJoin
                                startX: 10; startY: 3
                                PathLine { x: 5; y: 8 }
                                PathLine { x: 10; y: 13 }
                            }
                        }
                    }
                    onClicked: root.showLibrary()
                }
                Label {
                    Layout.fillWidth: true
                    text: root.displayTitle
                    textFormat: Text.PlainText
                    color: "#f2f2f2"
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                }
                ToolButton {
                    id: moreButton
                    objectName: "videoSettingsButton"
                    implicitWidth: 34
                    implicitHeight: 34
                    hoverEnabled: true
                    Accessible.name: qsTranslate("Oran7Ui", "详情与设置")
                    ToolTip.visible: hovered
                    ToolTip.delay: 600
                    ToolTip.text: Accessible.name
                    background: Rectangle {
                        radius: 7
                        color: moreButton.down || settings.visible ? "#3dffffff" : moreButton.hovered ? "#20ffffff" : "transparent"
                        Behavior on color { ColorAnimation { duration: 140 } }
                    }
                    contentItem: Item {
                        Row {
                            anchors.centerIn: parent
                            spacing: 3
                            Repeater {
                                model: 3
                                Rectangle { width: 3; height: 3; radius: 1.5; color: "#f2f2f2" }
                            }
                        }
                    }
                    onClicked: settings.open()
                }
            }
        }
        Column {
            anchors.centerIn: parent
            spacing: 16
            width: Math.min(420, parent.width - 48)
            visible: root.opening || root.failed || !root.ownsPlayback || root.statusText.length > 0
            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: root.textColor
                font.pixelSize: 15
                text: root.resolving ? qsTranslate("Oran7Ui", "正在连接直播间…")
                    : root.failed ? Client.playback.errorString
                    : root.opening ? qsTranslate("Oran7Ui", "正在准备画面…")
                    : root.statusText.length ? root.statusText : qsTranslate("Oran7Ui", "点击播放以打开当前内容")
            }
            Oran7StudioButton {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !root.opening && (root.failed || !root.ownsPlayback)
                text: qsTranslate("Oran7Ui", "重新播放")
                onClicked: root.retry()
            }
        }
        Oran7VideoPlaybackToolbar {
            id: toolbar
            objectName: "videoPlaybackToolbar"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 0
            height: implicitHeight
            opacity: root.controlsVisible ? 1 : 0
            enabled: opacity > 0.1
            Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
            live: root.live
            playing: root.ownsPlayback && Client.playback.active
            opening: root.opening
            controlsEnabled: !root.resolving
            fullscreen: root.fullscreen
            positionSeconds: root.ownsPlayback ? Client.playback.positionMs / 1000 : 0
            durationSeconds: root.ownsPlayback ? Client.playback.durationMs / 1000 : 0
            sourceKey: root.openedSource
            onActivity: root.wakeControls()
            onToggleRequested: root.togglePlayback()
            onSeekRequested: (seconds, sourceKey) => {
                if (root.ownsPlayback && !root.live && sourceKey === root.openedSource) Client.seekTo(seconds)
                root.wakeControls()
            }
            onFullscreenRequested: root.toggleFullscreen()
        }
        Popup {
            id: settings
            objectName: "videoSettingsPopup"
            parent: playerSurface
            x: parent.width - width - 12
            y: 64
            width: Math.min(320, parent.width - 24)
            height: Math.max(100, Math.min(contentColumn.implicitHeight + 32, parent.height - 80))
            padding: 16
            focus: true
            onOpened: root.wakeControls()
            onClosed: root.wakeControls()
            background: Rectangle { color: "#f2242933"; radius: 10; border.color: "#48505f" }
            ScrollView {
                anchors.fill: parent
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    id: contentColumn
                    width: parent.width
                    spacing: 12
                    Label { text: qsTranslate("Oran7Ui", "播放设置"); color: root.textColor; font.pixelSize: 16 }
                    Oran7StudioButton {
                        text: qsTranslate("Oran7Ui", "画面信息")
                        checkable: true
                        checked: root.showInfo
                        onToggled: root.showInfo = checked
                    }
                    Oran7StudioButton {
                        visible: root.live
                        text: qsTranslate("Oran7Ui", "弹幕")
                        checkable: true
                        checked: root.danmakuEnabled
                        onToggled: { root.danmakuEnabled = checked; root.syncDanmaku() }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: root.live
                        text: root.streamDescription + "\n" + (root.danmakuErrorText.length ? root.danmakuErrorText
                            : danmaku.isConnected ? qsTranslate("Oran7Ui", "弹幕已连接") : qsTranslate("Oran7Ui", "弹幕未连接"))
                        color: root.secondaryTextColor
                        wrapMode: Text.Wrap
                    }
                    Oran7StudioButton {
                        visible: root.live
                        text: qsTranslate("Oran7Ui", "刷新直播流")
                        onClicked: { settings.close(); root.playRoom(root.roomNumber) }
                    }
                    Oran7StudioButton {
                        text: BilibiliAuthManager.isLoggedIn ? qsTranslate("Oran7Ui", "退出B站登录") : qsTranslate("Oran7Ui", "B站登录")
                        onClicked: {
                            if (BilibiliAuthManager.isLoggedIn) BilibiliAuthManager.logout()
                            else root.login()
                        }
                    }
                }
            }
        }
    }
}
