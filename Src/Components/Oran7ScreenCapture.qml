//Oran7ScreenCapture.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Client 1.0

Item {
    id: root
    width: 960
    height: 540

    // 目标录制帧率
    property int targetFps: 15
    // 默认抓主屏
    property int screenIndex: 0

    Rectangle {
        anchors.fill: parent
        color: "#111"
        radius: 8
        border.color: "#333"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10

            // ====== 顶部控制条 ======
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Label { text: "Desktop Preview"; color: "white" }

                Item { Layout.fillWidth: true }

                Label { text: "FPS"; color: "white" }
                SpinBox {
                    id: fpsBox
                    from: 1
                    to: 60
                    value: root.targetFps
                    editable: true
                    onValueModified: root.targetFps = value
                    implicitWidth: 110
                }

                Button {
                    text: "Start Preview"
                    onClicked: {
                        Client.startPreview(root.screenIndex)
                    }
                }
            }

            // ====== 预览区域 ======
            Oran7VideoRendererItem {
                id: preview
                Layout.fillWidth: true
                Layout.fillHeight: true
                renderObject:Client.ScreenCaptureRender
                openVideoInfo: false
            }
        }
    }
}
