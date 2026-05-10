/*
 * WindowControlButton
 *
 * 通用窗口控制按钮组件，支持自定义图标和颜色
 */

import QtQuick

Rectangle {
    id: button
    color: buttonColor
    radius: 4

    property color buttonColor: "transparent"
    property color hoverColor: "#c42b1c"
    property string iconText: "✕"
    property int buttonTextPixelSize: 15
    signal clicked

    // 颜色过渡动画
    Behavior on color {
        ColorAnimation {
            duration: 150
            easing.type: Easing.InOutCubic
        }
    }

    // 透明度过渡动画
    Behavior on opacity {
        NumberAnimation {
            duration: 150
            easing.type: Easing.InOutCubic
        }
    }

    // 按下效果
    scale: mouseArea.pressed ? 0.95 : 1.0

    Behavior on scale {
        NumberAnimation {
            duration: 100
            easing.type: Easing.OutBack
        }
    }

    Text {
        id: buttonText
        anchors.centerIn: parent
        text: iconText
        color: "#ffffff"
        font.pixelSize: button.buttonTextPixelSize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        opacity: 0.8

        // 文字透明度过渡
        Behavior on opacity {
            NumberAnimation {
                duration: 150
                easing.type: Easing.InOutCubic
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        propagateComposedEvents: true

        onEntered: {
            button.color = hoverColor
            buttonText.opacity = 1.0
        }

        onExited: {
            button.color = button.buttonColor
            buttonText.opacity = 0.8
        }

        onClicked: {
            button.clicked()
        }

        onPressed: {
            button.scale = 0.95
        }

        onReleased: {
            button.scale = 1.0
        }

        onCanceled: {
            button.scale = 1.0
        }
    }
}
