import QtQuick

// 工作台线条图标：随控件颜色重绘，不依赖图片字体或额外图形效果。
Canvas {
    id: root
    property string glyph: "monitor"
    property color ink: "#e1e5ed"
    implicitWidth: 20
    implicitHeight: 20
    onGlyphChanged: requestPaint()
    onInkChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        var c = getContext("2d")
        c.reset()
        c.scale(width / 24, height / 24)
        c.strokeStyle = ink
        c.fillStyle = ink
        c.lineWidth = 1.6
        c.lineCap = "round"
        c.lineJoin = "round"
        c.beginPath()
        if (glyph === "monitor") {
            c.rect(3, 4, 18, 13)
            c.moveTo(12, 17); c.lineTo(12, 21)
            c.moveTo(8, 21); c.lineTo(16, 21)
        } else if (glyph === "speaker") {
            c.moveTo(3, 9); c.lineTo(7, 9); c.lineTo(12, 5)
            c.lineTo(12, 19); c.lineTo(7, 15); c.lineTo(3, 15); c.closePath()
            c.moveTo(16, 8); c.quadraticCurveTo(20, 12, 16, 16)
            c.moveTo(19, 5); c.quadraticCurveTo(25, 12, 19, 19)
        } else if (glyph === "microphone") {
            c.moveTo(9, 11); c.lineTo(9, 6); c.arc(12, 6, 3, Math.PI, 0)
            c.lineTo(15, 11); c.arc(12, 11, 3, 0, Math.PI)
            c.moveTo(6, 10); c.lineTo(6, 11); c.arc(12, 11, 6, Math.PI, 0, true)
            c.moveTo(12, 17); c.lineTo(12, 21)
            c.moveTo(9, 21); c.lineTo(15, 21)
        } else if (glyph === "folder") {
            c.moveTo(3, 8); c.lineTo(3, 5); c.lineTo(9, 5); c.lineTo(11, 8)
            c.lineTo(21, 8); c.lineTo(19, 19); c.lineTo(3, 19); c.closePath()
            c.moveTo(3, 9); c.lineTo(18, 9)
        } else if (glyph === "info") {
            c.arc(12, 12, 8, 0, 2 * Math.PI)
            c.moveTo(12, 11); c.lineTo(12, 16)
            c.moveTo(12, 7.5); c.lineTo(12, 7.6)
        } else if (glyph === "fit") {
            c.moveTo(8, 4); c.lineTo(4, 4); c.lineTo(4, 8)
            c.moveTo(16, 4); c.lineTo(20, 4); c.lineTo(20, 8)
            c.moveTo(4, 16); c.lineTo(4, 20); c.lineTo(8, 20)
            c.moveTo(20, 16); c.lineTo(20, 20); c.lineTo(16, 20)
            c.rect(8, 8, 8, 8)
        } else if (glyph === "stop") {
            c.rect(6, 6, 12, 12)
            c.fill()
        } else {
            c.arc(12, 12, 6, 0, 2 * Math.PI)
            c.fill()
        }
        c.stroke()
    }
}
