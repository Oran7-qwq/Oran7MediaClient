import QtQuick
import QtQuick.Window
import "../../src/qml/Components/Oran7GlobalComponents"

Window {
    width: 320; height: 200; visible: true
    color: "#663399"
    Oran7MainWindowBackground {
        objectName: "background"
        width: parent.width; height: parent.height
        mode: "Video"
        nativeReady: true
        imageUrl: fixtureImage
    }
}
