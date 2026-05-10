import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

Rectangle{
    id:root
    color:"transparent"
    width: 20
    height: width
    border.width: 2
    border.color: "#24161d"
    radius: 4

    // --- define properties ---
    property color checkedColor: "#fc3c55"
    property color borderColor: "#24161d"
    property string labelText: "undefined"
    property bool labelTextEnable: true

    // --- signal ---
    signal clicked()

    // 外部控制选中状态
    property bool checked: false
    onCheckedChanged:{
        if(root.checked === true)
        {
            ep_selectImage.visible = true
            ep_selectImage.scale = 1.0
        }
        else
        {
            ep_selectImage.scale = 0.5
            ep_selectImage.visible = false
        }
    }

    Image {
        id:ep_selectImage
        source: "qrc:/image/ep_select.png"
        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter
        sourceSize.width: parent.width * 1.2
        sourceSize.height: parent.height * 1.2
        visible: false
        mipmap: true
        antialiasing: true
        cache:true
        asynchronous:false
        scale:0.5
        Behavior on scale {
            NumberAnimation{
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
        layer.enabled: true
        layer.effect: ColorOverlay{
            source: ep_selectImage
            color:root.checkedColor
        }
    }
    MouseArea{
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
                root.clicked()
        }
    }
    Label{
        anchors.left: root.right
        anchors.leftMargin: 4
        anchors.verticalCenter: root.verticalCenter

        visible: root.labelTextEnable
        font.pixelSize: 16
        font.family: "微软雅黑"
        font.italic: true
        text: root.labelText
        color:"#24161d"
    }
}
