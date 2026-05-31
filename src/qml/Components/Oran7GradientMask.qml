// Oran7GradientMask.qml
import QtQuick 2.15
import Qt5Compat.GraphicalEffects

Item {
    id: root

    // 外部自定义内容区域，不限定 Column
    default property alias content: maskContent.data

    // 允许外部访问内容容器
    property alias contentItem: maskContent

    property Item maskSourceItem: null

    // 如果外部不设置尺寸，则尝试根据 childrenRect 自动计算
    property real contentWidth: -1
    property real contentHeight: -1

    implicitWidth: contentWidth > 0
                   ? contentWidth
                   : Math.max(0, maskContent.childrenRect.x + maskContent.childrenRect.width)

    implicitHeight: contentHeight > 0
                    ? contentHeight
                    : Math.max(0, maskContent.childrenRect.y + maskContent.childrenRect.height)

    width: implicitWidth
    height: implicitHeight

    property int gradientColorIndex: 0

    // true: 动态渐变；false: 静态渐变
    property bool dynamicGradient: false

    // true: 启用渐变遮罩；false: 显示原始内容
    property bool gradientMaskEnabled: true

    property int transitionDuration: 1000
    property int dynamicInterval: transitionDuration + 100

    property point gradientStart: Qt.point(0, 0)
    property point gradientEnd: Qt.point(width, height)

    property var colorItems: [
        { beginColorValue: "#96fbc4", middleColorValue: "#7ed321", endColorValue: "#f9f586" },
        { beginColorValue: "#fa709a", middleColorValue: "#fee140", endColorValue: "#ff9a8b" },
        { beginColorValue: "#fd63a3", middleColorValue: "#fe9800", endColorValue: "#ffb74d" },
        { beginColorValue: "#ff6b6b", middleColorValue: "#ff4757", endColorValue: "#ee5a52" },
        { beginColorValue: "#f093fb", middleColorValue: "#f5576c", endColorValue: "#4facfe" },
        { beginColorValue: "#0093e9", middleColorValue: "#00f2fe", endColorValue: "#4facfe" },
        { beginColorValue: "#ffcc02", middleColorValue: "#f7971e", endColorValue: "#ffd200" },
        { beginColorValue: "#2d5016", middleColorValue: "#a4de6c", endColorValue: "#40e0d0" }
    ]

    property color beginColor: "#96fbc4"
    property color middleColor: "#7ed321"
    property color endColor: "#f9f586"

    property int _dynamicIndex: gradientColorIndex

    function normalizedIndex(index) {
        if (colorItems.length <= 0)
            return 0

        var count = colorItems.length
        return ((index % count) + count) % count
    }

    function colorItem(index) {
        if (colorItems.length <= 0) {
            return {
                beginColorValue: "#ffffff",
                middleColorValue: "#ffffff",
                endColorValue: "#ffffff"
            }
        }

        return colorItems[normalizedIndex(index)]
    }

    function setGradientImmediately(index) {
        var item = colorItem(index)
        beginColor = item.beginColorValue
        middleColor = item.middleColorValue
        endColor = item.endColorValue
    }

    function animateToGradient(index) {
        var item = colorItem(index)

        beginColorAnimation.to = item.beginColorValue
        middleColorAnimation.to = item.middleColorValue
        endColorAnimation.to = item.endColorValue

        gradientColorAnimation.restart()
    }

    onGradientColorIndexChanged: {
        if (!dynamicGradient)
            animateToGradient(gradientColorIndex)
    }

    onDynamicGradientChanged: {
        if (dynamicGradient) {
            _dynamicIndex = normalizedIndex(gradientColorIndex)
            dynamicTimer.restart()
        } else {
            dynamicTimer.stop()
            animateToGradient(gradientColorIndex)
        }
    }

    Component.onCompleted: {
        setGradientImmediately(gradientColorIndex)
    }

    LinearGradient {
        id: gradientLayer
        anchors.fill: parent
        visible: false

        start: root.gradientStart
        end: root.gradientEnd

        gradient: Gradient {
            GradientStop { position: 0.0; color: root.beginColor }
            GradientStop { position: 0.5; color: root.middleColor }
            GradientStop { position: 1.0; color: root.endColor }
        }
    }

    Item {
        id: maskContent

        anchors.left: parent.left
        anchors.top: parent.top

        width: root.width
        height: root.height

        // 启用遮罩时，它只是 maskSource，不直接显示
        // 关闭遮罩时，显示原始内容
        visible: !root.gradientMaskEnabled && root.maskSourceItem === null
    }

    OpacityMask {
        anchors.fill: parent
        visible: root.gradientMaskEnabled

        source: gradientLayer
        maskSource: maskContent
    }

    ParallelAnimation {
        id: gradientColorAnimation

        PropertyAnimation {
            id: beginColorAnimation
            target: root
            property: "beginColor"
            duration: root.transitionDuration
        }

        PropertyAnimation {
            id: middleColorAnimation
            target: root
            property: "middleColor"
            duration: root.transitionDuration
        }

        PropertyAnimation {
            id: endColorAnimation
            target: root
            property: "endColor"
            duration: root.transitionDuration
        }
    }

    Timer {
        id: dynamicTimer
        interval: root.dynamicInterval
        repeat: true
        running: root.dynamicGradient

        onTriggered: {
            root.animateToGradient(root._dynamicIndex)
            root._dynamicIndex = root.normalizedIndex(root._dynamicIndex + 1)
        }
    }
}

//遮罩对象说明：
// 子对象类型                        是否会参与遮罩
// Text                                     会，文字笔画区域参与
// Image                            会，图片非透明区域参与
// Rectangle                    会，如果它有不透明颜色
// Row / Column / Item	本身通常不产生可见像素，但它们的可见子项会参与
// Timer / Connections                  不会，它们没有视觉输出
// color: "transparent" 的控件	基本不会产生遮罩
// opacity: 0 的控件               不会产生有效遮罩
// opacity: 0.5 的控件             会产生半透明遮罩
