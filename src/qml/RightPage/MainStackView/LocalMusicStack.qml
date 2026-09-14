import QtQuick 2.15
import QtQuick.Controls
import QtCore
import QtQuick.Dialogs
import Qt5Compat.GraphicalEffects
import "../../Basic"
import "../../Components"
import Client 1.0

Item {
    id: root
    anchors.left: parent.left
    anchors.leftMargin: 0
    anchors.top: parent.top
    anchors.topMargin: 14
    clip: true

    //====================   define Properties    =====================//

    property string pageName: ""

    // 获取mainWindow
    function findMainWindow() {
        return BasicConfig.mainWindow;
    }

    property var stackView_rootItemContainer: root //包含此组件的外部作为stackViewItem的根项
    property var mainWindow: null

    // ==================== StackView 活动状态检测 ====================//
    property bool isActive: false
    property var _myStackView: null

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
        // 页面激活同步：当前播放歌曲在当前页签列表则聚焦正确索引，否则重置 -1
        // 网络歌优先按 songId 定位（filepath 是临时地址，VIP 收藏不存 url）
        var idx = localMusciStack_playlistView.indexForCurrentPlaying();
        localMusciStack_playlistView.curSelectingIndex = idx;
        localMusciStack_playlistView.curPlayingIndex = BasicConfig.isPlaying ? idx : -1;
        localMusciStack_playlistView.animateItemsFromIndex(0)
    }

    Component.onCompleted: {
        Qt.callLater(findAndSetupStackView);

        // 延迟执行，确保BasicConfig.mainWindow已设置
        Qt.callLater(function() {
            mainWindow = findMainWindow();

            // 检查blurSource状态
            if(localMusciStack_playlistView.enableListBlurEffect && !localMusciStack_playlistView.blurSource){
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

    Oran7MusicPlaylistView {
        id: localMusciStack_playlistView
        anchors.fill: root

        leftMargin: 33

        enableListBlurEffect: false
        blurSource: root.mainWindow ? root.mainWindow.__Oran7WindowBackGround__ : null

        // 播放归属列表/索引（网络歌自动切歌、解析失败推进用，避免依赖当前页签的 filepath 匹配）
        property var _ownerModel: null
        property int _ownerIndex: -1
        // 网络歌解析失败连续计数（防无限切歌；达到列表数量即暂停）
        property int _networkFailCount: 0

        //初始化属性
        titleModel: ["我整合的音乐","本地音乐","下载中歌曲"]
        // 页签↔模型：0=我喜欢的音乐, 1=本地音乐, 2=下载中歌曲(暂未使用)
        pageListModels: [BasicConfig.myIntegratedMusicListModel, BasicConfig.localMusicListModel, null]
        // 「+」添加本地文件按钮只在本地音乐页签显示
        showAddButton: localMusciStack_playlistView.currentPageIndex === 1
        // 全部页签只显示「整合」列（"我整合的音乐"合集），不显示小心心
        showLoveButton: false
        showIntegrateButton: true

        // ------------- innder signals -------------
        onItemDoubleClicked: function (index) {
            //console.log("double clicked:",index)
            localMusciStack_playlistView.dealClicked(index);
        }

        onItemClicked: function (index) {
            // console.log("clicked:",index)
            localMusciStack_playlistView.dealClicked(index);
        }

        // ---- 整合按钮：加入/移出"我整合的音乐"合集（原喜欢图标的控制链路，现统一为整合操作）----
        onItemIntegrated: function (index) {
            var item = listModel.get(index);
            // 已整合 → 移出合集；未整合 → 加入合集（本地行 url 为空、存 filepath；整合行 url 优先）
            // 移除带 songId：VIP 整合行 url/filepath 为空，只能按 songId 精确移除
            if (BasicConfig.removeIntegratedMusic(item.songId || 0, item.url, item.filepath))
                return;
            var isVip = item.isVip === true;
            var playSrc = (item.url && item.url.length > 0) ? item.url : item.filepath;
            // VIP 歌音源链接临时会过期，整合不持久保存（存空），播放时用 songId 重新解析
            var saveSrc = isVip ? "" : playSrc;
            BasicConfig.addIntegratedMusicItem(item.icon, item.music_name, item.music_artist,
                                               item.music_album, item.timesize,
                                               saveSrc, saveSrc,
                                               item.songId || 0, isVip, "local");
        }

        onFocus_current_playlistItem: {
            localMusciStack_playlistView.ask_dir_of_item_index(BasicConfig.currentMediaFilePath);
        }

        // ------------- functions -------------

        function dealClicked(index) {
            //标记本列表为播放来源，自动切歌只在本列表内推进
            BasicConfig.currentPlaylistOwner = "local";
            // 记录播放归属列表/索引（自动切歌、解析失败推进用）
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
            // 播放源：优先 url（收藏的网络歌），不可用则用 filepath（本地歌）
            var playPath = (item.url && item.url.length > 0) ? item.url : item.filepath;
            // 注入当前歌曲 songId / isVip（会员歌拉歌词、最近播放记录用）
            Client.setCurrentMediaSongId(item.songId || 0);
            Client.setCurrentMediaIsVip(item.isVip === true);
            // 网络歌持久化元数据，供下次启动恢复上次播放
            // 用 songId>0 判定网络歌：VIP 收藏歌 url/filepath 可能为空，仅靠 playPath 无法识别，
            // 漏存会导致退出后 last_media_* 与 last_used_file 错配
            if (item.songId > 0 || playPath.indexOf("http://") === 0 || playPath.indexOf("https://") === 0) {
                Client.rememberLastMedia(item.music_name, item.music_artist, item.music_album,
                                         item.icon, item.timesize, item.songId || 0, item.isVip === true);
            }

            if (BasicConfig.isPlaying === false && curSelectingIndex === -1) {
                //--->first begain to play
                Client.playNetworkSong(item.songId || 0, playPath, item.isVip === true);
                BasicConfig.isPlaying = true;
                curPlayingIndex = index;

                //async bottom page info
                BasicConfig.currentMediaName = item.music_name;
                BasicConfig.currentMediaArtistAuthor = item.music_artist;
                BasicConfig.currentMediaFilePath = playPath;
                BasicConfig.currentMediaCoverFilePath = item.icon;//touch bottom page update
                BasicConfig.currentMediaAlbum = item.music_album;
                BasicConfig.currentMediaTimesize = item.timesize;
            } else if (BasicConfig.isPlaying === false && curSelectingIndex === index) {
                //-->pause statue,and the same file
                Client.playNetworkSong(item.songId || 0, playPath, item.isVip === true);
                BasicConfig.isPlaying = true;
                curPlayingIndex = index;
            } else if (BasicConfig.isPlaying === false && curSelectingIndex !== index) {
                //-->pause statue,and request to change file
                Client.playNetworkSong(item.songId || 0, playPath, item.isVip === true);
                BasicConfig.isPlaying = true;
                curPlayingIndex = index;

                //change file,async bottom page info
                BasicConfig.currentMediaName = item.music_name;
                BasicConfig.currentMediaArtistAuthor = item.music_artist;
                BasicConfig.currentMediaFilePath = playPath;
                BasicConfig.currentMediaCoverFilePath = item.icon;//touch bottom page update
                BasicConfig.currentMediaAlbum = item.music_album;
                BasicConfig.currentMediaTimesize = item.timesize;
            } else if (BasicConfig.isPlaying === true && curSelectingIndex !== index) {
                //--->is playing while change file
                Client.playNetworkSong(item.songId || 0, playPath, item.isVip === true);
                BasicConfig.isPlaying = true; //keep playing
                curPlayingIndex = index;

                //change file,async bottom page info
                BasicConfig.currentMediaName = item.music_name;
                BasicConfig.currentMediaArtistAuthor = item.music_artist;
                BasicConfig.currentMediaFilePath = playPath;
                BasicConfig.currentMediaCoverFilePath = item.icon;//touch bottom page update
                BasicConfig.currentMediaAlbum = item.music_album;
                BasicConfig.currentMediaTimesize = item.timesize;
            } else if (BasicConfig.isPlaying === true && curSelectingIndex === index) {
                //--->is playing ,the same of file for request to pause
                Client.playNetworkSong(item.songId || 0, playPath, item.isVip === true);
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

        // ---- 播放归属模型辅助：自动切歌应基于"当前播放歌曲真正所属的列表"，而不是当前页签 ----
        // 解决：在"我喜欢的音乐"播放时切到"本地音乐"页签，curPlayingIndex 被重置为 -1 导致无法自动切歌。

        function playingPathOf(item) {
            return (item.url && item.url.length > 0) ? item.url : item.filepath;
        }

        // 在所有页签模型中查找当前播放文件所属的模型（找不到返回 null）
        function modelForPlayingPath(path) {
            var models = pageListModels;
            if (!models) return null;
            for (var i = 0; i < models.length; i++) {
                var m = models[i];
                if (!m) continue;
                for (var j = 0; j < m.count; j++) {
                    if (String(playingPathOf(m.get(j))) === String(path))
                        return m;
                }
            }
            return null;
        }

        // 在指定模型中查找文件路径的索引，找不到返回 -1
        function indexInModel(model, path) {
            if (!model) return -1;
            for (var j = 0; j < model.count; j++) {
                if (String(playingPathOf(model.get(j))) === String(path))
                    return j;
            }
            return -1;
        }

        // 播放指定模型中的某首歌并刷新全局状态（自动切歌用，不走 dealClicked 的暂停分支）
        function playFromModel(model, index) {
            _ownerModel = model;
            _ownerIndex = index;
            var item = model.get(index);
            var playPath = playingPathOf(item);
            Client.setCurrentMediaSongId(item.songId || 0);
            Client.setCurrentMediaIsVip(item.isVip === true);
            // 网络歌持久化元数据，供下次启动恢复上次播放
            // 用 songId>0 判定网络歌：VIP 收藏歌 url/filepath 可能为空，仅靠 playPath 无法识别，
            // 漏存会导致退出后 last_media_* 与 last_used_file 错配
            if (item.songId > 0 || playPath.indexOf("http://") === 0 || playPath.indexOf("https://") === 0) {
                Client.rememberLastMedia(item.music_name, item.music_artist, item.music_album,
                                         item.icon, item.timesize, item.songId || 0, item.isVip === true);
            }
            Client.playNetworkSong(item.songId || 0, playPath, item.isVip === true);
            BasicConfig.isPlaying = true;
            BasicConfig.currentMediaName = item.music_name;
            BasicConfig.currentMediaArtistAuthor = item.music_artist;
            BasicConfig.currentMediaFilePath = playPath;
            BasicConfig.currentMediaCoverFilePath = item.icon;
            BasicConfig.currentMediaAlbum = item.music_album;
            BasicConfig.currentMediaTimesize = item.timesize;

            // 高亮：归属模型即当前页签模型则聚焦 index，否则当前页签无此歌 → -1
            curPlayingIndex = (_ownerModel === listModel) ? index : -1;
            curSelectingIndex = curPlayingIndex;
        }

        // 自动切到下一首/上一首（基于播放归属模型，跨页签也能正确推进）
        function autoAdvance(direction) {
            if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                return;
            if (BasicConfig.currentPlaylistOwner !== "local")
                return;
            // 新一轮正常切歌，重置解析失败计数
            _networkFailCount = 0;

            var playModel = _ownerModel;
            if (!playModel || playModel.count === 0)
                return;
            var curIdx = _ownerIndex;
            if (curIdx < 0 || curIdx >= playModel.count)
                return;

            // 单曲循环：重播当前歌（EOF 后播放器已销毁，需走完整播放流程重新播放）
            if (BasicConfig.playMode === 1) {
                playFromModel(playModel, curIdx);
                return;
            }
            // 按播放模式（顺序/随机）计算目标索引
            var newIndex = BasicConfig.nextPlayIndex(curIdx, playModel.count, direction);
            playFromModel(playModel, newIndex);
        }

        // 网络歌解析失败（未登录/无会员/链接失效）：自动尝试播放归属列表下一首；单首或整轮失败则暂停
        function handleNetworkPlayFailed() {
            var playModel = _ownerModel;
            if (!playModel || playModel.count <= 0) {
                pausePlayback();
                return;
            }
            _networkFailCount++;
            if (playModel.count <= 1 || _networkFailCount >= playModel.count) {
                _networkFailCount = 0;
                pausePlayback();
                return;
            }
            var newIndex = (_ownerIndex + 1) % playModel.count;
            playFromModel(playModel, newIndex);
        }

        function pausePlayback() {
            BasicConfig.isPlaying = false;
            curPlayingIndex = -1;
            curSelectingIndex = -1;
        }

        // -------------  Connections  -------------
        //<---signal from BasicConfig
        Connections {
            target: BasicConfig
            //<--signal from BasicConfig ,the player core playing statue is changed, now async the playlist Ui Of statues
            function onIsPlayingChanged() {
                if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex) {
                    localMusciStack_playlistView.curPlayingIndex = -1;
                    return;
                }

                localMusciStack_playlistView.ask_dir_of_item_index(BasicConfig.currentMediaFilePath);
            }
        }
        //<--- signal from this Oran7MusicPlaylistView inner
        Connections {
            target: localMusciStack_playlistView
            //--> signal from this for response to the file_path dir of item index
            function onReponse_dir_of_item_index(index) {
                localMusciStack_playlistView.deal_outClickplayReponse_asyncUiStatue(index);
            }

            // 以下均为本地音乐专属的 Client 操作，只在「本地音乐」页签(索引1)生效，
            // 避免在「我整合的音乐」页签误改本地数据
            function onReorderCompleted(newOrder){
                if (localMusciStack_playlistView.currentPageIndex === 1) {
                    Client.localMusicListReordered(newOrder);
                } else if (localMusciStack_playlistView.currentPageIndex === 0) {
                    // 整合页签：重排已在模型内 move 完成，直接把当前顺序持久化到 config.json
                    BasicConfig.persistIntegratedMusic();
                }
            }

            function onAddNewItemOfFiles(filesArray) {
                if (localMusciStack_playlistView.currentPageIndex === 1) {
                    var focusedItem = localMusciStack_playlistView.getfocusItem();
                    Client.addNewLocalMusic(filesArray);
                    localMusciStack_playlistView.refocusItem(focusedItem)
                }
            }

            function onRemoveItemOfFiles(itemsArray){
                if (localMusciStack_playlistView.currentPageIndex === 1) {
                    // 本地音乐页签：取 filepath 列表删本地文件
                    var paths = [];
                    for (var i = 0; i < itemsArray.length; i++)
                        paths.push(typeof itemsArray[i] === 'object' ? itemsArray[i].filepath : itemsArray[i]);
                    Client.deleteLocalMusicFiles(paths);
                } else if (localMusciStack_playlistView.currentPageIndex === 0) {
                    // 整合页签：模型项已移除，按 songId 精确持久化（VIP 歌 filepath 为空，不能按路径匹配）
                    for (var i = 0; i < itemsArray.length; i++) {
                        var it = itemsArray[i];
                        if (typeof it === 'object' && it.songId && it.songId > 0)
                            BasicConfig.removeIntegratedMusic(it.songId, "", "");
                        else
                            BasicConfig.removeIntegratedMusic(0, it.filepath || it, it.filepath || it);
                    }
                }
            }

            function onUpdateCurGlobalFocusMediaFile(filepath){
                if (localMusciStack_playlistView.currentPageIndex === 1)
                    Client.updateFocusCurMediaFile(filepath);
            }
        }
        //-->signal from Client Of request to play next or last
        Connections {
            target: Client
            function onPlayNextTriggered() {
                localMusciStack_playlistView.autoAdvance(1);
            }
            function onPlayPreviousTriggered() {
                localMusciStack_playlistView.autoAdvance(-1);
            }
            // 网络歌解析失败（未登录/无会员/链接失效）：自动播下一首；单首则暂停
            function onNetworkPlayResolveFailed(songId) {
                localMusciStack_playlistView.handleNetworkPlayFailed();
            }
        }
    }
}
