import QtQuick
import QtQuick.Controls
import "../../Components/Oran7GlobalComponents"
import Client 1.0
import Oran7UI.Impl

Item {
    id: root
    property string pageName: ""
    readonly property bool isActive: StackView.status === StackView.Active

    Oran7RecordingWorkspace {
        id: workspace
        anchors.fill: parent
        service: Client
        controller: Client.screenCapture()
        previewComponent: Component {
            Oran7ScreenCaptureComponent {
                renderObject: Client.ScreenCaptureRender
                openScreenInfo: workspace.showScreenInfo
            }
        }
        onPreviewScaleChanged: function(fill) {
            Oran7Theme.saveComponentToken("Oran7ScreenCapture", "ScaleMode", fill ? "Fill" : "Fit")
            Client.setScaleMode(Client.ScreenCaptureRender, fill ? Client.Fill : Client.Fit)
        }
        Component.onCompleted: Client.setScaleMode(Client.ScreenCaptureRender, fillPreview ? Client.Fill : Client.Fit)
    }
}
