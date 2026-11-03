import QtQuick
import QtQuick.Window
import "../../src/qml/Components/Oran7GlobalComponents"

Window {
    id: root
    objectName: "__Oran7Window__"
    width: 640; height: 420
    visible: false
    property bool startupPageReady: false
    property bool restored: false
    Rectangle { anchors.fill: parent; color: "#dce8e0"; Text { anchors.centerIn: parent; text: "Ready fixture" } }
    Rectangle {
        width: root.width; height: root.height
        visible: !Oran7Startup.revealMain
        color: "#171a22"
    }
}
