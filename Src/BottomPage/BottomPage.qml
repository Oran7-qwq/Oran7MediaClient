import QtQuick 2.15
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import Client 1.0
import "../Basic"

Rectangle{
    id:root

    property string musicName: "Oran7MusicPlayer期待你的播放~"
    property string singerName: "Author-Oran7ヾ(≧ ▽ ≦)ゝ"
    //唱片

    property real  visibleOpacity: 0.0
    onOpacityChanged: {
        if(visibleOpacity === 0.0)
            root.visible = false
        else
            root.visible = true
    }

    clip: true
    
    Rectangle{
        id:musicIconRectangle
        anchors.left: parent.left
        anchors.leftMargin: 30
        anchors.verticalCenter: parent.verticalCenter
        visible: root.visible
        width: 50
        height: 50
        radius: 7
        gradient: Gradient{
            orientation: Gradient.Vertical
            GradientStop{color: "#040404";position:0.0}
            GradientStop{color: "#303030";position: 0.5}
            GradientStop{color: "#040404";position: 1.0}
        }
    }
    OpacityMask {
        anchors.fill:musicIconRectangle
        source: Image {
            visible: root.visible
            id: musicIconImage
            source: "qrc:/image/transparent.png"
            anchors.centerIn: parent
            property real targetwidth :musicIconRectangle.width
            property real targetheight:musicIconRectangle.height
            onStatusChanged: {
                if(musicIconImage.status==Image.Ready)
                {
                    var scale=calculateScale(implicitWidth,implicitHeight)
                    musicIconImage.scale=scale*0.99
                }
            }
            function calculateScale(originalWidth,originalHeight)
            {
                var widthRatio=targetwidth/originalWidth
                var heightRatio=targetheight/originalHeight
                return Math.min(widthRatio,heightRatio)
            }
            fillMode: Image.PreserveAspectFit
            asynchronous: false  // 拖动时改为同步，避免异步计算导致的卡顿
            mipmap: true  // 启用mipmap，提高缩放性能
            smooth: false  // 拖动时关闭平滑，提高性能
            antialiasing: true
        }
        maskSource: musicIconRectangle
    }
    //musicNameTextLabel
    Label{
        id:musicNameTextLabel
        text: musicName
        color:"white"
        font.pixelSize: 16
        font.family: "微软雅黑"
        anchors.left: musicIconRectangle.right
        anchors.leftMargin: 10
        anchors.top: musicIconRectangle.top
        anchors.topMargin: 0
    }
    //singerNameTextLabel
    Label{
        id:singerNameTextLabel
        text: singerName
        color:"#ababaf"
        font.pixelSize: 14
        font.family: "微软雅黑"
        anchors.left: musicIconRectangle.right
        anchors.leftMargin: 10
        anchors.top: musicNameTextLabel.bottom
        anchors.topMargin: 4
    }
    //收藏、评论、分享、下载
    // Row{
    //     anchors.left: musicNameTextLabel.left
    //     anchors.top: musicNameTextLabel.bottom
    //     anchors.topMargin: 10
    //     spacing: 20
    //     Repeater{
    //         anchors.fill: parent
    //         model:["qrc:/image/colect.png","qrc:/image/coment.png","qrc:/image/share.png","qrc:/image/download.png"]
    //         delegate: Image {
    //             id:imageElement
    //             source: modelData
    //             anchors.verticalCenter: parent.verticalCenter
    //             layer.enabled: false
    //             layer.effect:ColorOverlay{
    //                 source: imageElement
    //                 color: "white"
    //             }
    //             MouseArea{
    //                 anchors.fill: parent
    //                 hoverEnabled: true
    //                 onEntered: {
    //                     parent.layer.enabled = true
    //                 }
    //                 onExited: {
    //                     parent.layer.enabled = false
    //                 }
    //                 onClicked: {
    //                     //---
    //                 }
    //             }
    //         }
    //     }
    // }

    //==中间部分==
    //播放与暂停
    Rectangle{
        id:playRectangle
        width: 40
        height: 40
        radius:height/2
        color: "#fc3c55"
        visible: root.visible
        anchors.top: parent.top
        anchors.topMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        Image {
            id: playImage
            source: "qrc:/image/playBtn.png"
            width: parent.width
            height: parent.height
            scale: 0.8
            visible: true
            anchors.centerIn: parent
            mipmap: true
            asynchronous: false
            cache: true
            antialiasing: true
            layer.enabled: true
            property string colorOverlay: "white"
            layer.effect: ColorOverlay{
                source: playImage
                color: playImage.colorOverlay
            }
        }
        Image{
            id:pauseImage
            source:"qrc:/image/pause.png"
            visible: false
            width: parent.width
            height: parent.height
            mipmap: true
            asynchronous: false
            cache: true
            antialiasing: true
            property string colorOverlay: "white"
            layer.enabled: true
            layer.effect: ColorOverlay{
                source: pauseImage
                color: pauseImage.colorOverlay
            }
        }
        MouseArea{
            id:playMouseArea
            anchors.fill:parent
            hoverEnabled: true
            property bool isPressed: false
            onEntered: {
                playRectangle.scale=1.04
            }
            onExited: {
                    playMouseArea.isPressed=false
                    playRectangle.scale=1.0
            }
            onPressed:  {
                playRectangle.color="#9a333f"
                playImage.colorOverlay="#9b9b9f"
                pauseImage.colorOverlay="#9b9b9f"
                playMouseArea.isPressed = true
                playRectangle.scale=0.96
            }
            onReleased: {
                if(playMouseArea.isPressed===true)
                    playRectangle.scale=1.04
                playRectangle.color="#fc3c55"
                playImage.colorOverlay="white"
                pauseImage.colorOverlay="white"
            }
            onClicked: {
                if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                {
                    BasicConfig.isPlaying =false
                    BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_MusicPlayerIndex
                }
                if(BasicConfig.isPlaying===true)
                {
                    if(BasicConfig.currentMediaFilePath === "")return;
                    BasicConfig.isPlaying=false
                    Client.qmlClickedReqPreparePlayMusic(BasicConfig.currentMediaFilePath)
                }
                else
                {
                    if(BasicConfig.currentMediaFilePath === "")return;
                    BasicConfig.isPlaying=true
                    Client.qmlClickedReqPreparePlayMusic(BasicConfig.currentMediaFilePath)
                }
            }
        }
        Connections{
            target:Client
            function onUpdataQmlTransforStopIcon()
            {
                playImage.visible = !playImage.visible
                pauseImage.visible = !pauseImage.visible

                BasicConfig.isPlaying=false

                //停止时确保Handler走向尽头
                musicProgressHandle.x=musicProgressRectanle.width-musicProgressHandle.width/2
                backgroundRectangle.visibleProgressX =musicProgressRectanle.width
                leftTimeLabel.text =String(Math.floor(Math.floor(allTime/60)/10))+String(Math.floor(allTime/60)%10)+": "+String(Math.floor((allTime%60/10)))+String((allTime%60%10))

                // 模拟视觉反馈
                playRectangle.scale = 1.0
                playRectangle.color = "#fc3c55"
                playImage.colorOverlay = "white"
                pauseImage.colorOverlay = "white"
            }
            function onConfigSignal_loadLastCloseAppFocusedMusic(icon_,music_name_,music_artist_,music_album_,timesize_,music_id_,music_filepath_){
                console.log("Loading foucsed music.")
                //由于Basic.currentMediaCoverFilePath连接着信号处理，最后再赋值它，顺带触发自动更新ui
                BasicConfig.currentMediaName=music_name_
                BasicConfig.currentMediaArtistAuthor=music_artist_
                BasicConfig.currentMediaFilePath=music_filepath_
                //显示的总时长一般都是C艹端在打开媒体文件后自动往qml端更新
                //这里额外直接设置
                root.allTime = timesize_

                //触发更新
                BasicConfig.currentMediaCoverFilePath=icon_
            }
        }
        //响应播放状态改变，播放图标对应改变
        Connections{
            target:BasicConfig
            function onIsPlayingChanged(){
                if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)return;
                if(BasicConfig.isPlaying===true)
                {
                    playImage.visible=false
                    pauseImage.visible=true
                }
                else
                {
                    playImage.visible=true
                    pauseImage.visible=false
                }
            }
            function onCurrentMediaCoverFilePathChanged(){
                musicIconImage.source = BasicConfig.currentMediaCoverFilePath
                musicNameTextLabel.text=BasicConfig.currentMediaName
                singerNameTextLabel.text=BasicConfig.currentMediaArtistAuthor
            }
            // function onPlayingIndexChanged(){//====>Discard
            //     //更新cpp端持有播放索引
            //     Client.updataCurrentPlayingIndex(BasicConfig.playingIndex)
            // }

            function onPlayerFocusChanged()
            {
                if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                {
                    playImage.visible=true
                    pauseImage.visible=false
                    BasicConfig.resetAllPlayListHeadicon()
                }
            }
        }
    }
    //两侧四个部分
    Image {
        id: loveImage
        source: "qrc:/image/love.png"
        anchors.verticalCenter: playRectangle.verticalCenter
        visible: root.visible
        anchors.right: playRectangle.left
        anchors.rightMargin: 60
        property bool isLoved: false
        property string colorOverlay: "white"
        layer.enabled: false
        layer.effect: ColorOverlay{
            id:colorOverlay
            source: loveImage
            color: loveImage.colorOverlay
        }
        MouseArea{
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                if(loveImage.isLoved===false)
                    loveImage.layer.enabled=true
            }
            onExited: {
                if(loveImage.isLoved===false)
                    loveImage.layer.enabled=false
            }
            onClicked: {
                if(loveImage.isLoved===false)
                {
                    loveImage.source="qrc:/image/love_NoMiddleTransparent.png"
                    loveImage.colorOverlay="#fc3c55"
                    loveImage.isLoved=true
                }
                else
                {
                    loveImage.source="qrc:/image/love.png"
                    loveImage.colorOverlay="white"
                    loveImage.isLoved=false
                }
            }
            onPressed: {
                loveImage.scale=0.9
            }
            onReleased: {
                loveImage.scale=1.1
            }
        }
    }
    Image {
        id: lastImage
        source: "qrc:/image/last.png"
        visible: root.visible
        anchors.verticalCenter: playRectangle.verticalCenter
        anchors.right: playRectangle.left
        anchors.rightMargin: 20
        layer.enabled: true
        layer.effect: ColorOverlay{
            source: lastImage
            color: "#d5d5d7"
        }
        MouseArea{
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                cursorShape = Qt.PointingHandCursor
            }
            onExited: {
                cursorShape = Qt.ArrowCursor
            }
            onPressed: {
                lastImage.layer.enabled=false
            }
            onReleased: {
                lastImage.layer.enabled=true
            }
            onClicked: {
                if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                {
                    playImage.visible=false
                    pauseImage.visible=true
                }
                Client.reqPlayLast(/*(BasicConfig.playingIndex - 1) < 0 ? BasicConfig.localMusicListModel.count-1 : BasicConfig.playingIndex - 1*/)
            }
        }
    }
    Image {
        id: nextImage
        source: "qrc:/image/next.png"
        visible: root.visible
        anchors.left: playRectangle.right
        anchors.leftMargin: 20
        anchors.verticalCenter: playRectangle.verticalCenter
        layer.enabled: true
        layer.effect: ColorOverlay{
            source: nextImage
            color: "#d5d5d7"
        }
        MouseArea{
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                cursorShape = Qt.PointingHandCursor
            }
            onExited: {
                cursorShape = Qt.ArrowCursor
            }
            onPressed: {
                nextImage.layer.enabled=false
            }
            onReleased: {
                nextImage.layer.enabled=true
            }
            onClicked: {
                if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                {
                    playImage.visible=false
                    pauseImage.visible=true
                }
                Client.reqPlayNext(/*(BasicConfig.playingIndex + 1)%BasicConfig.localMusicListModel.count*/)
            }
        }
    }
    Image {
        id: playstateImage
        source: "qrc:/image/playstate1.png"
        visible: root.visible
        anchors.left: playRectangle.right
        anchors.leftMargin: 60
        anchors.verticalCenter: playRectangle.verticalCenter
    }
    //音乐播放进度条Slider
    //Rectangle
    Rectangle{
        id:musicProgressRectanle
        anchors.top: root.top
        anchors.topMargin: 0
        anchors.left: root.left
        anchors.leftMargin: 2
        anchors.right: root.right
        anchors.rightMargin: 2
        height: 10
        color:"transparent"
        visible: root.visible

        property var sliderColor_themeItems : [
            {default_Color: "#00DDDD",sel_Color: "cyan"},
            {default_Color: "#c74054",sel_Color: "#fc3c55"}
        ]
        property int sliderColor_themeIndex: 1

        Slider{
            id:musicProgressSlier
            anchors.verticalCenter: parent.verticalCenter
            from: 0
            to: BasicConfig.max_Slider_Value
            height: 8
            width: parent.width
            handle:Rectangle{
                id:musicProgressHandle
                width:12
                height: width
                radius: width/2
                color: "white"
                visible: false
                anchors.verticalCenter: parent.verticalCenter
            }
            background: Rectangle{
                id:backgroundRectangle
                anchors.fill: parent
                // radius: musicProgressSlier.height/2
                color: "#4d4d56"
                clip: true
                property real visibleProgressX: 0.0
                property string visibleColor:musicProgressRectanle.sliderColor_themeItems[musicProgressRectanle.sliderColor_themeIndex].default_Color
                Rectangle{
                    id:visibleRectangle
                    height:parent.height
                    width: backgroundRectangle.visibleProgressX
                    // radius: height/2
                    color: backgroundRectangle.visibleColor
                    anchors.left: backgroundRectangle.left
                    anchors.top: backgroundRectangle.top
                }
            }
        }
        MouseArea{
            id:musicProgressRectanleMouseArea
            anchors.fill: parent
            hoverEnabled: true
            property bool isPressed: false
            property bool isInOutSide: true
            onEntered: {
                musicProgressHandle.visible = true
                musicProgressRectanleMouseArea.isInOutSide=false
                backgroundRectangle.visibleColor=musicProgressRectanle.sliderColor_themeItems[musicProgressRectanle.sliderColor_themeIndex].sel_Color
            }
            onExited: {
                if(musicProgressRectanleMouseArea.isPressed===false)
                {
                    musicProgressHandle.visible = false
                    backgroundRectangle.visibleColor=musicProgressRectanle.sliderColor_themeItems[musicProgressRectanle.sliderColor_themeIndex].default_Color
                }
                musicProgressRectanleMouseArea.isInOutSide=true
            }
            onPressed: (mouse)=>{
                   // var newPos = (mouse.x - musicProgressSlier.leftPadding) /musicProgressSlier.width
                   // newPos = Math.max(0, Math.min(1, newPos)) // 限制在 0~1 范围内
                   // musicProgressSlier.value = newPos
                    if(root.allTime !== 0)
                        musicProgressHandle.x=mouse.x
                    musicProgressRectanleMouseArea.isPressed=true
            }
            onReleased: {
                musicProgressRectanleMouseArea.isPressed=false
                if(musicProgressRectanleMouseArea.isInOutSide===true)
                {
                        musicProgressHandle.visible = false
                        backgroundRectangle.visibleColor=musicProgressRectanle.sliderColor_themeItems[musicProgressRectanle.sliderColor_themeIndex].default_Color
                }
                if(BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_MusicPlayerIndex)
                    Client.progressSlider_Seek(root.nowTime)
            }
            onMouseXChanged: (mouse)=>{
                if(musicProgressRectanleMouseArea.isPressed===true)
                {
                     if(mouse.x<=musicProgressRectanle.width&&mouse.x>=0)
                     {
                         musicProgressHandle.x=mouse.x-musicProgressHandle.width/2
                         backgroundRectangle.visibleProgressX =mouse.x
                         nowTime=allTime*(mouse.x/musicProgressRectanle.width)
                         leftTimeLabel.text = String(Math.floor(Math.floor(nowTime/60)/10))+String(Math.floor(nowTime/60)%10)+": "+String(Math.floor((nowTime%60/10)))+String((nowTime%60%10))
                     }
                    if(mouse.x<0)
                    {
                        musicProgressHandle.x=0-musicProgressHandle.width/2
                        backgroundRectangle.visibleProgressX =0
                         nowTime=0
                         leftTimeLabel.text = String(Math.floor(Math.floor(nowTime/60)/10))+String(Math.floor(nowTime/60)%10)+": "+String(Math.floor((nowTime%60/10)))+String((nowTime%60%10))
                    }
                    if(mouse.x>musicProgressRectanle.width)
                    {
                        musicProgressHandle.x=musicProgressRectanle.width-musicProgressHandle.width/2
                        backgroundRectangle.visibleProgressX =musicProgressRectanle.width
                        leftTimeLabel.text =String(Math.floor(Math.floor(allTime/60)/10))+String(Math.floor(allTime/60)%10)+": "+String(Math.floor((allTime%60/10)))+String((allTime%60%10))
                    }
                }
            }
        }
    }
    //ProgressSlider临时变量
    property int allTime:0          //单位s
    property int nowTime: 0     //单位s
    property real ratio :0
    Connections{
        target: Client
        function onUpdataQmlPlayProgressSliderCurPos(CurPos,CurTime_Second){
            if(musicProgressRectanleMouseArea.isPressed===false && BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_MusicPlayerIndex)
            {
                root.nowTime=CurTime_Second
                leftTimeLabel.text = String(Math.floor(Math.floor(CurTime_Second/60)/10))+String(Math.floor(CurTime_Second/60)%10)+": "+String(Math.floor((CurTime_Second%60/10)))+String((CurTime_Second%60%10))
                root.ratio=CurPos/BasicConfig.max_Slider_Value
                musicProgressHandle.x=musicProgressSlier.width * root.ratio - musicProgressHandle.width/2
                backgroundRectangle.visibleProgressX =musicProgressSlier.width * root.ratio
            }
        }
        function onUpdataQmlPlayNowFileAllTime(AllTime){
            if(BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_MusicPlayerIndex)
                root.allTime = AllTime
        }
    }

    //进度条两侧的时间label
    Label{
        id:leftTimeLabel
        anchors.right: rightTimeLabel.left
        anchors.rightMargin: 0
        anchors.verticalCenter: root.verticalCenter
        text: String(Math.floor(Math.floor(nowTime/60)/10))+String(Math.floor(nowTime/60)%10)+": "
              +String(Math.floor((nowTime%60/10)))+String((nowTime%60%10))
        color: "#fef2e8"
        visible: root.visible
        font.family: "微软雅黑"
        font.pixelSize: 16
    }
    Label{
        id:rightTimeLabel
        anchors.right: rightBottomRow.left
        anchors.rightMargin: 30
        anchors.verticalCenter: root.verticalCenter
        text: "  //  "+String(Math.floor(Math.floor(allTime/60)/10))+String(Math.floor(allTime/60)%10)+": "
              +String(Math.floor((allTime%60/10)))+String((allTime%60%10))
        color: "#fef2e8"
        visible: root.visible
        font.family: "微软雅黑"
        font.pixelSize: 16
    }

    //底部右侧的功能区
    Row{
        id:rightBottomRow
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 30
        spacing: 15
        visible: root.visible
        //音质选择
        Rectangle{
            id:soudEffectRectangle
            width: 30
            height: 18
            radius: 5
            anchors.verticalCenter:parent.verticalCenter
            color: "transparent"
            border.width: 1
            border.color: "#a5a5a9"
            Label{
                id:soudEffectRectangleLabel
                text:"标准"
                color: "#a5a5a9"
                anchors.centerIn: parent
            }
            MouseArea{
                anchors.fill: parent
                hoverEnabled:true
                onEntered: {
                    soudEffectRectangleLabel.color ="white"
                }
                onExited: {
                    soudEffectRectangleLabel.color="#a5a5a9"
                }
                onClicked: {
                    //----
                }
            }
        }
        //桌面歌词
        Image {
            id: deskLyricImage
            source: "qrc:/image/lyric.png"
            layer.enabled: false
            layer.effect: ColorOverlay{
                source: deskLyricImage
                color: "white"
            }
            MouseArea{
                anchors.fill: parent
                hoverEnabled:true
                onEntered: {
                    deskLyricImage.layer.enabled=true
                }
                onExited: {
                    deskLyricImage.layer.enabled=false
                }
            }
        }

        //音量
        Image {
            id: volumeIamge
            source: "qrc:/image/volume.png"

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
                    if(volumeIamge.popupHovered===false && !volumeImageMouseArea.containsMouse)
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
                width: 40
                height: 120
                color: "#fef2e8"
                radius: 5

                layer.enabled: true
                layer.effect: DropShadow{
                    anchors.fill: popupBackground
                    source: popupBackground
                    samples: 30
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
                        Client.reqChangeVolumeValue(Math.floor(progressRealValue))

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
                    function onConfigSignal_loadPlayerVolumeConfig(value)
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

        //vip音效
        Image {
            id: vipSoundEffectImage
            source: /*"qrc:/image/vipsoundeffect.png"*/"qrc:/image/transparent.png"
            sourceSize.width:playListImage.width
            sourceSize.height: playListImage.height
            layer.enabled: false
            layer.effect: ColorOverlay{
                source: vipSoundEffectImage
                color: "white"
            }
            MouseArea{
                anchors.fill: parent
                enabled: false
                hoverEnabled:true
                onEntered: {
                    vipSoundEffectImage.layer.enabled=true
                }
                onExited: {
                    vipSoundEffectImage.layer.enabled=false
                }
            }
        }

        //一起听
        Image {
            id: listentogetherImage
            source: /*"qrc:/image/listentogether.png"*/"qrc:/image/transparent.png"
            sourceSize.width:playListImage.width
            sourceSize.height: playListImage.height
            layer.enabled: false
            layer.effect: ColorOverlay{
                source: listentogetherImage
                color: "white"
            }
            MouseArea{
                anchors.fill: parent
                enabled: false
                hoverEnabled:true
                onEntered: {
                    listentogetherImage.layer.enabled=true
                }
                onExited: {
                    listentogetherImage.layer.enabled=false
                }
            }
        }

        //播放列表
        Image {
            id: playListImage
            source: "qrc:/image/playlist.png"
            layer.enabled: false
            layer.effect: ColorOverlay{
                source: playListImage
                color: "white"
            }
            MouseArea{
                anchors.fill: parent
                hoverEnabled:true
                onEntered: {
                    playListImage.layer.enabled=true
                }
                onExited: {
                    playListImage.layer.enabled=false
                }
            }
        }
    }
}
