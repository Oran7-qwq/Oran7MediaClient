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

ApplicationWindow {
    id: root
    visible: Oran7MainUiSetting.settingWin_isOpen
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    width: Screen.width
    height: Screen.height - root.taskbarHeight // 动态减去任务栏高度
    color: "transparent" // 全透明背景

    property real taskbarHeight: 50


    // 当设置窗口需要恢复主窗口焦点时发出
    signal restoreMainWindowFocusRequested


    Component.onCompleted: {
        detectTaskbarHeight();
        //groupChangeTimer.start();
        //textureAnimationTimer.start();
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
        console.log(root.visible)
    }

    Item {
        id: content
        anchors.fill: parent

        Oran7MainUiSettingWindow {
            id: mainUiSetting
            x:  40 + Oran7MainUiSetting.settingItemWinDefalutWidth * winIndex
            y: 20
            visible: false

            winIndex:0

            Behavior on x{NumberAnimation{duration: 50}}
            Behavior on y{NumberAnimation{duration: 50}}
        }

        Oran7MediaPlayerSettingWindow {
            id: mediaPlayerSetting
            x: 40 + Oran7MainUiSetting.settingItemWinDefalutWidth * winIndex
            y: 20
            visible: false

            winIndex:1

            Behavior on x{NumberAnimation{duration: 50}}
            Behavior on y{NumberAnimation{duration: 50}}
        }

        Oran7ScreenCaptureSettingWindow{
            id:screenCaptureSetting
            x: 40 + Oran7MainUiSetting.settingItemWinDefalutWidth * winIndex
            y: 20

            winIndex:2

            visible: false
            Behavior on x{NumberAnimation{duration: 50}}
            Behavior on y{NumberAnimation{duration: 50}}
        }

        Oran7MusicPlayListSettingWindow{
            id: musicPlayListSetting
            x: 40 + Oran7MainUiSetting.settingItemWinDefalutWidth * winIndex
            y: 20

            winIndex:3

            visible: false
            Behavior on x{NumberAnimation{duration: 50}}
            Behavior on y{NumberAnimation{duration: 50}}
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
        color:Oran7MainUiSetting.themeColor

        opacity: Oran7MainUiSetting.settingContent_visiable  ? 1 : 0
        Behavior on opacity {
            PropertyAnimation{
                duration: Oran7MainUiSetting.toggleOpenAniDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        onClicked: mouse => {
            Oran7MainUiSetting.clickedOutSide();
            Oran7MainUiSetting.callOpenSettingWindow()
            mouse.accepted = false;
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
        Qt.callLater(function () {
            root.restoreMainWindowFocusRequested();
        });
    }

    function handle_OpenSettingWin_signal(){//处理打开设置窗口的信号
        Oran7MainUiSetting.triggleOpen_Oran7MainUiSetting_window();
        if (Oran7MainUiSetting.settingWin_isOpen === false) {
            Oran7MainUiSetting.settingWin_isOpen = true;
            delayTimer.delay(100).then(function () {
                openSoundEffect.play();
            });
        }
        else
        {
            delayTimer.delay(handle_OpenSettingWin_delayTimer.interval).then(function () {
                Oran7MainUiSetting.clickedOutSide()
                Oran7MainUiSetting.settingWin_isOpen = false
                closeSoundEffect.play();
            });
        }
    }

    // 连接到全局键盘Keys事件过滤器
    Connections {
        target: globalEventFilter
        function onEscapeKeyPressed() {handle_OpenSettingWin_delayTimer.restart()}

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
        interval: 50
        repeat: true
        running: false
        onTriggered: {
            mainUiSetting.y -= 10
            mediaPlayerSetting.y -= 10
            screenCaptureSetting.y -= 10
            musicPlayListSetting.y -= 10
        }
    }

    // Down key timer - moves GUI downward
    Timer{
        id:handle_downKeyTimer
        interval: 50
        repeat: true
        running: false
        onTriggered: {
            mainUiSetting.y += 10
            mediaPlayerSetting.y += 10
            screenCaptureSetting.y += 10
            musicPlayListSetting.y +=10
        }
    }

    // Left key timer - moves GUI leftward
    Timer{
        id:handle_leftKeyTimer
        interval: 50
        repeat: true
        running: false
        onTriggered: {
            mainUiSetting.x -= 10
            mediaPlayerSetting.x -= 10
            screenCaptureSetting.x -= 10
            musicPlayListSetting.x -= 10
        }
    }

    // Right key timer - moves GUI rightward
    Timer{
        id:handle_rightKeyTimer
        interval: 50
        repeat: true
        running: false
        onTriggered: {
            mainUiSetting.x += 10
            mediaPlayerSetting.x += 10
            screenCaptureSetting.x +=10
            musicPlayListSetting.x +=10
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
