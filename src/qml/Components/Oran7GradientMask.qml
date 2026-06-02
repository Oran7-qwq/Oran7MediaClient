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
    // 注意：当子项使用 anchors.fill parent 时会产生绑定循环，
    // 此时不应依赖自动推算，由调用方显式设定尺寸
    property real contentWidth: -1
    property real contentHeight: -1

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
        // 0 活力青绿
        {
            beginColorValue: "#00F5A0",
            middleColorValue: "#00D9F5",
            endColorValue: "#00B8FF"
        },

        // 1 电光青蓝
        {
            beginColorValue: "#00C6FF",
            middleColorValue: "#0072FF",
            endColorValue: "#3A7BFF"
        },

        // 2 蓝紫霓虹
        {
            beginColorValue: "#3A7BFF",
            middleColorValue: "#7A5CFF",
            endColorValue: "#B84DFF"
        },

        // 3 紫粉高亮
        {
            beginColorValue: "#8E2DE2",
            middleColorValue: "#C471F5",
            endColorValue: "#F64FCE"
        },

        // 4 玫红紫
        {
            beginColorValue: "#F64FCE",
            middleColorValue: "#FF3CAC",
            endColorValue: "#FF5E8A"
        },

        // 5 樱桃粉红
        {
            beginColorValue: "#FF416C",
            middleColorValue: "#FF4B8B",
            endColorValue: "#FF6A88"
        },

        // 6 珊瑚橙红
        {
            beginColorValue: "#FF512F",
            middleColorValue: "#FF6A00",
            endColorValue: "#FF9500"
        },

        // 7 橙黄火焰
        {
            beginColorValue: "#FF9500",
            middleColorValue: "#FFC400",
            endColorValue: "#FFE600"
        },

        // 8 柠檬黄绿
        {
            beginColorValue: "#FFE600",
            middleColorValue: "#C6FF00",
            endColorValue: "#7FFF00"
        },

        // 9 荧光绿
        {
            beginColorValue: "#7FFF00",
            middleColorValue: "#39FF14",
            endColorValue: "#00F5A0"
        },

        // 10 青绿回环
        {
            beginColorValue: "#00F5A0",
            middleColorValue: "#00FFA3",
            endColorValue: "#00FFC6"
        },

        // 11 热带蓝绿
        {
            beginColorValue: "#00FFC6",
            middleColorValue: "#00E5FF",
            endColorValue: "#00A8FF"
        },

        // 12 深海亮蓝
        {
            beginColorValue: "#00A8FF",
            middleColorValue: "#006DFF",
            endColorValue: "#004DFF"
        },

        // 13 靛蓝紫
        {
            beginColorValue: "#004DFF",
            middleColorValue: "#5B2DFF",
            endColorValue: "#9B00FF"
        },

        // 14 赛博紫
        {
            beginColorValue: "#9B00FF",
            middleColorValue: "#D100FF",
            endColorValue: "#FF00C8"
        },

        // 15 霓虹粉
        {
            beginColorValue: "#FF00C8",
            middleColorValue: "#FF2D95",
            endColorValue: "#FF4D6D"
        },

        // 16 糖果红橙
        {
            beginColorValue: "#FF4D6D",
            middleColorValue: "#FF3D00",
            endColorValue: "#FF7A00"
        },

        // 17 金橙
        {
            beginColorValue: "#FF7A00",
            middleColorValue: "#FFB000",
            endColorValue: "#FFD500"
        },

        // 18 酸橙黄绿
        {
            beginColorValue: "#FFD500",
            middleColorValue: "#BFFF00",
            endColorValue: "#64FF00"
        },

        // 19 绿青闭环
        {
            beginColorValue: "#64FF00",
            middleColorValue: "#00FF85",
            endColorValue: "#00F5A0"
        }
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
        if (dynamicGradient) {
            _dynamicIndex = normalizedIndex(gradientColorIndex)
            animateToGradient(_dynamicIndex)
        } else {
            animateToGradient(gradientColorIndex)
        }
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

        anchors.fill: parent

        // 必须始终保持在渲染树中可见，否则 OpacityMask 无法捕获其像素作为遮罩（grab 到空图）
        // 启用遮罩时 OpacityMask 在上层覆盖，因此不会出现双重渲染的视觉问题
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
