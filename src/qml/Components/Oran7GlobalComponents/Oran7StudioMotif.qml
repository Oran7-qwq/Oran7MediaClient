import QtQuick

// 工作台共用的信号核心：开环、显示窗口与双路出口；仅属性变化时重绘。
Canvas {
    id: root
    property color ink: "#b8c8d1"
    property color signalColor: "#7edcc2"
    property bool energized: false
    implicitWidth: 200
    implicitHeight: 200
    onInkChanged: requestPaint()
    onSignalColorChanged: requestPaint()
    onEnergizedChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        var c = getContext("2d")
        c.reset()
        c.scale(width / 200, height / 200)
        c.strokeStyle = ink
        c.fillStyle = ink
        c.lineCap = "butt"
        c.lineWidth = 22
        c.beginPath()
        c.arc(100, 100, 72, Math.PI * 0.18, Math.PI * 1.82)
        c.stroke()
        c.globalAlpha = 0.4
        c.lineWidth = 1
        c.beginPath()
        c.arc(100, 100, 91, Math.PI * 0.36, Math.PI * 1.68)
        c.stroke()
        c.globalAlpha = 1
        c.lineWidth = 6
        c.strokeRect(66, 70, 63, 45)
        c.fillRect(91, 115, 6, 14)
        c.fillRect(79, 129, 30, 5)
        c.fillStyle = energized ? signalColor : ink
        c.fillRect(141, 80, 35, 10)
        c.fillRect(141, 108, 22, 10)
        c.beginPath()
        c.moveTo(178, 76); c.lineTo(188, 85); c.lineTo(178, 94); c.closePath(); c.fill()
        c.globalAlpha = 0.5
        c.fillRect(18, 160, 23, 3)
        c.fillRect(18, 167, 13, 3)
    }
}
