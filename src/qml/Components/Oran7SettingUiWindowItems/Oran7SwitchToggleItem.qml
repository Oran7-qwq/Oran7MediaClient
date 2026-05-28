pragma ComponentBehavior: Bound

import QtQuick 2.15
import Qt5Compat.GraphicalEffects

import "../../Settings/GlobalSettings"

import Oran7UI.Impl

Item {
    id: root

    //默认布局
    anchors.right: parent.right
    anchors.rightMargin: 4
    anchors.verticalCenter: parent.verticalCenter

    // 属性定义
    property bool checked: false
    property color onColor: Oran7Theme.Oran7MainGUI.themeColor
    property color offColor: Oran7MainUiSetting.itemBackColor
    property color borderColor: Oran7MainUiSetting.isDarkMode ? root.onColor : root.offColor

    // 信号定义
    signal switchToggleChanged(bool checked)

    height: Oran7MainUiSetting.itemHeight * 0.666
    width: height * 2

    // 开关背景
    Rectangle {
        id: toggleSwitch
        anchors.fill: parent
        radius: height / 2

        color: root.checked ? root.onColor : root.offColor
        Behavior on color{PropertyAnimation{duration:Oran7Theme.Primary.durationMid}}
        border.color: root.borderColor
        Behavior on border.color{PropertyAnimation{duration:Oran7Theme.Primary.durationMid}}
        border.width: 1
    }

    // 可移动的圆形指示器
    Rectangle {
        id: indicator
        anchors.verticalCenter: parent.verticalCenter
        width: Oran7MainUiSetting.itemHeight * 0.6
        height: width
        radius: height / 2
        color: "white"

        // 切换动画
        Behavior on x {
            NumberAnimation {
                duration: 200
                easing.type: Easing.InOutQuad
            }
        }

        // 初始位置
        x: root.checked ? toggleSwitch.width - width - 2 : 2

        // 阴影效果
        // layer.enabled: false
        // layer.effect: DropShadow {
        //     anchors.fill: indicator
        //     horizontalOffset: 0
        //     verticalOffset: 2
        //     radius: 4
        //     samples: 8
        //     color: "#4d000000"
        //     smooth: true
        // }
    }

    // 点击区域
    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.checked = !root.checked;
            root.switchToggleChanged(checked);
        }

        // 可选：添加悬停效果
        hoverEnabled: true
        onEntered: {
            toggleSwitch.scale = 1.05
            cursorShape = Qt.PointingHandCursor
        }
        onExited: {
            toggleSwitch.scale = 1.0
            cursorShape = Qt.ArrowCursor
        }

        // 添加点击缩放效果
        onPressed: {
            toggleSwitch.scale = 0.95
        }
        onReleased: {
            toggleSwitch.scale = 1.0
        }
    }
}
