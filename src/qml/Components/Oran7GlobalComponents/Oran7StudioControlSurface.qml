import QtQuick

// 紫色皮肤的控件表皮；关闭 polished 时与普通 Rectangle 一致。
Rectangle {
    id: root
    property bool polished: false
    property color tintColor: "#b6a3e6"
    property bool highlighted: false
    property bool depressed: false
    Item {
        anchors.fill: parent; anchors.margins: 2
        visible: root.polished
        Image {
            anchors.fill: parent
            source: root.polished ? Qt.resolvedUrl("../../../../image/oran7_studio_grain.png") : ""
            fillMode: Image.Tile; smooth: false; opacity: 0.008
        }
    }
}
