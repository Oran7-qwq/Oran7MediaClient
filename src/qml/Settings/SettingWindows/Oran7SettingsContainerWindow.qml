pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects
import QtQuick.Effects
import QtMultimedia

import "../GlobalSettings"
import "../PagesSettings"
import "../../Components"

import "../../Components/Oran7SettingUiWindowItems"

import Oran7UI.Impl

ApplicationWindow {
    id: root
    visible: Oran7Theme.Oran7MainGUI.OpenSettingWin
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    width: Screen.width
    height: Screen.height - root.taskbarHeight // 动态减去任务栏高度
    color: "transparent" // 全透明背景

    property real taskbarHeight: 50
    property bool isAnimating: false // 动画进行中锁，防止快速操作导致状态不一致

    // 当设置窗口需要恢复主窗口焦点时发出
    signal restoreMainWindowFocusRequested


    Component.onCompleted: {
        detectTaskbarHeight();
        //groupChangeTimer.start();
        //textureAnimationTimer.start();

        // 预加载所有设置窗口
        preloadTimer.start();
    }

    onScreenChanged: {
        detectTaskbarHeight();
    }

    function detectTaskbarHeight() {
        root.taskbarHeight = 50;
    }

    onVisibleChanged: {
        if (!visible)
            restoreMainWindowFocus()
    }

    // background: Oran7BlurCard{
    //     anchors.fill: parent
    //     blurEnabled: true
    //     borderWidth: 1
    //     visible: true
    //     themeColor: "#24FFFFFF"
    //     blurSource: root
    // }

    Item {
        id: content
        anchors.fill: parent

        // 背景点击区域 - 必须在面板之前声明，确保面板 z-order 在其上方
        MouseArea {
            anchors.fill: parent
            propagateComposedEvents: true
            onClicked: mouse => {
                Oran7MainUiSetting.clickedOutSide();
                //Oran7MainUiSetting.callOpenSettingWindow()
            }
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
                Behavior on x { enabled: !isAnimating; NumberAnimation { duration: 50 } }
                Behavior on y { enabled: !isAnimating; NumberAnimation { duration: 50 } }
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
            Oran7MediaPlayerSettingWindow {
                winIndex: 1
                Behavior on x { enabled: !isAnimating; NumberAnimation { duration: 50 } }
                Behavior on y { enabled: !isAnimating; NumberAnimation { duration: 50 } }
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
                winIndex: 2
                Behavior on x { enabled: !isAnimating; NumberAnimation { duration: 50 } }
                Behavior on y { enabled: !isAnimating; NumberAnimation { duration: 50 } }
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
            Oran7MusicPlayListSettingWindow {
                winIndex: 3
                Behavior on x { enabled: !isAnimating; NumberAnimation { duration: 50 } }
                Behavior on y { enabled: !isAnimating; NumberAnimation { duration: 50 } }
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
        interval: 50
        repeat: true
        running: false
        onTriggered: {
            if (mainUiSettingLoader.item) {
                mainUiSettingLoader.item.y -= 10
                mainUiSettingLoader.item.savedNormalY = mainUiSettingLoader.item.y
            }
            if (mediaPlayerSettingLoader.item) {
                mediaPlayerSettingLoader.item.y -= 10
                mediaPlayerSettingLoader.item.savedNormalY = mediaPlayerSettingLoader.item.y
            }
            if (screenCaptureSettingLoader.item) {
                screenCaptureSettingLoader.item.y -= 10
                screenCaptureSettingLoader.item.savedNormalY = screenCaptureSettingLoader.item.y
            }
            if (musicPlayListSettingLoader.item) {
                musicPlayListSettingLoader.item.y -= 10
                musicPlayListSettingLoader.item.savedNormalY = musicPlayListSettingLoader.item.y
            }
        }
    }

    // Down key timer - moves GUI downward
    Timer{
        id:handle_downKeyTimer
        interval: 50
        repeat: true
        running: false
        onTriggered: {
            if (mainUiSettingLoader.item) {
                mainUiSettingLoader.item.y += 10
                mainUiSettingLoader.item.savedNormalY = mainUiSettingLoader.item.y
            }
            if (mediaPlayerSettingLoader.item) {
                mediaPlayerSettingLoader.item.y += 10
                mediaPlayerSettingLoader.item.savedNormalY = mediaPlayerSettingLoader.item.y
            }
            if (screenCaptureSettingLoader.item) {
                screenCaptureSettingLoader.item.y += 10
                screenCaptureSettingLoader.item.savedNormalY = screenCaptureSettingLoader.item.y
            }
            if (musicPlayListSettingLoader.item) {
                musicPlayListSettingLoader.item.y += 10
                musicPlayListSettingLoader.item.savedNormalY = musicPlayListSettingLoader.item.y
            }
        }
    }

    // Left key timer - moves GUI leftward
    Timer{
        id:handle_leftKeyTimer
        interval: 50
        repeat: true
        running: false
        onTriggered: {
            if (mainUiSettingLoader.item) {
                mainUiSettingLoader.item.x -= 10
                mainUiSettingLoader.item.savedNormalX = mainUiSettingLoader.item.x
            }
            if (mediaPlayerSettingLoader.item) {
                mediaPlayerSettingLoader.item.x -= 10
                mediaPlayerSettingLoader.item.savedNormalX = mediaPlayerSettingLoader.item.x
            }
            if (screenCaptureSettingLoader.item) {
                screenCaptureSettingLoader.item.x -= 10
                screenCaptureSettingLoader.item.savedNormalX = screenCaptureSettingLoader.item.x
            }
            if (musicPlayListSettingLoader.item) {
                musicPlayListSettingLoader.item.x -= 10
                musicPlayListSettingLoader.item.savedNormalX = musicPlayListSettingLoader.item.x
            }
        }
    }

    // Right key timer - moves GUI rightward
    Timer{
        id:handle_rightKeyTimer
        interval: 50
        repeat: true
        running: false
        onTriggered: {
            if (mainUiSettingLoader.item) {
                mainUiSettingLoader.item.x += 10
                mainUiSettingLoader.item.savedNormalX = mainUiSettingLoader.item.x
            }
            if (mediaPlayerSettingLoader.item) {
                mediaPlayerSettingLoader.item.x += 10
                mediaPlayerSettingLoader.item.savedNormalX = mediaPlayerSettingLoader.item.x
            }
            if (screenCaptureSettingLoader.item) {
                screenCaptureSettingLoader.item.x += 10
                screenCaptureSettingLoader.item.savedNormalX = screenCaptureSettingLoader.item.x
            }
            if (musicPlayListSettingLoader.item) {
                musicPlayListSettingLoader.item.x += 10
                musicPlayListSettingLoader.item.savedNormalX = musicPlayListSettingLoader.item.x
            }
        }
    }

    // --- open and close sound effect ---
    SoundEffect {
        id: openSoundEffect
        source: "qrc:/sound/OpenSound.wav"
        volume: 0.6
    }
    SoundEffect {
        id: closeSoundEffect
        source: "qrc:/sound/CloseSound.wav"
        volume: 0.6
    }
}
