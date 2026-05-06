import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects

import "../GlobalSettings"
import "../../Components"

Window {
    id: root
    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    modality: Qt.NonModal
    width: 252
    height: 800
    opacity: 0
    x: root.savedNormalX
    y: root.savedNormalY

    readonly property real savedNormalX: 40 + 252 * 1
    readonly property real savedNormalY: 20
    readonly property real savedNormalHeight: 800

    property point clickPos: Qt.point(0, 0)
    property bool mouseIsPressed: false

    Connections {
        target: Oran7MainUiSetting
        function onTriggleOpen_Oran7MainUiSetting_window() {
            delayTimer.delay(50).then(function(){
                toggleOpen_delayTimer.restart();
            });
        }
    }

    // --- tools Component ---
    Oran7DelayTimer {
        id: delayTimer
    }

    Timer {
        id: toggleOpen_delayTimer
        running: false
        repeat: false
        interval: 100
        onTriggered: {
            if (root.visible === false) {
                root.visible = true;
                window_openAnimation.restart();
            } else {
                window_closeAnimation.restart();
            }
        }
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
            easing.type: Easing.InOutCubic
        }

        PropertyAnimation {
            target: root
            property: "y"
            from: -root.savedNormalY
            to: root.savedNormalY
            duration: window_openAnimation.aniDuration
            easing.type: Easing.InOutCubic
        }

        PropertyAnimation {
            target: root
            property: "height"
            from: 0
            to: root.savedNormalHeight
            duration: window_openAnimation.aniDuration
            easing.type: Easing.InOutCubic
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
            to: -root.savedNormalY
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

            Rectangle {
                id: topDragRect
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 2
                height: 30
                color: Oran7MainUiSetting.itemColor
                radius: 8
                Image{
                    id:topIcon
                    source: "qrc:/image/hugeicons_ai-setting.png"
                    sourceSize.height: topDragRect.height * 0.8
                    sourceSize.width: topDragRect.height * 0.8
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    mipmap: true
                    layer.enabled: true
                    layer.effect: ColorOverlay{
                        source: topIcon
                        color:Oran7MainUiSetting.textColor
                    }
                }
                Label{
                    anchors.left: topIcon.right
                    anchors.leftMargin: 4
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text:"MediaPlayerSetting"
                    color:Oran7MainUiSetting.textColor
                    font.pixelSize: Oran7MainUiSetting.textPixelSize
                    font.family: Oran7MainUiSetting.fontFamily
                    font.bold: true
                }
            }
        }
    }

    // 拖动区域 - 整个窗口可拖动
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPressed: mouse => {
            root.mouseIsPressed = true;
            root.clickPos = Qt.point(mouse.x, mouse.y);
        }
        onReleased: {
            root.mouseIsPressed = false;
        }
        onPositionChanged: mouse => {
            if (root.mouseIsPressed === false)
                return;
            if (root.clickPos.y > topDragRect.height + 16 || root.clickPos.y < 16)
                return;
            let delta = Qt.point(mouse.x - root.clickPos.x, mouse.y - root.clickPos.y);
            root.x += delta.x;
            root.y += delta.y;
        }
    }
}
