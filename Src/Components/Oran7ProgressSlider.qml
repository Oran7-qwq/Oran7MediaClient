import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import "../Basic"
import Client 1.0

Rectangle{
    id:root
    width:parent.width * 0.8
    height: 10
    color:"transparent"
    visible: true

    property var sliderColor_themeItems : [
        {default_Color: "#00DDDD",sel_Color: "cyan"},
        {default_Color: "#c74054",sel_Color: "#fc3c55"}
    ]
    property int sliderColor_themeIndex: 1

    property int progressHandleWidth: 12
    property color progressHandleColor: "white"

    property real visibleProgressX: 0.0
    Behavior on visibleProgressX{
        NumberAnimation{
            duration:   200 //10ms
            easing.type: Easing.OutCubic
        }
    }
    property real progressHandleX: 0-progressHandle.width/2
    Behavior on progressHandleX {
        NumberAnimation{
            duration:200
            easing.type:Easing.OutCubic
        }
    }
    property string visibleColor:root.sliderColor_themeItems[root.sliderColor_themeIndex].default_Color

    property int allSecondTime:0          //单位s
    property int nowSecondTime: 0     //单位s

    property string nowTimeText: "00:00"
    property string allTimeText: "00:00"

    property int focusedPlayer: -1

    //Mouse
    property bool isPressed: false
    property bool isInOutSide: true

    Slider{
        id:progressSlier
        anchors.verticalCenter: parent.verticalCenter
        from: 0
        to: BasicConfig.max_Slider_Value
        height: root.height
        width: root.width
        handle:Rectangle{
            id:progressHandle
            width:root.progressHandleWidth
            height: width
            radius: width/2
            color: root.progressHandleColor
            visible: false
            anchors.verticalCenter: parent.verticalCenter
            x:root.progressHandleX
        }
        background: Rectangle{
            id:progressBackgroundRectangle
            anchors.fill: parent
            radius: progressSlier.height/2
            color: "#4d4d56"
            clip: true
            Rectangle{
                id:visiableRectangle
                height:parent.height
                width: root.visibleProgressX
                color: root.visibleColor
                radius: width/2
                anchors.left: progressBackgroundRectangle.left
                anchors.top: progressBackgroundRectangle.top
            }
        }
    }
    MouseArea{
        id:progressRectanleMouseArea
        anchors.fill: parent
        hoverEnabled: true
        onEntered: {
            progressHandle.visible = true
            root.isInOutSide=false
            root.visibleColor=root.sliderColor_themeItems[root.sliderColor_themeIndex].sel_Color
        }
        onExited: {
            if(root.isPressed===false)
            {
                progressHandle.visible = false
                root.visibleColor=root.sliderColor_themeItems[root.sliderColor_themeIndex].default_Color
            }
            root.isInOutSide=true
        }
        onPressed: (mouse)=>{
            if(root.allSecondTime !== 0)
                root.progressHandleX=mouse.x
            root.isPressed=true
        }
        onReleased: {
            root.isPressed=false
            if(root.isInOutSide===true)
            {
                progressHandle.visible = false
                root.visibleColor=root.sliderColor_themeItems[root.sliderColor_themeIndex].default_Color
            }
            if(BasicConfig.globalPlayingFocus === root.focusedPlayer)
                Client.progressSlider_Seek(root.nowSecondTime)
        }
        onMouseXChanged: (mouse)=>{
            if(root.isPressed===true)
            {
                 if(mouse.x<=root.width&&mouse.x>=0)
                 {
                     root.progressHandleX = mouse.x - progressHandle.width/2
                     root.visibleProgressX =mouse.x
                     root.nowSecondTime=root.allSecondTime*(mouse.x/root.width)
                     root.nowTimeText = String(Math.floor(Math.floor(root.nowSecondTime/60)/10))+String(Math.floor(root.nowSecondTime/60)%10)+": "
                                     +String(Math.floor((root.nowSecondTime%60/10)))+String((root.nowSecondTime%60%10))
                 }
                if(mouse.x<0)
                {
                    root.progressHandleX=0-progressHandle.width/2
                    root.visibleProgressX =0
                    root.nowSecondTime=0
                    root.nowTimeText = String(Math.floor(Math.floor(root.nowSecondTime/60)/10))+String(Math.floor(root.nowSecondTime/60)%10)+": "
                                     +String(Math.floor((root.nowSecondTime%60/10)))+String((root.nowSecondTime%60%10))
                }
                if(mouse.x>root.width)
                {
                    root.progressHandleX=root.width-progressHandle.width/2
                    root.visibleProgressX =root.width
                    root.nowTimeText =String(Math.floor(Math.floor(root.allSecondTime/60)/10))+String(Math.floor(root.allSecondTime/60)%10)+": "
                                     +String(Math.floor((root.allSecondTime%60/10)))+String((root.allSecondTime%60%10))
                }
            }
        }
    }
}
