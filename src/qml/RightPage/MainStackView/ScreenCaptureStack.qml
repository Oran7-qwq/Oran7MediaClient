import QtQuick
import QtQuick.Controls
import "../../Components"
import Client 1.0

import Oran7UI.Impl

Item {
    id:root
    property string pageName: ""

    // ==================== StackView 活动状态检测 ====================//
    property bool isActive: false
    property var _myStackView: null
    property var stackView_rootItemContainer: root //包含此组件的外部作为stackViewItem的根项

    function findAndSetupStackView() {
        if (StackView.view) {
            _myStackView = StackView.view;
        } else {
            var parent = container.parent;
            var depth = 0;
            while (parent && depth < 20) {
                if (typeof parent.push === "function" && typeof parent.pop === "function") {
                    _myStackView = parent;
                    break;
                }
                parent = parent.parent;
                depth++;
            }
        }

        if (_myStackView) {
            _myStackView.currentItemChanged.connect(updateActiveState);
            updateActiveState();
        }
    }

    function updateActiveState() {
        if (!_myStackView || !_myStackView.currentItem) {
            root.isActive = false;
            return;
        }

        root.isActive = (_myStackView.currentItem === root.stackView_rootItemContainer);
        if (root.isActive) {
            //Page被激活 -> touch 业务
            root.activeResponseTask()
        }
        else
        {
            root.activeCanceledTask()
        }
    }

    //Page被激活 -> touch 业务
    function activeResponseTask(){
        // if(!sceenCaptureComponent.firstOpen){
        //     sceenCaptureComponent.screenCapture.start()
        //     //console.log("start preview")
        // }
    }

    //Page取消活跃业务
    function activeCanceledTask(){
        // if(!sceenCaptureComponent.firstOpen){
        //     sceenCaptureComponent.screenCapture.stop()
        //     //console.log("stop preview")
        //     Client.renderBlackFrame()
        // }
    }

    Component.onCompleted: {
        Qt.callLater(findAndSetupStackView);
    }

    Component.onDestruction: {
        if (_myStackView) {
            _myStackView.currentItemChanged.disconnect(updateActiveState);
        }
    }
    //================================================

    // =============================
    Oran7ScreenCaptureComponent {
        id:sceenCaptureComponent
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: screenCaptureControllers.top
        renderObject: Client.ScreenCaptureRender
        openScreenInfo: true
    }
    Rectangle{
        id:screenCaptureControllers
        anchors.left: parent.left
        anchors.right: parent.right
        height:50
        anchors.bottom: parent.bottom
        color:"transparent"
        Row{
            anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10
            Rectangle{
                width: 100
                height: 40
                color:"transparent"
                border.width: 2
                border.color: "white"
                radius: 7
                Label{
                    text:"开始录制"
                    font.pixelSize: 20
                    font.bold: true
                    anchors.centerIn: parent
                    color:Oran7Theme.Oran7MainGUI.themeColor
                }
                MouseArea{
                    anchors.fill: parent
                    hoverEnabled: true
                    onPressed: parent.scale = 0.95
                    onReleased: parent.scale = 1
                    onEntered: parent.scale = 1.05
                    onExited: parent.scale = 1
                    onClicked: sceenCaptureComponent.screenCapture.startRecording()
                }
            }
            Rectangle{
                width: 100
                height: 40
                color:"transparent"
                border.width: 2
                border.color: "white"
                radius: 7
                Label{
                    text:"停止录制"
                    font.pixelSize: parent.height/2
                    font.bold: true
                    anchors.centerIn: parent
                    color:Oran7Theme.Oran7MainGUI.themeColor
                }
                MouseArea{
                    anchors.fill: parent
                    hoverEnabled: true
                    onPressed: parent.scale = 0.95
                    onReleased: parent.scale = 1
                    onEntered: parent.scale = 1.05
                    onExited: parent.scale = 1
                    onClicked: sceenCaptureComponent.screenCapture.stopRecording()
                }
            }
        }
    }
}
