pragma Singleton
import QtQuick 2.15
import QtQuick.Controls
import QtQml.Models

import Client 1.0

Item {
    id: root
    /*==============================GLOBAL SIGNALS=============================*/
    //use in TopLeftBar and SearchPopup
    property real gradientPosition2: 1.0

    //use in TopMiddleMenuBar.qml for LoginPopup open
    signal openLoginPopup

    //use in LoginPopup.qml for MainLoginPopup open
    signal openMainLoginPopup

    //use in LeftPage clear element Rectangle backgroundColor
    signal clearElementBackgroundColorInLeftPage
    //use in LeftPage focusCurrent_SelectedMenuModel
    signal focusCurrent_SelectedMenuModel(var pageName)

    //use in MainStackView for SettingStack Top of subTitle maxiHightLight Clear
    signal clearElementSubTitleMaxiHightLight_InSettingStack

    //use in RightPage.qml mainStackView for push SuggestStack into ----at LeftPage
    signal pushVideoPlayerStackInto_RightPageMainStackView
    //use in RightPage.qml mainStackView for push MyFavoriteMusicStack into ----at LeftPage
    signal pushMyFavoriteMusicStackInto_RightPageMainStackView
    //use in RightPage.qml mainStackView for push LocakMusicStack into ----at LeftPage
    signal pushLocalMusicStackInto_RightPageMainStackView
    //use in RightPage.qml mainStackView for push screenCaptureStack into ----at LeftPage
    signal pushScreenCaptureStackInto_RightPageMainStackView

    //use in MainStackView for MyFavoriteMusicStack Top of subTitle maxiHightLight Clear
    signal clearElementSubTitleMaxiHightLight_InMyFavoriteMusicStack
    //use in MainStackView for LocalMusicStack Top of subTitle maxiHightLight Clear
    signal clearElementSubTitleMaxiHightLight_InLocalMusicStack

    //use in MyFavoriteMusicStack && LocalMusicStack && BottomPage PlayIcon Updata to Current PlayingState
    signal updatePlayIconToCurrentPlayingState
    //use in MyFavoriteMusicStack && LocalMusicStack for Reset all playlist header Icon to "ClearPlay"
    signal resetAllPlayListHeadicon
    //use in MyFavoriteMusicStack && LocalMusicStack for updata bottomPage to new display MusicMeida information
    signal updataBottomMusicMediaDisplayInformation

    // //use in MyFavoriteMusicStack && LocalMusicStack for focus current music in display list//-->Discard 2026/5/4
    // signal focusCurrentMusicInDisplayList()

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

    // 用于标志用户是否处于登录状态
    property bool isLogin: false

    //播放器是否处于播放状态
    property bool isPlaying: false
    //播放器当前播放文件路径
    property string currentMediaFilePath: ""
    //播放器当前播放文件的封面，用于底部动态加载
    property string currentMediaCoverFilePath: ""
    //播放器播放的当前音乐文件
    property string currentMediaName: ""
    //播放器播放的当前音乐的作者
    property string currentMediaArtistAuthor: ""

    //进度条最大值
    property int max_Slider_Value: 65536
    //记录全局播放器的音量
    property real playerVolume: 25

    //记录当前播放器的焦点,目前在MusicPlayer && VideoPlayer之间切换
    property int globalPlayingFocus: -1
    property int globalPlayer_MusicPlayerIndex: 0
    property int globalPlayer_VideoPlayerIndex: 1
    property int globalPlayer_LivePlayerIndex: 2
    //全局播放器使用foucus工厂
    property var globalPlayerItems: [
        {
            focusedPlayingItem: "MusicPlayer"
        }//0
        ,
        {
            focusedPlayingItem: "VideoPlayer"
        }//1
    ]
    signal playerFocusChanged
    onGlobalPlayingFocusChanged: {
        playerFocusChanged();
    }

    //记录当前播放列表的当前索引#index
    property int playingIndex: -1
    //save last addLocalMusic opend fileDialog direction path
    property string lastAddLocalMusicFolderPath: ""

    //<全局控件焦点管理器>  item控件必须要有focus属性！！
    property var lastFocusedItem: null
    property var currentFocusedItem: null

    //--------------------PropertyAnimation
    //是否开启颜色循环动画
    property bool runningInfinitePropertyAnimation: true

    //-------------------MainStackView页面工厂
    readonly property int videoPlayerPage: 0
    readonly property int localMusicPage: 1
    readonly property int myFavoritePage: 2
    readonly property int screenCapturePage: 3
    readonly property int settingPage: 4

    function getUrl(pageType) {
        switch (pageType) {
        case root.videoPlayerPage:
            return "qrc:/Src/RightPage/MainStackView/VideoPlayerStack.qml";
        case root.myFavoritePage:
            return "qrc:/Src/RightPage/MainStackView/MyFavoriteMusicStack.qml";
        case root.localMusicPage:
            return "qrc:/Src/RightPage/MainStackView/LocalMusicStack.qml";
        case root.screenCapturePage:
            return "qrc:/Src/RightPage/MainStackView/ScreenCaptureStack.qml";
        case root.settingPage:
            return "qrc:/Src/RightPage/MainStackView/SettingStack.qml";
        default:
            return "";
        }
    }
    /*-------------------Global ListModels------------------------*/
    /*myFavoriteMusicListModel*/
    // 初始化为空指针，在RightPage.qml的mainStackView中实例化并传入全局单例供MyFavoriteMusicStack.qml使用
    property ListModel myFavoriteMusicListModel: null
    function addMyFavoriteMusicItem(icon, name, artist, album, timesize) {
        if (myFavoriteMusicListModel) {
            // console.log("qml:new add");
            myFavoriteMusicListModel.append({
                icon: icon,
                music_name: name,
                music_artist: artist,
                music_album: album,
                timesize: timesize
            });
        } else
            console.warn("myFavoriteMusicListModel is not initialized!");
    }

    /*locakMusicListModel*/
    //初始化为空指针，在RightPage.qml的mainStackView中实例化并传入全局单例供LocalMusicStack.qml使用
    //property bool alreadyInit: false
    property ListModel localMusicListModel: null
    function addLocalMusicItem(icon, name, artist, album, timesize, music_id, filepath) {
        if (localMusicListModel) {
            // console.log("qml:new add");
            localMusicListModel.append({
                icon: icon,
                music_name: name,
                music_artist: artist,
                music_album: album,
                timesize: timesize,
                filepath: filepath
            });
            // console.log(JSON.stringify(localMusicListModel.get(0)));
        } else
            console.warn("localMusicListModel is not initialized!");
    }

    //=====================================GLOBAL FUNCTION===================================//

    //=====================================GLOBAL Connections==================================//
    Connections {
        target: BasicConfig
        function onNewTextAreaFocused(item) {
            BasicConfig.currentFocusedItem = item;
            if (BasicConfig.lastFocusedItem && BasicConfig.lastFocusedItem != BasicConfig.currentFocusedItem) {
                BasicConfig.lastFocusedItem.focus = false;
            }
            BasicConfig.lastFocusedItem = BasicConfig.currentFocusedItem;
        }
    }
}
