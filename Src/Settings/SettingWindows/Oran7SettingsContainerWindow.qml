pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects
import QtQuick.Effects
import QtMultimedia

import "../GlobalSettings"
import "../PagesSettings"
import "../../Components"

ApplicationWindow {
    id: root
    visible: Oran7MainUiSetting.settingWin_isOpen
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    width: Screen.width
    height: Screen.height - root.taskbarHeight // 动态减去任务栏高度
    color: "transparent" // 全透明背景

    // 当设置窗口需要恢复主窗口焦点时发出
    signal restoreMainWindowFocusRequested

    property real taskbarHeight: 50

    Component.onCompleted: {
        detectTaskbarHeight();
    }

    onScreenChanged: {
        detectTaskbarHeight();
    }

    function detectTaskbarHeight() {
        root.taskbarHeight = 50;
    }

    onVisibleChanged: {
        if (!visible) {
            restoreMainWindowFocus();
        }
    }

    function restoreMainWindowFocus() {
        // 延迟一小段时间确保设置窗口完全关闭后再发出信号
        Qt.callLater(function () {
            root.restoreMainWindowFocusRequested();
        });
    }

    Item {
        anchors.fill: parent

        Keys.onPressed: function (event) {
            if (event.key === Qt.Key_Escape) {
                Oran7MainUiSetting.triggleOpen_Oran7MainUiSetting_window();
                event.accepted = true;
            }
        }

        Oran7MainUiSettingWindow {
            id: mainUiSetting
            x: 40
            y: 20
            visible: false
        }

        Oran7MediaPlayerSettingWindow {
            id: mediaPlayerSetting
            x: 292
            y: 20
            visible: false
        }
    }

    // 连接到全局事件过滤器
    Connections {
        target: globalEventFilter
        function onEscapeKeyPressed() {
            Oran7MainUiSetting.triggleOpen_Oran7MainUiSetting_window();
        }
    }
    Connections {
        target: Oran7MainUiSetting
        function onTriggleOpen_Oran7MainUiSetting_window() {
            if (Oran7MainUiSetting.settingWin_isOpen === false) {
                Oran7MainUiSetting.settingWin_isOpen = true;
                delayTimer.delay(100).then(function(){
                    openSoundEffect.play()
                });
            } else {
                // 使用 .then() 方式等待延时完成
                delayTimer.delay(Oran7MainUiSetting.toggleOpenAniDuration).then(function () {
                    Oran7MainUiSetting.settingWin_isOpen = false;
                    closeSoundEffect.play()
                });
            }
        }
    }

    // --- tools Component ---
    Oran7DelayTimer {
        id: delayTimer
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
