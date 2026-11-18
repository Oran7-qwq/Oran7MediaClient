import QtQuick
import QtQuick.Controls

// 工作台通用按钮，颜色由工作台传入，不借用下拉设置窗口样式。
Button {
    id: root
    property color accentColor: "#80a9db"
    property color textColor: "#e7edf5"
    property color surfaceColor: "#242d39"
    property color hoverColor: "#2b394c"
    property color borderColor: "#334154"
    property color dangerColor: "#ed8795"
    property bool polished: false
    property color primaryTextColor: polished ? "#ffffff" : "#171a20"
    property bool primary: false
    property bool destructive: false
    property string symbol: ""
    property real cornerRadius: 7
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
        color: root.primary ? root.primaryTextColor : root.destructive ? root.dangerColor : root.checked ? root.accentColor : root.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Oran7StudioControlSurface {
        polished: root.polished; tintColor: root.accentColor
        highlighted: root.hovered || root.checked || root.activeFocus; depressed: root.down
        radius: root.cornerRadius
        color: root.primary ? (root.down ? Qt.darker(root.accentColor, root.polished ? 1.06 : 1.12) : root.hovered ? Qt.lighter(root.accentColor, root.polished ? 1.025 : 1.08) : root.accentColor)
            : root.checked ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.14) : root.down ? Qt.darker(root.hoverColor, 1.12) : root.hovered ? root.hoverColor : root.surfaceColor
        border.width: 1
        border.color: root.activeFocus || root.checked ? root.accentColor : root.primary ? "transparent" : root.borderColor
        Behavior on color { ColorAnimation { duration: root.polished ? 170 : 100 } }
    }
}
