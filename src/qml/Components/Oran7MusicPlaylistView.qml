import QtQuick 2.15
import QtQuick.Controls
import QtQuick.Dialogs
import Qt5Compat.GraphicalEffects
import "../Basic"
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

    // ===== 模型数据 =====
    property var titleModel: null
    property ListModel listModel: null
    property alias curPlayingIndex: playlistFlickable.playingIndex
    property alias curSelectingIndex: playlistFlickable.itemSelected_index
    property alias curHoveredIndex: playlistFlickable.itemHovered_index
    property bool isMultiSelected: false
    onIsMultiSelectedChanged: {
        if (!root.isMultiSelected && root.listModel) {
            for (var i = 0; i < root.listModel.count; i++) {
                root.listModel.setProperty(i, "isSelected", false);
            }
            multiSelectMenu.checked = false;
        }
    }

    property var blurSource: null
    property bool enableListBlurEffect: false

    // ----------  展示列表的元素间隔和元素高度 ----------
    property int listColumSpacing: 4
    property int elementRectangleHeight: 50

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
        if (!root.listModel) return -1;
        for (var k = 0; k < root.listModel.count; k++) {
            if (root.listModel.get(k).filepath === filepath) return k;
        }
        return -1;
    }

    // 搜索关闭时恢复播放/选中索引
    onSearchActiveChanged: {
        if (!root.searchActive) {
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

            // ===== 标题栏 =====
            Flow {
                id: titleFlow
                anchors.top: parent.top
                anchors.left: parent.left
                spacing: 16

                Repeater {
                    id: titleRepeater
                    model: root.titleModel ? root.titleModel : ["列表"]
                    anchors.fill: parent
                    property int focIndex: 0
                    delegate: Label {
                        text: modelData
                        font.family: "微软雅黑"
                        font.bold: true
                        font.pixelSize: 24
                        color: index === titleRepeater.focIndex ? Oran7Theme.Oran7MusicPlaylistView["colorPrimaryBase-5"] :
                                             Oran7Theme.Oran7MusicPlaylistView["colorPrimaryBase-8"]

                        Rectangle {
                            id: underline
                            width: parent.implicitWidth - 24
                            height: 3
                            radius: 1
                            anchors.top: parent.bottom
                            anchors.topMargin: 3
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: index === titleRepeater.focIndex ? Oran7Theme.Oran7MusicPlaylistView["colorPrimaryBase-7"] : "transparent"
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                titleRepeater.focIndex = index;
                            }
                        }
                    }
                }
            }

            // ===== 搜索框 =====
            Rectangle {
                id: searchRect
                anchors.right: multiSelectRect.left
                anchors.rightMargin: 10
                anchors.verticalCenter: titleFlow.verticalCenter
                anchors.top: parent.top

                property int initWidth: showSearch ? 60 : 0
                width: searchRect.initWidth
                height: 30
                color: "#fef2e8"
                radius: height / 2
                border.width: 1
                border.color: "#4d4d56"
                visible: showSearch

                TextField {
                    id: searchField
                    anchors.top: parent.top
                    anchors.topMargin: 2
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 2
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    color: "cyan"
                    font.pixelSize: 16
                    font.family: "微软雅黑"
                    background: Rectangle {
                        color: "transparent"
                    }

                    onFocusChanged: {
                        if (focus) {
                            BasicConfig.newTextAreaFocused(searchField);
                            searchRectParallelAnimation_maxsize.restart();
                        }
                        else{
                            if(searchField.text.length === 0)
                                searchRectParallelAnimation_minisize.restart();
                        }

                    }

                    onTextChanged: {
                        var text = searchField.text.trim();
                        if (text.length === 0) {
                            root.searchActive = false;
                            root.searchQuery = "";
                        } else {
                            if (!root.searchActive) {
                                _savedPlayingPath = playlistFlickable.playingIndex >= 0 ?
                                    root.listModel.get(playlistFlickable.playingIndex).filepath : "";
                                _savedSelectedPath = playlistFlickable.itemSelected_index >= 0 ?
                                    root.listModel.get(playlistFlickable.itemSelected_index).filepath : "";
                            }
                            root.searchActive = true;
                            root.searchQuery = text;
                        }
                    }
                }
                Connections {
                    target: BasicConfig
                    function onClickedOutside() {
                        if (searchField.focus === true)
                            searchField.focus = false;
                    }
                }

                ParallelAnimation {
                    id: searchRectParallelAnimation_maxsize
                    PropertyAnimation {
                        target: searchRect
                        property: "width"
                        from: searchRect.initWidth
                        to: searchRect.initWidth * 2
                        easing.type: Easing.OutCubic
                    }
                }
                ParallelAnimation {
                    id: searchRectParallelAnimation_minisize
                    PropertyAnimation {
                        target: searchRect
                        property: "width"
                        from: searchRect.width
                        to: searchRect.initWidth
                        easing.type: Easing.OutCubic
                    }
                }
                // MouseArea {
                //     hoverEnabled: true
                //     anchors.fill: parent
                //     onEntered: {
                //         searchRectParallelAnimation_maxsize.start();
                //     }
                //     onExited: {
                //         searchRectParallelAnimation_minisize.start();
                //         // searchRectangle.forceActiveFocus()
                //     }
                //     onClicked: {
                //         searchField.forceActiveFocus();
                //     }
                // }
            }

            // ===== 多选按钮 =====
            Rectangle {
                id: multiSelectRect
                width: 40
                height: width
                anchors.right: parent.right
                anchors.rightMargin: 30
                anchors.verticalCenter: searchRect.verticalCenter

                color: "transparent"
                visible: showMultiSelect

                Image {
                    id: multiSelectIcon
                    anchors.fill: parent
                    source: "qrc:/image/selectmore.png"
                    sourceSize.width: parent.width
                    sourceSize.height: parent.height

                    asynchronous: true
                    cache: false
                    mipmap: true

                    property color colorOverlay: Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        source: multiSelectIcon
                        color: multiSelectIcon.colorOverlay
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            multiSelectIcon.colorOverlay = Oran7Theme.Oran7MusicPlaylistView["textColorBase-4"];
                            cursorShape = Qt.PointingHandCursor;
                        }
                        onExited: {
                            multiSelectIcon.colorOverlay = Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"];
                            cursorShape = Qt.ArrowCursor;
                        }
                        onClicked: {
                            root.isMultiSelected = !root.isMultiSelected;
                        }
                    }
                }
            }

            // ===== 多选菜单 =====
            Rectangle {
                id: multiSelectMenu
                height: 28
                anchors.left: playlistFlickable.left
                anchors.right: playlistFlickable.right
                anchors.bottom: dive1.top
                anchors.bottomMargin: 7
                color: "transparent"
                visible: root.isMultiSelected

                property bool checked: false
                onCheckedChanged: {
                    if (!root.listModel)
                        return;
                    if (checked) {
                        for (var i = 0; i < root.listModel.count; i++) {
                            if (root.searchActive) {
                                if (root.matchesSearch(root.listModel.get(i).music_name,
                                                       root.listModel.get(i).music_artist,
                                                       root.listModel.get(i).music_album))
                                    root.listModel.setProperty(i, "isSelected", true);
                            } else {
                                root.listModel.setProperty(i, "isSelected", true);
                            }
                        }
                    } else {
                        for (var j = 0; j < root.listModel.count; j++)
                            root.listModel.setProperty(j, "isSelected", false);
                    }
                }

                Image {
                    id: selectAllIcon
                    sourceSize.width: 30
                    sourceSize.height: 30
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    anchors.top: parent.top
                    anchors.topMargin: 2

                    asynchronous: false
                    smooth: true
                    mipmap: true
                    antialiasing: true

                    source: "qrc:/image/selectAll.png"
                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        source: selectAllIcon
                        color: multiSelectMenu.checked ? Oran7Theme.Oran7MusicPlaylistView["textColorBase-7"] :
                                    Oran7Theme.Oran7MusicPlaylistView["textColorBase-5"]
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            cursorShape = Qt.PointingHandCursor;
                        }
                        onExited: {
                            cursorShape = Qt.ArrowCursor;
                        }
                        onClicked: {
                            multiSelectMenu.checked = !multiSelectMenu.checked;
                        }
                    }
                }

                Text {
                    id: selectAllText
                    anchors.left: selectAllIcon.right
                    anchors.leftMargin: 4
                    anchors.verticalCenter: selectAllIcon.verticalCenter
                    color: Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
                    text: "全选"
                    font.pixelSize: 18
                    font.family: "微软雅黑"
                    font.bold: false
                }

                Image{
                    id:deleteBtn
                    width: 37
                    height: 37
                    sourceSize:Qt.size(512,512) //分辨率
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 32
                    source: "qrc:/image/material-symbols_delete-outline-rounded.png"

                    property bool hovered: false
                    layer.enabled: true
                    layer.effect:ColorOverlay{
                        source:deleteBtn
                        color:deleteBtn.hovered ? Oran7Theme.Oran7MusicPlaylistView["textColorBase-4"] :
                                                Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
                    }
                    HoverHandler{
                        acceptedDevices: PointerDevice.Mouse
                        onHoveredChanged: {
                            parent.hovered = hovered
                        }
                    }
                    MouseArea{
                        anchors.fill: parent
                        onClicked: {
                            if (!root.listModel) return;
                            var selectedPaths = [];
                            var curPlayingFilePath = BasicConfig.currentMediaFilePath
                            var deletedPlaying = false;
                            for (var i = 0; i < root.listModel.count; i++)
                            {
                                if (root.listModel.get(i).isSelected === true) {
                                    selectedPaths.push(root.listModel.get(i).filepath);
                                    if (String(curPlayingFilePath) === String(root.listModel.get(i).filepath))
                                        deletedPlaying = true;
                                }
                            }
                            if (selectedPaths.length > 0)
                            {
                                // C++ 端内部会同步停止播放器再删文件，无需手动 sigStop()
                                root.removeItemOfFiles(selectedPaths);
                                // 从后往前移除，避免索引偏移
                                for (var j = root.listModel.count - 1; j >= 0; j--)
                                {
                                    if (root.listModel.get(j).isSelected === true) {
                                        root.listModel.remove(j);
                                    }
                                }

                                // 移除后用 filepath 重查索引（与 reOrder 相同模式）
                                if (root.listModel.count > 0) {
                                    var newPlayIdx = -1;
                                    for (var k = 0; k < root.listModel.count; k++) {
                                        var fp = root.listModel.get(k).filepath;
                                        if (curPlayingFilePath !== "" && String(fp) === String(curPlayingFilePath))
                                            newPlayIdx = k;
                                    }
                                    playlistFlickable.playingIndex = newPlayIdx;
                                    playlistFlickable.itemSelected_index = newPlayIdx;
                                }
                            }

                            // 当前播放的文件被删，重新 focus 列表首个元素
                            if (deletedPlaying && root.listModel.count > 0) {
                                root.updateCurGlobalFocusMediaFile(root.listModel.get(0).filepath);
                                BasicConfig.isPlaying = false
                            }

                        }
                    }
                }

                //<--multiSelectMenu END
            }

            //-----------------------  分割线1 -----------------------//
            Rectangle {
                id: dive1
                width: parent.width - 24
                height: 1
                color: Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
                anchors.left: parent.left
                anchors.top: listTopRow.bottom
                anchors.topMargin: 6
            }

            //-----------------------  Rectangle"#  , 标题 , 专辑 , '     ' ,  时长"  -----------------------//
            Rectangle {
                id: listTopRow
                anchors.top: titleFlow.bottom
                anchors.topMargin: 16
                anchors.left: parent.left
                anchors.right: parent.right
                height: 28

                color: "transparent"

                visible: root.isMultiSelected ? false : true

                //  #
                Label {
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: root.indexTopLabelLeftMargin
                    width: root.indexTopLabelWidth

                    text: " # "
                    font.family: "微软雅黑"
                    font.pixelSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize
                    //Behavior on font.pixelSize{NumberAnimation{duration: Oran7Theme.Primary.durationMid}}
                    color: Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
                    Behavior on color{PropertyAnimation{duration: Oran7Theme.Primary.durationMid}}
                }
                // 标题
                Label {
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter
                    width: root.titleTopLabelWidth
                    anchors.left: parent.left
                    anchors.leftMargin: root.titleTopLabelLeftMargin
                    text: "   标题"
                    font.family: "微软雅黑"
                    font.pixelSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize
                    //Behavior on font.pixelSize{NumberAnimation{duration: Oran7Theme.Primary.durationMid}}
                    color: Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
                    Behavior on color{PropertyAnimation{duration: Oran7Theme.Primary.durationMid}}
                    background: Rectangle {
                        anchors.fill: parent
                        anchors.top: parent.top
                        anchors.topMargin: -4
                        radius: 8
                        opacity: 0.15
                        color: "transparent"
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: {
                                parent.color = "#7d7d84";
                            }
                            onExited: {
                                parent.color = "transparent";
                            }
                        }
                    }
                }
                // 专辑
                Label {
                    id: localAlbumTopLabel
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter
                    text: "  专辑"
                    width: root.albumTopLabelWidth
                    anchors.left: parent.left
                    anchors.leftMargin: root.albumTopLabelLeftMargin

                    font.family: "微软雅黑"
                    font.pixelSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize
                    //Behavior on font.pixelSize{NumberAnimation{duration: Oran7Theme.Primary.durationMid}}
                    color: Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
                    Behavior on color{PropertyAnimation{duration: Oran7Theme.Primary.durationMid}}
                    background: Rectangle {
                        anchors.fill: parent
                        anchors.top: parent.top
                        anchors.topMargin: -4
                        radius: 8
                        opacity: 0.15
                        color: "transparent"
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: {
                                parent.color = "#7d7d84";
                            }
                            onExited: {
                                parent.color = "transparent";
                            }
                        }
                    }
                }
                // "    "
                Label {
                    id: localTopLabel
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter
                    text: "    "
                    width: 50
                    anchors.left: parent.left
                    anchors.leftMargin: 4

                    font.family: "微软雅黑"
                    font.pixelSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize
                    //Behavior on font.pixelSize{NumberAnimation{duration: Oran7Theme.Primary.durationMid}}
                    color: "#2a1a22"
                }
                // 时长
                Label {
                    id: localTimeSizeTopLabel
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter
                    text: "   时长"
                    width: root.timeSizeTopLabelWidth

                    anchors.left: parent.left
                    anchors.leftMargin: root.timeSizeTopLabelLeftMargin

                    font.family: "微软雅黑"
                    font.pixelSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize
                    //Behavior on font.pixelSize{NumberAnimation{duration: Oran7Theme.Primary.durationMid}}
                    color: Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
                    Behavior on color{PropertyAnimation{duration: Oran7Theme.Primary.durationMid}}
                    background: Rectangle {
                        anchors.fill: parent
                        anchors.top: parent.top
                        anchors.topMargin: -4
                        radius: 8
                        opacity: 0.15
                        color: "transparent"
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: {
                                parent.color = "#7d7d84";
                            }
                            onExited: {
                                parent.color = "transparent";
                            }
                        }
                    }
                }
            }

            // ===== 拖拽提示线 =====  // 插入位置提示线组件 , 以every item 的Top为标准-->
            Rectangle {
                id: insertLine
                visible: false
                z: playlistFlickable.z + 1
                y: dive1.y
                height: 4
                width: playlistFlickable.width - 40
                radius: 2

                Behavior on y {
                    enabled: insertLine.visible
                    NumberAnimation {
                        duration: 80
                        easing.type: Easing.OutCubic
                    }
                }

                // 渐变效果：两侧淡色，中间原色
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0.0
                        color: "#ffb3b3"
                    }
                    GradientStop {
                        position: 0.45
                        color: "#ff7384"
                    }
                    GradientStop {
                        position: 0.5
                        color: "#ff7384"
                    }
                    GradientStop {
                        position: 0.8
                        color: "#ff9999"
                    }
                    GradientStop {
                        position: 1.0
                        color: "#ffb3b3"
                    }
                }
                opacity: 1
                Behavior on opacity {
                    NumberAnimation {
                        duration: 100
                        easing.type: Easing.OutQuad
                    }
                }
            }
            //<----The dragElementFloatTag Ui
            Oran7BlurCard {
                id: dragElementFloatTag
                x: root.x + 100
                y: 0
                z: playlistFlickable.z + 1
                clip: true
                width: 300
                height: 48
                borderRadius: 7
                themeColor: "#04FFFFFF"
                blurSource: BasicConfig.mainWindow ? BasicConfig.mainWindow.__Oran7WindowBackGround__ : null
                opacity: 1
                visible: root.show_dragElement_floatTag
                //这些都是默认甚至是无效的初值，需要外部预先初始化设定
                property int dragElementFloatTag_ParallelAniamtion_Duration: 200
                property int dragElementFloatTag_YChangeAmination_fromY: 0
                property int dragElementFloatTag_YChangeAmination_toY: 0
                property real dragElementFloatTag_OpacityChangeAnimation_fromValue: 0
                property real dragElementFloatTag_OpacityChangeAnimation_toValue: 1
                ParallelAnimation {
                    id: dragElementFloatTag_ParallelAnimation
                    PropertyAnimation {
                        target: dragElementFloatTag
                        property: "y"
                        from: dragElementFloatTag.dragElementFloatTag_YChangeAmination_fromY
                        to: dragElementFloatTag.dragElementFloatTag_YChangeAmination_toY
                        duration: dragElementFloatTag.dragElementFloatTag_ParallelAniamtion_Duration
                        easing.type: Easing.OutQuad
                    }
                }
                PropertyAnimation {
                    id: dragElementFloatTag_OpacityChangeAnimation
                    target: dragElementFloatTag
                    properties: "opacity"
                    from: dragElementFloatTag.dragElementFloatTag_OpacityChangeAnimation_fromValue
                    to: dragElementFloatTag.dragElementFloatTag_OpacityChangeAnimation_toValue
                    duration: dragElementFloatTag.dragElementFloatTag_ParallelAniamtion_Duration
                    easing.type: Easing.OutQuad
                }

                property string musicTagIconImageSource: "qrc:/image/Oran7.jpg"
                property string musicTagName: "Oran7"
                property string musicTagArtist: "Oran柒"

                Oran7RoundedImage{
                    id: musicTagIconImageRectangle
                    width: 36
                    height: width
                    anchors.left: parent.left
                    anchors.leftMargin: 7
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 7
                    source: dragElementFloatTag.musicTagIconImageSource
                }

                Label {
                    text: dragElementFloatTag.musicTagName
                    color: Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
                    clip: true
                    font.pixelSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize
                    font.family: "微软雅黑"
                    font.bold: true
                    width: parent.width
                    anchors.left: musicTagIconImageRectangle.right
                    anchors.leftMargin: 4
                    anchors.top: parent.top
                    anchors.topMargin: 2
                }

                //仿：超清母带
                Rectangle {
                    id: audioTagTypeRectangle
                    color: "transparent"
                    width: 44
                    height: 16
                    anchors.left: musicTagIconImageRectangle.right
                    anchors.leftMargin: 4
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin:6
                    border.color: Oran7Theme.Oran7MusicPlaylistView["colorPrimaryBase-8"]
                    border.width: 0.8
                    radius: 2
                    Label {
                        text: "超清母带"
                        color: Oran7Theme.Oran7MusicPlaylistView["colorPrimaryBase-5"]
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                        font.family: "微软雅黑"
                        font.bold: true
                        font.pixelSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize-7 < 10 ? 10 :
                                                                                   Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize-7
                        //Behavior on font.pixelSize{NumberAnimation{duration: Oran7Theme.Primary.durationMid}}
                    }
                }
                Label {
                    text: dragElementFloatTag.musicTagArtist
                    color: Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
                    clip: true
                    font.pixelSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize - 5 < 12 ? 12 :
                                                    Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize - 5
                    font.family: "微软雅黑"
                    font.bold: true
                    font.italic: true
                    width: dragElementFloatTag.width - 100
                    anchors.left: audioTagTypeRectangle.right
                    anchors.leftMargin: 2
                    anchors.bottom: audioTagTypeRectangle.bottom
                }
            }

            // ---- 拖动到边缘时自动滚动 ----
            Timer {
                id: flickTimer
                repeat: true
                running: false
                interval: 16

                property int direction: 0
                property real speed: 0

                onTriggered: {
                    if (!root.dragActive || direction === 0) {
                        direction = 0;
                        speed = 0;
                        stop();
                        return;
                    }

                    var minY = container.minContentY();
                    var maxY = container.maxContentY();

                    var cy = container.clampValue(playlistFlickable.contentY, minY, maxY);
                    if (playlistFlickable.contentY !== cy)
                        playlistFlickable.contentY = cy;

                    if ((direction < 0 && cy <= minY) ||
                        (direction > 0 && cy >= maxY)) {
                        direction = 0;
                        speed = 0;
                        stop();
                        container.updateInsertLineOnly(container.dragMouseYInContainer);
                        return;
                    }

                    var newY = container.clampValue(cy + direction * speed, minY, maxY);

                    if (Math.abs(newY - cy) < 0.01) {
                        direction = 0;
                        speed = 0;
                        stop();
                        return;
                    }

                    playlistFlickable.contentY = newY;
                    container.updateInsertLineOnly(container.dragMouseYInContainer);
                }
            }
            //  ----------  drag of algorithm function ----------

            property real dragMouseYInContainer: -1

            // ---- 基础工具函数 ----
            function clampValue(v, minValue, maxValue) {
                return Math.max(minValue, Math.min(v, maxValue));
            }

            function minContentY() {
                return playlistFlickable.originY;
            }

            function maxContentY() {
                return Math.max(
                    minContentY(),
                    playlistFlickable.originY + playlistFlickable.contentHeight - playlistFlickable.height
                );
            }

            function clampContentY() {
                var minY = minContentY();
                var maxY = maxContentY();
                playlistFlickable.contentY = clampValue(playlistFlickable.contentY, minY, maxY);
            }

            function _slotHeight() { return root.elementRectangleHeight + root.listColumSpacing }

            // 返回 item 在「内容坐标」中的矩形（不含 contentY 偏移）
            function calculateItemRect(index) {
                var itemY = playlistFlickable.topMargin + index * _slotHeight();
                return {
                    y: itemY,
                    top: itemY,
                    center: itemY + root.elementRectangleHeight / 2,
                    bottom: itemY + root.elementRectangleHeight
                };
            }

            // 用 ListView.indexAt() 取真实索引，不手算
            function listLocalPosToIndex(localX, localY) {
                clampContentY();

                // ListView.indexAt() 使用 content 坐标
                var contentX = localX;
                var contentY = localY + playlistFlickable.contentY;

                var idx = playlistFlickable.indexAt(contentX, contentY);

                if (idx < 0 || idx >= root.listModel.count)
                    return -1;

                return idx;
            }

            // 判断 (localX, localY) 是否命中拖拽手柄，返回索引或 -1
            function dragHandleIndexAt(localX, localY) {
                var left = playlistFlickable.timeLeftMargin + 4;
                var right = left + 44;

                if (localX < left || localX > right)
                    return -1;

                return listLocalPosToIndex(localX, localY);
            }

            // 鼠标位置 → 插入到的间隙索引
            // ---- 基于 delegate 真实位置的坐标函数 ----

            // 获取 item 顶部在 container 坐标系中的 Y
            function itemTopYInContainer(index) {
                var item = playlistFlickable.itemAtIndex(index);
                if (!item)
                    return NaN;
                return item.mapToItem(container, 0, 0).y;
            }

            // 获取 item 底部在 container 坐标系中的 Y
            function itemBottomYInContainer(index) {
                var item = playlistFlickable.itemAtIndex(index);
                if (!item)
                    return NaN;
                return item.mapToItem(container, 0, item.height).y;
            }

            // gap 索引 → 提示线中心点的 container 坐标
            function gapCenterYInContainer(gapIndex) {
                var count = root.listModel.count;

                if (count <= 0)
                    return playlistFlickable.y;

                gapIndex = clampValue(gapIndex, 0, count);

                var spacing = playlistFlickable.spacing;

                if (gapIndex === 0) {
                    var firstTop = itemTopYInContainer(0);
                    if (!isNaN(firstTop))
                        return firstTop - spacing / 2;
                }

                if (gapIndex === count) {
                    var lastBottom = itemBottomYInContainer(count - 1);
                    if (!isNaN(lastBottom))
                        return lastBottom + Math.max(spacing / 2, insertLine.height + 4);
                }

                var nextTop = itemTopYInContainer(gapIndex);
                if (!isNaN(nextTop))
                    return nextTop - spacing / 2;

                var prevBottom = itemBottomYInContainer(gapIndex - 1);
                if (!isNaN(prevBottom))
                    return prevBottom + spacing / 2;

                // 兜底：itemAtIndex 拿不到时用旧公式
                var fallbackContentY = playlistFlickable.topMargin + gapIndex * _slotHeight();
                return playlistFlickable.y + fallbackContentY - playlistFlickable.contentY;
            }

            // 鼠标位置 → 插入到的间隙索引（基于真实 delegate 位置）
            function findHoveredGap(mouseY) {
                var count = root.listModel.count;
                if (count <= 0)
                    return 0;

                clampContentY();

                var localY = mouseY - playlistFlickable.y;
                var contentY = localY + playlistFlickable.contentY;

                // 先让 ListView 自己判断鼠标在哪个 delegate 上
                var idx = playlistFlickable.indexAt(playlistFlickable.width / 2, contentY);

                if (idx >= 0 && idx < count) {
                    var item = playlistFlickable.itemAtIndex(idx);
                    if (item) {
                        var topY = item.mapToItem(container, 0, 0).y;
                        var centerY = topY + item.height / 2;
                        return mouseY < centerY ? idx : idx + 1;
                    }
                }

                // 鼠标在 spacing 空隙里时，indexAt 可能返回 -1
                // 用可见 delegate 的边界找最近 gap
                var bestGap = root.dragTargetIndex >= 0 ? root.dragTargetIndex : 0;
                var bestDistance = 999999;

                for (var i = 0; i < count; i++) {
                    var visibleItem = playlistFlickable.itemAtIndex(i);
                    if (!visibleItem)
                        continue;

                    var itemTop = visibleItem.mapToItem(container, 0, 0).y;
                    var itemBottom = itemTop + visibleItem.height;

                    var beforeGapY = itemTop - playlistFlickable.spacing / 2;
                    var afterGapY = itemBottom + playlistFlickable.spacing / 2;

                    var beforeDistance = Math.abs(mouseY - beforeGapY);
                    if (beforeDistance < bestDistance) {
                        bestDistance = beforeDistance;
                        bestGap = i;
                    }

                    var afterDistance = Math.abs(mouseY - afterGapY);
                    if (afterDistance < bestDistance) {
                        bestDistance = afterDistance;
                        bestGap = i + 1;
                    }
                }

                return clampValue(bestGap, 0, count);
            }

            // 内容坐标 → 容器坐标（仅作兜底用）
            function contentYToContainerY(contentYValue) {
                return playlistFlickable.y + contentYValue - playlistFlickable.contentY;
            }

            // 设置浮动标签位置（直接定位，不用动画）
            function setFloatTagByMouseY(mouseY) {
                dragElementFloatTag_ParallelAnimation.stop();
                var minY = playlistFlickable.y;
                var maxY = playlistFlickable.y + playlistFlickable.height - dragElementFloatTag.height;
                dragElementFloatTag.y = clampValue(mouseY - dragElementFloatTag.height / 2, minY, maxY);
            }

            // 自动滚动控制
            function updateAutoScroll(mouseY) {
                if (!root.dragActive) {
                    flickTimer.direction = 0;
                    flickTimer.speed = 0;
                    flickTimer.stop();
                    return;
                }

                // 不要在这里 clampContentY()，否则会把 contentY 提前钳到错误的 maxY

                var minY = minContentY();
                var maxY = maxContentY();

                if (maxY <= minY) {
                    flickTimer.direction = 0;
                    flickTimer.speed = 0;
                    flickTimer.stop();
                    return;
                }

                var relY = mouseY - playlistFlickable.y;
                var edge = 48;
                var minSpeed = 1.5;
                var maxSpeed = 7.0;
                var cy = playlistFlickable.contentY;

                if (relY >= 0 && relY < edge && cy > minY) {
                    var tTop = (edge - relY) / edge;
                    flickTimer.direction = -1;
                    flickTimer.speed = minSpeed + tTop * (maxSpeed - minSpeed);
                    if (!flickTimer.running)
                        flickTimer.start();
                } else if (relY <= playlistFlickable.height &&
                           relY > playlistFlickable.height - edge &&
                           cy < maxY) {
                    var tBottom = (relY - (playlistFlickable.height - edge)) / edge;
                    flickTimer.direction = 1;
                    flickTimer.speed = minSpeed + tBottom * (maxSpeed - minSpeed);
                    if (!flickTimer.running)
                        flickTimer.start();
                } else {
                    flickTimer.direction = 0;
                    flickTimer.speed = 0;
                    flickTimer.stop();
                }
            }

            // 只更新提示线位置，不触发自动滚动
            function updateInsertLineOnly(mouseY) {
                if (!root.dragActive || mouseY < 0)
                    return;

                root.dragTargetIndex = findHoveredGap(mouseY);

                var count = root.listModel.count;
                root.dragTargetIndex = clampValue(root.dragTargetIndex, 0, count);

                var centerY = gapCenterYInContainer(root.dragTargetIndex);

                // clamp 提示线中心点，不是 top
                var minCenterY = playlistFlickable.y + insertLine.height / 2;
                var maxCenterY = playlistFlickable.y + playlistFlickable.height - insertLine.height / 2;
                centerY = clampValue(centerY, minCenterY, maxCenterY);

                insertLine.x = playlistFlickable.x + 10;
                insertLine.y = centerY - insertLine.height / 2;
                insertLine.visible = true;
                insertLine.opacity = 1;
            }

            // 渲染insertLine <<----主接口（可以启动自动滚动）
            function renderInsertLine(mouseY) {
                if (!root.dragActive || mouseY < 0)
                    return;

                dragMouseYInContainer = mouseY;
                setFloatTagByMouseY(mouseY);
                updateAutoScroll(mouseY);
                updateInsertLineOnly(mouseY);
            }

            // 开始拖拽
            function beginDrag(sourceIndex, mouseYInContainer) {
                if (sourceIndex < 0 || sourceIndex >= root.listModel.count)
                    return;

                playlistFlickable.cancelFlick();
                playlistFlickable.interactive = false;
                clampContentY();

                var item = root.listModel.get(sourceIndex);

                dragElementFloatTag.musicTagIconImageSource = item.icon;
                dragElementFloatTag.musicTagName = item.music_name;
                dragElementFloatTag.musicTagArtist = item.music_artist;

                root.dragSourceIndex = sourceIndex;
                root.dragTargetIndex = sourceIndex;
                root.dragActive = true;
                root.dragHasMoved = false;

                dragMouseYInContainer = mouseYInContainer;

                root.show_dragElement_floatTag = true;

                dragElementFloatTag_ParallelAnimation.stop();

                dragElementFloatTag.dragElementFloatTag_OpacityChangeAnimation_fromValue = 0;
                dragElementFloatTag.dragElementFloatTag_OpacityChangeAnimation_toValue = 1;
                dragElementFloatTag_OpacityChangeAnimation.restart();

                renderInsertLine(mouseYInContainer);
            }

            // 结束拖拽
            function endDrag(commitReorder) {
                flickTimer.direction = 0;
                flickTimer.speed = 0;
                flickTimer.stop();

                playlistFlickable.cancelFlick();
                playlistFlickable.interactive = true;
                clampContentY();

                var shouldReorder =
                    commitReorder &&
                    root.dragHasMoved &&
                    root.dragSourceIndex >= 0 &&
                    root.dragTargetIndex >= 0 &&
                    !(root.dragTargetIndex === root.dragSourceIndex ||
                      root.dragTargetIndex === root.dragSourceIndex + 1);

                root.show_dragElement_floatTag = false;
                insertLine.visible = false;

                root.dragActive = false;
                root.dragHasMoved = false;
                dragMouseYInContainer = -1;

                if (shouldReorder)
                    reOrder();

                root.dragSourceIndex = -1;
                root.dragTargetIndex = -1;
            }

            //重排序reOrder <<---主要接口
            // 直接在 ListModel 上原地 move，不清空不重建
            function reOrder() {
                var fromIdx = root.dragSourceIndex;
                var toIdx = root.dragTargetIndex;

                // move() 的 to 是"移动后应到达的索引"
                // 向下拖：item 被移走后，目标位置会前移一位，所以 to = toIdx - 1
                // 向上拖：目标位置不受影响，to = toIdx
                var insertIdx;

                if (fromIdx < toIdx) {
                    // 向下拖：先移除 fromIdx，后面元素前移，所以插入到 toIdx - 1
                    insertIdx = toIdx - 1;
                } else {
                    // 向上拖：直接插入到 toIdx
                    insertIdx = toIdx;
                }

                // move 前记录当前播放/选中项的 filepath，move 后按 filepath 重新定位
                var playingPath = "";
                var selectedPath = "";
                if (playlistFlickable.playingIndex >= 0 && playlistFlickable.playingIndex < root.listModel.count)
                    playingPath = root.listModel.get(playlistFlickable.playingIndex).filepath;
                if (playlistFlickable.itemSelected_index >= 0 && playlistFlickable.itemSelected_index < root.listModel.count)
                    selectedPath = root.listModel.get(playlistFlickable.itemSelected_index).filepath;

                // ListModel.move(from, to, 1) — 原地移动单条
                root.listModel.move(fromIdx, insertIdx, 1);

                // move 后根据 filepath 找到新索引并更新
                var newPlayingIdx = -1;
                var newSelectedIdx = -1;
                for (var k = 0; k < root.listModel.count; k++) {
                    var fp = root.listModel.get(k).filepath;
                    if (fp === playingPath)  newPlayingIdx = k;
                    if (fp === selectedPath) newSelectedIdx = k;
                }
                playlistFlickable.playingIndex = newPlayingIdx;
                playlistFlickable.itemSelected_index = newSelectedIdx;

                // 通知后台新的文件顺序
                var listCount = root.listModel.count;
                var itemsPath = [];
                for (var i = 0; i < listCount; i++) {
                    itemsPath.push(root.listModel.get(i).filepath);
                }
                root.reorderCompleted(itemsPath)

                // 强制触发可视区域所有项的 fadeIn 动画
                playlistFlickable.forceLayout();
                for (var vi = 0; vi < listCount; vi++) {
                    var visibleItem = playlistFlickable.itemAtIndex(vi);
                    if (visibleItem)
                        visibleItem.animateIn();
                }
            }

            //-----------------------滑动列表右下的手动添加导入本地文件"+"-----------------------//
            Rectangle {
                id: addNewFileRectangle
                property int initWidth: 50
                width: addNewFileRectangle.initWidth
                height: addNewFileRectangle.width
                z: 1
                anchors.right: parent.right
                anchors.rightMargin: addNewFileRectangle.initWidth
                anchors.bottom: parent.bottom
                anchors.bottomMargin: addNewFileRectangle.initWidth
                color: "white"
                radius: addNewFileRectangle.initWidth / 2

                visible: root.isMultiSelected ? false : true

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

                Rectangle {
                    width: addNewFileRectangle.initWidth * 0.5
                    height: 4
                    radius: addNewFileRectangle.initWidth / 2
                    color: "gray"
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Rectangle {
                    width: 4
                    height: addNewFileRectangle.initWidth * 0.5
                    radius: addNewFileRectangle.initWidth / 2
                    color: "gray"
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                ParallelAnimation {
                    id: addLocalMusicRectangleParallelAnimation_minisize
                    PropertyAnimation {
                        target: addNewFileRectangle
                        property: "width"
                        from: addNewFileRectangle.initWidth
                        to: addNewFileRectangle.initWidth * 0.9
                        duration: 80
                        easing.type: Easing.InQuad
                    }
                }
                ParallelAnimation {
                    id: addLocalMusicRectangleParallelAnimation_maxsize
                    PropertyAnimation {
                        target: addNewFileRectangle
                        property: "width"
                        from: addNewFileRectangle.width
                        to: addNewFileRectangle.initWidth
                        duration: 80
                        easing.type: Easing.InQuad
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
                    anchors.fill: parent
                    hoverEnabled: true
                    onPressed: {
                        addLocalMusicRectangleParallelAnimation_minisize.start();
                    }
                    onReleased: {
                        addLocalMusicRectangleParallelAnimation_maxsize.start();
                    }
                    onClicked: {
                        addNewFileRectangle_FileDialog.open();
                    }
                }
            }

            // ======== main of playlistFlickable ========
            ListView {
                id: playlistFlickable
                model: root.listModel

                boundsBehavior: Flickable.StopAtBounds
                reuseItems: false

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
                    root.listModel.setProperty(index, "needsAnimateIn", true);
                    var item = playlistFlickable.itemAtIndex(index);
                    if (item) {
                        item.needsAnimateIn = true;
                        item.animateIn();
                    }
                }
                // 对外接口--->触发所有最近添加的元素的动画
                function animateRecentItems(startIndex) {
                    for (var i = startIndex; i < root.listModel.count; i++) {
                        root.listModel.setProperty(i, "needsAnimateIn", true);
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
                    var minY = container.minContentY();
                    var maxY = container.maxContentY();
                    var tolerance = 1.0;
                    playlistFlickable.contentY_atBottom = contentY >= (maxY - tolerance);
                    playlistFlickable.contentY_atTop = contentY <= (minY + tolerance);
                }

                HoverHandler {
                    id: playlistHoverHandler
                    acceptedDevices: PointerDevice.Mouse
                    onHoveredChanged: {
                        root.mouseInPlaylist = playlistHoverHandler.hovered;
                        if (!playlistHoverHandler.hovered) {
                            playlistFlickable.itemHovered_index = -1;
                        }
                    }
                }

                delegate: Rectangle {
                            id: playlistItem

                            height: root.matchesSearch(model.music_name, model.music_artist, model.music_album)
                                    ? root.elementRectangleHeight : 0
                            visible: root.matchesSearch(model.music_name, model.music_artist, model.music_album)
                            width: playlistFlickable.width - 20
                            radius: 10
                            color: root.isMultiSelected ? (model.isSelected === true ? "#8Efef2e8" : "transparent") :
                                            playlistItem.itemIsSelected ? "#8Efef2e8" :
                                                    playlistItem.itemIsHovered && root.mouseInPlaylist ? "#8EBEBEBE" : "transparent"

                            Behavior on color {PropertyAnimation {duration: 500 ;easing.type: Easing.OutCubic}}

                            property int itemIndex: model.index
                            property string itemIcon: model.icon
                            property string itemTitle: model.music_name
                            property string itemArtist: model.music_artist
                            property string itemAlbum: model.music_album
                            property string itemDuration: model.timesize
                            property string itemFilepath: model.filepath
                            property bool needsAnimateIn: model.needsAnimateIn === true

                            property bool itemIsSelected: model.index === playlistFlickable.itemSelected_index //bind
                            property bool itemIsHovered: model.index === playlistFlickable.itemHovered_index //bind
                            property bool itemIsPlaying: model.index === playlistFlickable.playingIndex //bind

                            // --- item opacity Animation ---
                            property bool animationEnabled: true
                            property bool isAnimating: false
                            opacity: playlistItem.needsAnimateIn ? 0 : 1
                            Component.onCompleted: {
                                if (playlistItem.needsAnimateIn)
                                    playlistItem.animateIn();
                            }
                            function animateIn() {
                                if (playlistItem.animationEnabled && !playlistItem.isAnimating) {
                                    playlistItem.isAnimating = true;
                                    fadeInAnimation.start();
                                }
                            }
                            SequentialAnimation {
                                id: fadeInAnimation
                                NumberAnimation {
                                    target: playlistItem
                                    property: "opacity"
                                    from: 0
                                    to: 1
                                    duration: 300
                                    easing.type: Easing.OutCubic
                                }
                                ScriptAction {
                                    script: {
                                        playlistItem.isAnimating = false;
                                        playlistItem.needsAnimateIn = false;
                                    }
                                }
                            }
                            //-----------------------------  Connections -----------------------------//
                            Connections {
                                target: root
                                function onAsk_dir_of_item_index(file_path) {
                                    if (playlistItem.itemFilepath === file_path) {
                                        root.reponse_dir_of_item_index(playlistItem.itemIndex);
                                    }
                                }
                            }

                            //----------------------------------  Ui  ----------------------------------//

                            // --- 多选复选框 ---
                            Oran7ESelectRect {
                                id: checkBox
                                width: 20
                                anchors.left: parent.left
                                anchors.leftMargin: playlistFlickable.indexLeftMargin
                                anchors.verticalCenter: parent.verticalCenter
                                z: 2
                                labelTextEnable: false
                                checkedColor: "cyan"
                                border.color: "cyan"
                                visible: root.isMultiSelected

                                checked: model.isSelected === true
                                onClicked: {
                                    root.listModel.setProperty(model.index, "isSelected", !(model.isSelected === true));
                                }
                            }

                            // 序号/播放图标
                            Rectangle {
                                id: indexRect
                                width: playlistFlickable.indexWidth
                                height: width
                                anchors.left: parent.left
                                anchors.leftMargin: playlistFlickable.indexLeftMargin
                                anchors.verticalCenter: parent.verticalCenter
                                color: "transparent"
                                Label {
                                    id: indexLabel
                                    visible: !root.isMultiSelected && (!playlistItem.itemIsHovered || !root.mouseInPlaylist) && !playlistItem.itemIsSelected
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.leftMargin: 5
                                    text: model.index < 9 ? "0" + (model.index + 1) : (model.index + 1)
                                    font.family: "微软雅黑"
                                    font.bold: true
                                    font.pixelSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize
                                    //Behavior on font.pixelSize{NumberAnimation{duration: Oran7Theme.Primary.durationMid}}
                                    color:Oran7Theme.Oran7MusicPlaylistView["textColorBase-5"]
                                }

                                Image {
                                    id: playIcon
                                    visible: !root.isMultiSelected && playlistItem.itemIsHovered && root.mouseInPlaylist
                                    source: playlistItem.itemIsPlaying ? "qrc:/image/ClearPause.png" : "qrc:/image/ClearPlay.png"
                                    scale: 0.7
                                    anchors.fill: parent
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.leftMargin: 5
                                    z: playlistItem.z + 1

                                    layer.enabled: true
                                    layer.effect: ColorOverlay {
                                        source: playIcon
                                        color: Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
                                    }
                                }
                                Oran7WaveBarChart {
                                    id: waveBarChart
                                    anchors.top: playIcon.top
                                    anchors.topMargin: 0
                                    anchors.horizontalCenter: playIcon.horizontalCenter

                                    visible: !root.isMultiSelected&& !playlistItem.itemIsHovered && playlistItem.itemIsSelected

                                    animationEnabled: playlistItem.itemIsPlaying
                                    waveColor: Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        root.itemClicked(model.index);
                                        //console.log("clicked")
                                    }
                                }
                            }
                            // 专辑封面
                            Oran7RoundedImage{
                                id:coverImage
                                width: 36
                                height: width
                                anchors.left: parent.left
                                anchors.leftMargin: 50
                                anchors.verticalCenter: parent.verticalCenter
                                radius: 7
                                source:playlistItem.itemIcon
                            }

                            // 标题和艺术家
                            Column {
                                anchors.left: coverImage.right
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter

                                Label {
                                    id: titleLabel
                                    text: playlistItem.itemTitle
                                    color: Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
                                    Behavior on color{PropertyAnimation{duration: Oran7Theme.Primary.durationMid}}
                                    font.pixelSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize
                                    //Behavior on font.pixelSize{NumberAnimation{duration: Oran7Theme.Primary.durationMid}}
                                    font.family: "微软雅黑"
                                    width: parent.width - 100
                                }

                                Rectangle {
                                    id: typeRect
                                    width: 44
                                    height: 16
                                    color: "transparent"
                                    border.color: Oran7Theme.Oran7MusicPlaylistView["colorPrimaryBase-8"]
                                    border.width: 0.8
                                    radius: 2

                                    Label {
                                        text: "超清母带"
                                        color: Oran7Theme.Oran7MusicPlaylistView["colorPrimaryBase-5"]
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        font.family: "微软雅黑"
                                        font.bold: true
                                        font.pixelSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize-7 < 10 ? 10 :
                                                                                    Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize-7
                                        //Behavior on font.pixelSize{NumberAnimation{duration: Oran7Theme.Primary.durationMid}}
                                        Label {
                                            id: artistLabel
                                            text: playlistItem.itemArtist
                                            anchors.left: parent.right
                                            anchors.leftMargin: 4
                                            anchors.bottom: parent.bottom
                                            width: playlistFlickable.titleWidth - typeRect.width - coverImage.width - 7
                                            clip: true
                                            font.family: "微软雅黑"
                                            font.bold: true
                                            font.italic: true
                                            font.pixelSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize - 5 < 12 ? 12 :
                                                                                    Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize - 5
                                            //Behavior on font.pixelSize{NumberAnimation{duration: Oran7Theme.Primary.durationMid}}
                                            color: Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
                                            Behavior on color{PropertyAnimation{duration: Oran7Theme.Primary.durationMid}}
                                        }
                                    }
                                }
                            }

                            // 专辑
                            Label {
                                id: albumLabel
                                anchors.left: parent.left
                                anchors.leftMargin: playlistFlickable.albumLeftMargin
                                anchors.verticalCenter: parent.verticalCenter
                                width: 200
                                text: itemAlbum
                                font.family: "微软雅黑"
                                font.bold: true
                                font.italic: true
                                font.pixelSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize
                                //Behavior on font.pixelSize{NumberAnimation{duration: Oran7Theme.Primary.durationMid}}
                                color: Oran7Theme.Oran7MusicPlaylistView["textColorBase-4"]
                                Behavior on color{PropertyAnimation{duration: Oran7Theme.Primary.durationMid}}
                            }

                            // 时长
                            Label {
                                id: durationLabel
                                anchors.left: parent.left
                                anchors.leftMargin: playlistFlickable.timeLeftMargin + 4
                                anchors.verticalCenter: parent.verticalCenter
                                width: 50
                                text: formatDuration(itemDuration)
                                font.family: "微软雅黑"
                                font.bold: true
                                font.pixelSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize
                                //Behavior on font.pixelSize{NumberAnimation{duration: Oran7Theme.Primary.durationMid}}
                                color: Oran7Theme.Oran7MusicPlaylistView["textColorBase-7"]
                                Behavior on color{PropertyAnimation{duration: Oran7Theme.Primary.durationMid}}
                                visible: !root.isMultiSelected
                            }

                            // 拖拽图标（仅显示，不处理事件）
                            Image {
                                id: dragIcon
                                anchors.left: parent.left
                                anchors.leftMargin: playlistFlickable.timeLeftMargin + 4
                                anchors.verticalCenter: parent.verticalCenter
                                sourceSize.width: 40
                                sourceSize.height: 40
                                source: "qrc:/image/drag.png"
                                visible: root.isMultiSelected && root.allowDragReorder

                                property bool item_isDraging: root.dragActive && root.dragSourceIndex === playlistItem.itemIndex

                                layer.enabled: true
                                layer.effect: ColorOverlay {
                                    source: dragIcon
                                    color: dragIcon.item_isDraging ? Oran7Theme.Oran7MusicPlaylistView["textColorBase-4"] :
                                                                     Oran7Theme.Oran7MusicPlaylistView["textColorBase-6"]
                                }
                            }
                            // 鼠标交互
                            MouseArea {
                                id: mouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                enabled: !root.isMultiSelected
                                propagateComposedEvents: true

                                // onClicked: {
                                //     if (!root.isMultiSelected) {
                                //         root.itemClicked(model.index);
                                //     }
                                // }

                                // onPositionChanged: (mouse) =>{
                                //     var posInParent = mapToItem(root, mouse.x, mouse.y)

                                //     // console.log("父控件坐标:", posInParent.x, posInParent.y)
                                //     // console.log("全局坐标:", mapToGlobal(mouse.x, mouse.y))
                                //     // //相对坐标
                                //     // var relativeX = mouse.x/* + dragImage.x*/
                                //     // var relativeY = mouse.y/* + dragImage.y*/
                                //     // console.log("相对坐标:", relativeX, relativeY)
                                // }

                                onDoubleClicked: {
                                    if (!root.isMultiSelected) {
                                        root.itemDoubleClicked(model.index);
                                    }
                                }

                                onEntered: {
                                    if (!root.isMultiSelected) {
                                        playlistFlickable.itemHovered_index = playlistItem.itemIndex;
                                    }
                                }
                            }

                            function formatDuration(seconds) {
                                var total = parseInt(seconds);
                                var minutes = Math.floor(total / 60);
                                var secs = total % 60;
                                return Math.floor(minutes / 10) + (minutes % 10) + ":" + Math.floor(secs / 10) + (secs % 10);
                            }
                        }
            }

            // ===== 统一拖拽控制器（在 ListView 外部，不会因 delegate 回收而丢失） =====
            MouseArea {
                id: dragController

                anchors.fill: parent
                z: playlistFlickable.z + 20

                enabled: root.isMultiSelected && root.allowDragReorder && !root.searchActive
                hoverEnabled: false
                preventStealing: true
                acceptedButtons: Qt.LeftButton

                property bool pressedOnDragHandle: false

                onPressed: mouse => {
                    pressedOnDragHandle = false;

                    // 坐标映射到 ListView 内部再判断命中
                    var posInList = mapToItem(playlistFlickable, mouse.x, mouse.y);
                    var sourceIndex = container.dragHandleIndexAt(posInList.x, posInList.y);
                    if (sourceIndex < 0) {
                        mouse.accepted = false;
                        return;
                    }

                    mouse.accepted = true;
                    pressedOnDragHandle = true;

                    var posInContainer = mapToItem(container, mouse.x, mouse.y);
                    container.beginDrag(sourceIndex, posInContainer.y);
                }

                onPositionChanged: mouse => {
                    if (!pressedOnDragHandle || !root.dragActive)
                        return;

                    var posInContainer = mapToItem(container, mouse.x, mouse.y);

                    root.dragHasMoved = true;
                    container.renderInsertLine(posInContainer.y);
                }

                onReleased: {
                    if (!pressedOnDragHandle)
                        return;

                    pressedOnDragHandle = false;
                    container.endDrag(true);
                }

                onCanceled: {
                    pressedOnDragHandle = false;
                    container.endDrag(false);
                }
            }
        }
    }
}
