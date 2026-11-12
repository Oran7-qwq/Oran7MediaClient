import QtQuick 2.15
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import Client 1.0
import "../../Basic"
import "."
import Oran7NetEaseAuth 1.0

import Oran7UI.Impl

Rectangle{
    id:root

    property alias musicName: musicNameTextLabel.text
    property alias singerName: singerNameTextLabel.text

    property real  visibleOpacity: 0.0
    onOpacityChanged: {
        if(visibleOpacity === 0.0)
            root.visible = false
        else
            root.visible = true
    }

    color:"transparent"

    clip: true

    signal albumCoverClicked()
    signal lyricsButtonClicked()

    // 标签直接绑定快照；这里仅刷新收藏/整合状态与恢复时长。
    function refreshTrackStatus(usePlaybackDuration) {
        loveImage.refreshLoved()
        integrateImage.refreshIntegrated()
        var seconds = parseInt(BasicConfig.currentMediaTimesize, 10)
        musicProgressSlier.allSecondTime = usePlaybackDuration && BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_MusicPlayerIndex
            && Client.playback.durationMs > 0 ? Math.floor(Client.playback.durationMs / 1000)
            : (isNaN(seconds) ? 0 : seconds)
    }
    Component.onCompleted: refreshTrackStatus(true)
    signal islandClicked()
    signal floatingLyricsClicked()

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
            source: BasicConfig.currentMediaCoverFilePath || "qrc:/image/transparent.png"
        }
        MouseArea {
            anchors.fill: parent
            onPressed: parent.scale = 0.95
            onReleased: parent.scale = 1.0
            onCanceled: parent.scale = 1.0
            onClicked: root.albumCoverClicked()
        }
    }

    //musicNameTextLabel
    Label{
        id:musicNameTextLabel
        text: BasicConfig.currentMediaName || qsTranslate("Oran7Ui", "Oran7MusicPlayer期待你的播放~")
        color:Oran7Theme.Oran7MusicPlayControls["textColorBase-6"]
        font.pixelSize: 16
        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
        font.bold:true
        anchors.left: musicIconRectangle.right
        anchors.leftMargin: 10
        anchors.top: musicIconRectangle.top
        anchors.topMargin: 0
    }
    //singerNameTextLabel
    Label{
        id:singerNameTextLabel
        text: BasicConfig.currentMediaArtistAuthor || "Author-Oran7ヾ(≧ ▽ ≦)ゝ"
        color:Oran7Theme.Oran7MusicPlayControls["textColorBase-5"]
        font.pixelSize: 14
        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
        font.bold:true
        font.italic: true
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
        property string colorOverlay: "transparent"

        Oran7GradientMask{
            anchors.fill: parent
            dynamicGradient:true
            gradientMaskEnabled:true
            gradientColorIndex: Oran7Theme.Oran7MusicPlayControls.progressVisibleBar_ColorIndex //与进度条的炫彩同步
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
                Client.musicQueue.toggleCurrent()
            }
        }
        Connections {
            target: Client.playback
            function onCurrentTrackChanged() { root.refreshTrackStatus(false) }
        }
    }
    //两侧四个部分
    // 整合（"我整合的音乐"合集）按钮 —— 位于「喜欢」左侧，本地/网易云免费/VIP 等异构数据源统一管理
    Image {
        id: integrateImage
        source: integrateImage.isIntegrated ? "qrc:/image/ep_collection-tag.-no-middle-transparent.png" :
                                              "qrc:/image/ep_collection-tag.png"
        anchors.verticalCenter: playRectangle.verticalCenter
        visible: root.visible
        width: 25
        height: 25
        sourceSize.width: 1080
        sourceSize.height: 1080
        smooth: true        // 平滑缩放
        mipmap: true        // mipmap 预过滤：大图小尺寸显示不再锯齿（关键）
        antialiasing: true  // 缩放时边缘抗锯齿
        anchors.right: loveImage.left
        anchors.rightMargin: 17
        // 是否已整合到"我整合的音乐"：按当前歌 songId/url/filepath 扫描全局整合模型
        property bool isIntegrated: false
        readonly property string colorOverlay: Oran7Theme.Oran7MusicPlayControls["iconColorBase-6"]
        layer.enabled: true
        layer.effect: ColorOverlay{
            id: integrateColorOverlay
            source: integrateImage
            color: integrateImage.colorOverlay
        }
        scale: isPressed ? 0.95 : 1.00
        property bool isPressed: false

        function refreshIntegrated() {
            var songId = Client.currentSongId();
            var fp = BasicConfig.currentMediaFilePath;
            isIntegrated = BasicConfig.isIntegrated(songId, fp, fp);
        }

        Component.onCompleted: refreshIntegrated()

        // 整合模型增删（任意入口整合/取消整合）时刷新状态
        Connections {
            target: BasicConfig.myIntegratedMusicListModel
            function onCountChanged() { integrateImage.refreshIntegrated(); }
        }

        // 切换整合：已整合 → 移出合集；未整合 → 加入合集
        function toggleIntegrated() {
            var songId = Client.currentSongId();
            var fp = BasicConfig.currentMediaFilePath;
            if (BasicConfig.isIntegrated(songId, fp, fp)) {
                BasicConfig.removeIntegratedMusic(songId, fp, fp);
                return;
            }
            var isVip = Client.currentIsVip();
            // VIP 歌音源链接临时会过期，整合不持久保存（存空），播放时用 songId 重新解析
            var saveSrc = isVip ? "" : fp;
            BasicConfig.addIntegratedMusicItem(BasicConfig.currentMediaCoverFilePath,
                                               BasicConfig.currentMediaName,
                                               BasicConfig.currentMediaArtistAuthor,
                                               BasicConfig.currentMediaAlbum,
                                               BasicConfig.currentMediaTimesize,
                                               saveSrc, saveSrc,
                                               songId, isVip,
                                               songId > 0 ? "netease" : "local");
        }

        MouseArea{
            id: integrateMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: integrateImage.toggleIntegrated()
            onPressed: parent.isPressed = true
            onReleased: parent.isPressed = false
        }
    }
    Image {
        id: loveImage
        source: loveImage.isLoved ? "qrc:/image/love_NoMiddleTransparent.png" : "qrc:/image/love.png"
        anchors.verticalCenter: playRectangle.verticalCenter
        visible: root.visible
        width: 25
        height: 25
        sourceSize.width: 1080
        sourceSize.height: 1080
        smooth: true        // 平滑缩放
        mipmap: true        // mipmap 预过滤：大图小尺寸显示不再锯齿（关键）
        antialiasing: true  // 缩放时边缘抗锯齿
        anchors.right: playRectangle.left
        anchors.rightMargin: 60
        // 是否已收藏（网易云）：反映全局网易云喜欢集合，点击后取反；只同步网易云，不写本地 config
        property bool isLoved: _loved
        property bool _loved: false
        readonly property string colorOverlay: Oran7Theme.Oran7MusicPlayControls["iconColorBase-6"]
        layer.enabled: true
        layer.effect: ColorOverlay{
            id:colorOverlay
            source: loveImage
            color: loveImage.colorOverlay
        }
        scale: isPressed ? 0.95 : 1.00
        property bool isPressed: false

        function checkLoved() {
            var songId = Client.currentSongId();
            return songId > 0 && BasicConfig.isNeteaseLiked(songId);
        }

        function refreshLoved() { _loved = checkLoved(); }

        Component.onCompleted: refreshLoved()

        // 云端歌单集合变化（启动预拉取/刷新）时刷新喜欢状态
        Connections {
            target: BasicConfig.neteaseLikedListModel
            function onCountChanged() { loveImage.refreshLoved(); }
        }

        // 切换网易云喜欢：只同步到网易云服务器 + 更新全局网易云喜欢集合，不写本地"我喜欢的音乐"/config
        function toggleFavorite() {
            Client.cloudMusic.toggleLike({songId: Client.currentSongId(),
                cover: BasicConfig.currentMediaCoverFilePath, title: BasicConfig.currentMediaName,
                artist: BasicConfig.currentMediaArtistAuthor, album: BasicConfig.currentMediaAlbum,
                timesize: BasicConfig.currentMediaTimesize, isVip: Client.currentIsVip()});
            refreshLoved();
        }

        MouseArea{
            id:loveMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: loveImage.toggleFavorite()
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
    // 播放顺序只轮换 0/1/2，网易云推荐使用右侧独立入口。
    Image {
        id: playstateImage
        source: [ "qrc:/image/playstate1.png",
                  "qrc:/image/ph_repeat-once-bold.png",
                  "qrc:/image/fa7-solid_random.png" ][Client.musicQueue.orderMode]
        visible: root.visible
        anchors.left: playRectangle.right
        anchors.leftMargin: 60
        anchors.verticalCenter: root.verticalCenter
        layer.enabled: true
        layer.effect: ColorOverlay{
            source: playstateImage
            color:playstateImage.isPressed ? Oran7Theme.Oran7MusicPlayControls["iconColorBase-7"] :
                                     Oran7Theme.Oran7MusicPlayControls["iconColorBase-6"]
        }
        sourceSize:Qt.size(512,512)
        width: 25
        height: 25
        mipmap: true
        antialiasing: true

        scale: isPressed ? 0.95 : 1.00
        property bool isPressed: false
        MouseArea{
            anchors.fill: parent
            hoverEnabled: true
            onEntered: cursorShape = Qt.PointingHandCursor
            onExited:cursorShape = Qt.ArrowCursor
            onPressed: parent.isPressed = true
            onReleased: parent.isPressed = false
            onClicked: {
                // 切换播放顺序同时退出推荐，沿用之前的普通列表归属。
                var next = (Client.musicQueue.orderMode + 1) % 3;
                Client.setPlayMode(next);
            }
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
        visibleProgresTrack_GradientMaskEnabeld:true
        visibleProgresTrack_GradientMaskColorIndex:Oran7Theme.Oran7MusicPlayControls.progressVisibleBar_ColorIndex
        visibleProgressTrack_DynamicGradientMaskEnabled:Oran7Theme.Oran7MusicPlayControls.progressVisibleBar_dynamicGradientEnabled
        visibleProgressTrack_DrnamicGradientMask_transitionDuration:Oran7Theme.Primary.durationMid * 4
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
                // 播放结束：恢复"播放"按钮状态

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
        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
        font.pixelSize: 16
        font.bold:true
    }
    Label{
        id:rightTimeLabel
        anchors.right: rightBottomRow.left
        anchors.rightMargin: 30
        anchors.verticalCenter: root.verticalCenter
        text: "  //  "+musicProgressSlier.allTimeText
        color: Oran7Theme.Oran7MusicPlayControls["textColorBase-6"]
        visible: root.visible
        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
        font.pixelSize: 16
        font.bold:true
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
        Oran7RecommendationButton {
            queue: Client.musicQueue
            mode: Client.playback.mode
            ink: Oran7Theme.Oran7MusicPlayControls["textColorBase-6"]
            accent: Oran7Theme.Oran7MusicPlayControls["colorPrimaryBase-5"]
            anchors.verticalCenter: parent.verticalCenter
        }
        Rectangle{
            id:soudEffectRectangle
            width: 30
            height: 18
            visible: false // < ---- false
            radius: 5
            anchors.verticalCenter:parent.verticalCenter
            color: "transparent"
            border.width: 1
            border.color:Oran7Theme.Oran7MusicPlayControls["iconColorBase-5"]
            Label{
                id:soudEffectRectangleLabel
                text:qsTranslate("Oran7Ui", "标准")
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
        // Image {
        //     id: deskLyricImage
        //     source: "qrc:/image/lyric.png"
        //     layer.enabled: true
        //     layer.effect: ColorOverlay{
        //         source: deskLyricImage
        //         color: deskLyricImage.hovered ? Oran7Theme.Oran7MusicPlayControls["iconColorBase-5"] :
        //                                         Oran7Theme.Oran7MusicPlayControls["iconColorBase-7"]
        //     }
        //     property bool hovered: false
        //     MouseArea{
        //         anchors.fill: parent
        //         hoverEnabled:true
        //         onEntered:parent.hovered = true
        //         onExited:parent.hovered = false
        //         onClicked: root.lyricsButtonClicked()
        //     }
        // }
        Label{
            id: deskLyrics_Island
            text:qsTranslate("Oran7Ui", "灵动岛")
            color:deskLyrics_Island.hovered ? Oran7Theme.Oran7MusicPlayControls["iconColorBase-5"] :
                                                                                Oran7Theme.Oran7MusicPlayControls["iconColorBase-7"]
            font.pixelSize:17
            font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
            property bool hovered: false
            MouseArea{
                anchors.fill: parent
                hoverEnabled:true
                cursorShape: Qt.PointingHandCursor
                onEntered:parent.hovered = true
                onExited:parent.hovered = false
                onClicked: root.islandClicked()
            }
        }

        // Label{
        //     id: deskLyrics_Floating
        //     text:"悬浮歌词"
        //     color: deskLyrics_Floating.hovered ? Oran7Theme.Oran7MusicPlayControls["iconColorBase-5"] :
        //                                              Oran7Theme.Oran7MusicPlayControls["iconColorBase-7"]
        //     font.pixelSize:17
        //     font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
        //     property bool hovered: false
        //     MouseArea{
        //         anchors.fill: parent
        //         hoverEnabled:true
        //         cursorShape: Qt.PointingHandCursor
        //         onEntered:parent.hovered = true
        //         onExited:parent.hovered = false
        //         onClicked: root.floatingLyricsClicked()
        //     }
        // }

        Oran7PlayerVolumeComponent{
            id:bottomPage_playerVolumeControl
            // QML 颜色属性直接赋值主题色即可（color(...) 不是函数，会解析到组件自身的 color 属性）
            icon_defaultColor: Oran7Theme.Oran7MusicPlayControls["iconColorBase-5"]
            icon_hoverdColor: Oran7Theme.Oran7MusicPlayControls["iconColorBase-7"]
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
