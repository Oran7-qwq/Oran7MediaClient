import QtQuick
import QtQuick.Window
import Client 1.0
import Oran7UI.Impl

Window {
    id: root
    objectName: "__Oran7AudioVisualizerWindow__"
    required property Oran7AudioVisualizer visualizer
    readonly property bool topmost: visualizer.settings.always_on_top
    transientParent: null
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowTransparentForInput | Qt.WindowDoesNotAcceptFocus
           | Qt.NoDropShadowWindowHint
           | (topmost ? Qt.WindowStaysOnTopHint : Qt.WindowStaysOnBottomHint)
    color: "transparent"
    visible: visualizer.showing
    function applyNativeStyle() {
        if (visible) visualizer.prepareOverlay(root)
    }
    Component.onCompleted: visualizer.placeOverlay(root)
    onVisibleChanged: applyNativeStyle()
    onTopmostChanged: if (visible) Qt.callLater(applyNativeStyle)
    Connections {
        target: root.visualizer
        function onLayoutChanged() {
            root.visualizer.placeOverlay(root)
            root.applyNativeStyle()
        }
    }
    Oran7SpectrumItem {
        anchors.fill: parent
        visualizer: root.visualizer
        barColor: Oran7Theme.Oran7MainGUI.themeColor
    }
}
