import QtQuick
import "../../src/qml/RightPage/MainStackView"
Window {
    width: 1200; height: 720; visible: true
    ScreenCaptureStack { anchors.fill: parent; pageName: "ScreenCapturePage" }
}
