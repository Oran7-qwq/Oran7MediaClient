pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects
import QtQuick.Effects
import Oran7Sound 1.0

import "../GlobalSettings"
import "../../Components/Oran7GlobalComponents"

import "../../Components/Oran7SettingUiWindowItems"

import Oran7UI.Impl

ApplicationWindow {
    id: root
    visible: false
    opacity: 0
    flags: Qt.Window | Qt.FramelessWindowHint
    width: Screen.width
    height: Screen.desktopAvailableHeight
    x: 0
    y: 0
    color: "transparent"
    objectName: "__Oran7SettingsContainerWindow__"

    property real taskbarHeight: 50
    property string transitionState: "Closed"
    readonly property bool isAnimating: transitionState !== "Closed" && transitionState !== "Open"
    property bool initializationComplete: false
    property bool blankFrameReady: false
    property bool openDelayReady: false
    property var openingPanels: []
    property var closingPanels: []
    readonly property var panels: [mainUiSettingLoader.item, mediaPlayerSettingLoader.item, musicPlayListSettingLoader.item]
    property bool displayAllowed: true
    // 主窗口显式借入账号浮层；设置容器不依赖音乐页或其业务会话。
    property var activeMainPopup: null
    property bool requestedVisible: displayAllowed && Oran7MainUiSetting.openSettingWin
    property bool enable__dwmBlur__: true

    signal restoreMainWindowFocusRequested

    Oran7WindowAgent {
        id: __windowAgent
    }

    Component.onCompleted: {
        detectTaskbarHeight()

        //setup 只做一次，并且在 QML 组件完成后做
        __windowAgent.setup(root)
        root.color = "transparent"
        root.initializationComplete = true
        if (requestedVisible)
            openAllPanels()
    }

    onScreenChanged: {
        detectTaskbarHeight()
    }

    onRequestedVisibleChanged: {
        if (!initializationComplete) return
        if (requestedVisible && transitionState === "Closed") openAllPanels()
        else if (!requestedVisible && transitionState !== "Closed") closeAllPanels()
    }

    // 透明展示期间先提交一帧已清空的内容，避免原生窗口复用上次的面板画面。
    onFrameSwapped: {
        if (transitionState === "Preparing" && root.visible && !blankFrameReady) {
            blankFrameReady = true
            Qt.callLater(root.tryStartOpening)
        }
    }

    // onEnable__dwmBlur__Changed: {
    //     if(root.enable__dwmBlur__){
    //         __blurTimer.stop()
    //          __windowAgent.setWindowAttribute("dwm-blur", false)
    //     }
    // }

    Timer {
        id: __blurTimer
        interval: 16
        repeat: false
        onTriggered: {
            root.color = "transparent"
            __windowAgent.setWindowAttribute("dwm-blur", true)
        }
    }

    Item {
        id: content
        anchors.fill: parent
        visible: false

        // 背景点击区域 - 必须在面板之前声明，确保面板 z-order 在其上方
        MouseArea {
            anchors.fill: parent
            propagateComposedEvents: true
            onClicked: Oran7MainUiSetting.clickedOutSide();
        }

        //CloseBtn
        Image{
            id:closeSetWinBtn
            property int __size__: 50
            sourceSize.height: __size__
            sourceSize.width:__size__
            source: "qrc:/image/mynaui_panel-left-close-solid.png"
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.top: parent.top
            anchors.topMargin: 80
            layer.enabled: true
            opacity: Oran7MainUiSetting.settingContentVisible ? 1 : 0
            Behavior on opacity {NumberAnimation{duration:Oran7Theme.Primary.durationMid}}
            Behavior on scale {NumberAnimation{duration: Oran7Theme.Primary.durationVeryFast}}
            layer.effect: ColorOverlay{
                source: closeSetWinBtn
                color:Oran7Theme.Oran7MainGUI.themeColor
            }
            MouseArea{
                anchors.fill: parent
                hoverEnabled: true
                onEntered: closeSetWinBtn.scale = 1.05
                onExited: closeSetWinBtn.scale = 1
                onPressed: closeSetWinBtn.scale = 0.95
                onReleased: closeSetWinBtn.scale = 1
                onClicked: {
                    Oran7MainUiSetting.callOpenSettingWindow()
                }
            }
        }

        // 使用 Loader 预加载窗口，在启动时加载，避免首次打开卡顿
        Loader {
            id: mainUiSettingLoader
            active: false  // 默认不加载,由外部逐个控制 active 激活(不连锁)
            sourceComponent: mainUiSettingComponent
            x: 0
            y: 0
        }
        Component {
            id: mainUiSettingComponent
            Oran7MainUiSettingWindow {
                winIndex: 0
            }
        }

        Loader {
            id: mediaPlayerSettingLoader
            active: false  // 默认不加载,由外部逐个控制 active 激活(不连锁)
            sourceComponent: mediaPlayerSettingComponent
            x: 0
            y: 0
        }
        Component {
            id: mediaPlayerSettingComponent
            Oran7VideoPlayerSettingWindow {
                winIndex: 2
            }
        }

        Loader {
            id: musicPlayListSettingLoader
            active: false  // 默认不加载,由外部逐个控制 active 激活(不连锁)
            sourceComponent: musicPlayListSettingComponent
            x: 0
            y: 0
        }
        Component {
            id: musicPlayListSettingComponent
            Oran7MusicPlayerSettingWindow {
                winIndex:1
            }
        }

        Column {
            anchors.bottom: content.bottom
            anchors.left: content.left
            anchors.margins: 20
            spacing: 4
            UseExplainLabel{
                text:qsTranslate("Oran7Ui", "点击方向键：移动设置窗口。")
            }
            UseExplainLabel {
                text: qsTranslate("Oran7Ui", "ESC：打开或关闭设置窗口。")
            }
            UseExplainLabel{
                text:qsTranslate("Oran7Ui", "点击渐变标题：展开或收起设置。")
            }
            UseExplainLabel{
                text:qsTranslate("Oran7Ui", "点击设置窗口空白区域：关闭设置窗口。")
            }
            UseExplainLabel {
                text: qsTranslate("Oran7Ui", "Enter：保存并应用当前输入。")
            }
            UseExplainLabel{
                text:qsTranslate("Oran7Ui", "左 Shift + Delete：清空当前输入框。")
            }
        }
    }
    component UseExplainLabel: Label {
        font.pixelSize: Oran7MainUiSetting.textPixelSize - 2
        font.family: Oran7MainUiSetting.fontFamily
        font.bold: true
        font.italic: true
        color:Oran7Theme.Oran7MainGUI.themeColor

        opacity: Oran7MainUiSetting.settingContentVisible  ? 1 : 0
        Behavior on opacity {
            PropertyAnimation{
                duration: Oran7MainUiSetting.toggleOpenAniDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    Connections{
        target: Oran7MainUiSetting
        function onCallOpenSettingWindow()
        {
            handle_OpenSettingWin_delayTimer.restart()
        }
    }

    // --- functions ---
    function detectTaskbarHeight() {
        root.taskbarHeight = 50
    }

    function openWindow() {
        root.opacity = 0
        if (root.enable__dwmBlur__) {
            __blurTimer.stop()
            __windowAgent.setWindowAttribute("dwm-blur", false)
        }
        if (root.visibility !== Window.Maximized) root.showMaximized()
        else root.visible = true
        if (root.enable__dwmBlur__) __blurTimer.start()
    }

    function closeWindow() {
        __blurTimer.stop()
        if (root.enable__dwmBlur__) __windowAgent.setWindowAttribute("dwm-blur", false)
        root.opacity = 0
        content.visible = false
        root.visible = false
        root.restoreMainWindowFocusRequested()
    }

    function handle_OpenSettingWin_signal() {
        // 初次准备和整个关闭过程都算忙，快速按键不能插入半初始化的开关请求。
        if (root.isAnimating) return
        if (root.transitionState === "Closed") root.openAllPanels()
        else root.closeAllPanels()
    }

    function openAllPanels() {
        if (root.transitionState !== "Closed") return
        root.transitionState = "Preparing"
        root.blankFrameReady = false
        root.openDelayReady = false
        content.visible = false
        root.opacity = 0
        Oran7MainUiSetting.settingWindow_isOpening_Or_isClosing = true
        Oran7MainUiSetting.settingContentVisible = false
        Oran7MainUiSetting.openSettingWin = true
        mainUiSettingLoader.active = true
        mediaPlayerSettingLoader.active = true
        musicPlayListSettingLoader.active = true
        openPanelsTimer.tries = 0
        openPanelsTimer.start()
    }

    function tryStartOpening() {
        if (root.transitionState !== "Preparing" || !root.blankFrameReady || !root.openDelayReady) return
        blankFrameTimeout.stop()
        root.transitionState = "Opening"
        root.openingPanels = root.panels.slice()
        content.visible = true
        root.opacity = 1
        Oran7MainUiSetting.settingContentVisible = true
        settingSound.playOpen()
        for (var i = 0; i < root.panels.length; ++i) root.panels[i].startOpenAnimation()
    }

    function panelOpened(panel) {
        if (root.transitionState !== "Opening" || root.openingPanels.indexOf(panel) < 0) return
        root.openingPanels = root.openingPanels.filter(function(item) { return item !== panel })
        if (root.openingPanels.length === 0) {
            root.transitionState = "Open"
            Oran7MainUiSetting.settingWindow_isOpening_Or_isClosing = false
        }
    }

    function closeAllPanels() {
        if (root.transitionState === "Closed" || root.transitionState === "Closing") return
        openPanelsTimer.stop()
        openPanelsDelayTimer.stop()
        blankFrameTimeout.stop()
        root.openingPanels = []
        root.transitionState = "Closing"
        Oran7MainUiSetting.settingWindow_isOpening_Or_isClosing = true
        Oran7MainUiSetting.settingContentVisible = false
        Oran7MainUiSetting.clickedOutSide()
        var loaded = root.panels.filter(function(panel) { return panel !== null })
        root.closingPanels = loaded.slice()
        if (loaded.length === 0) { root.finishClosing(); return }
        for (var i = 0; i < loaded.length; ++i) loaded[i].startCloseAnimation()
    }

    function panelClosed(panel) {
        if (root.transitionState !== "Closing" || root.closingPanels.indexOf(panel) < 0) return
        root.closingPanels = root.closingPanels.filter(function(item) { return item !== panel })
        // 所有面板都归零后才能隐藏原生窗口，不能由第一个完成的面板提前收走帧时钟。
        if (root.closingPanels.length === 0) root.finishClosing()
    }

    function finishClosing() {
        root.transitionState = "Closed"
        root.closeWindow()
        Oran7MainUiSetting.openSettingWin = false
        Oran7MainUiSetting.settingWindow_isOpening_Or_isClosing = false
        settingSound.playClose()
    }

    Timer {
        id: openPanelsTimer
        interval: 16
        repeat: true
        property int tries: 0
        onTriggered: {
            if (root.transitionState !== "Preparing") { stop(); return }
            if (tries++ > 180) {
                stop()
                console.warn("设置面板加载失败，已取消打开")
                root.closeAllPanels()
                return
            }
            for (var i = 0; i < root.panels.length; ++i) if (!root.panels[i]) return
            stop()
            // 此时窗口仍隐藏；准备阶段完成后，所有面板都不可见且高度为零。
            for (var j = 0; j < root.panels.length; ++j) root.panels[j].prepareForOpen()
            root.openWindow()
            openPanelsDelayTimer.start()
            blankFrameTimeout.start()
        }
    }
    Timer {
        id: openPanelsDelayTimer
        interval: 120
        onTriggered: {
            root.openDelayReady = true
            root.tryStartOpening()
        }
    }
    Timer {
        id: blankFrameTimeout
        interval: 3000
        onTriggered: {
            console.warn("设置窗口未提交准备帧，已取消打开")
            root.closeAllPanels()
        }
    }
    Connections {
        target: mainUiSettingLoader.item
        function onOpened() { root.panelOpened(mainUiSettingLoader.item) }
        function onClosed() { root.panelClosed(mainUiSettingLoader.item) }
    }
    Connections {
        target: mediaPlayerSettingLoader.item
        function onOpened() { root.panelOpened(mediaPlayerSettingLoader.item) }
        function onClosed() { root.panelClosed(mediaPlayerSettingLoader.item) }
    }
    Connections {
        target: musicPlayListSettingLoader.item
        function onOpened() { root.panelOpened(musicPlayListSettingLoader.item) }
        function onClosed() { root.panelClosed(musicPlayListSettingLoader.item) }
    }

    // 连接到全局键盘Keys事件过滤器
    Connections {
        target: globalEventFilter
        function onEscapeKeyPressed() {
            if (root.activeMainPopup) {
                root.activeMainPopup.close()
                return
            }
            if (Oran7MainUiSetting.activeSettingsPopup) {
                Oran7MainUiSetting.activeSettingsPopup.close()
                return
            }
            // 选择菜单优先关闭，避免同一次 ESC 又把下拉设置容器切走。
            if (mainUiSettingLoader.item && mainUiSettingLoader.item.preferencesPopupOpen) {
                mainUiSettingLoader.item.closePreferencesPopup()
                return
            }
            handle_OpenSettingWin_delayTimer.restart()
        }

        // Up key - move GUI upward
        function onUpKeyPressed(){
            if(Oran7MainUiSetting.__SurelySetingWinIsFocus__)
                handle_upKeyTimer.running = true
        }

        function onUpKeyReleased(){
            if(Oran7MainUiSetting.__SurelySetingWinIsFocus__)
                handle_upKeyTimer.running = false
        }

        // Down key - move GUI downward
        function onDownKeyPressed(){
            if(Oran7MainUiSetting.__SurelySetingWinIsFocus__)
                handle_downKeyTimer.running = true
        }

        function onDownKeyReleased(){
            if(Oran7MainUiSetting.__SurelySetingWinIsFocus__)
                handle_downKeyTimer.running = false
        }

        // Left key - move GUI leftward
        function onLeftKeyPressed(){
            if(Oran7MainUiSetting.__SurelySetingWinIsFocus__)
                handle_leftKeyTimer.running = true
        }

        function onLeftKeyReleased(){
            if(Oran7MainUiSetting.__SurelySetingWinIsFocus__)
                handle_leftKeyTimer.running = false
        }

        // Right key - move GUI rightward
        function onRightKeyPressed(){
            if(Oran7MainUiSetting.__SurelySetingWinIsFocus__)
                handle_rightKeyTimer.running = true
        }

        function onRightKeyReleased(){
            if(Oran7MainUiSetting.__SurelySetingWinIsFocus__)
                handle_rightKeyTimer.running = false
        }
    }

    // --- tools Component ---
    Oran7DelayTimer {
        id: delayTimer
    }

    Timer{
        id:handle_OpenSettingWin_delayTimer
        interval: 200
        onTriggered: {
            root.handle_OpenSettingWin_signal()
        }
    }

    // Up key timer - moves GUI upward
    Timer{
        id:handle_upKeyTimer
        interval: 16
        repeat: true
        running: false
        onTriggered: {
            if (mainUiSettingLoader.item) {
                mainUiSettingLoader.item.y -= 4
                mainUiSettingLoader.item.savedNormalY = mainUiSettingLoader.item.y
            }
            if (mediaPlayerSettingLoader.item) {
                mediaPlayerSettingLoader.item.y -= 4
                mediaPlayerSettingLoader.item.savedNormalY = mediaPlayerSettingLoader.item.y
            }
            if (musicPlayListSettingLoader.item) {
                musicPlayListSettingLoader.item.y -= 4
                musicPlayListSettingLoader.item.savedNormalY = musicPlayListSettingLoader.item.y
            }
        }
    }

    // Down key timer - moves GUI downward
    Timer{
        id:handle_downKeyTimer
        interval: 16
        repeat: true
        running: false
        onTriggered: {
            if (mainUiSettingLoader.item) {
                mainUiSettingLoader.item.y += 4
                mainUiSettingLoader.item.savedNormalY = mainUiSettingLoader.item.y
            }
            if (mediaPlayerSettingLoader.item) {
                mediaPlayerSettingLoader.item.y += 4
                mediaPlayerSettingLoader.item.savedNormalY = mediaPlayerSettingLoader.item.y
            }
            if (musicPlayListSettingLoader.item) {
                musicPlayListSettingLoader.item.y += 4
                musicPlayListSettingLoader.item.savedNormalY = musicPlayListSettingLoader.item.y
            }
        }
    }

    // Left key timer - moves GUI leftward
    Timer{
        id:handle_leftKeyTimer
        interval: 16
        repeat: true
        running: false
        onTriggered: {
            if (mainUiSettingLoader.item) {
                mainUiSettingLoader.item.x -= 4
                mainUiSettingLoader.item.savedNormalX = mainUiSettingLoader.item.x
            }
            if (mediaPlayerSettingLoader.item) {
                mediaPlayerSettingLoader.item.x -= 4
                mediaPlayerSettingLoader.item.savedNormalX = mediaPlayerSettingLoader.item.x
            }
            if (musicPlayListSettingLoader.item) {
                musicPlayListSettingLoader.item.x -= 4
                musicPlayListSettingLoader.item.savedNormalX = musicPlayListSettingLoader.item.x
            }
        }
    }

    // Right key timer - moves GUI rightward
    Timer{
        id:handle_rightKeyTimer
        interval: 16
        repeat: true
        running: false
        onTriggered: {
            if (mainUiSettingLoader.item) {
                mainUiSettingLoader.item.x += 4
                mainUiSettingLoader.item.savedNormalX = mainUiSettingLoader.item.x
            }
            if (mediaPlayerSettingLoader.item) {
                mediaPlayerSettingLoader.item.x += 4
                mediaPlayerSettingLoader.item.savedNormalX = mediaPlayerSettingLoader.item.x
            }
            if (musicPlayListSettingLoader.item) {
                musicPlayListSettingLoader.item.x += 4
                musicPlayListSettingLoader.item.savedNormalX = musicPlayListSettingLoader.item.x
            }
        }
    }

    // --- open and close sound effect ---
    // C++ 包装类：在 C++ 层调用 QSoundEffect::setAudioDevice()，跟随系统默认设备
    Oran7SoundEffect {
        id: settingSound
        volume: Oran7Theme.Oran7MainGUI.soundEffectVolume
    }
}
