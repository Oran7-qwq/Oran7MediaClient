import QtQuick
import QtQuick.Controls

// 工作台通用按钮，颜色由工作台传入，不借用下拉设置窗口样式。
Button {
    id: root
    property color accentColor: "#beb7ff"
    property color textColor: "#e1e5ed"
    property color surfaceColor: "#292d36"
    property color hoverColor: "#363b47"
    property color borderColor: "#424650"
    property color dangerColor: "#e36a75"
    property bool primary: false
    property bool destructive: false
    property string symbol: ""
    implicitHeight: 36
    implicitWidth: Math.max(72, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: 14; rightPadding: 14
    topPadding: 6; bottomPadding: 6
    font.pixelSize: 13
    font.bold: primary
    opacity: enabled ? 1 : 0.45
    contentItem: Text {
        text: (root.symbol.length ? root.symbol + "  " : "") + root.text
        font: root.font
        color: root.primary ? "#171a20" : root.destructive ? root.dangerColor : root.checked ? root.accentColor : root.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 7
        color: root.primary ? (root.down ? Qt.darker(root.accentColor, 1.12) : root.hovered ? Qt.lighter(root.accentColor, 1.08) : root.accentColor)
            : root.checked ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.14) : root.down ? Qt.darker(root.hoverColor, 1.12) : root.hovered ? root.hoverColor : root.surfaceColor
        border.width: 1
        border.color: root.activeFocus || root.checked ? root.accentColor : root.primary ? "transparent" : root.borderColor
        Behavior on color { ColorAnimation { duration: 100 } }
    }
}
