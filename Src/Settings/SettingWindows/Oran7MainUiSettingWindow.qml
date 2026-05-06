pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects

import "../GlobalSettings"
import "../../Components"

Window {
    id: root
    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    modality: Qt.NonModal
    width: 252
    height: 600
    opacity: 0
    x: root.savedNormalX
    y: root.savedNormalY

    readonly property int savedNormalX: 40
    readonly property real savedNormalY: 20
    readonly property real savedNormalHeight: 600

    property point clickPos: Qt.point(0, 0)
    property bool mouseIsPressed: false

    Connections {
        target: Oran7MainUiSetting
        function onTriggleOpen_Oran7MainUiSetting_window() {
            toggleOpen_delayTimer.restart();
        }
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
        readonly property real aniDuration: Oran7MainUiSetting.toggleOpenAniDuration

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
        readonly property real aniDuration: Oran7MainUiSetting.toggleOpenAniDuration

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
                    text:"MainUiSettings"
                    color:Oran7MainUiSetting.textColor
                    font.pixelSize: Oran7MainUiSetting.textPixelSize
                    font.family: Oran7MainUiSetting.fontFamily
                    font.bold: true
                }
            }
            Column {
                anchors.top: topDragRect.bottom
                anchors.topMargin: 8
                anchors.left: parent.left
                anchors.leftMargin: 4
                anchors.right: parent.right
                spacing: 2
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 30
                    color: "transparent"

                    // 开关按钮
                    Rectangle {
                        id: toggleSwitch
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        width: 40
                        height: 20
                        radius: height / 2

                        color: Oran7MainUiSetting.isDarkMode ? Oran7MainUiSetting.themeColor : Oran7MainUiSetting.itemColor
                        border.color: Oran7MainUiSetting.isDarkMode ? Oran7MainUiSetting.themeColor : Oran7MainUiSetting.itemColor
                        border.width: 1

                        // 可移动的圆形指示器
                        Rectangle {
                            id: indicator
                            anchors.verticalCenter: parent.verticalCenter
                            width: 18
                            height: 18
                            radius: height / 2
                            color: "white"

                            // 初始位置在右边
                            x: Oran7MainUiSetting.isDarkMode ? toggleSwitch.width - width - 2 : 2

                            // 阴影效果
                            layer.enabled: false
                            layer.effect: DropShadow {
                                anchors.fill: indicator
                                horizontalOffset: 0
                                verticalOffset: 2
                                radius: 4
                                samples: 8
                                color: "#4d000000"
                                smooth: true
                            }
                        }

                        // 鼠标区域处理点击
                        MouseArea {
                            id: toggleSwitchMouseArea
                            anchors.fill: parent
                            onClicked: {
                                Oran7MainUiSetting.isDarkMode = !Oran7MainUiSetting.isDarkMode;
                            }
                        }
                    }

                    Label {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        height: 30
                        color: Oran7MainUiSetting.textColor
                        text: "isDarkMode"
                        font.pixelSize: Oran7MainUiSetting.textPixelSize
                        font.family: Oran7MainUiSetting.fontFamily
                    }
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 30
                    color: "transparent"
                    Label {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 30
                        color: Oran7MainUiSetting.textColor
                        text: "backgroundImagePath"
                        font.pixelSize: Oran7MainUiSetting.textPixelSize
                        font.family: Oran7MainUiSetting.fontFamily
                    }
                }
            }
        }
    }

    // 拖动区域 - 整个窗口可拖动
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents: true
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
