pragma Singleton
import QtQuick 2.15
import QtQuick.Controls
import QtQml.Models

import Client 1.0

Item {
    id: root
    /*==============================GLOBAL SIGNALS=============================*/
    //use in TopLeftBar
    property real gradientPosition2: 1.0

    //use in TopMiddleMenuBar.qml for LoginPopup open
    signal openNeteaseLoginPopup
    // 自动重新登录挂起标记：Cookie 失效时置 true 并跳搜索页打开登录弹窗；
    // 若搜索页尚未实例化（首次跳转），由 SearchMusicStack.Component.onCompleted 消费后清 false
    property bool pendingNeteaseLoginPopup: false

    //use for BilibiliLoginPopup open
    signal openBilibiliLoginPopup

    //use in LeftPage clear element Rectangle backgroundColor
    signal clearElementBackgroundColorInLeftPage
    //use in LeftPage focusCurrent_SelectedMenuModel
    signal focusCurrent_SelectedMenuModel(var pageName)

    //use in RightPage.qml mainStackView for push SuggestStack into ----at LeftPage
    signal pushVideoPlayerStackInto_RightPageMainStackView
    //use in RightPage.qml mainStackView for push MyFavoriteMusicStack into ----at LeftPage
    signal pushSearchMusicStackInto_RightPageMainStackView
    //use in RightPage.qml mainStackView for push LocakMusicStack into ----at LeftPage
    signal pushLocalMusicStackInto_RightPageMainStackView
    //use in RightPage.qml mainStackView for push screenCaptureStack into ----at LeftPage
    signal pushScreenCaptureStackInto_RightPageMainStackView



    //use in localMusicStack of MainStackView for trigger addly amination
    signal triggerLoad_localMusicList_Aniamtion(var index)

    //use in deal with global mouse clicked and cancel some control "FOCUS"
    signal clickedOutside
    signal newTextAreaFocused(var item)

    //triggered by VideoPlayerStack use in clear all ui in videoRender Area
    /*NOW connect in
        main.qml of openSemiCircle
        VideoPlayerStack.qml of openSemiCircleRect
    */
    signal clearAllUi_inVIdeoRenderArea(var ok)

    /*===============================GLOBAL INSTANCE============================*/
    /*-------------------Global Paragramers--------------------------*/
    //<AppWindowParas>

    // 主窗口引用
    property var mainWindow: null

    // 主窗口背景的共享渲染源（视频/静态图一体的 mainWindowBackground Item）
    // 由 main.qml 赋值，供开发者头像窗口、歌词窗口等复用同一份动态背景，
    // 避免为每处背景各实例化一个视频播放器
    property var backgroundVideoSource: null

    // 用于标志用户是否处于登录状态
    property bool isLogin: false

    // 播放按钮的只读兼容状态，包含打开意图；实际播放状态见 Client.playback.playing。
    readonly property bool isPlaying: Client.playback.active
    //当前播放由哪个列表发起（"search"=搜索结果列表, "local"=本地音乐页列表），
    //自动切歌只在该列表内推进，避免跨列表切错歌
    readonly property string currentPlaylistOwner: Client.musicQueue.owner
    // 播放模式：0=顺序播放 1=单曲循环 2=随机播放 3=网易云漫游（心动模式，BottomPage 播放模式图标轮换控制）
    readonly property int playMode: Client.playback.mode
    // 修改模式统一调用 Client.setPlayMode()。
    // SearchMusicStack 是否处于聚焦(当前显示)状态：仅聚焦时模式轮转才允许切入网易云漫游(3)
    property bool searchMusicStackActive: false
    // 按播放模式计算自动切歌目标索引（direction：1=下一首，-1=上一首）
    // 顺序：±1；单曲循环：重播当前；随机：随机选一首且不等于当前（count>1 时）



    //==================== 网易云登录状态（UI 接口） ====================
    // 预留属性：当前未被 UI 消费（新 ncm 模块直接由 NcmSession/NcmWebLogin 单例提供）
    property bool netEaseIsLogin: false
    property string netEaseUserName: ""
    property string netEaseQrCodeUrl: ""
    //播放器当前播放文件路径
    readonly property string currentMediaFilePath: Client.playback.currentTrack.path
    //播放器当前播放文件的封面，用于底部动态加载
    readonly property string currentMediaCoverFilePath: Client.playback.currentTrack.cover
    //播放器播放的当前音乐文件
    readonly property string currentMediaName: Client.playback.currentTrack.title
    //播放器播放的当前音乐的作者
    readonly property string currentMediaArtistAuthor: Client.playback.currentTrack.artist
    //播放器播放的当前音乐的专辑
    readonly property string currentMediaAlbum: Client.playback.currentTrack.album
    //播放器当前播放音乐的时长（秒）
    readonly property string currentMediaTimesize: Client.playback.currentTrack.timesize

    //进度条精度最大值
    property int max_Slider_Value: 65536
    //记录全局播放器的音量
    readonly property real playerVolume: Client.playback.volume

    //记录当前播放器的焦点,目前在MusicPlayer && VideoPlayer之间切换
    property int globalPlayingFocus: -1
    property int globalPlayer_MusicPlayerIndex: 0
    property int globalPlayer_VideoPlayerIndex: 1
    property int globalPlayer_LivePlayerIndex: 2
    signal playerFocusChanged
    onGlobalPlayingFocusChanged: {
        playerFocusChanged();
    }

    //<全局控件焦点管理器>  item控件必须要有focus属性！！
    property var lastFocusedItem: null
    property var currentFocusedItem: null

    //--------------------PropertyAnimation
    //是否开启颜色循环动画
    property bool runningInfinitePropertyAnimation: true

    //-------------------MainStackView页面工厂
    readonly property int videoPlayerPage: 0
    readonly property int localMusicPage: 1
    readonly property int searchMusicPage: 2
    readonly property int screenCapturePage: 3

    function getUrl(pageType) {
        switch (pageType) {
        case root.videoPlayerPage:
            return "qrc:/src/qml/RightPage/MainStackView/VideoPlayerStack.qml";
        case root.searchMusicPage:
            return "qrc:/src/qml/RightPage/MainStackView/SearchMusicStack.qml";
        case root.localMusicPage:
            return "qrc:/src/qml/RightPage/MainStackView/LocalMusicStack.qml";
        case root.screenCapturePage:
            return "qrc:/src/qml/RightPage/MainStackView/ScreenCaptureStack.qml";
        default:
            return "";
        }
    }
    /*-------------------Global ListModels------------------------*/
    /*myIntegratedMusicListModel*/
    // 实例由 Client 持有，页面只读借用。
    // "我整合的音乐"合集：维护非常杂的数据源（本地音乐 / 网易云免费url / 网易云VIP url /
    // 未来酷狗、QQ 等），通过 source 字段区分来源。
    readonly property var myIntegratedMusicListModel: Client.tracks.integrated
    // 收藏缓存和请求代际由 C++ 服务管理，保留页面的只读兼容入口。
    readonly property bool neteaseLikedDirty: Client.cloudMusic.likedDirty
    readonly property bool neteaseLikedFetching: Client.cloudMusic.likedFetching
    readonly property bool neteaseLikedLoaded: Client.cloudMusic.likedLoaded
    readonly property var neteaseLikedListModel: Client.tracks.liked
    signal neteaseLikedChanged(var songId, bool liked)
    signal neteaseLikedFetchFinished(int count)
    function isNeteaseLiked(songId) { return Client.tracks.isLiked(songId || 0); }
    function fetchNeteaseLikedSongs(forceRefresh) { return Client.cloudMusic.fetchLiked(forceRefresh === true); }
    Connections {
        target: Client.cloudMusic
        function onLikedChanged(songId, liked) { root.neteaseLikedChanged(songId, liked); }
        function onLikedFetchFinished(count) { root.neteaseLikedFetchFinished(count); }
    }
    Connections {
        target: Client.musicQueue
        function onMusicActivated() { root.globalPlayingFocus = root.globalPlayer_MusicPlayerIndex; }
    }
    // 模型和身份索引由 C++ 持有；保留界面调用入口，避免在 QML 重复保存数据。
    function isIntegrated(songId, url, filepath) {
        return Client.tracks.isIntegrated(songId || 0, url || "", filepath || "");
    }
    function addIntegratedMusicItem(icon, name, artist, album, timesize, url, filepath, songId, isVip, source) {
        Client.tracks.addIntegratedTrack({cover: icon, title: name, artist: artist, album: album,
            timesize: timesize, url: url || "", path: filepath || "", songId: songId || 0,
            isVip: isVip === true, source: source || ""});
    }
    function removeIntegratedMusic(songId, url, filepath) {
        return Client.tracks.removeIntegratedTrack(songId || 0, url || "", filepath || "");
    }
    function batchAddIntegratedItems(items) {
        Client.tracks.addIntegratedTracks(items || []);
    }
    readonly property var localMusicListModel: Client.tracks.local

    //=====================================GLOBAL FUNCTION===================================//

    //=====================================GLOBAL Connections==================================//
    Connections {
        target: BasicConfig
        function onNewTextAreaFocused(item)
        {
            BasicConfig.currentFocusedItem = item;
            if (BasicConfig.lastFocusedItem && BasicConfig.lastFocusedItem != BasicConfig.currentFocusedItem) {
                BasicConfig.lastFocusedItem.focus = false;
            }
            BasicConfig.lastFocusedItem = BasicConfig.currentFocusedItem;
        }
    }

    Connections {
        target: globalEventFilter
        function onKeyCombinationTriggered(CombinationS)
        {
            if (CombinationS  === "Ctrl+Right") {
                Client.playNext()
            } else if (CombinationS === "Ctrl+Left") {
                Client.playPrevious()
            }
        }
    }
}
