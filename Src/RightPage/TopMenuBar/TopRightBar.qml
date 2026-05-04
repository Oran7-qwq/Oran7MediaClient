import QtQuick 2.15
import Qt5Compat.GraphicalEffects

//rightTopMenu,迷你窗口，最小化，最大化，关闭程序
Item{
    id:rightTopMenuItem
    anchors.rightMargin: 44
    Row{
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        //迷你窗口
        Image{
            id:miniImage
            anchors.verticalCenter: parent.verticalCenter
            source: "/image/miniwindow.png"
            property string miniImageColorOverlay_Color: "#2a1a22"
            layer.enabled: true
            layer.effect: ColorOverlay{
                source: miniImage
                color: miniImage.miniImageColorOverlay_Color
            }
            MouseArea{
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    miniImage.miniImageColorOverlay_Color = "#616161"
                }
                onExited: {
                    miniImage.miniImageColorOverlay_Color = "#2a1a22"
                }
                onClicked:{
                    //----
                }
            }
        }
    }
}
