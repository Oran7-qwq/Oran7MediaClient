import QtQuick
import "../Oran7SettingUiWindowItems"

// 工作台独立表皮；默认皮肤直接委托原开关，配置提交仍由外部处理。
Item {
    id: root
    property bool polished: false
    property bool checked: false
    property color onColor: "#b6a3e6"
    property color offColor: "#35303f"
    property color borderColor: "#665b7b"
    property color thumbColor: "#ede7f6"
    property real controlHeight: 20
    signal switchToggleChanged(bool checked)

    anchors.right: parent.right
    anchors.rightMargin: 4
    anchors.verticalCenter: parent.verticalCenter
    width: height * 2
    height: controlHeight
    activeFocusOnTab: polished && enabled
    opacity: polished && !enabled ? 0.45 : 1

    function toggle() {
        if (!enabled) return
        checked = !checked
        switchToggleChanged(checked)
    }
    Keys.onSpacePressed: event => { if (polished) { toggle(); event.accepted = true } }
    Accessible.role: Accessible.CheckBox
    Accessible.checked: checked
    Accessible.onToggleAction: toggle()

    Loader {
        anchors.fill: parent
        active: !root.polished
        sourceComponent: Component {
            Oran7SwitchToggleItem {
                id: legacy
                anchors.rightMargin: 0
                onColor: root.onColor; offColor: root.offColor
                borderColor: root.borderColor; thumbColor: root.thumbColor
                controlHeight: root.controlHeight
                Binding { target: legacy; property: "checked"; value: root.checked }
                onSwitchToggleChanged: function(checked) {
                    root.checked = checked
                    root.switchToggleChanged(checked)
                }
            }
        }
    }
    Rectangle {
        anchors.fill: parent
        visible: root.polished
        radius: height / 2
        color: root.checked ? Qt.rgba(root.onColor.r, root.onColor.g, root.onColor.b, pointer.containsMouse ? 0.4 : 0.32)
            : pointer.containsMouse ? Qt.lighter(root.offColor, 1.12) : root.offColor
        border.color: root.activeFocus ? root.onColor : root.borderColor
        Behavior on color { ColorAnimation { duration: 180 } }
        Behavior on border.color { ColorAnimation { duration: 180 } }
        Rectangle {
            id: thumb
            width: root.height - 6; height: width; radius: width / 2
            x: root.checked ? root.width - width - 3 : 3
            y: 3
            color: root.checked ? root.thumbColor : Qt.darker(root.thumbColor, 1.18)
            scale: pointer.pressed ? 0.93 : 1
            Behavior on x { NumberAnimation { duration: 230; easing.type: Easing.OutCubic } }
            Behavior on color { ColorAnimation { duration: 180 } }
            Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
        }
    }
    MouseArea {
        id: pointer
        anchors.fill: parent
        visible: root.polished
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggle()
    }
}
