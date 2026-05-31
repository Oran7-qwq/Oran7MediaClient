import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import "../Basic"
import Client 1.0

Item{
    id:root
    width: volumeIamge.width
    height:volumeIamge.height

    Image {
        id: volumeIamge
        source: "qrc:/image/volume.png"

        anchors.verticalCenter: root.verticalCenter
        anchors.horizontalCenter: root.horizontalCenter

        // 状态管理
        property bool hovered: false
        property bool popupHovered: false

        layer.enabled: false
        layer.effect: ColorOverlay{
            source: volumeIamge
            color: "white"
        }
        MouseArea{
            id:volumeImageMouseArea
            anchors.fill: parent
            hoverEnabled:true
            onEntered: {
                 volumeIamge.layer.enabled=true
                 volumeIamge.hovered=true
                 volumeSliderPopup.open()
            }
            onExited: {
                volumeIamge.layer.enabled=false
                volumeIamge_exitTimer.restart()
            }
        }
        Timer{
            id:volumeIamge_exitTimer
            interval: 80
            onTriggered: {
                volumeIamge.hovered=false
                if(volumeIamge.popupHovered === false && !volumeImageMouseArea.containsMouse)
                {
                    volumeSliderPopup.close()
                }
            }
        }
    }

    //音量调节Popup
    Popup{
        id:volumeSliderPopup
        modal: false
        focus:false
        closePolicy: Popup.NoAutoClose
        padding: 0

        y:volumeIamge.y - popupBackground.height - 2
        x:volumeIamge.x - (popupBackground.width - volumeIamge.width)/2

        background: Rectangle{
            id: popupBackground
            width: 30
            height: 120
            color: "#fef2e8"
            radius: 5

            layer.enabled: true
            layer.effect: DropShadow{
                anchors.fill: popupBackground
                source: popupBackground
                samples: 35
                spread: 0.6
                color:"#40000000"
                radius: 6
                horizontalOffset: 1
                verticalOffset: 1
            }

            //MouseArea
            MouseArea{
                id:volumeSliderPopupMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    volumeIamge.popupHovered=true
                }
                onExited: {
                    volumeSliderPopup_exitTimer.restart()
                }
            }

            // 音量滑块
            Slider {
                id: volumeSlider
                anchors.top: parent.top
                anchors.topMargin: 10
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 22

                visible: true
                from: 0
                to: 100
                value: 50

                // 确保Slider本身不会拦截鼠标悬停事件
                hoverEnabled: false

                orientation: Qt.Vertical

                property real volumeSlider_progressValue: BasicConfig.playerVolume
                Behavior on volumeSlider_progressValue{
                    NumberAnimation{
                        duration:200
                        easing.type:Easing.OutCubic
                    }
                }
                property real volumeSliderVisibleRectangle_posY: (volumeSlider.volumeSlider_progressValue / 100 ) * volumeSlider.height
                                                                 - volumeSliderHandleRectangle.width/2
                background: Rectangle{
                    id:volumeSliderBackgroundRectangle
                    implicitWidth: 7
                    implicitHeight: volumeSlider.availableHeight
                    width: implicitWidth
                    radius: implicitWidth/2
                    anchors.horizontalCenter: parent.horizontalCenter
                    color:"#aeaeae"
                    Rectangle{
                        id:volumeSliderVisibleRectangle
                        width: volumeSliderBackgroundRectangle.width
                        anchors.bottom: parent.bottom
                        height: volumeSlider.volumeSliderVisibleRectangle_posY
                        color: "#ff7384"
                        radius: width/2
                    }
                }
                property real volumeSliderHandle_posY: volumeSliderBackgroundRectangle.height - volumeSliderHandleRectangle.width/2 + 2
                                                                                - (volumeSlider.volumeSlider_progressValue / 100 ) * volumeSlider.height
                handle: Rectangle{
                    id:volumeSliderHandleRectangle
                    width: volumeSliderBackgroundRectangle.width + 6
                    height: width
                    radius: width/2
                    anchors.horizontalCenter: volumeSliderBackgroundRectangle.horizontalCenter
                    color:"#f35476"

                    y:volumeSlider.volumeSliderHandle_posY
                }
            }
            //volumeSliderMouseArea   but  anchors.fill:popupBackground
            MouseArea{
                id:volumeSliderMouseArea
                property bool volumeSliderMouseArea_isPressed: false
                anchors.fill: parent
                onPressed:{
                    volumeSliderMouseArea.volumeSliderMouseArea_isPressed=true
                }
                onReleased: {
                    volumeSliderMouseArea.volumeSliderMouseArea_isPressed=false
                }
                onPositionChanged: (mouse) =>{
                    if(volumeSliderMouseArea.volumeSliderMouseArea_isPressed === false)return ;

                    var pos = mapToItem(volumeSlider,mouse.x, mouse.y)
                    var posY=pos.y
                    var progressRealValue=100 - (posY / volumeSlider.height) * 100

                    if(progressRealValue > 100 || progressRealValue < 0)return ;

                    //console.log(progressRealValue)
                    //volumeSlider.volumeSlider_progressValue = progressRealValue
                    BasicConfig.playerVolume = progressRealValue
                    Client.setVolume(Math.floor(progressRealValue))

                    volumeValue.text = Math.floor(progressRealValue) + "%"
                }
            }

            //volumeValue Label
            Label{
                id:volumeValue
                anchors.top:volumeSlider.bottom
                anchors.topMargin: 7
                anchors.horizontalCenter: volumeSlider.horizontalCenter
                font.pixelSize: 11
                font.bold: true
                color:"#2a1a22"
                text:Math.floor(volumeSlider.volumeSlider_progressValue) + "%"
            }

            Connections{
                target:Client
                function onPlayerVolumeConfigLoaded(value)
                {
                    BasicConfig.playerVolume = value
                }
            }
        }

        //Popup exit Timer
        Timer{
            id:volumeSliderPopup_exitTimer
            interval: 80
            onTriggered: {
                volumeIamge.popupHovered=false
                if(volumeIamge.hovered===false && !volumeSliderPopupMouseArea.containsMouse)
                {
                    volumeSliderPopup.close()
                }
            }
        }
        //enter Animation
        enter: Transition {
            NumberAnimation {
                property: "opacity";
                from: 0.0;
                to: 1.0;
                duration: 100
                easing.type:Easing.OutCubic
            }
        }
        //exit Aniamtion
        exit: Transition {
            NumberAnimation {
                property: "opacity";
                from: 1.0;
                to: 0.0;
                duration: 100
                easing.type:Easing.OutCubic
            }
        }
    }
}
