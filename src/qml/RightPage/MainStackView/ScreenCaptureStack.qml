import QtQuick
import QtQuick.Controls
import "../../Components/Oran7GlobalComponents"
import Client 1.0
import Oran7UI.Impl

Item {
    id: root
    property string pageName: ""
    readonly property bool isActive: StackView.status === StackView.Active
    // 两套皮肤共用编辑器计算画面矩形，D3D11 直接铺满该矩形。
    Component.onCompleted: Client.setScaleMode(Client.ScreenCaptureRender, Client.Fill)

    Oran7ProcessLoadMonitor {
        id: processLoad
        active: workspace.audioMonitoringActive
    }

    Oran7RecordingWorkspace {
        id: workspace
        anchors.fill: parent
        audioMonitoringActive: root.isActive && workspace.visible && (!Window.window ||
            (Window.window.visible && Window.window.visibility !== Window.Minimized && Window.window.opacity > 0))
        service: Client
        controller: Client.screenCapture()
        processCpuPercent: processLoad.cpuPercent
        processGpuPercent: processLoad.gpuPercent
        previewComponent: Component {
            Oran7ScreenCaptureComponent {
                renderObject: Client.ScreenCaptureRender
                openScreenInfo: workspace.showScreenInfo
                infoFontFamily: workspace.fontFamily
                infoPixelSize: workspace.captionPixelSize
                sourceSizeHint: Qt.size(workspace.previewSource.width || 0, workspace.previewSource.height || 0)
            }
        }
        onPreviewScaleChanged: function(fill) {
            Oran7Theme.saveComponentToken("Oran7ScreenCapture", "ScaleMode", fill ? "Fill" : "Fit")
        }
    }
}
