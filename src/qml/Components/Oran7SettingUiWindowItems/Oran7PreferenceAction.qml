import QtQuick
import "../Oran7GlobalComponents"
import "../../Settings/GlobalSettings"
import Oran7UI.Impl

// 与语言/字体选择框一致的圆角操作入口，复用自绘按钮的交互反馈。
Oran7StudioButton {
    id: control
    property string badge: "EQ"
    implicitWidth: 190
    implicitHeight: 38
    cornerRadius: 8
    leftPadding: badge.length ? 40 : 12
    rightPadding: 12
    accentColor: Oran7Theme.Oran7MainGUI.themeColor
    textColor: Oran7MainUiSetting.textColor
    surfaceColor: Oran7MainUiSetting.itemBackColor
    hoverColor: Qt.alpha(accentColor, 0.12)
    borderColor: Qt.alpha(textColor, hovered ? 0.25 : 0.10)
    font.family: Oran7MainUiSetting.fontFamily
    contentItem: Text {
        text: control.text; font: control.font; color: control.textColor
        verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
    }
    Rectangle {
        visible: control.badge.length > 0
        x: 7; anchors.verticalCenter: parent.verticalCenter
        width: 25; height: 25; radius: 6; color: Qt.alpha(control.accentColor, 0.14)
        Text { anchors.centerIn: parent; text: control.badge; color: control.textColor; font.pixelSize: 11 }
    }
}
