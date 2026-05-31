import QtQuick
import QtQuick.Window
import Qt5Compat.GraphicalEffects

Item {
    id: root

    property url source
    property real radius: 7
    property bool async: true
    readonly property real dpr: Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1

    Image {
        id: img
        anchors.fill: parent
        source: root.source
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(root.width * root.dpr, root.height * root.dpr)
        asynchronous: root.async
        smooth: true
        mipmap: true
        visible: false
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        radius: root.radius
        color: "white"
        antialiasing: true
        visible: false
    }

    OpacityMask {
        anchors.fill: parent
        source: img
        maskSource: mask
        cached: false
    }
}
