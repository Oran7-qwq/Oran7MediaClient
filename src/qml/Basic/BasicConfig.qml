pragma Singleton
import QtQuick 2.15
import QtQuick.Controls
import QtQml.Models

import Client 1.0
import Oran7NetEaseAuth 1.0
import Oran7Async 1.0

Item {
    id: root
    /*==============================GLOBAL SIGNALS=============================*/
    //use in TopLeftBar and SearchPopup
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

    //播放器是否处于播放状态
    property bool isPlaying: false
    //当前播放由哪个列表发起（"search"=搜索结果列表, "local"=本地音乐页列表），
    //自动切歌只在该列表内推进，避免跨列表切错歌
    property string currentPlaylistOwner: ""
    // 播放模式：0=顺序播放 1=单曲循环 2=随机播放 3=网易云漫游（心动模式，BottomPage 播放模式图标轮换控制）
    property int playMode: 0
    // 同步给 C++（预留：单曲循环/漫游等模式由 C++ 感知播放状态）
    onPlayModeChanged: Client.setPlayMode(BasicConfig.playMode)
    // SearchMusicStack 是否处于聚焦(当前显示)状态：仅聚焦时模式轮转才允许切入网易云漫游(3)
    property bool searchMusicStackActive: false
    // 按播放模式计算自动切歌目标索引（direction：1=下一首，-1=上一首）
    // 顺序：±1；单曲循环：重播当前；随机：随机选一首且不等于当前（count>1 时）
    function nextPlayIndex(curIdx, count, direction) {
        if (!count || count <= 0) return -1;
        if (count === 1) return 0;
        if (root.playMode === 1)
            return curIdx < 0 ? 0 : curIdx;
        if (root.playMode === 2)
            return (curIdx + 1 + Math.floor(Math.random() * (count - 1))) % count;
        if (direction > 0)
            return (curIdx + 1) % count;
        return curIdx - 1 < 0 ? count - 1 : curIdx - 1;
    }

    //==================== 网易云登录状态（UI 接口） ====================
    // 预留属性：当前未被 UI 消费（新 ncm 模块直接由 NcmSession/NcmQrLogin 单例提供）
    property bool netEaseIsLogin: false
    property string netEaseUserName: ""
    property string netEaseQrCodeUrl: ""
    //播放器当前播放文件路径
    property string currentMediaFilePath: ""
    //播放器当前播放文件的封面，用于底部动态加载
    property string currentMediaCoverFilePath: ""
    //播放器播放的当前音乐文件
    property string currentMediaName: ""
    //播放器播放的当前音乐的作者
    property string currentMediaArtistAuthor: ""
    //播放器播放的当前音乐的专辑
    property string currentMediaAlbum: ""
    //播放器当前播放音乐的时长（秒）
    property string currentMediaTimesize: ""

    //进度条精度最大值
    property int max_Slider_Value: 65536
    //记录全局播放器的音量
    property real playerVolume: 25

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
    readonly property int settingPage: 4

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
        case root.settingPage:
            return "qrc:/src/qml/RightPage/MainStackView/SettingStack.qml";
        default:
            return "";
        }
    }
    /*-------------------Global ListModels------------------------*/
    /*myIntegratedMusicListModel*/
    // 初始化为空指针，在RightPage.qml的mainStackView中实例化并传入全局单例供各页面使用。
    // "我整合的音乐"合集：维护非常杂的数据源（本地音乐 / 网易云免费url / 网易云VIP url /
    // 未来酷狗、QQ 等），通过 source 字段区分来源。
    property ListModel myIntegratedMusicListModel: null
    // 全局脏标记：任意地方做了网易云 like/unlike 后置 true，进入"网易云中我喜欢的音乐"页签时据此刷新歌单
    property bool neteaseLikedDirty: false
    // 云端歌单拉取进行中标志（替代原 NcmPlaylistApi.m_fetchInProgress，防并发拉取重复 append）
    property bool neteaseLikedFetching: false
    // 网易云"我喜欢的音乐"歌单模型（云端 like 状态唯一来源，搜索结果/BottomPage 用它判断小心心）
    property ListModel neteaseLikedListModel: ListModel {}
    // 网易云喜欢状态变化信号（任意入口 like/unlike 后发出，供搜索结果等列表同步小心心）
    signal neteaseLikedChanged(int songId, bool liked)
    // 云端"我喜欢的音乐"歌单拉取完成（由 applyNeteaseLikedFetchResult 发出，供 SearchMusicStack 等做定位/动画）
    signal neteaseLikedFetchFinished(int count)

    // 判断某 songId 是否网易云喜欢
    function isNeteaseLiked(songId) {
        if (!neteaseLikedListModel || !songId || songId <= 0) return false;
        for (var i = 0; i < neteaseLikedListModel.count; i++) {
            if (neteaseLikedListModel.get(i).songId === songId) return true;
        }
        return false;
    }
    // 添加网易云喜欢（并写入全局集合；新喜欢的歌插到列表最上面，与网易云官网“我喜欢的音乐”新歌在前一致）
    function addNeteaseLiked(songId, icon, name, artist, album, timesize, isVip) {
        if (!neteaseLikedListModel || !songId || songId <= 0) return;
        if (isNeteaseLiked(songId)) return;
        neteaseLikedListModel.insert(0, {
            icon: icon, music_name: name, music_artist: artist,
            music_album: album, timesize: timesize, filepath: "", url: "",
            songId: songId, isVip: isVip === true, isLoved: true,
            isSelected: false, needsAnimateIn: true
        });
        neteaseLikedChanged(songId, true);
    }
    // 移除网易云喜欢（从全局集合移除）
    function removeNeteaseLiked(songId) {
        if (!neteaseLikedListModel || !songId || songId <= 0) return;
        for (var i = 0; i < neteaseLikedListModel.count; i++) {
            if (neteaseLikedListModel.get(i).songId === songId) {
                neteaseLikedListModel.remove(i);
                neteaseLikedChanged(songId, false);
                return;
            }
        }
    }

    // 云端"我喜欢的音乐"歌单拉取结果的统一落盘入口（启动预拉取 / 页签进入 / 登录 / 排序重试共用）。
    // result 由 Oran7AsyncManager("fetchLikedSongs") handler 返回：{ playlistId, songs:[...] }
    function applyNeteaseLikedFetchResult(result) {
        if (!result) return;
        NcmPlaylistApi.likedPlaylistId = result.playlistId || 0;   // 供 reorderLikedSongs / roamSongs 使用
        var songs = result.songs || [];
        for (var i = 0; i < songs.length; i++) {
            var s = songs[i];
            neteaseLikedListModel.append({
                icon: s.icon, music_name: s.music_name, music_artist: s.music_artist,
                music_album: s.music_album, timesize: s.timesize,
                filepath: "", url: "", songId: s.songId, isVip: s.isVip === true,
                isLoved: true, isSelected: false, needsAnimateIn: true
            });
        }
        neteaseLikedFetchFinished(songs.length);
    }

    // 云端"我喜欢的音乐"统一拉取入口（Oran7AsyncManager 线程池 handler）。
    // 启动预拉取 / 页签进入 / 登录 / 排序重试共用；neteaseLikedFetching 防并发拉取。
    function fetchNeteaseLikedSongs() {
        if (neteaseLikedFetching) return;
        neteaseLikedFetching = true;
        Oran7AsyncManager.run("fetchLikedSongs", {
            cookie: NcmSession.cookieString,
            csrf: NcmSession.csrfToken
        }, function(result) {
            BasicConfig.neteaseLikedFetching = false;
            BasicConfig.applyNeteaseLikedFetchResult(result);
        }, function(error) {
            BasicConfig.neteaseLikedFetching = false;
            console.warn("云端我喜欢的音乐拉取失败:", error);
            BasicConfig.neteaseLikedDirty = true;   // 保留脏标记，下次进入页签可重试
        });
    }
    // ---------------- "我整合的音乐"查重哈希表 ----------------
    // 目的：批量整合/逐条整合判重 O(1)，避免每次遍历整个整合列表（元素多时卡顿）
    // 键格式：songId -> "s:<id>"；url -> "u:<url>"；filepath -> "p:<path>"
    property var integratedKeySet: ({})
    property bool integratedKeySetReady: false

    // 生成某歌曲的查重键数组
    function _integratedKeys(songId, url, filepath) {
        var keys = [];
        if (songId > 0) keys.push("s:" + songId);
        if (url && url.length > 0) keys.push("u:" + url);
        if (filepath && filepath.length > 0) keys.push("p:" + filepath);
        return keys;
    }

    // 从整合模型全量重建查重哈希表（加载完成后/模型被外部清空时调用）
    function rebuildIntegratedKeySet() {
        integratedKeySet = ({});
        integratedKeySetReady = true;
        if (!myIntegratedMusicListModel) return;
        for (var i = 0; i < myIntegratedMusicListModel.count; i++) {
            var m = myIntegratedMusicListModel.get(i);
            var keys = _integratedKeys(m.songId, m.url, m.filepath);
            for (var k = 0; k < keys.length; k++)
                integratedKeySet[keys[k]] = true;
        }
    }

    // O(1) 判重：songId / url / filepath 任一命中即视为已整合
    function keyIsIntegrated(songId, url, filepath) {
        if (!integratedKeySetReady)
            rebuildIntegratedKeySet();
        var keys = _integratedKeys(songId, url, filepath);
        for (var k = 0; k < keys.length; k++) {
            if (integratedKeySet[keys[k]] === true) return true;
        }
        return false;
    }

    function _addIntegratedKeys(songId, url, filepath) {
        var keys = _integratedKeys(songId, url, filepath);
        for (var k = 0; k < keys.length; k++)
            integratedKeySet[keys[k]] = true;
    }

    function _removeIntegratedKeys(songId, url, filepath) {
        var keys = _integratedKeys(songId, url, filepath);
        for (var k = 0; k < keys.length; k++)
            delete integratedKeySet[keys[k]];
    }

    // 判断某歌曲是否已整合到"我整合的音乐"：按 songId / url / filepath 任一命中即视为已整合
    // （VIP 歌 filepath/url 为空，必须支持按 songId 匹配；本地歌则按 filepath 匹配）
    function isIntegrated(songId, url, filepath) {
        if (!myIntegratedMusicListModel) return false;
        return keyIsIntegrated(songId, url, filepath);
    }
    // 整合歌曲到"我整合的音乐"：url 为首选可播放源，filepath 为兜底（播放时优先 url，不可用则用 filepath）
    // 新整合插入到列表顶部（index 0），最新整合排在最上面
    // songId 用于会员歌拉歌词；isVip 用于列表 VIP 标识；source 区分数据来源（local/netease/未来 kugou/qq）
    function addIntegratedMusicItem(icon, name, artist, album, timesize, url, filepath, songId, isVip, source) {
        if (!myIntegratedMusicListModel) {
            console.warn("myIntegratedMusicListModel is not initialized!");
            return;
        }
        if (keyIsIntegrated(songId, url, filepath)) return;   // O(1) 去重
        // 先更新哈希表再插入模型：insert 会触发 countChanged（BottomPage 等据此刷新整合状态），
        // 若后更新哈希，刷新时会读到旧哈希导致状态不一致
        _addIntegratedKeys(songId, url, filepath);
        myIntegratedMusicListModel.insert(0, {
            icon: icon,
            music_name: name,
            music_artist: artist,
            music_album: album,
            timesize: timesize,
            url: url,
            filepath: filepath,
            songId: songId || 0,
            isVip: isVip === true,
            source: source || "",
            //--- state properties ---
            isSelected: false
        });
        persistIntegratedMusic();
    }

    // 移除一条整合（songId / url / filepath 任一命中即移除）并持久化，返回是否真的移除了
    function removeIntegratedMusic(songId, url, filepath) {
        if (!myIntegratedMusicListModel) return false;
        for (var i = 0; i < myIntegratedMusicListModel.count; i++) {
            var m = myIntegratedMusicListModel.get(i);
            var hit = (songId > 0 && m.songId === songId)
                   || (url && url.length > 0 && m.url === url)
                   || (filepath && filepath.length > 0 && m.filepath === filepath);
            if (hit) {
                _removeIntegratedKeys(m.songId, m.url, m.filepath);
                myIntegratedMusicListModel.remove(i);
                persistIntegratedMusic();
                return true;
            }
        }
        return false;
    }

    // 批量整合（多选）：基于哈希查重 O(1) 跳过已整合项，分片异步插入避免阻塞 UI。
    // items：数组，每项 {icon, music_name, music_artist, music_album, timesize, url, filepath, songId, isVip, source}
    property var _pendingBatchItems: []
    property int _batchChunkSize: 30
    Timer {
        id: _integratedBatchTimer
        interval: 16
        repeat: false
        onTriggered: root._flushIntegratedBatch()
    }

    function _flushIntegratedBatch() {
        if (!myIntegratedMusicListModel) {
            _pendingBatchItems = [];
            return;
        }
        var n = Math.min(_batchChunkSize, _pendingBatchItems.length);
        for (var i = 0; i < n; i++) {
            // 从尾部取再 insert(0)，保证顶部顺序与收集顺序一致（先选中的在最上）
            var obj = _pendingBatchItems.pop();
            // 先更新哈希表再插入模型（同 addIntegratedMusicItem，避免 countChanged 刷新读到旧哈希）
            _addIntegratedKeys(obj.songId, obj.url, obj.filepath);
            myIntegratedMusicListModel.insert(0, obj);
        }
        if (_pendingBatchItems.length > 0)
            _integratedBatchTimer.start();
        else
            persistIntegratedMusic();
    }

    function batchAddIntegratedItems(items)
    {
        if (!myIntegratedMusicListModel || !items || items.length === 0) return;
        if (!integratedKeySetReady)
            rebuildIntegratedKeySet();
        var toAdd = [];
        var localSet = {};   // 批内判重，避免同一批里重复加入
        for (var i = 0; i < items.length; i++) {
            var it = items[i];
            var songId = it.songId || 0;
            var isVip = it.isVip === true;
            // VIP 音源链接会过期，不持久化（存空），播放时按 songId 重新解析
            var playSrc = (it.url && it.url.length > 0) ? it.url : (it.filepath || "");
            var saveSrc = isVip ? "" : (playSrc || "");
            var keys = _integratedKeys(songId, saveSrc, saveSrc);
            var dup = false;
            for (var k = 0; k < keys.length; k++) {
                if (integratedKeySet[keys[k]] === true || localSet[keys[k]] === true) {
                    dup = true;
                    break;
                }
            }
            if (dup) continue;
            for (var kk = 0; kk < keys.length; kk++)
                localSet[keys[kk]] = true;
            toAdd.push({
                icon: it.icon,
                music_name: it.music_name,
                music_artist: it.music_artist,
                music_album: it.music_album,
                timesize: it.timesize,
                url: saveSrc,
                filepath: saveSrc,
                songId: songId,
                isVip: isVip,
                source: it.source || "",
                isSelected: false
            });
        }
        if (toAdd.length === 0) return;
        _pendingBatchItems = toAdd;
        _flushIntegratedBatch();
    }

    // 序列化整合模型为 JS 数组（保持模型顺序，最新在前；不含 isSelected 等 UI 状态）
    function serializeIntegratedMusic() {
        var arr = [];
        if (!myIntegratedMusicListModel) return arr;
        for (var i = 0; i < myIntegratedMusicListModel.count; i++) {
            var m = myIntegratedMusicListModel.get(i);
            arr.push({
                icon: m.icon,
                music_name: m.music_name,
                music_artist: m.music_artist,
                music_album: m.music_album,
                timesize: m.timesize,
                url: m.url,
                filepath: m.filepath,
                songId: m.songId || 0,
                isVip: m.isVip === true,
                source: m.source || ""
            });
        }
        return arr;
    }

    // 持久化整合到 config.json 的 integratedMusic.list（每次增删后调用，顺序=模型顺序）
    function persistIntegratedMusic() {
        if (myIntegratedMusicListModel)
            Client.saveIntegratedMusic(serializeIntegratedMusic());
    }

    // 从 config.json 加载整合（应用启动时调用），按存盘顺序重建模型
    // 注意：存盘不含 isSelected 等 UI 状态，重建时必须补默认值，否则 setProperty/model.isSelected 失效
    function loadIntegratedMusicFromStore() {
        if (!myIntegratedMusicListModel) return;
        var items = Client.loadIntegratedMusic();
        myIntegratedMusicListModel.clear();
        for (var i = 0; i < items.length; i++) {
            var m = items[i];
            myIntegratedMusicListModel.append({
                icon: m.icon,
                music_name: m.music_name,
                music_artist: m.music_artist,
                music_album: m.music_album,
                timesize: m.timesize,
                url: m.url,
                filepath: m.filepath,
                songId: m.songId || 0,
                isVip: m.isVip === true,
                source: m.source || "",
                isSelected: false
            });
        }
        rebuildIntegratedKeySet();   // 重建查重哈希表
    }

    /*locakMusicListModel*/
    //初始化为空指针，在RightPage.qml的mainStackView中实例化并传入全局单例供LocalMusicStack.qml使用
    //property bool alreadyInit: false
    property ListModel localMusicListModel: null
    function addLocalMusicItem(icon, name, artist, album, timesize, filepath) {
        if (localMusicListModel) {
            // console.log("qml:new add");
            localMusicListModel.append({
                icon: icon,
                music_name: name,
                music_artist: artist,
                music_album: album,
                timesize: timesize,
                filepath: filepath,
                //--- state properties ---
                isSelected: false
            });
            // console.log(JSON.stringify(localMusicListModel.get(0)));
        } else
            console.warn("localMusicListModel is not initialized!");
    }

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
            }
        }
    }
}
