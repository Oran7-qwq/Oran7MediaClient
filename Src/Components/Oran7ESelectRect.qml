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

    // 外部控制选中状态
    property bool checked: false
    property string labelText: "undefined"

    signal clicked()

    Image {
        id:ep_selectImage
        source: "qrc:/image/ep_select.png"
        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter
        sourceSize.width: parent.width * 1.2
        sourceSize.height: parent.height * 1.2
        visible: root.checked
        layer.enabled: true
        mipmap: true
        antialiasing: true
        cache:true
        asynchronous:false
        layer.effect: ColorOverlay{
            source: ep_selectImage
            color:"#fc3c55"
        }
    }
    MouseArea{
        anchors.fill: parent
        onClicked: {
                root.clicked()
        }
    }
    Label{
        anchors.left: root.right
        anchors.leftMargin: 4
        anchors.verticalCenter: root.verticalCenter
        font.pixelSize: 16
        font.family: "微软雅黑"
        font.italic: true
        text: root.labelText
        color:"#24161d"
    }
}
