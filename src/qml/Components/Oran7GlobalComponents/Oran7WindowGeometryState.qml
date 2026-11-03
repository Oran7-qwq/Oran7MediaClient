import QtQuick
import QtQuick.Window

// 主窗唯一的普通窗口几何快照；不经设置样式单例中转。
QtObject {
    id: root
    required property Window targetWindow
    property bool active: false
    property rect normalGeometry: Qt.rect(0, 0, 0, 0)
    function initialize() {
        normalGeometry = Qt.rect(targetWindow.x, targetWindow.y, targetWindow.width, targetWindow.height)
    }
    function capture() {
        if (active && targetWindow.visibility === Window.Windowed)
            normalGeometry = Qt.rect(targetWindow.x, targetWindow.y, targetWindow.width, targetWindow.height)
    }
    property Timer settle: Timer {
        interval: 200
        onTriggered: root.capture()
    }
    property Connections changes: Connections {
        target: root.targetWindow
        function onWidthChanged() { if (root.active) root.settle.restart() }
        function onHeightChanged() { if (root.active) root.settle.restart() }
        function onXChanged() { if (root.active) root.settle.restart() }
        function onYChanged() { if (root.active) root.settle.restart() }
        function onVisibilityChanged() { if (root.active) root.settle.restart() }
        function onClosing() { root.capture() }
    }
}
