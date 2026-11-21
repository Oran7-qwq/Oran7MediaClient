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
        } else if (glyph === "window") {
            c.rect(3, 4, 18, 16)
            c.moveTo(3, 9); c.lineTo(21, 9)
            c.moveTo(6, 6.5); c.lineTo(8, 6.5)
        } else if (glyph === "image") {
            c.rect(3, 4, 18, 16)
            c.moveTo(3, 17); c.lineTo(9, 11); c.lineTo(13, 15); c.lineTo(16, 12); c.lineTo(21, 17)
            c.moveTo(17, 8); c.lineTo(17, 8.1)
        } else if (glyph === "add") {
            c.moveTo(12, 5); c.lineTo(12, 19)
            c.moveTo(5, 12); c.lineTo(19, 12)
        } else if (glyph === "up" || glyph === "down") {
            const up = glyph === "up"
            c.moveTo(6, up ? 15 : 9); c.lineTo(12, up ? 9 : 15); c.lineTo(18, up ? 15 : 9)
        } else if (glyph === "settings") {
            for (let i = 0; i < 8; ++i) {
                const angle = Math.PI * i / 4
                c.moveTo(12 + 7 * Math.cos(angle), 12 + 7 * Math.sin(angle))
                c.lineTo(12 + 9 * Math.cos(angle), 12 + 9 * Math.sin(angle))
            }
            c.moveTo(19, 12); c.arc(12, 12, 7, 0, 2 * Math.PI)
            c.moveTo(15, 12); c.arc(12, 12, 3, 0, 2 * Math.PI)
        } else if (glyph === "eye" || glyph === "eyeOff") {
            c.moveTo(2, 12); c.quadraticCurveTo(12, 0, 22, 12)
            c.quadraticCurveTo(12, 24, 2, 12)
            c.moveTo(15, 12); c.arc(12, 12, 3, 0, 2 * Math.PI)
            if (glyph === "eyeOff") {
                c.moveTo(4, 4); c.lineTo(20, 20)
            }
        } else if (glyph === "lock" || glyph === "unlock") {
            c.rect(5, 10, 14, 10)
            c.moveTo(8, glyph === "lock" ? 10 : 7); c.lineTo(8, 7)
            c.arc(12, 7, 4, Math.PI, 0); c.lineTo(16, 10)
            c.moveTo(12, 14); c.lineTo(12, 16)
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
        } else if (glyph === "fill") {
            c.rect(4, 4, 16, 16)
            c.moveTo(4, 4); c.lineTo(9, 9)
            c.moveTo(20, 4); c.lineTo(15, 9)
            c.moveTo(4, 20); c.lineTo(9, 15)
            c.moveTo(20, 20); c.lineTo(15, 15)
        } else if (glyph === "crop") {
            c.moveTo(7, 3); c.lineTo(7, 17); c.lineTo(21, 17)
            c.moveTo(3, 7); c.lineTo(17, 7); c.lineTo(17, 21)
        } else if (glyph === "mirror") {
            c.moveTo(9, 5); c.lineTo(3, 19); c.lineTo(9, 19); c.closePath()
            c.moveTo(15, 5); c.lineTo(21, 19); c.lineTo(15, 19); c.closePath()
            c.moveTo(12, 4); c.lineTo(12, 20)
        } else if (glyph === "rotateLeft" || glyph === "reset") {
            if (glyph === "rotateLeft") {
                c.arc(12, 12, 7, 2.85, 0.3)
                c.moveTo(4, 8); c.lineTo(5.3, 14); c.lineTo(10, 11.5)
            } else {
                c.arc(12, 12, 7, 0.3, 5.6)
                c.moveTo(19, 4); c.lineTo(19, 8); c.lineTo(15, 8)
            }
        } else if (glyph === "toFront" || glyph === "toBack") {
            const top = glyph === "toFront"
            const tip = top ? 8 : 16
            const bar = top ? 4 : 20
            c.moveTo(5, bar); c.lineTo(19, bar)
            c.moveTo(12, 24 - bar); c.lineTo(12, tip)
            c.moveTo(7, 12); c.lineTo(12, tip); c.lineTo(17, 12)
        } else if (glyph === "remove") {
            c.moveTo(3, 6); c.lineTo(21, 6)
            c.moveTo(8, 6); c.lineTo(9, 3); c.lineTo(15, 3); c.lineTo(16, 6)
            c.moveTo(6, 8); c.lineTo(7, 21); c.lineTo(17, 21); c.lineTo(18, 8)
            c.moveTo(10, 10); c.lineTo(10, 17)
            c.moveTo(14, 10); c.lineTo(14, 17)
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
