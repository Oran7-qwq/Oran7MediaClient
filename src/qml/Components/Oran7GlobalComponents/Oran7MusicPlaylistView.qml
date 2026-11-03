import QtQuick 2.15
import QtQuick.Shapes as Shapes
import QtQuick.Controls
import QtQuick.Dialogs
import Qt5Compat.GraphicalEffects
import "../../Basic"
import Client 1.0

import Oran7UI.Impl

Item {
    id: root

    // ===== 外部属性配置 =====
    property string pageName: ""
    property int leftMargin: 0
    property int rightMargin: 0
    property bool showAddButton: true
    property bool showSearch: true
    property bool showMultiSelect: true
    property bool allowDragReorder: true
    property bool showDuration: true
    // 是否显示「喜欢」列与每行的心形收藏按钮（搜索页开启）
    property bool showLoveButton: false
    // 是否显示「整合」列与每行的整合按钮（"我整合的音乐"合集；仅在 SearchMusicStack / LocalMusicStack 开启）
    property bool showIntegrateButton: false
    // 空列表占位提示：与 pageListModels 逐页对齐，每页一个文案；空字符串表示该页不显示提示
    property var pageEmptyHintTexts: []
    // 当前页签的空列表占位文案（当前页模型为空且文案非空时，在列表中间显示渐变大字）
    readonly property string currentEmptyHintText: root.pageEmptyHintTexts && root.currentPageIndex < root.pageEmptyHintTexts.length
            ? (root.pageEmptyHintTexts[root.currentPageIndex] || "") : ""

    // ===== 模型数据 =====
    property var titleModel: null
    property var listModel: null

    // 多页面模型：数组与 titleModel 索引对齐，切换页签(focIndex)时 listModel 跟随
    // 未设置(默认 null)时保持旧行为，listModel 仍可由外部直接绑定
    property var pageListModels: null
    readonly property int currentPageIndex: playlistHeader.currentPageIndex

    // 初始化：落到第一个页签对应的模型（pageListModels 为空时保持外部 listModel 绑定不动）
    Component.onCompleted: {
        if (root.pageListModels && root.pageListModels.length > 0 && root.pageListModels[0])
            root.listModel = root.pageListModels[0];
    }

    // 把播放/选中索引同步到当前播放歌曲在本列表的位置：
    // 包含当前歌曲则聚焦正确行，否则重置 -1（避免残留旧状态）
    function syncIndicesToCurrentMedia() {
        var idx = root.indexForCurrentPlaying();
        playlistFlickable.itemSelected_index = idx;
        playlistFlickable.playingIndex = BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_MusicPlayerIndex
            && Client.playback.active ? idx : -1;
    }
    // 把当前选中/播放项滚动到列表可见区域：优先居中展示；若上方或下方元素不足
    // （靠近列表首尾）则自动贴合可见区域边缘，仅保证该项可见
    function scrollToCurrentIndex() {
        var idx = playlistFlickable.itemSelected_index >= 0 ? playlistFlickable.itemSelected_index : playlistFlickable.playingIndex;
        if (idx < 0 || !root.listModel || idx >= root.listModel.count)
            return;
        playlistFlickable.positionViewAtIndex(idx, ListView.Center);
    }
    // 歌曲身份和状态统一驱动索引；空 URL 的 VIP 歌也通过 songId 正确同步。
    Connections {
        target: Client.playback
        function onCurrentTrackChanged() { root.syncIndicesToCurrentMedia(); }
        function onStateChanged() { root.syncIndicesToCurrentMedia(); }
    }
    Connections {
        target: BasicConfig
        function onPlayerFocusChanged() { root.syncIndicesToCurrentMedia(); }
    }
    // 批量清空/追加完成后按歌曲身份恢复索引，避免导入新行后高亮停在旧下标。
    onListModelChanged: {
        Qt.callLater(root.syncIndicesToCurrentMedia)
        if (playlistFlickable) Qt.callLater(playlistFlickable.updateHoveredItem)
    }
    Connections {
        target: root.listModel
        function onCountChanged() { Qt.callLater(root.syncIndicesToCurrentMedia); }
    }

    property alias curPlayingIndex: playlistFlickable.playingIndex
    property alias curSelectingIndex: playlistFlickable.itemSelected_index
    property alias curHoveredIndex: playlistFlickable.itemHovered_index
    property bool isMultiSelected: false
    onIsMultiSelectedChanged: {
        if (!root.isMultiSelected && root.listModel) {
            for (var i = 0; i < root.listModel.count; i++) {
                root.listModel.setSelected(i, false);
            }
            multiSelectMenu.checked = false;
        }
        if (playlistFlickable) playlistFlickable.updateHoveredItem();
    }

    property var blurSource: null
    property bool enableListBlurEffect: false

    // ----------  展示列表的元素间隔和元素高度 ----------
    property int listColumSpacing: 4
    property int elementRectangleHeight: Math.max(56, playlistFontSize * 2 + 24)

    // 列表独立主题分组，不依赖下拉设置界面、主窗混色或全局强调色色阶。
    readonly property color playlistBackgroundColor: Oran7Theme.Oran7MusicPlaylistView.rowBackgroundColor
    // 背景可独立取色，文字对比度随实际底色变化，不跟随设置窗口的明暗模式。
    readonly property string playlistColorMode: (playlistBackgroundColor.r * 0.2126
            + playlistBackgroundColor.g * 0.7152 + playlistBackgroundColor.b * 0.0722) > 0.58 ? "Light" : "Dark"
    readonly property int playlistFontSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize
    readonly property color playlistBackgroundInk: Oran7Theme.Oran7MusicPlaylistView["rowText" + playlistColorMode]
    property color playlistSurface: playlistBackgroundColor
    property color playlistSurfaceHover: Qt.tint(playlistBackgroundColor,
        Qt.alpha(Oran7Theme.Oran7MusicPlaylistView["rowSurfaceHover" + playlistColorMode], 0.18))
    property color playlistSurfaceSelected: Qt.tint(playlistBackgroundColor,
        Qt.alpha(Oran7Theme.Oran7MusicPlaylistView["rowSurfaceSelected" + playlistColorMode], 0.32))
    // 空值保留原版主次配色；手动取色只覆盖文字，不污染原版预设。
    readonly property string playlistCustomTextColor: Oran7Theme.Oran7MusicPlaylistView.rowTextColor || ""
    property color playlistText: playlistCustomTextColor.length ? playlistCustomTextColor
        : Oran7Theme.Oran7MusicPlaylistView["rowText" + playlistColorMode]
    property color playlistSecondaryText: playlistCustomTextColor.length
        ? Qt.tint(playlistText, Qt.alpha(playlistBackgroundColor, 0.24))
        : Oran7Theme.Oran7MusicPlaylistView["rowSecondaryText" + playlistColorMode]
    property color playlistDetail: Oran7Theme.Oran7MusicPlaylistView["rowDetail" + playlistColorMode]
    property color playlistAccent: Oran7Theme.Oran7MusicPlaylistView.rowAccentColor

    // ----------  定义Colum的各个element统一换算比例的宽度 ----------
    property int indexTopLabelWidth: 30
    property int titleTopLabelWidth: (parent.width) * (0.5 - 0.02)  //微调
    property int albumTopLabelWidth: (parent.width) * 0.3
    property int timeSizeTopLabelWidth: (parent.width) * (0.1 - 0.02) //微调
    // ----------  定义Colum的各个element统一换算比例的Margin ----------
    property int indexTopLabelLeftMargin: 10
    property int titleTopLabelLeftMargin: 45
    property int albumTopLabelLeftMargin: root.titleTopLabelLeftMargin + root.titleTopLabelWidth
    property int timeSizeTopLabelLeftMargin: root.albumTopLabelLeftMargin + root.albumTopLabelWidth + 20

    //-----   Drag define  properties  ------
    property bool show_dragElement_floatTag: false
    property int dragSourceIndex: -1
    property int dragTargetIndex: -1
    property int hoveredGapIndex: -1
    property bool dragActive: false
    onDragActiveChanged: if (playlistFlickable) playlistFlickable.updateHoveredItem()
    property bool dragHasMoved: false

    property bool mouseInPlaylist: false

    // ===== 搜索过滤状态 =====
    property bool searchActive: false
    property string searchQuery: ""
    property string _savedPlayingPath: ""
    property string _savedSelectedPath: ""

    // ===== 事件信号 =====
    signal itemClicked(int index)
    signal itemDoubleClicked(int index)
    signal itemAdded(var files)
    signal reorderCompleted(var newOrder)
    signal itemLoved(int index)
    signal itemIntegrated(int index)

    signal itemSelectAll
    signal itemClearSelectAll

    signal ask_dir_of_item_index(string file_path)
    signal reponse_dir_of_item_index(int index)

    signal focus_current_playlistItem

    signal addNewItemOfFiles(var filesArray)
    signal removeItemOfFiles(var filesArray)
    signal updateCurGlobalFocusMediaFile(var filepath)

    // ===== 功能函数 =====

    //重新Focus playingItm 和 selectedItem
    function getfocusItem() {
        var playingPath = "";
        var selectedPath = "";

        if (playlistFlickable.playingIndex >= 0 &&
            playlistFlickable.playingIndex < root.listModel.count) {
            playingPath = root.listModel.get(playlistFlickable.playingIndex).filepath;
        }
        if (playlistFlickable.itemSelected_index >= 0 &&
            playlistFlickable.itemSelected_index < root.listModel.count) {
            selectedPath = root.listModel.get(playlistFlickable.itemSelected_index).filepath;
        }
        return {
            playingPath: playingPath,
            selectedPath: selectedPath
        };
    }

    function refocusItem(focusedItem) {
        if (!focusedItem) {
            return;
        }
        var playingPath = focusedItem.playingPath;
        var selectedPath = focusedItem.selectedPath;

        var newPlayingIndex = -1;
        var newSelectedIndex = -1;

        for (var k = 0; k < root.listModel.count; k++) {
            var fp = root.listModel.get(k).filepath;
            if (playingPath !== "" && fp === playingPath) {
                newPlayingIndex = k;
            }
            if (selectedPath !== "" && fp === selectedPath) {
                newSelectedIndex = k;
            }
        }

        playlistFlickable.playingIndex = newPlayingIndex;
        playlistFlickable.itemSelected_index = newSelectedIndex;
    }

    //从指定索引开始触发所有 playlistItem 的 animateIn 动画
    function animateItemsFromIndex(startIndex) {
        playlistFlickable.animateRecentItems(startIndex);
    }

    // 搜索匹配：检查 music_name / music_artist / music_album 是否包含关键字（不区分大小写）
    function matchesSearch(name, artist, album) {
        if (!root.searchActive) return true;
        var q = root.searchQuery.toLowerCase();
        return name.toLowerCase().indexOf(q) >= 0 ||
               artist.toLowerCase().indexOf(q) >= 0 ||
               album.toLowerCase().indexOf(q) >= 0;
    }

    // 根据 filepath 反查 model index
    function indexForFilepath(filepath) {
        if (!root.listModel || !filepath) return -1;
        return root.listModel.indexOfSource(filepath);
    }

    // 切换到指定页签（搜索结果=0 / 网易云中我喜欢的音乐=1 等）
    function switchToPage(index) {
        if (playlistHeader && index >= 0 && root.titleModel && index < root.titleModel.length)
            playlistHeader.currentPageIndex = index;
    }

    // 根据网易云 songId 反查 model index（网络歌 filepath 是临时地址会变，songId 是稳定标识）
    function indexForSongId(songId) {
        if (!root.listModel || !songId || songId <= 0) return -1;
        return root.listModel.indexOfSongId(songId);
    }

    // 定位当前播放歌曲在列表的索引：优先按 songId（网络/云端歌稳定标识），
    // 其次按 filepath（本地歌；必须非空，避免空路径误匹配所有云端项）
    function indexForCurrentPlaying() {
        if (Client.playback.currentTrack.songId > 0) {
            var sidx = root.indexForSongId(Client.playback.currentTrack.songId);
            if (sidx >= 0)
                return sidx;
        }
        var fp = Client.playback.currentTrack.path;
        if (fp && fp.length > 0) {
            var fpidx = root.indexForFilepath(fp);
            if (fpidx >= 0)
                return fpidx;
        }
        return -1;
    }

    // 搜索关闭时恢复播放/选中索引
    onSearchActiveChanged: {
        if (!root.searchActive){
            if (_savedPlayingPath !== "")
                playlistFlickable.playingIndex = indexForFilepath(_savedPlayingPath);
            if (_savedSelectedPath !== "")
                playlistFlickable.itemSelected_index = indexForFilepath(_savedSelectedPath);
            _savedPlayingPath = "";
            _savedSelectedPath = "";
            playlistFlickable.itemHovered_index = -1;
        }
    }

    // ===== 根容器 =====
    Oran7BlurCard {
        borderRadius: 7
        visible: true
        themeColor: "#0EFFFFFF"
        blurSource: root.blurSource
        blurEnabled: root.enableListBlurEffect
        anchors.fill: parent
        anchors.leftMargin: root.leftMargin
        anchors.rightMargin: root.rightMargin
        Item {
            id: container
            clip: true

            anchors.fill: parent

            Oran7PlaylistHeader {
                id: playlistHeader
                objectName: "playlistHeader"
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                z: 100
                view: root
                listView: playlistFlickable
                reorderController: playlistReorder
            }

            // 两种表头模式共用唯一背景，切换时装饰、圆角和位置均保持不变。
            Rectangle {
                id: tableHeaderBackdrop
                objectName: "playlistTableHeaderBackdrop"
                anchors.fill: listTopRow
                radius: listTopRow.radius
                color: Qt.alpha(root.playlistSurface, 0.90)
                readonly property real panelX: Math.max(142, width - 104
                    - (root.showLoveButton ? 36 : 0) - (root.showIntegrateButton ? 36 : 0))
                // 表头更矮，横向偏移按有效高度缩放，分别与 item 的左右分区保持平行。
                readonly property real leftSlant: 12 * Math.max(0, height - 2) / root.elementRectangleHeight
                readonly property real rightSlant: 18 * Math.max(0, height - 2) / root.elementRectangleHeight
                Shapes.Shape {
                    anchors.fill: parent
                    Shapes.ShapePath {
                        strokeColor: "transparent"
                        fillColor: Qt.alpha(root.playlistBackgroundInk, 0.07)
                        startX: 4; startY: 1
                        PathLine { x: 99; y: 1 }
                        PathLine { x: 99 - tableHeaderBackdrop.leftSlant; y: tableHeaderBackdrop.height - 1 }
                        PathLine { x: 4; y: tableHeaderBackdrop.height - 1 }
                        PathLine { x: 1; y: tableHeaderBackdrop.height - 4 }
                        PathLine { x: 1; y: 4 }
                        PathLine { x: 4; y: 1 }
                    }
                    Shapes.ShapePath {
                        strokeColor: "transparent"
                        fillColor: Qt.alpha(root.playlistBackgroundInk, 0.055)
                        startX: tableHeaderBackdrop.panelX + tableHeaderBackdrop.rightSlant; startY: 1
                        PathLine { x: tableHeaderBackdrop.width - 4; y: 1 }
                        PathLine { x: tableHeaderBackdrop.width - 1; y: 4 }
                        PathLine { x: tableHeaderBackdrop.width - 1; y: tableHeaderBackdrop.height - 4 }
                        PathLine { x: tableHeaderBackdrop.width - 4; y: tableHeaderBackdrop.height - 1 }
                        PathLine { x: tableHeaderBackdrop.panelX; y: tableHeaderBackdrop.height - 1 }
                        PathLine { x: tableHeaderBackdrop.panelX + tableHeaderBackdrop.rightSlant; y: 1 }
                    }
                    Shapes.ShapePath {
                        strokeColor: "transparent"
                        fillColor: Qt.alpha(root.playlistAccent, 0.16)
                        startX: tableHeaderBackdrop.panelX - 1; startY: 1
                        PathLine { x: tableHeaderBackdrop.panelX + 4; y: 1 }
                        PathLine { x: tableHeaderBackdrop.panelX + 4 - tableHeaderBackdrop.rightSlant; y: tableHeaderBackdrop.height - 1 }
                        PathLine { x: tableHeaderBackdrop.panelX - 1 - tableHeaderBackdrop.rightSlant; y: tableHeaderBackdrop.height - 1 }
                        PathLine { x: tableHeaderBackdrop.panelX - 1; y: 1 }
                    }
                }
                Rectangle {
                    x: 104; y: 1; height: 1
                    width: Math.max(0, tableHeaderBackdrop.panelX - 128)
                    color: Qt.alpha(root.playlistBackgroundInk, 0.12)
                }
                Row {
                    x: 104; y: parent.height - 3
                    spacing: 4
                    Rectangle { width: 25; height: 1; color: Qt.alpha(root.playlistAccent, 0.75) }
                    Rectangle { width: 4; height: 1; color: Qt.alpha(root.playlistAccent, 0.45) }
                    Rectangle { width: 2; height: 1; color: Qt.alpha(root.playlistAccent, 0.45) }
                }
                Item {
                    x: parent.width - 118; y: 3
                    width: 16; height: 4
                    visible: tableHeaderBackdrop.width >= 500
                    Repeater {
                        model: 8
                        Rectangle {
                            required property int index
                            x: (index % 4) * 5; y: Math.floor(index / 4) * 3
                            width: 1; height: 1
                            color: Qt.alpha(root.playlistBackgroundInk, 0.24)
                        }
                    }
                }
            }

            // ===== 多选菜单 =====
            Oran7PlaylistSelection {
                id: multiSelectMenu
                objectName: "playlistSelection"
                view: root
                listView: playlistFlickable
                reorderController: playlistReorder
                headerRow: listTopRow
            }

            //-----------------------  分割线1 -----------------------//
            Rectangle {
                id: dive1
                width: parent.width - 20
                height: 1
                color: Qt.alpha(root.playlistText, 0.18)
                anchors.left: parent.left
                anchors.top: listTopRow.bottom
                anchors.topMargin: 4
            }

            //-----------------------  Rectangle"#  , 标题 , 专辑 , '     ' ,  时长"  -----------------------//
            Oran7PlaylistColumns {
                id: listTopRow
                objectName: "playlistColumns"
                view: root
                header: playlistHeader
            }

            Oran7PlaylistReorderController {
                id: playlistReorder
                objectName: "playlistReorder"
                anchors.fill: parent
                z: 20
                view: root
                listView: playlistFlickable
                divider: dive1
            }

            //-----------------------滑动列表右下的手动添加导入本地文件"+"-----------------------//
            Rectangle {
                id: addNewFileRectangle
                property int initWidth: 50
                property bool hovered: false
                width: addNewFileRectangle.hovered ? initWidth*1.05 : initWidth
                Behavior on width{NumberAnimation{duration: Oran7Theme.Primary.durationMid;easing.type: Easing.OutCubic}}
                height: addNewFileRectangle.hovered ? initWidth*1.05 : initWidth
                Behavior on height{NumberAnimation{duration: Oran7Theme.Primary.durationMid;easing.type: Easing.OutCubic}}
                z: 1
                anchors.right: parent.right
                anchors.rightMargin: addNewFileRectangle.initWidth
                anchors.bottom: parent.bottom
                anchors.bottomMargin: addNewFileRectangle.initWidth
                color: "white"
                radius: addNewFileRectangle.initWidth / 2

                visible: root.showAddButton && (root.isMultiSelected ? false : true)

                layer.enabled: true
                layer.effect: DropShadow {
                    id: addNewFileRectangleDropShadow
                    anchors.fill: addNewFileRectangle
                    z: addNewFileRectangle.z
                    source: addNewFileRectangle
                    color: "#80000000"
                    radius: 15
                    samples: 30
                    horizontalOffset: 5
                    verticalOffset: 5
                }
                Item{
                    anchors.fill: parent
                    rotation: addNewFileRectangle.hovered ? 90 : 0
                    Behavior on rotation{NumberAnimation{duration: Oran7Theme.Primary.durationMid;easing.type: Easing.OutCubic}}
                    Oran7GradientMask {
                        id: addIconMask
                        anchors.centerIn: parent

                        width: addNewFileRectangle.initWidth * 0.55
                        height: addNewFileRectangle.initWidth * 0.55

                        gradientMaskEnabled: true
                        dynamicGradient: true
                        transitionDuration:  400
                        //radientColorIndex: 1
                        dynamicInterval: 400

                        Rectangle {
                            width: addIconMask.width
                            height: 5.5
                            radius: 2
                            color: "white"
                            anchors.centerIn: parent
                        }

                        Rectangle {
                            width: 5.5
                            height: addIconMask.height
                            radius: 2
                            color: "white"
                            anchors.centerIn: parent
                        }
                    }
                }

                // <<<<<<功能组件，打开文件对话框>>>>>>//
                Oran7FileDialog {
                    id: addNewFileRectangle_FileDialog
                    selectReset: true
                    fileMode: FileDialog.OpenFiles
                    onReady: {
                        root.addNewItemOfFiles(filesArray);
                    }
                }

                MouseArea {
                    id: addMusicMouseArea
                    anchors.centerIn: addNewFileRectangle
                    width: addNewFileRectangle.initWidth
                    height: width
                    onPressed: {
                        addNewFileRectangle.width = addNewFileRectangle.initWidth*0.95
                        addNewFileRectangle.height = addNewFileRectangle.initWidth*0.95
                    }
                    onReleased: {
                        addNewFileRectangle.width = addNewFileRectangle.initWidth
                        addNewFileRectangle.height = addNewFileRectangle.initWidth
                    }
                    onClicked: {
                        addNewFileRectangle_FileDialog.open();
                    }
                }
                HoverHandler{
                    enabled: true
                    acceptedDevices: PointerDevice.Mouse
                    onHoveredChanged: {
                        playlistReorder.cursorShape=  hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
                        parent.hovered = hovered
                    }
                }
            }

            // ======== main of playlistFlickable ========
            ListView {
                id: playlistFlickable
                objectName: "playlistListView"
                model: root.listModel

                boundsBehavior: Flickable.StopAtBounds
                pixelAligned: true
                reuseItems: false

                // 滚动期间仅暂停过渡动画，仍按鼠标实际位置保留当前行高亮。
                readonly property bool hoverSuspended: moving || dragging || flicking || scrollQuietTimer.running
                onMovingChanged: {
                    if (moving) scrollQuietTimer.restart()
                    updateHoveredItem()
                }
                Timer {
                    id: scrollQuietTimer
                    interval: 120
                }

                spacing: root.listColumSpacing
                topMargin: root.listColumSpacing

                footer: Item {
                    width: playlistFlickable.width
                    height: root.elementRectangleHeight + root.listColumSpacing * 2
                }

                cacheBuffer: root.elementRectangleHeight * 3
                anchors.top: dive1.bottom
                anchors.topMargin: 2
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                clip: true

                property int playingIndex: -1 // '-1' of no item isPlaying ,what is paused statue
                property int itemSelected_index: -1
                property int itemHovered_index: -1
                // 统一命中视口中的鼠标；内容滚动也重新计算，不依赖逐行 HoverHandler 收到鼠标移动。
                function updateHoveredItem() {
                    if (!playlistHoverHandler) return
                    var next = -1
                    // 处理器独立挂在固定视口，坐标不随 contentItem 滚动。
                    var pointer = playlistHoverHandler.point.position
                    var point = mapFromItem(playlistPointerViewport, pointer.x, pointer.y)
                    if (playlistHoverHandler.hovered && !root.isMultiSelected && !root.dragActive
                        && point.x >= 0 && point.x < width && point.y >= 0 && point.y < height) {
                        var candidate = indexAt(point.x + contentX, point.y + contentY)
                        var item = candidate >= 0 ? itemAtIndex(candidate) : null
                        if (item && item.visible) {
                            var local = item.mapFromItem(playlistFlickable, point.x, point.y)
                            if (local.x >= 0 && local.x < item.width && local.y >= 0 && local.y < item.height)
                                next = candidate
                        }
                    }
                    if (itemHovered_index !== next) itemHovered_index = next
                }
                onContentXChanged: updateHoveredItem()
                onCountChanged: Qt.callLater(playlistFlickable.updateHoveredItem)
                onWidthChanged: Qt.callLater(playlistFlickable.updateHoveredItem)
                onHeightChanged: Qt.callLater(playlistFlickable.updateHoveredItem)

                property int indexWidth: 30
                property int titleWidth: root.titleTopLabelWidth
                property int albumWidth: root.albumTopLabelWidth
                property int timeWidth: root.timeSizeTopLabelWidth

                property int indexLeftMargin: 10
                property int titleLeftMargin: 45
                property int albumLeftMargin: titleLeftMargin + titleWidth + 10
                property int timeLeftMargin: albumLeftMargin + albumWidth + 20

                ScrollBar.vertical: ScrollBar {
                    anchors.right: parent.right
                    anchors.rightMargin: 0
                    width: 10
                }

                // 对外接口--->触发指定元素的动画
                function animateItem(index) {
                    root.listModel.setAnimationPending(index, true);
                    var item = playlistFlickable.itemAtIndex(index);
                    if (item) {
                        item.needsAnimateIn = true;
                        item.animateIn();
                    }
                }
                // 对外接口--->触发所有最近添加的元素的动画
                function animateRecentItems(startIndex) {
                    for (var i = startIndex; i < root.listModel.count; i++) {
                        root.listModel.setAnimationPending(i, true);
                        var item = playlistFlickable.itemAtIndex(i);
                        if (item) {
                            item.needsAnimateIn = true;
                            item.animateIn();
                        }
                    }
                }

                // 监听 contentY 变化
                property bool contentY_atTop: true
                property bool contentY_atBottom: false
                onContentYChanged: {
                    scrollQuietTimer.restart();
                    updateHoveredItem();
                    var minY = playlistReorder.minContentY();
                    var maxY = playlistReorder.maxContentY();
                    var tolerance = 1.0;
                    playlistFlickable.contentY_atBottom = contentY >= (maxY - tolerance);
                    playlistFlickable.contentY_atTop = contentY <= (minY + tolerance);
                }

                delegate: Oran7PlaylistDelegate {
                    view: root
                    listView: playlistFlickable
                    reorderController: playlistReorder
                }
            }
            // 与 ListView 同级，避免 Flickable 把处理器自动挂入可滚动的 contentItem。
            Item {
                id: playlistPointerViewport
                anchors.fill: playlistFlickable
                HoverHandler {
                    id: playlistHoverHandler
                    acceptedDevices: PointerDevice.Mouse
                    onPointChanged: playlistFlickable.updateHoveredItem()
                    onHoveredChanged: {
                        root.mouseInPlaylist = playlistHoverHandler.hovered;
                        playlistFlickable.updateHoveredItem();
                    }
                }
            }

            // ===== 空列表占位提示：当前页签模型为空时，在列表中间显示渐变大字（内容由 pageEmptyHintTexts 提供）=====
            Item {
                id: emptyHintArea
                visible: root.currentEmptyHintText.length > 0
                        && root.listModel && root.listModel.count === 0
                        && !root.isMultiSelected
                anchors.horizontalCenter: playlistFlickable.horizontalCenter
                anchors.verticalCenter: playlistFlickable.verticalCenter
                z: 5

                Oran7GradientMask {
                    anchors.centerIn: parent
                    width: emptyHintLabel.implicitWidth
                    height: emptyHintLabel.implicitHeight
                    gradientColorIndex: 0
                    dynamicGradient: true
                    gradientMaskEnabled: true
                    transitionDuration: 1000
                    dynamicInterval: 1000

                    Label {
                        id: emptyHintLabel
                        text: root.currentEmptyHintText
                        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                        font.bold: false
                        font.pixelSize: 20
                        color: "white"
                    }
                }
            }
        }
    }
}
