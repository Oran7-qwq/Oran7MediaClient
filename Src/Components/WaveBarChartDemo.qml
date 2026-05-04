import QtQuick 2.15
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    width: 400
    height: 300
    color: "#fef2e8"

    // 标题
    Text {
        id: titleText
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        text: "波形柱状图演示"
        font.pixelSize: 24
        font.family: "微软雅黑"
        color: "#2a1a22"
    }

    // 波形柱状图组件
    WaveBarChart {
        id: waveBarChart
        anchors.centerIn: parent
        animationEnabled: isPlaying // 绑定到播放状态

        // 可选：自定义属性
        barWidth: 10
        barSpacing: 3
        maxHeight: 30
        minHeight: 5
    }

    // 状态标签
    Text {
        id: statusText
        anchors.top: waveBarChart.bottom
        anchors.topMargin: 30
        anchors.horizontalCenter: parent.horizontalCenter
        text: isPlaying ? "状态：动态（波浪效果）" : "状态：静态（固定高度）"
        font.pixelSize: 16
        font.family: "微软雅黑"
        color: "#2a1a22"
    }

    // 播放状态
    property bool isPlaying: false

    // 控制按钮
    Rectangle {
        id: controlButton
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 30
        anchors.horizontalCenter: parent.horizontalCenter
        width: 120
        height: 40
        radius: 20
        color: isPlaying ? "#ff3a3a" : "#FF8F6E"

        Text {
            anchors.centerIn: parent
            text: isPlaying ? "停止动画" : "开始动画"
            font.pixelSize: 16
            font.family: "微软雅黑"
            color: "white"
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                cursorShape = Qt.PointingHandCursor
                controlButton.scale = 1.05
            }
            onExited: {
                cursorShape = Qt.ArrowCursor
                controlButton.scale = 1.0
            }
            onClicked: {
                isPlaying = !isPlaying
                waveBarChart.toggleAnimation(isPlaying)
            }
        }

        Behavior on scale {
            NumberAnimation {
                duration: 100
                easing.type: Easing.OutCubic
            }
        }
    }

    Behavior on color {
        ColorAnimation {
            duration: 300
        }
    }
}