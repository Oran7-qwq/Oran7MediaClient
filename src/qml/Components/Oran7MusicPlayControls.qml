import QtQuick 2.15
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import Client 1.0
import "../Basic"
import "../Components"
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

    /*!
      把「当前播放信息抄进底栏」集中成一处。
      这段逻辑原先只挂在 BasicConfig.currentMediaCoverFilePath 的变更信号上，
      而 Qt 只在值**真的变了**时才 emit —— 于是：
        · 同一张专辑的两首歌 picUrl 完全相同
        · 漫游歌曲封面为空，空 → 空也不算变化
      这两种情况下封面字符串没变，handler 一次都不触发，
      歌名/作者虽然已经写进 BasicConfig，却永远抄不到标签上，
      底栏就停在上一首（或占位文本）上。这就是「有时候加载不出来」的来源。
      网络歌没有兜底：本地歌还有 ffprobe 之后的 focusedMusicRestored 再灌一遍，
      而 updateFocusCurMediaFile 对网络 URL 是直接 return 的。
      所以改为「歌名 / 作者 / 封面任一变化都刷新」——见下方 Connections。
    */
    function refreshNowPlayingUI() {
        iconimage.source = BasicConfig.currentMediaCoverFilePath
        musicNameTextLabel.text = BasicConfig.currentMediaName
        singerNameTextLabel.text = BasicConfig.currentMediaArtistAuthor
        // 切歌时刷新网易云喜欢状态（当前歌的 songId 变化）
        loveImage.refreshLoved()
        // 切歌时刷新"我整合的音乐"状态
        integrateImage.refreshIntegrated()
    }
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
            source:"qrc:/image/transparent.png"
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
        text: "Oran7MusicPlayer期待你的播放~"
        color:Oran7Theme.Oran7MusicPlayControls["textColorBase-6"]
        font.pixelSize: 16
        font.family: "微软雅黑"
        font.bold:true
        anchors.left: musicIconRectangle.right
        anchors.leftMargin: 10
        anchors.top: musicIconRectangle.top
        anchors.topMargin: 0
    }
    //singerNameTextLabel
    Label{
        id:singerNameTextLabel
        text: "Author-Oran7ヾ(≧ ▽ ≦)ゝ"
        color:Oran7Theme.Oran7MusicPlayControls["textColorBase-5"]
        font.pixelSize: 14
        font.family: "微软雅黑"
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
                if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                {
                    BasicConfig.isPlaying =false
                    BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_MusicPlayerIndex
                }
                // 通过底部播放按钮起播时,同样持久化当前歌曲元数据,
                // 保持 last_media_* 与 last_used_file 指向同一首歌(否则退出后恢复会元数据错配)
                if(Client.currentSongId() > 0)
                    Client.rememberLastMedia(BasicConfig.currentMediaName, BasicConfig.currentMediaArtistAuthor,
                                             BasicConfig.currentMediaAlbum, BasicConfig.currentMediaCoverFilePath,
                                             BasicConfig.currentMediaTimesize, Client.currentSongId(), Client.currentIsVip())
                if(BasicConfig.isPlaying===true)
                {
                    BasicConfig.isPlaying=false
                    // VIP 收藏歌未持久化 url，currentMediaFilePath 可能为空，此时用当前 songId 重新解析
                    if(BasicConfig.currentMediaFilePath !== "")
                        Client.playNetworkSong(Client.currentSongId(), BasicConfig.currentMediaFilePath, Client.currentIsVip())
                    else if(Client.currentSongId() > 0)
                        Client.playNetworkSong(Client.currentSongId(), "", Client.currentIsVip())
                }
                else
                {
                    BasicConfig.isPlaying=true
                    if(BasicConfig.currentMediaFilePath !== "")
                        Client.playNetworkSong(Client.currentSongId(), BasicConfig.currentMediaFilePath, Client.currentIsVip())
                    else if(Client.currentSongId() > 0)
                        Client.playNetworkSong(Client.currentSongId(), "", Client.currentIsVip())
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
                BasicConfig.currentMediaAlbum=music_album_
                //显示的总时长一般都是C艹端在打开媒体文件后自动往qml端更新
                //这里额外直接设置(部分网易云歌无时长元数据,空值保护为0避免显示NaN)
                var _sec = parseInt(timesize_, 10)
                musicProgressSlier.allSecondTime = isNaN(_sec) ? 0 : _sec
                BasicConfig.currentMediaTimesize = timesize_

                //触发更新
                BasicConfig.currentMediaCoverFilePath=icon_
            }
        }
        // 当前播放信息变化 → 刷新底栏。
        // 三个属性都要挂：只挂封面会在「封面没变但歌名变了」时漏掉刷新
        // （同专辑切歌、漫游歌封面为空），详见 root.refreshNowPlayingUI 的说明。
        Connections{
            target:BasicConfig
            function onCurrentMediaCoverFilePathChanged(){ root.refreshNowPlayingUI() }
            function onCurrentMediaNameChanged(){ root.refreshNowPlayingUI() }
            function onCurrentMediaArtistAuthorChanged(){ root.refreshNowPlayingUI() }
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
            var songId = Client.currentSongId();
            if (songId <= 0) return;              // 仅网易云歌曲（本地/其它平台 songId=0）
            if (!NcmSession.isLoggedIn) return;   // 未登录禁用网易云喜欢操作
            var newLoved = !isLoved;
            NcmPlaylistApi.setLike(songId, newLoved);
            if (newLoved)
                BasicConfig.addNeteaseLiked(songId, BasicConfig.currentMediaCoverFilePath,
                                            BasicConfig.currentMediaName, BasicConfig.currentMediaArtistAuthor,
                                            BasicConfig.currentMediaAlbum, BasicConfig.currentMediaTimesize,
                                            Client.currentIsVip());
            else
                BasicConfig.removeNeteaseLiked(songId);
            _loved = newLoved;
            // 置全局脏标记，下次进入"网易云中我喜欢的音乐"页签时刷新
            BasicConfig.neteaseLikedDirty = true;
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
    // 播放模式切换按钮：四种模式轮换（0=顺序播放 1=单曲循环 2=随机播放 3=网易云漫游）
    Image {
        id: playstateImage
        source: [ "qrc:/image/playstate1.png",
                  "qrc:/image/ph_repeat-once-bold.png",
                  "qrc:/image/fa7-solid_random.png",
                  "qrc:/image/streamline-flex_heart-rate.png" ][BasicConfig.playMode]
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
                // 0→1→2→3→0 轮换；切入网易云漫游(3)需聚焦网易云播放页，否则跳过；关闭漫游(3→0)不受限
                var next = (BasicConfig.playMode + 1) % 4;
                if (next === 3 && !BasicConfig.searchMusicStackActive)
                    next = 0;
                BasicConfig.playMode = next;
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
                BasicConfig.isPlaying = false;
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
        font.family: "微软雅黑"
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
            text:"灵动岛"
            color:deskLyrics_Island.hovered ? Oran7Theme.Oran7MusicPlayControls["iconColorBase-5"] :
                                                                                Oran7Theme.Oran7MusicPlayControls["iconColorBase-7"]
            font.pixelSize:17
            font.family: "微软雅黑"
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
        //     font.family: "微软雅黑"
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
