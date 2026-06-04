pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects
import QtQuick.Effects
import Oran7Sound 1.0

import "../GlobalSettings"
import "../PagesSettings"
import "../../Components"

import "../../Components/Oran7SettingUiWindowItems"

import Oran7UI.Impl

ApplicationWindow {
    id: root
    visible: false
    flags: Qt.Window | Qt.FramelessWindowHint
    width: Screen.width
    height: Screen.desktopAvailableHeight
    x: 0
    y: 0
    color: "transparent"
    objectName: "__Oran7SettingsContainerWindow__"

    property real taskbarHeight: 50
    property bool isAnimating: false
    property bool requestedVisible: Oran7Theme.Oran7MainGUI.OpenSettingWin
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

        preloadTimer.start()

        if (requestedVisible)
            openWindow()
    }

    onScreenChanged: {
        detectTaskbarHeight()
    }

    onRequestedVisibleChanged: {
        if (requestedVisible)
            openWindow()
        else
            closeWindow()
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
            opacity: Oran7Theme.Oran7MainGUI.settingContent_visiable ? 1 : 0
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
            active: false  // 启动时由预加载定时器触发
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
            active: false  // 启动时由预加载定时器触发
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
            id: screenCaptureSettingLoader
            active: false  // 启动时由预加载定时器触发
            sourceComponent: screenCaptureSettingComponent
            x: 0
            y: 0
        }
        Component {
            id: screenCaptureSettingComponent
            Oran7ScreenCaptureSettingWindow {
                winIndex: 3
            }
        }

        Loader {
            id: musicPlayListSettingLoader
            active: false  // 启动时由预加载定时器触发
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
                text:"Click->↑→↓←: Move the Setting GUI Position."
            }
            UseExplainLabel {
                text: "ESC: Open or close the setting windows."
            }
            UseExplainLabel{
                text:"Left or right clicke colorful gradient setting item：will extend or shrink setting content."
            }
            UseExplainLabel{
                text:"LeftClick: Click at blank area of setting window will close all setting windows."
            }
            UseExplainLabel {
                text: "Enter: Ensure the foucsed TextField value is saved and the setting is applied."
            }
            UseExplainLabel{
                text:"LShift+Delete: Clear the content of foucsed TextField."
            }
        }
    }
    component UseExplainLabel: Label {
        font.pixelSize: Oran7MainUiSetting.textPixelSize + 5
        font.family: Oran7MainUiSetting.fontFamily
        font.bold: true
        font.italic: true
        color:Oran7Theme.Oran7MainGUI.themeColor

        opacity: Oran7Theme.Oran7MainGUI.settingContent_visiable  ? 1 : 0
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
        if(root.enable__dwmBlur__)
            __blurTimer.stop()

        // 先清一次，避免第二次打开 true -> true 没有真正重新应用
        if(root.enable__dwmBlur__)
             __windowAgent.setWindowAttribute("dwm-blur", false)

        if (root.visibility !== Window.Maximized)
            root.showMaximized()
        else
            root.visible = true

        if(root.enable__dwmBlur__)
            __blurTimer.start()
    }

    function closeWindow() {
        if(root.enable__dwmBlur__)
            __blurTimer.stop()

        // 关闭前清掉 native blur 状态
        if(root.enable__dwmBlur__)
            __windowAgent.setWindowAttribute("dwm-blur", false)

        root.visible = false
        restoreMainWindowFocus()
    }

    function restoreMainWindowFocus() {
        // 延迟一小段时间确保设置窗口完全关闭后再发出信号
        root.restoreMainWindowFocusRequested();
    }

    function handle_OpenSettingWin_signal(){//处理打开设置窗口的信号
        if(Oran7MainUiSetting.settingWindow_isOpening_Or_isClosing)return;

        if (!Oran7Theme.Oran7MainGUI.OpenSettingWin) {
            openAllPanels();
        } else {
            closeAllPanels();
        }
    }

    function openAllPanels() {
        var panels = [
            mainUiSettingLoader.item,
            mediaPlayerSettingLoader.item,
            screenCaptureSettingLoader.item,
            musicPlayListSettingLoader.item
        ];
        // 所有面板同时准备（进入场景图，height=0）
        for (var i = 0; i < panels.length; i++) {
            if (panels[i] && panels[i].prepareForOpen) {
                panels[i].prepareForOpen();
            }
        }
        for (var j = 0; j < panels.length; j++) {
            if (panels[j] && panels[j].startOpenAnimation) {
                panels[j].startOpenAnimation();
            }
        }
    }

    function closeAllPanels() {
        var panels = [
            mainUiSettingLoader.item,
            mediaPlayerSettingLoader.item,
            screenCaptureSettingLoader.item,
            musicPlayListSettingLoader.item
        ];
        for (var i = 0; i < panels.length; i++) {
            if (panels[i] && panels[i].startCloseAnimation) {
                panels[i].startCloseAnimation();
            }
        }
    }

    // 连接到全局键盘Keys事件过滤器
    Connections {
        target: globalEventFilter
        function onEscapeKeyPressed() {
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

    Timer {
        id: preloadTimer
        interval: 10
        onTriggered: {
            mainUiSettingLoader.active = true;
            mediaPlayerSettingLoader.active = true;
            screenCaptureSettingLoader.active = true;
            musicPlayListSettingLoader.active = true;
        }
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
            if (screenCaptureSettingLoader.item) {
                screenCaptureSettingLoader.item.y -= 4
                screenCaptureSettingLoader.item.savedNormalY = screenCaptureSettingLoader.item.y
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
            if (screenCaptureSettingLoader.item) {
                screenCaptureSettingLoader.item.y += 4
                screenCaptureSettingLoader.item.savedNormalY = screenCaptureSettingLoader.item.y
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
            if (screenCaptureSettingLoader.item) {
                screenCaptureSettingLoader.item.x -= 4
                screenCaptureSettingLoader.item.savedNormalX = screenCaptureSettingLoader.item.x
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
            if (screenCaptureSettingLoader.item) {
                screenCaptureSettingLoader.item.x += 4
                screenCaptureSettingLoader.item.savedNormalX = screenCaptureSettingLoader.item.x
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
