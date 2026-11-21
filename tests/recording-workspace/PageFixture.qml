import QtQuick
import QtQuick.Controls
import "../../src/qml/RightPage/MainStackView"
Window {
    id: root
    width: 1200; height: 720; visible: true
    property int navigationClicks: 0
    Button {
        id: navigation
        objectName: "fixtureNavigation"
        width: 44; height: 30
        onClicked: root.navigationClicks++
    }
    ScreenCaptureStack {
        anchors { left: navigation.right; top: parent.top; right: parent.right; bottom: parent.bottom }
        pageName: "ScreenCapturePage"
    }
}
