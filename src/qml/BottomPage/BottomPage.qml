import QtQuick 2.15
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import Client 1.0
import "../Basic"
import "../Components"

import Oran7UI.Impl

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
        Oran7RoundedImage{
            id:iconimage
            width: parent.width
            height: parent.height
            radius: parent.radius
            source:"qrc:/image/transparent.png"
        }
        MouseArea {
            anchors.fill: parent
            onPressed: parent.scale = 0.95
            onReleased: parent.scale = 1.0
            onCanceled: parent.scale = 1.0
            onClicked:animatedWindow_LyricsWin.open(musicIconRectangle);
        }
    }
    Oran7AnimatedWindow{
        id: animatedWindow_LyricsWin
        buttonColor: "#f8c7c7"
        fullscreenColor: "#f8c7c7"
        maxTiltAngle: 10
        animDuration: 400
        showCloseButton: false  // 使用自定义关闭按钮，不显示内置的

        // 挂到 mainWindow 的 contentItem 上，使全屏展开覆盖整个窗口
        Component.onCompleted: {
            var win = Window.window;
            if (win && win.contentItem)
                parent = win.contentItem;
        }

        // 独立的背景图副本，避免模糊源引用 mainWindow 本身导致关闭时闪烁
        Image {
            id: lyricsWinBackground
            width: mainWindow.width
            height: mainWindow.height
            sourceSize.width: Screen.width * Screen.devicePixelRatio
            sourceSize.height: Screen.height * Screen.devicePixelRatio
            source: filehepler.fileExists("file:///" + Oran7Theme.Oran7MainGUI.backgroundImage) ?
                        "file:///" + Oran7Theme.Oran7MainGUI.backgroundImage : "qrc:/image/defaultBg.jpg"
            fillMode: Image.PreserveAspectCrop
            asynchronous: false
            mipmap: true
            smooth: false
            cache: true
            transformOrigin: Item.Center
        }
        Oran7BlurCard {
            anchors.fill: parent
            blurSource: lyricsWinBackground
            themeColor: "#04FFFFFF"
            Oran7BlurCard{
                width: 40
                height: width
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 20
                themeColor: "#04FFFFFF"
                blurSource: lyricsWinBackground
                borderWidth: 2
                borderRadius: 10
                Image{
                    id:downImage
                    anchors.fill: parent
                    scale:0.8
                    source: "qrc:/image/mingcute_arrows-down-fill.png"
                    layer.enabled: true
                    layer.effect: ColorOverlay{
                        source:downImage
                        color:Oran7Theme.Oran7MainGUI.themeColor
                    }
                }
                MouseArea{
                    anchors.fill: parent
                    onClicked: {
                        if (animatedWindow_LyricsWin.isAnimating) return;
                        animatedWindow_LyricsWin.isAnimating = true;
                        animatedWindow_LyricsWin.state = "iconState";
                    }
                }
            }
        }
    }

    //musicNameTextLabel
    Label{
        id:musicNameTextLabel
        text: musicName
        color:Oran7Theme.Oran7MusicPlayControls["textColorBase-6"]
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
        color:Oran7Theme.Oran7MusicPlayControls["textColorBase-5"]
        font.pixelSize: 14
        font.family: "微软雅黑"
        anchors.left: musicIconRectangle.right
        anchors.leftMargin: 10
        anchors.top: musicNameTextLabel.bottom
        anchors.topMargin: 4
    }

    //==中间部分==
    //播放与暂停
    Rectangle{
        id:playRectangle
        width: 50
        height: 50
        radius:height/2
        color: "transparent"//Oran7Theme.Oran7MusicPlayControls["iconColorBase-2"]
        visible: root.visible
        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter
        property string colorOverlay: "transparent"/*playMouseArea.isPressed ? Oran7Theme.Oran7MusicPlayControls["playButtonColor"] :
                                                                Oran7Theme.Oran7MusicPlayControls["playButtonColor"]*/

        Oran7GradientMask{
            anchors.fill: parent
            dynamicGradient:true
            gradientMaskEnabled:true
            transitionDuration:Oran7Theme.Primary.durationMid * 4
            dynamicInterval:Oran7Theme.Primary.durationMid * 4
            Image{
                id: playImage
                source: "qrc:/image/playBtn.png"
                width: parent.width
                height: parent.height
                scale: 0.8
                visible: !BasicConfig.isPlaying || BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex
                anchors.centerIn: parent
                mipmap: true
                asynchronous: false
                cache: true
                antialiasing: true
                layer.enabled: true
                layer.effect: ColorOverlay{
                    source: playImage
                    color: playRectangle.colorOverlay
                }
            }
            Image{
                id:pauseImage
                source:"qrc:/image/pause.png"
                visible: BasicConfig.isPlaying && BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_MusicPlayerIndex
                width: parent.width
                height: parent.height
                mipmap: true
                asynchronous: false
                cache: true
                antialiasing: true
                layer.enabled: true
                layer.effect: ColorOverlay{
                    source: pauseImage
                    color: playRectangle.colorOverlay
                }
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
                playMouseArea.isPressed = true
                playRectangle.scale=0.96
            }
            onReleased: {
                if(playMouseArea.isPressed===true)
                    playRectangle.scale=1.04
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
                    Client.requestPlayMusic(BasicConfig.currentMediaFilePath)
                }
                else
                {
                    if(BasicConfig.currentMediaFilePath === "")return;
                    BasicConfig.isPlaying=true
                    Client.requestPlayMusic(BasicConfig.currentMediaFilePath)
                }
            }
        }
        Connections{
            target:Client
            function onFocusedMusicRestored(icon_,music_name_,music_artist_,music_album_,timesize_,music_id_,music_filepath_){
                console.log("Loading foucsed music.")
                //由于Basic.currentMediaCoverFilePath连接着信号处理，最后再赋值它，顺带触发自动更新ui
                BasicConfig.currentMediaName=music_name_
                BasicConfig.currentMediaArtistAuthor=music_artist_
                BasicConfig.currentMediaFilePath=music_filepath_
                //显示的总时长一般都是C艹端在打开媒体文件后自动往qml端更新
                //这里额外直接设置
                musicProgressSlier.allSecondTime = timesize_

                //触发更新
                BasicConfig.currentMediaCoverFilePath=icon_
            }
        }
        //响应播放状态改变，播放图标对应改变
        Connections{
            target:BasicConfig
            function onCurrentMediaCoverFilePathChanged(){
                iconimage.source = BasicConfig.currentMediaCoverFilePath
                musicNameTextLabel.text=BasicConfig.currentMediaName
                singerNameTextLabel.text=BasicConfig.currentMediaArtistAuthor
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
        readonly property string colorOverlay: Oran7Theme.Oran7MusicPlayControls["iconColorBase-6"]
        layer.enabled: true
        layer.effect: ColorOverlay{
            id:colorOverlay
            source: loveImage
            color: loveImage.colorOverlay
        }
        scale: isPressed ? 0.95 : 1.00
        property bool isPressed: false
        MouseArea{
            id:loveMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                if(loveImage.isLoved===false)
                {
                    loveImage.source="qrc:/image/love_NoMiddleTransparent.png"
                    loveImage.isLoved=true
                }
                else
                {
                    loveImage.source="qrc:/image/love.png"
                    loveImage.isLoved=false
                }
            }
            onPressed: parent.isPressed = true
            onReleased: parent.isPressed = false
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
            color: lastImage.isPressed ? Oran7Theme.Oran7MusicPlayControls["iconColorBase-8"] :
                       Oran7Theme.Oran7MusicPlayControls["iconColorBase-5"]
        }
        scale: isPressed ? 0.95 : 1.00
        property bool isPressed: false
        MouseArea{
            anchors.fill: parent
            hoverEnabled: true
            onEntered: cursorShape = Qt.PointingHandCursor
            onExited:cursorShape = Qt.ArrowCursor
            onPressed: parent.isPressed = true
            onReleased: parent.isPressed = false
            onClicked:Client.playPrevious()
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
            color: nextImage.isPressed ? Oran7Theme.Oran7MusicPlayControls["iconColorBase-8"] :
                       Oran7Theme.Oran7MusicPlayControls["iconColorBase-5"]
        }
        scale: isPressed ? 0.95 : 1.00
        property bool isPressed: false
        MouseArea{
            anchors.fill: parent
            hoverEnabled: true
            onEntered: cursorShape = Qt.PointingHandCursor
            onExited:cursorShape = Qt.ArrowCursor
            onPressed: parent.isPressed = true
            onReleased: parent.isPressed = false
            onClicked: Client.playNext()
        }
    }
    Image {
        id: playstateImage
        source: "qrc:/image/playstate1.png"
        visible: root.visible
        anchors.left: playRectangle.right
        anchors.leftMargin: 60
        anchors.verticalCenter: playRectangle.verticalCenter
        layer.enabled: true
        layer.effect: ColorOverlay{
            source: playstateImage
            color:playstateImage.isPressed ? Oran7Theme.Oran7MusicPlayControls["iconColorBase-7"] :
                                     Oran7Theme.Oran7MusicPlayControls["iconColorBase-6"]
        }
        scale: isPressed ? 0.95 : 1.00
        property bool isPressed: false
        MouseArea{
            anchors.fill: parent
            hoverEnabled: true
            onEntered: cursorShape = Qt.PointingHandCursor
            onExited:cursorShape = Qt.ArrowCursor
            onPressed: parent.isPressed = true
            onReleased: parent.isPressed = false
            onClicked: {}
        }
    }
    //音乐播放进度条Slider
    Oran7ProgressSlider{
        id:musicProgressSlier
        anchors.top: root.top
        anchors.topMargin: 0
        anchors.left: root.left
        anchors.leftMargin: 2
        anchors.right: root.right
        anchors.rightMargin: 2
        height: 10
        color:"transparent"
        sliderColor_themeIndex:0
        visible: root.visible
        focusedPlayer: BasicConfig.globalPlayer_MusicPlayerIndex

        Connections{
            target: Client
            function onPlayProgressUpdated(CurPos,CurTime_Second){
                if(musicProgressSlier.isPressed===false && BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_MusicPlayerIndex)
                {
                    musicProgressSlier.nowSecondTime=CurTime_Second
                    musicProgressSlier.ratio=CurPos/BasicConfig.max_Slider_Value
                    musicProgressSlier.progressHandleX=musicProgressSlier.width * musicProgressSlier.ratio - musicProgressSlier.progressHandleWidth/2
                    musicProgressSlier.visibleProgressX =musicProgressSlier.width * musicProgressSlier.ratio
                }
            }
            function onTotalDurationUpdated(AllTime){
                if(BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_MusicPlayerIndex){
                    musicProgressSlier.allSecondTime = AllTime;
                }
            }
            function onStopIconUpdated() {
                musicProgressSlier.progressHandleX = musicProgressSlier.width - musicProgressSlier.progressHandleWidth / 2;
                musicProgressSlier.visibleProgressX = musicProgressSlier.width;
            }
        }
    }

    //进度条两侧的时间label
    Label{
        id:leftTimeLabel
        anchors.right: rightTimeLabel.left
        anchors.rightMargin: 0
        anchors.verticalCenter: root.verticalCenter
        text: musicProgressSlier.nowTimeText
        color: Oran7Theme.Oran7MusicPlayControls["textColorBase-6"]
        visible: root.visible
        font.family: "微软雅黑"
        font.pixelSize: 16
    }
    Label{
        id:rightTimeLabel
        anchors.right: rightBottomRow.left
        anchors.rightMargin: 30
        anchors.verticalCenter: root.verticalCenter
        text: "  //  "+musicProgressSlier.allTimeText
        color: Oran7Theme.Oran7MusicPlayControls["textColorBase-6"]
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
            border.color:Oran7Theme.Oran7MusicPlayControls["iconColorBase-5"]
            Label{
                id:soudEffectRectangleLabel
                text:"标准"
                color:Oran7Theme.Oran7MusicPlayControls["iconColorBase-5"]
                anchors.centerIn: parent
            }
            MouseArea{
                anchors.fill: parent
                hoverEnabled:true
                onEntered: cursorShape = Qt.PointingHandCursor
                onExited:cursorShape = Qt.ArrowCursor
                onClicked: {}
            }
        }
        //桌面歌词-->未来开发为灵动岛歌词(2026/6/2)
        Image {
            id: deskLyricImage
            source: "qrc:/image/lyric.png"
            layer.enabled: true
            layer.effect: ColorOverlay{
                source: deskLyricImage
                color: deskLyricImage.hovered ? Oran7Theme.Oran7MusicPlayControls["iconColorBase-5"] :
                                                Oran7Theme.Oran7MusicPlayControls["iconColorBase-7"]
            }
            property bool hovered: false
            MouseArea{
                anchors.fill: parent
                hoverEnabled:true
                onEntered:parent.hovered = true
                onExited:parent.hovered = false
            }
        }

        Oran7PlayerVolumeComponent{
            id:bottomPage_playerVolumeControl
        }

        //播放列表
        Image {
            id: playListImage
            source: "qrc:/image/playlist.png"
            layer.enabled: true
            layer.effect: ColorOverlay{
                source: playListImage
                color: playListImage.hovered ? Oran7Theme.Oran7MusicPlayControls["iconColorBase-5"] :
                                                Oran7Theme.Oran7MusicPlayControls["iconColorBase-7"]
            }
            property bool hovered: false
            MouseArea{
                anchors.fill: parent
                hoverEnabled:true
                onEntered:parent.hovered = true
                onExited:parent.hovered = false
            }
        }
    }
}
