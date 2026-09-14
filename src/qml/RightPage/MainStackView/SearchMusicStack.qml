import QtQuick 2.15
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import QtCore
import QtQuick.Dialogs
import "../../Basic"
import "../../Components"
import Client 1.0
import Oran7MusicSearch 1.0
import Oran7NetEaseAuth 1.0
import Oran7Async 1.0

import Oran7UI.Impl

Item {
    id: root
    anchors.left: parent.left
    anchors.leftMargin: 0
    anchors.top: parent.top
    anchors.topMargin: 14
    clip: true

    //====================   define Properties    =====================//

    property string pageName: ""

    // 搜索结果的内部缓存 ListModel
    // 暂时存放通过第三方(网易云)音乐库搜索到的资源URL等音乐信息列表；
    // 后续阶段由搜索服务把结果写入此模型
    property ListModel searchCacheListModel: ListModel {
    }

    // 云端"网易云中我喜欢的音乐"歌单模型：用 BasicConfig 全局集合（搜索结果/BottomPage 共享）
    // 云端歌单是否已拉取过（避免每次激活重复拉取）
    property bool _likedLoaded: false
    // 小心心点击冷却时间戳（防频繁点击同步网易云，冷却 2 秒）
    property double _lastLikeClick: 0
    // 云端歌单是否已拉取过（首次进入需要拉取）
    property bool _likedEverLoaded: false
    // 云端歌单上次拉取时间戳（>30 秒进入页签则重新拉取，感知网易云官网/程序外更新）
    property double _lastLikedFetch: 0

    // ---- 网易云漫游（心动模式）队列 ----
    // 队列式缓存：旧的出队、新的入队；请求新一批后把已播放过的前段删掉
    property var _roamQueue: []
    property int _roamIndex: 0
    property bool _roamFetching: false
    property bool _roamWaiting: false   // true=队列耗尽等待新批，新批到达后立即开播
    // 队首再上一首的延迟重播：先停掉播放器再重播，C++ 才会走正常播放路径而非"同一首=切换暂停"
    property var _restartRoamSong: null
    Timer {
        id: _restartRoamTimer
        interval: 350
        repeat: false
        onTriggered: {
            var song = root._restartRoamSong;
            root._restartRoamSong = null;
            // 期间若已切到其它歌（songId 不匹配）则放弃重播
            if (song && Client.currentSongId() === (song.songId || 0))
                root.playRoamSong(song);
        }
    }

    // 清空漫游队列（退出心动模式/播放来源切换时调用）
    function clearRoamQueue() {
        _restartRoamTimer.stop();
        _restartRoamSong = null;
        _roamQueue = [];
        _roamIndex = 0;
        _roamFetching = false;
        _roamWaiting = false;
    }

    // 播放一首漫游推荐歌（不在任何列表模型里，独立于 owner 模型）
    function playRoamSong(song) {
        if (!song) return;
        // 兼容两类字段格式：漫游推荐(name/artist/album) 与 列表模型(music_name/music_artist/music_album)。
        // 漫游歌单里可能混入与"网易云中我喜欢的音乐"(neteaseLikedListModel)同源的数据，
        // 字段缺失时兜底取值，保证 BottomPage 歌曲名/作者/封面始终能加载
        var name = song.name !== undefined ? song.name : song.music_name;
        var artist = song.artist !== undefined ? song.artist : song.music_artist;
        var album = song.album !== undefined ? song.album : song.music_album;
        var icon = song.icon !== undefined ? song.icon : "";
        var timesize = song.timesize !== undefined ? song.timesize : "";
        Client.setCurrentMediaSongId(song.songId || 0);
        Client.setCurrentMediaIsVip(song.isVip === true);
        Client.rememberLastMedia(name, artist, album, icon,
                                 timesize, song.songId || 0, song.isVip === true);
        Client.playNetworkSong(song.songId || 0, "", song.isVip === true);
        BasicConfig.isPlaying = true;
        BasicConfig.currentMediaName = name;
        BasicConfig.currentMediaArtistAuthor = artist;
        BasicConfig.currentMediaFilePath = "";
        BasicConfig.currentMediaCoverFilePath = icon;
        BasicConfig.currentMediaAlbum = album;
        BasicConfig.currentMediaTimesize = timesize;
        // 漫游歌不在列表，聚焦索引置 -1；并清除播放归属（不隶属任何列表），
        // 避免后续切歌/解析失败误用上一次点击"网易云中我喜欢的音乐"时的归属模型
        searchMuscStack_playlistView._ownerModel = null;
        searchMuscStack_playlistView._ownerIndex = -1;
        searchMuscStack_playlistView.curPlayingIndex = -1;
        searchMuscStack_playlistView.curSelectingIndex = -1;
    }

    // 播漫游队列的下一首；剩余不足一半时预取新一批（不打断当前播放）
    function playNextRoamSong() {
        if (_roamIndex < _roamQueue.length) {
            var song = _roamQueue[_roamIndex];
            _roamIndex++;
            playRoamSong(song);
            // 队列式预取：剩余未播不足一半时提前请求下一批（以当前歌为种子）
            var remaining = _roamQueue.length - _roamIndex;
            if (_roamQueue.length >= 2 && remaining <= Math.floor(_roamQueue.length / 2) && !_roamFetching) {
                _roamFetching = true;
                root.requestRoam(Client.currentSongId());
            }
            return;
        }
        // 队列耗尽：以当前（刚播完的）歌为种子请求新一批，标记等待
        var seed = Client.currentSongId();
        if (seed > 0 && !_roamFetching) {
            _roamFetching = true;
            _roamWaiting = true;
            root.requestRoam(seed);
        } else if (seed <= 0) {
            // 无种子则回退普通切歌
            searchMuscStack_playlistView._ownerModel = null;
            searchMuscStack_playlistView._ownerIndex = -1;
            root.stopNetworkPlayback();
        }
    }

    // 无歌可播时真正停止播放器：仅置 isPlaying=false 不会停止音频，
    // 会导致播放图标显示"暂停"但音乐仍在继续（如漫游队列被清空后切歌）
    function stopNetworkPlayback() {
        Client.stopRequested();
        BasicConfig.isPlaying = false;
        searchMuscStack_playlistView.curPlayingIndex = -1;
    }

    // 播漫游队列的上一首：当前正在播的是 _roamIndex-1，其上一首是 _roamIndex-2。
    // 队首（_roamIndex==1）再上一首 = 延迟重播当前歌；队列尚无漫游歌时保持当前不动。
    // 始终返回 true，不回落归属列表，避免误切到"网易云中我喜欢的音乐"等列表
    function playPreviousRoamSong() {
        var prevIndex = _roamIndex - 2;
        if (prevIndex >= 0 && prevIndex < _roamQueue.length) {
            var song = _roamQueue[prevIndex];
            _roamIndex = prevIndex + 1;
            playRoamSong(song);
            return true;
        }
        // 已回到队首（_roamIndex==1，当前为第 0 首）：再上一首 = 重播当前歌。
        // 直接 playRoamSong 会被 C++ 判定"同一首正在播"而切暂停，需先停掉播放器再延迟重播
        if (_roamIndex === 1 && _roamQueue.length > 0) {
            root._restartRoamSong = _roamQueue[0];
            Client.stopRequested();
            _restartRoamTimer.restart();
            return true;
        }
        // 漫游队列尚无已播放的漫游歌（当前为种子歌/队列被清空）：保持当前歌曲不动，
        // 不回落归属列表，避免误切到"网易云中我喜欢的音乐"等列表
        return true;
    }

    // 同步搜索结果列表里指定 songId 的心形状态（云端/搜索结果 like/unlike 后保持一致）
    function syncSearchLoved(songId, liked) {
        if (!songId || songId <= 0) return;
        for (var k = 0; k < searchCacheListModel.count; k++) {
            if (searchCacheListModel.get(k).songId === songId)
                searchCacheListModel.setProperty(k, "isLoved", liked);
        }
    }

    // 登录后/云端歌单拉取完成后，按全局网易云集合刷新搜索结果列表所有项的心形状态
    function refreshSearchLovedAll() {
        for (var k = 0; k < searchCacheListModel.count; k++) {
            var sid = searchCacheListModel.get(k).songId;
            var liked = sid > 0 && BasicConfig.isNeteaseLiked(sid);
            if (searchCacheListModel.get(k).isLoved !== liked)
                searchCacheListModel.setProperty(k, "isLoved", liked);
        }
    }

    // 网络歌解析失败连续计数（防无限切歌；达到列表数量即暂停）
    property int _networkFailCount: 0

    // 搜索代数：每次 runSearch 自增；回调里若代数已过期（期间又发起新搜索）则丢弃，防止旧结果覆盖新结果
    property int _searchGeneration: 0

    // 搜索框触发搜索的信号（由网易云搜索服务消费）
    signal searchTriggered(string keywords)
    onSearchTriggered: function(keywords) {
        // 先切回"搜索结果"页签，再触发搜索（点图标/回车两条路都走这里）
        searchMuscStack_playlistView.switchToPage(0);
        root.runSearch(keywords);
    }

    // 网易云搜索：统一走 Oran7AsyncManager("musicSearch") 线程池 handler
    function runSearch(keywords) {
        var gen = ++root._searchGeneration;
        // 搜索开始即清空并重置索引（对应原 onSearchStarted）
        searchCacheListModel.clear();
        searchMuscStack_playlistView.curPlayingIndex = -1;
        searchMuscStack_playlistView.curSelectingIndex = -1;

        Oran7AsyncManager.run("musicSearch", {
            keywords: keywords,
            cookie: NcmSession.cookieString,
            isLoggedIn: NcmSession.isLoggedIn
        }, function(result) {
            if (gen !== root._searchGeneration) return;   // 过期结果（期间又发起新搜索），丢弃
            // --- 原 onSearchResultReady：批量填充；songId 已由 C++ handler 算好 ---
            for (var i = 0; i < result.length; i++) {
                var s = result[i];
                searchCacheListModel.append({
                    icon: s.icon,
                    music_name: s.music_name,
                    music_artist: s.music_artist,
                    music_album: s.music_album,
                    timesize: s.timesize,
                    filepath: s.filepath,
                    songId: s.songId,
                    isVip: s.isVip,
                    isLoved: BasicConfig.isNeteaseLiked(s.songId),
                    isSelected: false,
                    needsAnimateIn: true
                });
            }
            // --- 原 onSearchFinished：重新定位当前播放 + 入场动画 ---
            var playingIdx = searchMuscStack_playlistView.indexForCurrentPlaying();
            searchMuscStack_playlistView.curPlayingIndex = BasicConfig.isPlaying ? playingIdx : -1;
            searchMuscStack_playlistView.curSelectingIndex = playingIdx;
            if (result.length > 0)
                searchMuscStack_playlistView.animateItemsFromIndex(0);
        }, function(error) {
            if (gen !== root._searchGeneration) return;   // 过期请求的失败，忽略
            console.warn("MusicSearch failed:", error);   // 原 onSearchFailed
        });
    }

    // 获取mainWindow
    function findMainWindow() {
        return BasicConfig.mainWindow;
    }

    property var mainWindow: null

    // ==================== StackView 活动状态检测 ====================//
    property bool isActive: false
    // 页面激活：提示文案淡入并计时 5 秒后淡出；页面失活：停止计时立即淡出
    onIsActiveChanged: {
        // 同步聚焦状态到全局：仅聚焦时底部模式轮转才允许切入网易云漫游(3)
        BasicConfig.searchMusicStackActive = root.isActive;
        // 漫游（心动模式）仅在本搜索页内可用：离开该 Stack 时若仍处于 3，强制切回顺序播放(0)，(3+1)%4=0
        if (!root.isActive && BasicConfig.playMode === 3)
            BasicConfig.playMode = 0;
        if (root.isActive) {
            // 超过三次不再重新显示
            if (ncmPlainTextLabel.showCount >= 3)
                return;
            ncmPlainTextLabel.opacity = 1.0;
            ncmHintFadeTimer.restart();
            ncmPlainTextLabel.showCount += 1;
        } else {
            ncmHintFadeTimer.stop();
            ncmPlainTextLabel.opacity = 0.0;
        }
    }
    // 提示文案展示计时：激活 5 秒后自动淡出
    Timer {
        id: ncmHintFadeTimer
        interval: 5000
        onTriggered: ncmPlainTextLabel.opacity = 0.0
    }
    property var _myStackView: null
    property var stackView_rootItemContainer: root //包含此组件的外部作为stackViewItem的根项

    function findAndSetupStackView() {
        if (StackView.view) {
            _myStackView = StackView.view;
        } else {
            var parent = container.parent;
            var depth = 0;
            while (parent && depth < 20) {
                if (typeof parent.push === "function" && typeof parent.pop === "function") {
                    _myStackView = parent;
                    break;
                }
                parent = parent.parent;
                depth++;
            }
        }

        if (_myStackView) {
            _myStackView.currentItemChanged.connect(updateActiveState);
            updateActiveState();
        }
    }

    function updateActiveState() {
        if (!_myStackView || !_myStackView.currentItem) {
            root.isActive = false;
            return;
        }

        root.isActive = (_myStackView.currentItem === root.stackView_rootItemContainer);
        if (root.isActive) {
            //Page被激活 -> touch 业务
            root.activeResponseTask()
        }
    }

    //Page被激活 -> touch 业务
    function activeResponseTask(){
        // 页面激活同步：当前播放歌曲在本列表则聚焦正确索引，否则重置 -1
        // 网络歌优先按 songId 定位（filepath 是临时地址）
        var idx = searchMuscStack_playlistView.indexForCurrentPlaying();
        searchMuscStack_playlistView.curSelectingIndex = idx;
        searchMuscStack_playlistView.curPlayingIndex = BasicConfig.isPlaying ? idx : -1;
        searchMuscStack_playlistView.animateItemsFromIndex(0)
        // 长列表定位：把当前选中/播放项滚动到可见区域最顶部
        searchMuscStack_playlistView.scrollToCurrentIndex()
    }

    Component.onCompleted: {
        Qt.callLater(findAndSetupStackView);

        // 自动重新登录：页面实例化时若有挂起的重登请求（跳转先于弹窗创建），在此补开
        if (BasicConfig.pendingNeteaseLoginPopup) {
            BasicConfig.pendingNeteaseLoginPopup = false;
            netEaseLoginPopup.open();
        }

        // 延迟执行，确保BasicConfig.mainWindow已设置
        Qt.callLater(function() {
            mainWindow = findMainWindow();

            // 检查blurSource状态
            if(searchMuscStack_playlistView.enableListBlurEffect && !searchMuscStack_playlistView.blurSource){
                console.warn("Warning: blurSource is null. mainWindow:", mainWindow);
            }
        });
    }

    Component.onDestruction: {
        if (_myStackView) {
            _myStackView.currentItemChanged.disconnect(updateActiveState);
        }
    }

    //=======================  Ui  ====================//

// === plain text ===//
    Label{
        id:ncmPlainTextLabel
        // 提示文案已显示次数（纯内存计数，启动重置为 0；超过 3 次后不再显示）
        property int showCount: 0
        // 页面激活时淡入显示提示，5 秒后自动淡出；页面失活立即淡出（由 onIsActiveChanged 控制）
        opacity: 0
        Behavior on opacity {
            NumberAnimation {
                duration: 1000
                easing.type: Easing.OutCubic
            }
        }
        anchors.left: root.left
        anchors.leftMargin: 30
        anchors.top: root.top
        anchors.topMargin: 10
        color:Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
        text:"注意这里的 \"喜欢\"操作 和 \"拖动排序操作\":\n都会同步到您的网易云音乐官方账号上哦。"
        font.pixelSize: 12
        font.bold: false
        font.italic: true
        Rectangle{
            width: 2
            radius: width/2
            height: parent.implicitHeight
            color:parent.color
            anchors.left: parent.left
            anchors.leftMargin: -4
            anchors.top: parent.top
        }
    }

    // =====================  搜索框  =====================//
    // 提取自 TopLeftBar.qml 的 searchRectangleBorder（TopLeftBar 组件已暂停维护）
    Rectangle {
        id: searchRectangleBorder
        visible: true
        anchors.horizontalCenter: root.horizontalCenter
        anchors.top: root.top
        anchors.topMargin: 7
        width: 248
        height: 38
        radius: width/2
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { color: "#2a1a22"; position: 0.0 }
            GradientStop { color: "#382635"; position: 1.0 }
        }
        Rectangle {
            id: searchRectangle
            anchors.fill: parent
            anchors.margins: 1
            color: "#191d2b"
            radius: width/2
            anchors.verticalCenter: parent.verticalCenter
            clip: true
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { color: "#fef2e8"; position: 0.0 }
                GradientStop { color: "#fef2e8"; position: BasicConfig.gradientPosition2 }
            }

            Image {
                id: serchImage
                height: parent.height
                width: height
                source: "/image/search.png"
                scale: 1
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                layer.enabled: false
                layer.effect: ColorOverlay {
                    source: serchImage
                    color: "#2a1a22"
                }
                MouseArea {
                    anchors.fill: serchImage
                    hoverEnabled: true
                    onEntered: {
                        serchImage.layer.enabled = true
                    }
                    onExited: {
                        serchImage.layer.enabled = false
                    }
                    onClicked: {
                        var kw = searchTextField.text.trim()
                        if (kw.length > 0)
                            root.searchTriggered(kw)
                    }
                }
            }
            TextField {
                id: searchTextField
                anchors.left: serchImage.right
                anchors.leftMargin: 5
                anchors.right: parent.right
                anchors.rightMargin: 5
                anchors.top: parent.top
                anchors.topMargin: 2
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 2

                placeholderText: "网易云音乐"
                placeholderTextColor: "#98989b"
                // 内框是浅色渐变(#fef2e8)，深色文字/光标才可见（原 white 在浅底上不可见）
                color: "#2a1a22"
                maximumLength: 24
                font.pixelSize: 14
                font.family: "微软雅黑"

                verticalAlignment: TextInput.AlignVCenter
                background: Rectangle {
                    color: "transparent"
                    border.color: "transparent"
                }

                // 聚焦时向全局焦点管理器登记（与 Oran7MusicPlaylistView.searchRect 同机制）
                onFocusChanged: {
                    if (searchTextField.focus)
                        BasicConfig.newTextAreaFocused(searchTextField);
                }

                // 回车触发搜索
                onAccepted: {
                    var kw = searchTextField.text.trim()
                    if (kw.length > 0){
                        root.searchTriggered(kw)
                        if (searchTextField.focus === true)
                            searchTextField.focus = false;
                    }
                }
            }
            // 点击页面其它处时收起搜索框焦点（与 Oran7MusicPlaylistView.searchRect 同机制）
            Connections {
                target: BasicConfig
                function onClickedOutside() {
                    if (searchTextField.focus === true)
                        searchTextField.focus = false;
                }
            }
        }
    }

    // 网易云登录按钮（搜索框右侧）：头像圆圈 + 登录状态文案
    Oran7NetEaseLoginButton {
        id: netEaseLoginButton
        height: 38
        anchors.left: searchRectangleBorder.right
        anchors.leftMargin: 8
        anchors.verticalCenter: searchRectangleBorder.verticalCenter
        // 登录：点按钮弹出登录弹窗（网页登录为主，二维码因 8821 风控拦截已停用为主入口）
        onLoginClicked: netEaseLoginPopup.open()
    }
    // 网易云网页登录弹窗
    Oran7NetEaseLoginPopup {
        id: netEaseLoginPopup
    }

    // 自动重新登录：外部（网络监测器 Cookie 失效）请求打开登录弹窗
    Connections {
        target: BasicConfig
        function onOpenNeteaseLoginPopup() {
            if (BasicConfig.pendingNeteaseLoginPopup) {
                BasicConfig.pendingNeteaseLoginPopup = false;
                netEaseLoginPopup.open();
            }
        }
    }

    Oran7MusicPlaylistView {
        id: searchMuscStack_playlistView
        anchors.top: searchRectangleBorder.bottom
        anchors.topMargin: 7
        anchors.left: root.left
        anchors.right: root.right
        anchors.bottom: root.bottom

        leftMargin: 33

        enableListBlurEffect: false
        blurSource: root.mainWindow ? root.mainWindow.__Oran7WindowBackGround__ : null

        showLoveButton: true        // 搜索列表显示「喜欢」列与心形收藏按钮
        showIntegrateButton: true // 同时显示「整合」列与每行整合按钮（同步到"我整合的音乐"合集）
        showSearch:false                // 不启用列表检索功能
        showAddButton:false         //不启用从文件夹添加功能

        // 播放归属列表/索引（自动切歌用：播放可能来自搜索结果或云端页签，切页签后仍应在归属列表推进）
        property var _ownerModel: null
        property int _ownerIndex: -1

        //初始化属性
        titleModel: ["搜索结果", "网易云中我喜欢的音乐"]
        // 页面数据源：0=搜索结果缓存模型，1=云端"我喜欢的音乐"模型
        pageListModels: [searchCacheListModel, BasicConfig.neteaseLikedListModel]
        // 空列表占位提示（逐页对齐）：搜索结果页为空时显示引导大字，网易云喜欢页不显示
        pageEmptyHintTexts: ["", ""]

        // ------------- innder signals -------------
        onItemDoubleClicked: function (index) {
            //console.log("double clicked:",index)
            searchMuscStack_playlistView.dealClicked(index);
        }

        onItemClicked: function (index) {
            // console.log("clicked:",index)
            searchMuscStack_playlistView.dealClicked(index);
        }

        onItemLoved: function (index) {
            if (searchMuscStack_playlistView.currentPageIndex === 1) {
                // ---- 云端"我喜欢的音乐"页签：小心心只同步网易云官网（like/unlike），
                //      不再同步到本地"我喜欢的音乐"与 config（本地收藏走独立接口，后续维护）----
                // 防频繁点击：冷却 2 秒
                var now = Date.now();
                if (now - root._lastLikeClick < 2000)
                    return;
                root._lastLikeClick = now;

                var nitem = BasicConfig.neteaseLikedListModel.get(index);
                if (nitem.isLoved === true) {
                    // 取消喜欢 → 只同步网易云 unlike，并移除云端列表行
                    NcmPlaylistApi.setLike(nitem.songId, false);
                    BasicConfig.removeNeteaseLiked(nitem.songId);
                    BasicConfig.neteaseLikedDirty = true;   // 全局脏标记，下次进入页签刷新
                    // 行移除后索引统一由 onNeteaseLikedChanged 重新定位：
                    // 当前播放歌被移除则自动置 -1，仍在列表则聚焦到新位置
                } else {
                    // 喜欢 → 只同步网易云 like
                    NcmPlaylistApi.setLike(nitem.songId, true);
                    BasicConfig.addNeteaseLiked(nitem.songId, nitem.icon, nitem.music_name,
                                               nitem.music_artist, nitem.music_album,
                                               nitem.timesize, nitem.isVip);
                    BasicConfig.neteaseLikedDirty = true;   // 全局脏标记，下次进入页签刷新
                }
                return;
            }
            // ---- 搜索结果页签：小心心只同步网易云，不写本地"我喜欢的音乐"/config ----
            if (!NcmSession.isLoggedIn)
                return;   // 未登录禁用网易云喜欢操作（搜索结果均为网易云歌曲）
            var item = searchCacheListModel.get(index);
            var songId = item.songId || 0;
            if (songId <= 0)
                return;   // 无 songId 无法同步网易云，忽略
            var newLoved = !(item.isLoved === true);
            NcmPlaylistApi.setLike(songId, newLoved);
            // 同步全局网易云喜欢集合（搜索结果/云端/BottomPage 共享）
            if (newLoved)
                BasicConfig.addNeteaseLiked(songId, item.icon, item.music_name, item.music_artist,
                                            item.music_album, item.timesize, item.isVip);
            else
                BasicConfig.removeNeteaseLiked(songId);
            BasicConfig.neteaseLikedDirty = true;   // 全局脏标记，下次进入云端页签刷新
        }

        // ---- 整合按钮：把当前行加入/移出"我整合的音乐"合集（本地/网易云免费/VIP url 等异构数据源统一管理）----
        onItemIntegrated: function (index) {
            var item = listModel.get(index);
            if (!item) return;
            var sid = item.songId || 0;
            // 已整合 → 移出合集；未整合 → 加入合集
            if (BasicConfig.isIntegrated(sid, item.url, item.filepath)) {
                BasicConfig.removeIntegratedMusic(sid, item.url, item.filepath);
                return;
            }
            var isVip = item.isVip === true;
            var playSrc = (item.url && item.url.length > 0) ? item.url : item.filepath;
            // VIP 歌音源链接临时会过期，整合不持久保存（存空），播放时用 songId 重新解析
            var saveSrc = isVip ? "" : playSrc;
            BasicConfig.addIntegratedMusicItem(item.icon, item.music_name, item.music_artist,
                                               item.music_album, item.timesize,
                                               saveSrc, saveSrc,
                                               sid, isVip, "netease");
        }

        onFocus_current_playlistItem: {
            searchMuscStack_playlistView.ask_dir_of_item_index(BasicConfig.currentMediaFilePath);
        }

        // ------------- functions -------------

        function dealClicked(index) {
            //标记本列表为播放来源，自动切歌只在本列表内推进
            BasicConfig.currentPlaylistOwner = "search";
            // 记录播放归属列表/索引（自动切歌在归属列表推进，切页签后仍正确）
            _ownerModel = listModel;
            _ownerIndex = index;
            // 新一轮播放尝试，重置网络歌解析失败计数
            _networkFailCount = 0;
            //async globalPlayingFocus
            if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex) {
                BasicConfig.isPlaying = false;
                BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_MusicPlayerIndex;
            }

            var item = listModel.get(index);
            // 注入当前歌曲 songId / isVip（会员歌拉歌词、最近播放记录用）
            Client.setCurrentMediaSongId(item.songId || 0);
            Client.setCurrentMediaIsVip(item.isVip === true);
            // 持久化网络歌元数据，供下次启动恢复上次播放
            Client.rememberLastMedia(item.music_name, item.music_artist, item.music_album,
                                     item.icon, item.timesize, item.songId || 0, item.isVip === true);

            if (BasicConfig.isPlaying === false && curSelectingIndex === -1) {
                //--->first begain to play
                Client.playNetworkSong(item.songId || 0, item.filepath, item.isVip === true);
                BasicConfig.isPlaying = true;
                curPlayingIndex = index;

                //async bottom page info
                BasicConfig.currentMediaName = item.music_name;
                BasicConfig.currentMediaArtistAuthor = item.music_artist;
                BasicConfig.currentMediaFilePath = item.filepath;
                BasicConfig.currentMediaCoverFilePath = item.icon;//touch bottom page update
                BasicConfig.currentMediaAlbum = item.music_album;
                BasicConfig.currentMediaTimesize = item.timesize;
            } else if (BasicConfig.isPlaying === false && curSelectingIndex === index) {
                //-->pause statue,and the same file
                Client.playNetworkSong(item.songId || 0, item.filepath, item.isVip === true);
                BasicConfig.isPlaying = true;
                curPlayingIndex = index;
            } else if (BasicConfig.isPlaying === false && curSelectingIndex !== index) {
                //-->pause statue,and request to change file
                Client.playNetworkSong(item.songId || 0, item.filepath, item.isVip === true);
                BasicConfig.isPlaying = true;
                curPlayingIndex = index;

                //change file,async bottom page info
                BasicConfig.currentMediaName = item.music_name;
                BasicConfig.currentMediaArtistAuthor = item.music_artist;
                BasicConfig.currentMediaFilePath = item.filepath;
                BasicConfig.currentMediaCoverFilePath = item.icon;//touch bottom page update
                BasicConfig.currentMediaAlbum = item.music_album;
                BasicConfig.currentMediaTimesize = item.timesize;
            } else if (BasicConfig.isPlaying === true && curSelectingIndex !== index) {
                //--->is playing while change file
                Client.playNetworkSong(item.songId || 0, item.filepath, item.isVip === true);
                BasicConfig.isPlaying = true; //keep playing
                curPlayingIndex = index;

                //change file,async bottom page info
                BasicConfig.currentMediaName = item.music_name;
                BasicConfig.currentMediaArtistAuthor = item.music_artist;
                BasicConfig.currentMediaFilePath = item.filepath;
                BasicConfig.currentMediaCoverFilePath = item.icon;//touch bottom page update
                BasicConfig.currentMediaAlbum = item.music_album;
                BasicConfig.currentMediaTimesize = item.timesize;
            } else if (BasicConfig.isPlaying === true && curSelectingIndex === index) {
                //--->is playing ,the same of file for request to pause
                Client.playNetworkSong(item.songId || 0, item.filepath, item.isVip === true);
                BasicConfig.isPlaying = false;
                curPlayingIndex = -1;
                curSelectingIndex = -1;   // 取消当前播放/选中：selected 索引也重置为 -1
                return;                    // 不再被下方 curSelectingIndex = index 覆盖
            }

            //updata now selectingIndex
            curSelectingIndex = index;
        }

        function deal_outClickplayReponse_asyncUiStatue(index) {
            if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                return;
            if (BasicConfig.isPlaying === false) {
                curPlayingIndex = -1;
                //console.log(index)
            } else if (BasicConfig.isPlaying === true) {
                curPlayingIndex = index;
            }

            curSelectingIndex = index;
        }

        // 强制播放归属列表中的某首歌（自动切歌用，不走 dealClicked 的暂停分支）
        function playFromOwner(model, index) {
            _ownerModel = model;
            _ownerIndex = index;
            var item = model.get(index);
            Client.setCurrentMediaSongId(item.songId || 0);
            Client.setCurrentMediaIsVip(item.isVip === true);
            Client.rememberLastMedia(item.music_name, item.music_artist, item.music_album,
                                     item.icon, item.timesize, item.songId || 0, item.isVip === true);
            Client.playNetworkSong(item.songId || 0, item.filepath, item.isVip === true);
            BasicConfig.isPlaying = true;
            BasicConfig.currentMediaName = item.music_name;
            BasicConfig.currentMediaArtistAuthor = item.music_artist;
            BasicConfig.currentMediaFilePath = item.filepath;
            BasicConfig.currentMediaCoverFilePath = item.icon;
            BasicConfig.currentMediaAlbum = item.music_album;
            BasicConfig.currentMediaTimesize = item.timesize;
            // 高亮：归属模型即当前页签模型则聚焦 index，否则当前页签无此歌 → -1
            curPlayingIndex = (_ownerModel === listModel) ? index : -1;
            curSelectingIndex = curPlayingIndex;
        }

        // 根据当前播放 songId 反查归属列表（云端"我喜欢的音乐"或搜索结果），供自动切歌用
        function findOwnerForCurrent()
        {
            var sid = Client.currentSongId();
            if (sid > 0) {
                for (var i = 0; i < BasicConfig.neteaseLikedListModel.count; i++) {
                    if (BasicConfig.neteaseLikedListModel.get(i).songId === sid)
                        return { model: BasicConfig.neteaseLikedListModel, index: i };
                }
                for (i = 0; i < searchCacheListModel.count; i++) {
                    if (searchCacheListModel.get(i).songId === sid)
                        return { model: searchCacheListModel, index: i };
                }
            }
            return null;
        }

        // -------------  Connections  -------------
        //<---signal from BasicConfig
        Connections {
            target: BasicConfig
            //<--signal from BasicConfig ,the player core playing statue is changed, now async the playlist Ui Of statues
            function onIsPlayingChanged() {
                if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex) {
                    searchMuscStack_playlistView.curPlayingIndex = -1;
                    return;
                }

                searchMuscStack_playlistView.ask_dir_of_item_index(BasicConfig.currentMediaFilePath);
            }
        }
        //<--- signal from this Oran7MusicPlaylistView inner
        Connections {
            target: searchMuscStack_playlistView
            //--> signal from this for response to the file_path dir of item index
            function onReponse_dir_of_item_index(index) {
                searchMuscStack_playlistView.deal_outClickplayReponse_asyncUiStatue(index);
            }
            //--> 拖拽重排完成：云端"我喜欢的音乐"页签 → 提交新顺序同步网易云官方服务器
            //  （本地音乐/搜索结果的增删改由其它页签处理，这里只管云端歌单）
            function onReorderCompleted(newOrder) {
                if (searchMuscStack_playlistView.currentPageIndex !== 1)
                    return;
                if (!NcmSession.isLoggedIn)
                    return;
                // reorderCompleted 传的是 filepath（云端歌单 filepath 全为空），
                // 直接按当前模型顺序（move 已完成）取 songId 列表作为新顺序
                var ids = [];
                for (var i = 0; i < BasicConfig.neteaseLikedListModel.count; i++) {
                    var sid = BasicConfig.neteaseLikedListModel.get(i).songId;
                    if (sid > 0)
                        ids.push(sid);
                }
                if (ids.length > 0)
                    NcmPlaylistApi.reorderLikedSongs(ids);
            }
        }
        //-->signal from Client Of request to play next or last
        Connections {
            target: Client
            function onPlayNextTriggered() {
                if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                    return;
                // 本地播放由 LocalMusicStack 处理
                if (BasicConfig.currentPlaylistOwner === "local")
                    return;
                // 退出登录后不再自动切网易云歌（当前播放的云端歌播完即停）
                if (!NcmSession.isLoggedIn) {
                    root.stopNetworkPlayback();
                    return;
                }
                // ---- 网易云漫游（心动模式）：不按列表顺序，改播推荐队列；离开本页也继续 ----
                if (BasicConfig.playMode === 3) {
                    root.playNextRoamSong();
                    return;
                }
                // 归属列表：优先 _ownerModel；否则按当前 songId 反查
                var model = searchMuscStack_playlistView._ownerModel;
                var curIdx = searchMuscStack_playlistView._ownerIndex;
                if (!model || curIdx < 0 || curIdx >= model.count) {
                    var found = searchMuscStack_playlistView.findOwnerForCurrent();
                    if (!found) {
                        // 归属找不到：真正停播，避免图标显示暂停但音乐仍在播放
                        root.stopNetworkPlayback();
                        return;
                    }
                    model = found.model;
                    curIdx = found.index;
                }
                // 单曲循环：重播当前歌（EOF 后播放器已销毁，需走完整播放流程重新播放）
                if (BasicConfig.playMode === 1) {
                    searchMuscStack_playlistView.playFromOwner(model, curIdx);
                    return;
                }
                // 按播放模式（顺序/随机）计算目标索引
                var newIndex = BasicConfig.nextPlayIndex(curIdx, model.count, 1);
                searchMuscStack_playlistView.playFromOwner(model, newIndex);
            }
            function onPlayPreviousTriggered() {
                if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                    return;
                if (BasicConfig.currentPlaylistOwner === "local")
                    return;
                if (!NcmSession.isLoggedIn) {
                    root.stopNetworkPlayback();
                    return;
                }
                // ---- 网易云漫游（心动模式）：播漫游队列的上一首（非当前歌所属列表的上一首）----
                if (BasicConfig.playMode === 3) {
                    if (root.playPreviousRoamSong())
                        return;
                    // 队列中没有更早的漫游歌，回退归属列表逻辑
                }
                var model = searchMuscStack_playlistView._ownerModel;
                var curIdx = searchMuscStack_playlistView._ownerIndex;
                if (!model || curIdx < 0 || curIdx >= model.count) {
                    var found = searchMuscStack_playlistView.findOwnerForCurrent();
                    if (!found) {
                        root.stopNetworkPlayback();
                        return;
                    }
                    model = found.model;
                    curIdx = found.index;
                }
                // 单曲循环：重播当前歌（EOF 后播放器已销毁，需走完整播放流程重新播放）
                if (BasicConfig.playMode === 1) {
                    searchMuscStack_playlistView.playFromOwner(model, curIdx);
                    return;
                }
                // 按播放模式（顺序/随机）计算目标索引
                var newIndex = BasicConfig.nextPlayIndex(curIdx, model.count, -1);
                searchMuscStack_playlistView.playFromOwner(model, newIndex);
            }
        }
    }

    // 搜索结果的加载统一走 root.runSearch → Oran7AsyncManager("musicSearch")，
    // 原 Oran7MusicSearchService 的 onSearchStarted/onSearchResultReady/onSearchFinished/onSearchFailed
    // 四个信号处理已并入 runSearch 的完成/失败回调（见上文）。

    // ------------- 网络歌解析失败处理 -------------
    // 未登录/无会员/链接失效时无法拿到播放地址 → 自动播下一首；列表只有一首则暂停
    Connections {
        target: Client
        function onNetworkPlayResolveFailed(songId) {
            // 退出登录后解析失败直接停播，不再切网易云歌
            if (!NcmSession.isLoggedIn) {
                root.stopNetworkPlayback();
                return;
            }
            // 网易云漫游（心动模式）：继续漫游队列播下一首，不回退到"网易云中我喜欢的音乐"等归属列表
            // （漫游推荐歌可能也存在于归属列表，若按归属推进会误切到列表歌曲，且 BottomPage 信息随之错乱）
            if (BasicConfig.playMode === 3) {
                _networkFailCount = 0;
                root.playNextRoamSong();
                // 队列已耗尽、新一批还在抓取 → 无歌可播先停播，等 onRoamSongsReady 自动续播
                if (root._roamIndex >= root._roamQueue.length)
                    root.stopNetworkPlayback();
                return;
            }
            // 基于播放归属列表推进（切页签后 listModel/curPlayingIndex 不代表播放归属）
            var model = searchMuscStack_playlistView._ownerModel;
            var idx = searchMuscStack_playlistView._ownerIndex;
            if (!model || idx < 0 || idx >= model.count) {
                var found = searchMuscStack_playlistView.findOwnerForCurrent();
                if (!found) {
                    root.stopNetworkPlayback();
                    return;
                }
                model = found.model;
                idx = found.index;
            }
            _networkFailCount++;
            if (model.count <= 1 || _networkFailCount >= model.count) {
                // 只有一首 或 整轮都解析失败 → 停播，避免无限切歌
                _networkFailCount = 0;
                root.stopNetworkPlayback();
                return;
            }
            var next = (idx + 1) % model.count;
            // playFromOwner 会同步播放、BottomPage 信息与索引（不走 dealClicked，避免重置失败计数）
            searchMuscStack_playlistView.playFromOwner(model, next);
            BasicConfig.isPlaying = true;
        }
    }

    // 登录成功后同步搜索列表与 BottomPage 的喜欢状态（拉取云端歌单填充全局集合）；
    // 退出登录后清空云端歌单与喜欢状态
    Connections {
        target: NcmSession
        function onLoginStatusChanged() {
            if (NcmSession.isLoggedIn) {
                root._likedEverLoaded = false;
                BasicConfig.neteaseLikedDirty = true;
                BasicConfig.neteaseLikedListModel.clear();
                BasicConfig.fetchNeteaseLikedSongs();
                root.refreshSearchLovedAll();
            } else {
                // 退出登录：清空云端"我喜欢的音乐"列表与喜欢状态（搜索列表心形同步变空心），
                // 并重置播放归属，防止当前云端歌播放完毕误切到搜索结果
                BasicConfig.neteaseLikedListModel.clear();
                searchMuscStack_playlistView._ownerModel = null;
                searchMuscStack_playlistView._ownerIndex = -1;
                root._networkFailCount = 0;
                root._likedEverLoaded = false;
                root.refreshSearchLovedAll();
            }
        }
    }

    // 同步搜索结果列表小心心：任意入口（云端/搜索结果/BottomPage）like/unlike 都会经
    // BasicConfig.addNeteaseLiked/removeNeteaseLiked 发出 neteaseLikedChanged，据此更新搜索列表
    Connections {
        target: BasicConfig
        function onNeteaseLikedChanged(songId, liked) {
            root.syncSearchLoved(songId, liked);
            // 任意入口（列表行心形 / BottomPage 心形）增删云端歌单行后，统一重新定位当前播放歌：
            // 已在当前列表 → 聚焦新位置；被移除/不在列表 → 重置为 -1。
            // 注意不能拿 int 参数 songId 与 currentSongId 比较（songId 可能被 int 截断），直接反查模型即可
            var idx = searchMuscStack_playlistView.indexForCurrentPlaying();
            searchMuscStack_playlistView.curSelectingIndex = idx;
            searchMuscStack_playlistView.curPlayingIndex = BasicConfig.isPlaying ? idx : -1;
        }
    }

    // 退出网易云漫游（播放模式离开 3）时清空漫游队列
    Connections {
        target: BasicConfig
        function onPlayModeChanged() {
            if (BasicConfig.playMode !== 3)
                root.clearRoamQueue();
        }
    }

    // ------------- 云端"我喜欢的音乐"歌单拉取 -------------
    Connections {
        target: searchMuscStack_playlistView
        // 切到"网易云中我喜欢的音乐"页签：
        //  拉取条件 = 首次进入 || 本地做过 like/unlike（脏标记）|| 距上次拉取超 30 秒（感知官网/程序外更新）
        function onCurrentPageIndexChanged() {
            if (searchMuscStack_playlistView.currentPageIndex === 1 && NcmSession.isLoggedIn) {
                var now = Date.now();
                if (BasicConfig.neteaseLikedDirty || !root._likedEverLoaded
                        || (root._lastLikedFetch > 0 && now - root._lastLikedFetch > 30000)) {
                    BasicConfig.neteaseLikedDirty = false;
                    root._likedEverLoaded = true;
                    root._lastLikedFetch = now;
                    BasicConfig.neteaseLikedListModel.clear();
                    BasicConfig.fetchNeteaseLikedSongs();
                }
            }
        }
    }
    // ------------- 云端"我喜欢的音乐"拉取完成定位（原 NcmPlaylistApi.onLikedSongsFinished） -------------
    function onLikedSongsFinishedAsync(count) {
        root._likedEverLoaded = true;   // 拉取成功，标记已加载过（避免下次进入页签重复拉取）
        if (count > 0)
            searchMuscStack_playlistView.animateItemsFromIndex(0);
        // 拉取完成后刷新搜索结果列表喜欢状态（按全局网易云集合）
        root.refreshSearchLovedAll();
        // 定位当前播放歌曲
        var idx = searchMuscStack_playlistView.indexForCurrentPlaying();
        searchMuscStack_playlistView.curSelectingIndex = idx;
        searchMuscStack_playlistView.curPlayingIndex = BasicConfig.isPlaying ? idx : -1;
        // 列表加载完成后再滚动定位（拉取是异步的，进入页签时列表可能还是空的）
        if (searchMuscStack_playlistView.currentPageIndex === 1)
            searchMuscStack_playlistView.scrollToCurrentIndex();
    }

    // 云端"我喜欢的音乐"拉取统一走 BasicConfig.fetchNeteaseLikedSongs（Oran7AsyncManager handler），
    // 完成信号转发到上面的定位函数（启动预拉取 + 本页进入拉取共用）
    Connections {
        target: BasicConfig
        function onNeteaseLikedFetchFinished(count) {
            root.onLikedSongsFinishedAsync(count);
        }
    }

    // 网易云漫游推荐：统一走 Oran7AsyncManager("roamSongs")，结果经 onRoamSongsReady / onRoamSongsFailed 消费
    function requestRoam(songId) {
        Oran7AsyncManager.run("roamSongs", {
            songId: songId,
            playlistId: NcmPlaylistApi.likedPlaylistId,
            cookie: NcmSession.cookieString,
            csrf: NcmSession.csrfToken
        }, function(songs) {
            root.onRoamSongsReady(songs);
        }, function(error) {
            root.onRoamSongsFailed(error);
        });
    }

    //--> 网易云漫游推荐结果：维护队列，旧的出队、新的入队（原 NcmPlaylistApi.onRoamSongsReady）
    function onRoamSongsReady(songs) {
        root._roamFetching = false;
        if (!songs || songs.length === 0) {
            root._roamWaiting = false;
            // 无推荐：回退普通切歌（真正停播，避免图标暂停但音频仍在播放）
            root.stopNetworkPlayback();
            return;
        }
        // 队列维护：丢弃已播放过的前段（旧的出队），但保留当前歌及其上一首，
        // 否则新批次入队截断后漫游"上一首"会丢失（_roamIndex 同步前移）
        if (root._roamIndex > 0) {
            var keepFrom = Math.max(0, root._roamIndex - 2);
            root._roamQueue = root._roamQueue.slice(keepFrom);
            root._roamIndex = root._roamQueue.length > 0 ? (root._roamIndex - keepFrom) : 0;
        }
        // 新的入队
        for (var i = 0; i < songs.length; i++)
            root._roamQueue.push(songs[i]);
        // 仅在"队列耗尽等待"时立即开播；预取场景不打断当前播放
        if (root._roamWaiting) {
            root._roamWaiting = false;
            root.playNextRoamSong();
        }
    }

    //--> 网易云漫游失败（原 NcmPlaylistApi.onRoamSongsFailed）
    function onRoamSongsFailed(message) {
        console.warn("网易云漫游失败:", message);
        root._roamFetching = false;
        root._roamWaiting = false;
        // 回退普通切歌（真正停播，避免图标暂停但音频仍在播放）
        root.stopNetworkPlayback();
    }

    // 网易云排序同步：reorder 仍走 NcmPlaylistApi（动作类操作，不在本次加载改造范围）
    Connections {
        target: NcmPlaylistApi
        //--> 拖拽排序同步结果：失败则重新拉取云端歌单，恢复服务器上的真实顺序
        function onReorderResult(success, message) {
            if (success)
                return;
            console.warn("网易云排序同步失败:", message);
            BasicConfig.neteaseLikedDirty = true;
            root._likedEverLoaded = false;
            BasicConfig.neteaseLikedListModel.clear();
            BasicConfig.fetchNeteaseLikedSongs();
        }
    }
}
