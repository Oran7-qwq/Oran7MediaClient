import QtQuick 2.15
import QtQuick.Controls

Rectangle {
    id: root

    // ===== 外部属性 =====
    property bool animationEnabled: false  // 动画开关：true=动态，false=静态

    // ===== 内部尺寸定义 =====
    property int barWidth: 5              // 柱子宽度
    property int barSpacing: 3             // 柱子间隔
    property int maxHeight: 24             // 最大高度
    property int minHeight: 5              // 最小高度

    // ===== 静态状态的高度值 =====
    readonly property var staticHeights: [8, 20, 14]  // 静态高度从左到右

    // ===== 容器尺寸 =====
    width: (barWidth * 3) + (barSpacing * 2)
    height: maxHeight
    color: "transparent"

    // ===== 动画状态管理 =====
    property bool isAnimating: false

    // ===== 动画控制 =====
    function toggleAnimation(enable) {
        if (enable && !isAnimating) {
            startWaveAnimation()
        } else if (!enable) {
            stopWaveAnimation()
        }
    }

    // 启动波浪动画
    function startWaveAnimation() {
        isAnimating = true
        waveAnimation.restart()
    }

    // 停止波浪动画
    function stopWaveAnimation() {
        isAnimating = false
        waveAnimation.stop()
        // 恢复到静态高度
        bar1.height = staticHeights[0]
        bar2.height = staticHeights[1]
        bar3.height = staticHeights[2]
    }

    // ===== 波浪动画 =====
    SequentialAnimation {
        id: waveAnimation
        loops: Animation.Infinite

        // 第一阶段：波浪向右移动
        ParallelAnimation {
            // 柱子1
            NumberAnimation {
                target: bar1
                property: "height"
                from: maxHeight
                to: minHeight
                duration: 300 * 2
                easing.type: Easing.InOutSine
            }
            // 柱子2
            NumberAnimation {
                target: bar2
                property: "height"
                from: staticHeights[1]
                to: maxHeight / 2
                duration: 300 * 2
                easing.type: Easing.InOutSine
            }
            // 柱子3
            NumberAnimation {
                target: bar3
                property: "height"
                from: minHeight
                to: maxHeight
                duration: 300 * 2
                easing.type: Easing.InOutSine
            }
        }

        // 第二阶段：恢复初始态 (反向执行)
        ParallelAnimation {
            NumberAnimation {
                target: bar1
                property: "height"
                from: minHeight
                to: maxHeight
                duration: 300 * 2
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                target: bar2
                property: "height"
                from: maxHeight/2
                to:staticHeights[1]
                duration: 300 * 2
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                target: bar3
                property: "height"
                from: maxHeight
                to: minHeight
                duration: 300 * 2
                easing.type: Easing.InOutSine
            }
        }
    }

    // ===== 柱子定义 =====

    // 柱子1（左）
    Rectangle {
        id: bar1
        width: root.barWidth
        height: root.staticHeights[0]
        x: 0
        y: root.height - height
        radius: 2
        color: "#FF8F6E"

        Behavior on height {
            NumberAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
    }

    // 柱子2（中）
    Rectangle {
        id: bar2
        width: root.barWidth
        height: root.staticHeights[1]
        x: bar1.width + root.barSpacing
        y: root.height - height
        radius: 2
        color: "#FF8F6E"

        Behavior on height {
            NumberAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
    }

    // 柱子3（右）
    Rectangle {
        id: bar3
        width: root.barWidth
        height: root.staticHeights[2]
        x: bar2.x + bar2.width + root.barSpacing
        y: root.height - height
        radius: 2
        color: "#FF8F6E"

        Behavior on height {
            NumberAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
    }

    // ===== 监听动画开关 =====
    Connections {
        target: root
        function onAnimationEnabledChanged() {
            toggleAnimation(root.animationEnabled)
        }
    }

    // ===== 组件初始化 =====
    Component.onCompleted: {
        // 初始化为静态状态
        bar1.height = staticHeights[0]
        bar2.height = staticHeights[1]
        bar3.height = staticHeights[2]
    }
}
