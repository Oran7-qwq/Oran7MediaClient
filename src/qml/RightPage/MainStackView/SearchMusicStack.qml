import QtQuick 2.15
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import QtCore
import QtQuick.Dialogs
import "../../Basic"
import "../../Components/Oran7GlobalComponents"
import Client 1.0
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
    readonly property int likedPageIndex: 0
    readonly property int searchPageIndex: 1

    // 搜索结果由 Client 持有；网络返回后一次提交完整快照。
    readonly property var searchCacheListModel: Client.tracks.search

    // 搜索代数：每次 runSearch 自增；回调里若代数已过期（期间又发起新搜索）则丢弃，防止旧结果覆盖新结果
    property int _searchGeneration: 0

    // 搜索框触发搜索的信号（由网易云搜索服务消费）
    signal searchTriggered(string keywords)
    onSearchTriggered: function(keywords) {
        // 先切回"搜索结果"页签，再触发搜索（点图标/回车两条路都走这里）
        searchMuscStack_playlistView.switchToPage(root.searchPageIndex);
        root.runSearch(keywords);
    }

    // 网易云搜索：统一走 Oran7AsyncManager("musicSearch") 线程池 handler
    function runSearch(keywords) {
        var gen = ++root._searchGeneration;
        // 搜索开始即清空并重置索引（对应原 onSearchStarted）
        Client.tracks.clearSearchResults();
        searchMuscStack_playlistView.curPlayingIndex = -1;
        searchMuscStack_playlistView.curSelectingIndex = -1;

        Oran7AsyncManager.run("musicSearch", {
            keywords: keywords,
            cookie: NcmSession.cookieString,
            isLoggedIn: NcmSession.isLoggedIn
        }, function(result) {
            if (gen !== root._searchGeneration) return;   // 过期结果（期间又发起新搜索），丢弃
            Client.tracks.replaceSearchResults(result);
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
            Client.setPlayMode(0);
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
            var parent = root.parent;
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
        // 喜欢列表成为首个页签，首次进入也提交加载意图，沿用服务已有缓存去重。
        BasicConfig.fetchNeteaseLikedSongs();

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
        text:qsTranslate("Oran7Ui", "注意这里的 \"喜欢\"操作 和 \"拖动排序操作\":\n都会同步到您的网易云音乐官方账号上哦。")
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
                source: "qrc:/image/search.png"
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

                placeholderText: qsTranslate("Oran7Ui", "网易云音乐")
                placeholderTextColor: "#98989b"
                // 内框是浅色渐变(#fef2e8)，深色文字/光标才可见（原 white 在浅底上不可见）
                color: "#2a1a22"
                maximumLength: 24
                font.pixelSize: 14
                font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family

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
        objectName: "cloudMusicPlaylist"
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


        //初始化属性
        titleModel: [qsTranslate("Oran7Ui", "我喜欢的音乐"), qsTranslate("Oran7Ui", "搜索结果")]
        // 页面数据源：0=云端"我喜欢的音乐"模型，1=搜索结果缓存模型。
        pageListModels: [BasicConfig.neteaseLikedListModel, searchCacheListModel]
        // 空列表占位提示（逐页对齐）：搜索结果页为空时显示引导大字，网易云喜欢页不显示
        pageEmptyHintTexts: ["", ""]

        // ------------- innder signals -------------
        onItemDoubleClicked: function (index) {
            //console.log("double clicked:",index)
            Client.musicQueue.activate(listModel, index);
            syncIndicesToCurrentMedia();
        }

        onItemClicked: function (index) {
            // console.log("clicked:",index)
            Client.musicQueue.activate(listModel, index);
            syncIndicesToCurrentMedia();
        }

        onItemLoved: function (index) {
            Client.cloudMusic.toggleLike(listModel.get(index));
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

        // -------------  Connections  -------------
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
                if (searchMuscStack_playlistView.currentPageIndex !== root.likedPageIndex)
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

    }

    // 搜索结果的加载统一走 root.runSearch → Oran7AsyncManager("musicSearch")，
    // 搜索请求的代际与批量结果处理仍由本页维护。
    // 四个信号处理已并入 runSearch 的完成/失败回调（见上文）。

    // 同步搜索结果列表小心心：任意入口（云端/搜索结果/BottomPage）like/unlike 都会经
    // Client.cloudMusic 转发 neteaseLikedChanged，页面仅同步显示索引
    Connections {
        target: BasicConfig
        function onNeteaseLikedChanged(songId, liked) {
            // 任意入口（列表行心形 / BottomPage 心形）增删云端歌单行后，统一重新定位当前播放歌：
            // 已在当前列表 → 聚焦新位置；被移除/不在列表 → 重置为 -1。
            // 注意不能拿 int 参数 songId 与 currentSongId 比较（songId 可能被 int 截断），直接反查模型即可
            var idx = searchMuscStack_playlistView.indexForCurrentPlaying();
            searchMuscStack_playlistView.curSelectingIndex = idx;
            searchMuscStack_playlistView.curPlayingIndex = BasicConfig.isPlaying ? idx : -1;
        }
    }

    // ------------- 云端"我喜欢的音乐"歌单拉取 -------------
    Connections {
        target: searchMuscStack_playlistView
        // 页面只表达加载意图；启动缓存、并发与刷新间隔由全局入口判断。
        function onCurrentPageIndexChanged() {
            if (searchMuscStack_playlistView.currentPageIndex === root.likedPageIndex)
                BasicConfig.fetchNeteaseLikedSongs();
        }
    }
    // ------------- 云端"我喜欢的音乐"拉取完成定位（原 NcmPlaylistApi.onLikedSongsFinished） -------------
    function onLikedSongsFinishedAsync(count) {
        if (count > 0)
            searchMuscStack_playlistView.animateItemsFromIndex(0);
        // 拉取完成后刷新搜索结果列表喜欢状态（按全局网易云集合）
        // 定位当前播放歌曲
        var idx = searchMuscStack_playlistView.indexForCurrentPlaying();
        searchMuscStack_playlistView.curSelectingIndex = idx;
        searchMuscStack_playlistView.curPlayingIndex = BasicConfig.isPlaying ? idx : -1;
        // 列表加载完成后再滚动定位（拉取是异步的，进入页签时列表可能还是空的）
        if (searchMuscStack_playlistView.currentPageIndex === root.likedPageIndex)
            searchMuscStack_playlistView.scrollToCurrentIndex();
    }

    // 云端"我喜欢的音乐"拉取统一走 Client.cloudMusic 的收藏缓存服务，
    // 完成信号转发到上面的定位函数（启动预拉取 + 本页进入拉取共用）
    Connections {
        target: BasicConfig
        function onNeteaseLikedFetchFinished(count) {
            root.onLikedSongsFinishedAsync(count);
        }
    }

}
