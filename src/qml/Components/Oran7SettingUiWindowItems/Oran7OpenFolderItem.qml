import QtQuick
import QtQuick.Dialogs
import Qt5Compat.GraphicalEffects

import "../../Settings/GlobalSettings"
import "../"

Item{
    id:root

    //默认布局
    anchors.right: parent.right
    anchors.rightMargin: 6
    anchors.verticalCenter: parent.verticalCenter

    width: Oran7MainUiSetting.itemHeight * 0.833
    height: Oran7MainUiSetting.itemHeight * 0.833

    property color buttonColor: Oran7MainUiSetting.themeColor
    //文件夹对话框每次选择文件是否清空上次的选择缓存
    property bool fileDialog_selectReset: true
    //选择文件后保存的文件
    property alias filesArray: oran7FileDialog.filesArray

    //是否选择多个文件
    property bool isMultiSelect: false

    signal ready
    Image {
        id: buttonImage
        source: "qrc:/image/formkit_file.png"
        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter
        width:root.width
        height:root.height
        scale: 1

        Behavior on scale {
            NumberAnimation {
                duration: 50
                easing.type: Easing.OutCubic
            }
        }

        asynchronous: false
        cache: true
        mipmap: false
        antialiasing: true

        layer.enabled: true
        layer.effect: ColorOverlay {
            source: buttonImage
            color: Oran7MainUiSetting.themeColor
        }
    }
    Oran7FileDialog {
        id: oran7FileDialog
        selectReset: root.fileDialog_selectReset
        fileMode: root.isMultiSelect ? FileDialog.OpenFiles : FileDialog.OpenFile
        onReady: {
            root.ready();
        }
    }
    MouseArea {
        anchors.fill: parent
        onPressed: {
            buttonImage.scale = 0.95
        }
        onReleased: {
            buttonImage.scale = 1
        }
        onClicked: oran7FileDialog.open()

        hoverEnabled: true
        onEntered: {
            buttonImage.scale = 1.05
        }
        onExited: {
            buttonImage.scale = 1
        }
    }
}
