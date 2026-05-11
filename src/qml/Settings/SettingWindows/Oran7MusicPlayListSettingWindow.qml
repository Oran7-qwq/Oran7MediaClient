import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects

import "../GlobalSettings"
import "../../Components"
import "../../Components/Oran7SettingUiWindowItems"

Window {
    id: root
    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    modality: Qt.NonModal
    width: Oran7MainUiSetting.settingItemWinDefalutWidth
    height: root.savedNormalHeight
    opacity: 0
    x: root.savedNormalX
    y: 0

    property int  winIndex : 3

    property real savedNormalX: 40 + Oran7MainUiSetting.settingItemWinDefalutWidth * root.winIndex
    property real savedNormalY: 20
    property real savedNormalHeight: 600

    property point clickPos: Qt.point(0, 0)
    property bool mouseIsPressed: false

    Connections {
        target: Oran7MainUiSetting
        function onTriggleOpen_Oran7MainUiSetting_window() {
            if (root.visible === false) {
                delayTimer.delay(10 + 40*root.winIndex).then(function () {
                    root.visible = true;
                    window_openAnimation.restart();
                });
            } else {
                window_openAnimation.stop();
                window_closeAnimation.restart();
            }
        }
    }

    // --- tools Component ---
    Oran7DelayTimer {
        id: delayTimer
    }

    ParallelAnimation {
        id: window_openAnimation
        property real aniDuration: Oran7MainUiSetting.toggleOpenAniDuration

        PropertyAnimation {
            target: root
            property: "opacity"
            from: 0
            to: 1
            duration: window_openAnimation.aniDuration
            easing.type: Easing.OutCubic
        }

        PropertyAnimation {
            target: root
            property: "y"
            from: 0
            to: root.savedNormalY
            duration: window_openAnimation.aniDuration
            easing.type: Easing.OutCubic
        }

        PropertyAnimation {
            target: root
            property: "height"
            from: 0
            to: root.savedNormalHeight
            duration: window_openAnimation.aniDuration
            easing.type: Easing.OutCubic
        }
    }

    ParallelAnimation {
        id: window_closeAnimation
        property real aniDuration: Oran7MainUiSetting.toggleOpenAniDuration

        PropertyAnimation {
            target: root
            property: "opacity"
            from: 1
            to: 0
            duration: window_closeAnimation.aniDuration
            easing.type: Easing.InExpo
        }

        PropertyAnimation {
            target: root
            property: "y"
            from: root.savedNormalY
            to: 0
            duration: window_closeAnimation.aniDuration
            easing.type: Easing.InExpo
        }

        PropertyAnimation {
            target: root
            property: "height"
            from: root.savedNormalHeight
            to: 0
            duration: window_openAnimation.aniDuration
            easing.type: Easing.InExpo
        }

        onFinished: {
            root.visible = false;
        }
    }

    Rectangle {
        id: ui_root
        anchors.fill: parent
        color: "transparent"
        radius: 10

        // 阴影效果 - 添加在内容下方
        layer.enabled: true
        layer.effect: DropShadow {
            horizontalOffset: 0
            verticalOffset: 5
            radius: 12
            samples: 16
            spread: 0.7
            color: Oran7MainUiSetting.winShadowColor
            transparentBorder: true
        }

        Rectangle {
            id: ui_content
            anchors.fill: parent
            anchors.margins: 16  // 避开阴影边缘
            color: Oran7MainUiSetting.backColor
            radius: 10
            opacity: 1

            Oran7SetingTitleItem{
                id:topDragRect
                title:"MusicPlayListSettings"
                anchors.top: parent.top
                anchors.margins: 2
            }

            //<--- ui content goes here --->

            //<--- ui content ends here --->
        }
    }

    // 拖动区域 - 整个窗口可拖动
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        onPressed: function(mouse) {
            root.mouseIsPressed = true;
            root.clickPos = Qt.point(mouse.x, mouse.y);

            // 检查是否在标题栏区域（用于拖动）
            let inTitleBar = mouse.y <= topDragRect.height + 16 && mouse.y >= 16;

            if (inTitleBar) {
                mouse.accepted = true; // 标题栏拖动事件被接受
            } else {
                mouse.accepted = false; // 其他区域事件传递给子组件
                // 立即触发 clickedOutSide，让 TextField 失去焦点\
                //console.log("clickedOutSide")
                Oran7MainUiSetting.clickedOutSide();
            }
        }
        onReleased: function(mouse) {
            root.mouseIsPressed = false;
            root.savedNormalX = root.x
            root.savedNormalY = root.y
            mouse.accepted = false;
        }
        onPositionChanged: function(mouse) {
            if (root.mouseIsPressed === false)
                return;
            if (root.clickPos.y > topDragRect.height + 16 || root.clickPos.y < 16)
                return;
            let delta = Qt.point(mouse.x - root.clickPos.x, mouse.y - root.clickPos.y);
            root.x += delta.x;
            root.y += delta.y;
        }
        onClicked: function(mouse) {
            mouse.accepted = false; // 允许事件继续传递给子组件
        }
    }
}
