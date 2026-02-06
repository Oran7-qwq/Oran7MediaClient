import QtQuick 2.15
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import "../../Basic"

Popup{
    id:loginPopup
    Connections
    {
        target:BasicConfig
        function onOpenLoginPopup(){
            loginPopup.open()
        }
    }

    closePolicy: Popup.NoAutoClose
    background: Rectangle{
        id:loginPopupBackgroundRectangle
        anchors.fill: parent
        color: "#fef2e8"
        radius: 10
        border.color: "#3a3a3d"
        border.width: 2
        //右上角CloseIcon
        Image {
            id: loginPopupCloseImag
            source: "qrc:/image/close.png"
            scale: 1.5
            anchors.right: parent.right
            anchors.rightMargin: 30
            anchors.top: parent.top
            anchors.topMargin: 25
            layer.enabled:false
            layer.effect: ColorOverlay{
                source: loginPopupCloseImag
                color:"white"
            }
            MouseArea{
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    loginPopupCloseImag.layer.enabled =true
                    cursorShape = Qt.PointingHandCursor
                }
                onExited: {
                    loginPopupCloseImag.layer.enabled = false
                    cursorShape = Qt.ArrowCursor
                }
                onClicked: {
                    loginPopup.close()
                }
            }
        }
        //标题
        Label{
            anchors.top: parent.top
            anchors.topMargin: 80
            anchors.horizontalCenter: parent.horizontalCenter
            text:"扫码登录"
            font.pixelSize: 20
            font.family: "微软雅黑"
            color:"#2a1a22"
            font.bold: true
        }
        //鼠标动画观测区域
        Rectangle{
            id:loginPopupOnlookerRectangle
            width: parent.width-50
            height: useAppImage.height
            x:useAppImage.x
            y:useAppImage.y
            color:"transparent"
            MouseArea{
                propagateComposedEvents: true
                anchors.fill:parent
                hoverEnabled: true
                onEntered:(mouse)=> {
                    loginPopupParallelAnimation1.start()
                    // mouse.accepted = false
                }
                onExited: (mouse)=>{
                    loginPopupParallelAnimation2.start()
                }
            }
        }
        ParallelAnimation{
            id:loginPopupParallelAnimation1
            NumberAnimation{
                target: scanImage
                property: "x"
                duration: 200
                from: scanImage.x
                to:loginPopupBackgroundRectangle.x+95
            }
            NumberAnimation{
                target:scanImage
                property: "height"
                duration: 200
                from:scanImage.height
                to:200
            }
            NumberAnimation{
                target: scanImage
                property: "width"
                duration: 200
                from: scanImage.width
                to:200
            }
            PropertyAnimation{
                target: useAppImage
                property: "opacity"
                from: 1
                to:0
                duration: 200
            }
        }
        ParallelAnimation{
            id:loginPopupParallelAnimation2
            NumberAnimation{
                target: scanImage
                property: "x"
                duration: 200
                from: scanImage.x
                to:loginPopupBackgroundRectangle.x+190
            }
            NumberAnimation{
                target:scanImage
                property: "height"
                duration: 200
                from:scanImage.height
                to:140
            }
            NumberAnimation{
                target: scanImage
                property: "width"
                duration: 200
                from: scanImage.width
                to:140
            }
            PropertyAnimation{
                target: useAppImage
                property: "opacity"
                from: 0
                to:1
                duration: 200
            }
        }

        //App扫码方法Image
        BorderImage {
            id:useAppImage
            source: "qrc:/image/useApp.png"
            anchors.verticalCenter: parent.verticalCenter
            x:parent.x+40
            width: 140; height: 260
            border.left: 10; border.top: 10
            border.right: 10; border.bottom: 10
        }
        //二维码
        Image{
            id:scanImage
            width: 140
            height: 140
            source:"qrc:/image/scan.png"
            anchors.top: useAppImage.top
            x:parent.x+190
        }
        //“使用网易云App扫码登录”
        Label{
            width: scanImage.width
            anchors.top: scanImage.bottom
            anchors.topMargin: 20
            anchors.left: scanImage.left
            anchors.leftMargin: 4
            Flow{
                anchors.fill: parent
                spacing: 0
                Repeater{
                    model:{"使用 Oran柒云音乐App 扫码登录"}
                    delegate: Text{
                        text: modelData
                        font.pixelSize: 14
                        color: index >=3 && index<=14 ? "#5e7cbd" : "#bbbbbe"
                    }
                }
            }
        }
        //"选择其它登录方式>"
        Label{
            id:otherLoginWaysLable
            text: "选择其它登录方式>"
            color:"#2a1a22"
            font.family: "微软雅黑"
            font.bold: true
            font.pixelSize: 15
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 60
            layer.enabled: false
            layer.effect:ColorOverlay{
                source: otherLoginWaysLable
                color: "#8d8d92"
            }
            MouseArea{
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    otherLoginWaysLable.layer.enabled = true
                    cursorShape = Qt.PointingHandCursor
                }
                onExited: {
                    otherLoginWaysLable.layer.enabled = false
                    cursorShape = Qt.ArrowCursor
                }
                onClicked: {
                    BasicConfig.openMainLoginPopup()
                    loginPopup.close()
                }
            }
        }
    }
}
