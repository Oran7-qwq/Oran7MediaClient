import QtQuick 2.15
import QtQuick.Controls
import QtCore
import QtQuick.Dialogs
import Qt5Compat.GraphicalEffects
import "../../Basic"
import "../../Components/Oran7GlobalComponents"
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


        //初始化属性
        titleModel: [qsTranslate("Oran7Ui", "我整合的音乐"),qsTranslate("Oran7Ui", "本地音乐"),qsTranslate("Oran7Ui", "下载中歌曲")]
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
            Client.musicQueue.activate(listModel, index);
            syncIndicesToCurrentMedia();
        }

        onItemClicked: function (index) {
            // console.log("clicked:",index)
            Client.musicQueue.activate(listModel, index);
            syncIndicesToCurrentMedia();
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
            target: localMusciStack_playlistView
            //--> signal from this for response to the file_path dir of item index
            function onReponse_dir_of_item_index(index) {
                localMusciStack_playlistView.deal_outClickplayReponse_asyncUiStatue(index);
            }

            // 以下均为本地音乐专属的 Client 操作，只在「本地音乐」页签(索引1)生效，
            // 避免在「我整合的音乐」页签误改本地数据
            // 模型顺序由 C++ 直接同步到曲库/配置，不再通过路径数组回传。
            function onAddNewItemOfFiles(filesArray) {
                if (localMusciStack_playlistView.currentPageIndex === 1) {
                    var focusedItem = localMusciStack_playlistView.getfocusItem();
                    Client.importLocalMusic(filesArray);
                    localMusciStack_playlistView.refocusItem(focusedItem)
                }
            }

            function onRemoveItemOfFiles(itemsArray){
                if (localMusciStack_playlistView.currentPageIndex === 1) {
                    // 本地音乐页签：取 filepath 列表删本地文件
                    var paths = [];
                    for (var i = 0; i < itemsArray.length; i++)
                        paths.push(typeof itemsArray[i] === 'object' ? itemsArray[i].filepath : itemsArray[i]);
                    Client.removeLocalMusic(paths);
                } else if (localMusciStack_playlistView.currentPageIndex === 0) {
                    // 整合页签：模型项已移除，按 songId 精确持久化（VIP 歌 filepath 为空，不能按路径匹配）
                    for (var i = 0; i < itemsArray.length; i++) {
                        var it = itemsArray[i];
                        if (typeof it === 'object' && it.songId && it.songId > 0)
                            BasicConfig.removeIntegratedMusic(it.songId, "", "");
                        else
                            BasicConfig.removeIntegratedMusic(0, it.url || "", it.filepath || (typeof it === "string" ? it : ""));
                    }
                }
            }

            function onUpdateCurGlobalFocusMediaFile(filepath){
                if (localMusciStack_playlistView.currentPageIndex === 1)
                    Client.focusLocalMusic(filepath);
            }
        }

    }
}
