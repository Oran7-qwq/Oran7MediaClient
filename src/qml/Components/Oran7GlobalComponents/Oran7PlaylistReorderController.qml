import QtQuick 2.15
import QtQuick.Controls
import QtQuick.Dialogs
import Qt5Compat.GraphicalEffects
import "../../Basic"
import Client 1.0

import Oran7UI.Impl

Item {
    id: reorderController
    required property var view
    required property var listView
    required property var divider
    property alias cursorShape: dragController.cursorShape
    // ===== 拖拽提示线 =====  // 插入位置提示线组件 , 以every item 的Top为标准-->
    Rectangle {
        id: insertLine
        visible: false
        z: listView.z + 1
        y: divider.y
        height: 4
        width: listView.width - 40
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
        x: view.x + 100
        y: 0
        z: listView.z + 1
        clip: true
        width: 300
        height: 48
        borderRadius: 7
        themeColor: "#04FFFFFF"
        blurSource: BasicConfig.mainWindow ? BasicConfig.mainWindow.__Oran7WindowBackGround__ : null
        opacity: 1
        visible: view.show_dragElement_floatTag
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
        property string musicTagName: "Oran7QwQ"
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
            font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
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
                text: qsTranslate("Oran7Ui", "超清母带")
                color: Oran7Theme.Oran7MusicPlaylistView["colorPrimaryBase-5"]
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
                font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
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
            font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
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
            if (!view.dragActive || direction === 0) {
                direction = 0;
                speed = 0;
                stop();
                return;
            }

            var minY = reorderController.minContentY();
            var maxY = reorderController.maxContentY();

            var cy = reorderController.clampValue(listView.contentY, minY, maxY);
            if (listView.contentY !== cy)
                listView.contentY = cy;

            if ((direction < 0 && cy <= minY) ||
                (direction > 0 && cy >= maxY)) {
                direction = 0;
                speed = 0;
                stop();
                reorderController.updateInsertLineOnly(reorderController.dragMouseYInContainer);
                return;
            }

            var newY = reorderController.clampValue(cy + direction * speed, minY, maxY);

            if (Math.abs(newY - cy) < 0.01) {
                direction = 0;
                speed = 0;
                stop();
                return;
            }

            listView.contentY = newY;
            reorderController.updateInsertLineOnly(reorderController.dragMouseYInContainer);
        }
    }
    //  ----------  drag of algorithm function ----------

    property real dragMouseYInContainer: -1

    // ---- 基础工具函数 ----
    function clampValue(v, minValue, maxValue) {
        return Math.max(minValue, Math.min(v, maxValue));
    }

    function minContentY() {
        return listView.originY;
    }

    function maxContentY() {
        return Math.max(
            minContentY(),
            listView.originY + listView.contentHeight - listView.height
        );
    }

    function clampContentY() {
        var minY = minContentY();
        var maxY = maxContentY();
        listView.contentY = clampValue(listView.contentY, minY, maxY);
    }

    function _slotHeight() { return view.elementRectangleHeight + view.listColumSpacing }

    // 返回 item 在「内容坐标」中的矩形（不含 contentY 偏移）
    function calculateItemRect(index) {
        var itemY = listView.topMargin + index * _slotHeight();
        return {
            y: itemY,
            top: itemY,
            center: itemY + view.elementRectangleHeight / 2,
            bottom: itemY + view.elementRectangleHeight
        };
    }

    // 用 ListView.indexAt() 取真实索引，不手算
    function listLocalPosToIndex(localX, localY) {
        clampContentY();

        // ListView.indexAt() 使用 content 坐标
        var contentX = localX;
        var contentY = localY + listView.contentY;

        var idx = listView.indexAt(contentX, contentY);

        if (idx < 0 || idx >= view.listModel.count)
            return -1;

        return idx;
    }

    // 判断 (localX, localY) 是否命中拖拽手柄，返回索引或 -1
    function dragHandleIndexAt(localX, localY) {
        if (localX < 0 || localX >= listView.width || localY < 0 || localY >= listView.height)
            return -1;

        var index = listLocalPosToIndex(localX, localY);
        if (index < 0)
            return -1;
        var item = listView.itemAtIndex(index);
        var handle = item ? item.dragHandleItem : null;
        if (!handle || !handle.visible)
            return -1;
        // 使用真实手柄坐标；滚动、缩放窗口和更换行布局后仍与图标对齐。
        var point = handle.mapFromItem(listView, localX, localY);
        var padding = 6;
        return point.x >= -padding && point.x < handle.width + padding
                && point.y >= -padding && point.y < handle.height + padding ? index : -1;
    }

    // 鼠标位置 → 插入到的间隙索引
    // ---- 基于 delegate 真实位置的坐标函数 ----

    // 获取 item 顶部在 reorderController 坐标系中的 Y
    function itemTopYInContainer(index) {
        var item = listView.itemAtIndex(index);
        if (!item)
            return NaN;
        return item.mapToItem(reorderController, 0, 0).y;
    }

    // 获取 item 底部在 reorderController 坐标系中的 Y
    function itemBottomYInContainer(index) {
        var item = listView.itemAtIndex(index);
        if (!item)
            return NaN;
        return item.mapToItem(reorderController, 0, item.height).y;
    }

    // gap 索引 → 提示线中心点的 reorderController 坐标
    function gapCenterYInContainer(gapIndex) {
        var count = view.listModel.count;

        if (count <= 0)
            return listView.y;

        gapIndex = clampValue(gapIndex, 0, count);

        var spacing = listView.spacing;

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
        var fallbackContentY = listView.topMargin + gapIndex * _slotHeight();
        return listView.y + fallbackContentY - listView.contentY;
    }

    // 鼠标位置 → 插入到的间隙索引（基于真实 delegate 位置）
    function findHoveredGap(mouseY) {
        var count = view.listModel.count;
        if (count <= 0)
            return 0;

        clampContentY();

        var localY = mouseY - listView.y;
        var contentY = localY + listView.contentY;

        // 先让 ListView 自己判断鼠标在哪个 delegate 上
        var idx = listView.indexAt(listView.width / 2, contentY);

        if (idx >= 0 && idx < count) {
            var item = listView.itemAtIndex(idx);
            if (item) {
                var topY = item.mapToItem(reorderController, 0, 0).y;
                var centerY = topY + item.height / 2;
                return mouseY < centerY ? idx : idx + 1;
            }
        }

        // 鼠标在 spacing 空隙里时，indexAt 可能返回 -1
        // 用可见 delegate 的边界找最近 gap
        var bestGap = view.dragTargetIndex >= 0 ? view.dragTargetIndex : 0;
        var bestDistance = 999999;

        for (var i = 0; i < count; i++) {
            var visibleItem = listView.itemAtIndex(i);
            if (!visibleItem)
                continue;

            var itemTop = visibleItem.mapToItem(reorderController, 0, 0).y;
            var itemBottom = itemTop + visibleItem.height;

            var beforeGapY = itemTop - listView.spacing / 2;
            var afterGapY = itemBottom + listView.spacing / 2;

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
        return listView.y + contentYValue - listView.contentY;
    }

    // 设置浮动标签位置（直接定位，不用动画）
    function setFloatTagByMouseY(mouseY) {
        dragElementFloatTag_ParallelAnimation.stop();
        var minY = listView.y;
        var maxY = listView.y + listView.height - dragElementFloatTag.height;
        dragElementFloatTag.y = clampValue(mouseY - dragElementFloatTag.height / 2, minY, maxY);
    }

    // 自动滚动控制
    function updateAutoScroll(mouseY) {
        if (!view.dragActive) {
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

        var relY = mouseY - listView.y;
        var edge = 48;
        var minSpeed = 1.5;
        var maxSpeed = 7.0;
        var cy = listView.contentY;

        if (relY >= 0 && relY < edge && cy > minY) {
            var tTop = (edge - relY) / edge;
            flickTimer.direction = -1;
            flickTimer.speed = minSpeed + tTop * (maxSpeed - minSpeed);
            if (!flickTimer.running)
                flickTimer.start();
        } else if (relY <= listView.height &&
                   relY > listView.height - edge &&
                   cy < maxY) {
            var tBottom = (relY - (listView.height - edge)) / edge;
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
        if (!view.dragActive || mouseY < 0)
            return;

        view.dragTargetIndex = findHoveredGap(mouseY);

        var count = view.listModel.count;
        view.dragTargetIndex = clampValue(view.dragTargetIndex, 0, count);

        var centerY = gapCenterYInContainer(view.dragTargetIndex);

        // clamp 提示线中心点，不是 top
        var minCenterY = listView.y + insertLine.height / 2;
        var maxCenterY = listView.y + listView.height - insertLine.height / 2;
        centerY = clampValue(centerY, minCenterY, maxCenterY);

        insertLine.x = listView.x + 10;
        insertLine.y = centerY - insertLine.height / 2;
        insertLine.visible = true;
        insertLine.opacity = 1;
    }

    // 渲染insertLine <<----主接口（可以启动自动滚动）
    function renderInsertLine(mouseY) {
        if (!view.dragActive || mouseY < 0)
            return;

        dragMouseYInContainer = mouseY;
        setFloatTagByMouseY(mouseY);
        updateAutoScroll(mouseY);
        updateInsertLineOnly(mouseY);
    }

    // 开始拖拽
    function beginDrag(sourceIndex, mouseYInContainer) {
        if (sourceIndex < 0 || sourceIndex >= view.listModel.count)
            return;

        listView.cancelFlick();
        listView.interactive = false;
        clampContentY();

        var item = view.listModel.get(sourceIndex);

        dragElementFloatTag.musicTagIconImageSource = item.icon;
        dragElementFloatTag.musicTagName = item.music_name;
        dragElementFloatTag.musicTagArtist = item.music_artist;

        view.dragSourceIndex = sourceIndex;
        view.dragTargetIndex = sourceIndex;
        view.dragActive = true;
        view.dragHasMoved = false;

        dragMouseYInContainer = mouseYInContainer;

        view.show_dragElement_floatTag = true;

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

        listView.cancelFlick();
        listView.interactive = true;
        clampContentY();

        var shouldReorder =
            commitReorder &&
            view.dragHasMoved &&
            view.dragSourceIndex >= 0 &&
            view.dragTargetIndex >= 0 &&
            !(view.dragTargetIndex === view.dragSourceIndex ||
              view.dragTargetIndex === view.dragSourceIndex + 1);

        view.show_dragElement_floatTag = false;
        insertLine.visible = false;

        view.dragActive = false;
        view.dragHasMoved = false;
        dragMouseYInContainer = -1;

        if (shouldReorder)
            reOrder();

        view.dragSourceIndex = -1;
        view.dragTargetIndex = -1;
    }

    //重排序reOrder <<---主要接口
    // 直接在 ListModel 上原地 move，不清空不重建
    function reOrder() {
        var fromIdx = view.dragSourceIndex;
        var toIdx = view.dragTargetIndex;

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

        // move 前记录当前播放/选中项的定位键：songId（网易云/云端歌稳定标识）优先，filepath 兜底。
        // 注意："网易云中我喜欢的音乐"等云端列表所有项 filepath 均为空，必须按 songId 定位
        var playingId = -1;
        var selectedId = -1;
        var playingPath = "";
        var selectedPath = "";
        if (listView.playingIndex >= 0 && listView.playingIndex < view.listModel.count) {
            var pItem = view.listModel.get(listView.playingIndex);
            playingId = (pItem.songId || 0) > 0 ? pItem.songId : -1;
            playingPath = pItem.filepath;
        }
        if (listView.itemSelected_index >= 0 && listView.itemSelected_index < view.listModel.count) {
            var sItem = view.listModel.get(listView.itemSelected_index);
            selectedId = (sItem.songId || 0) > 0 ? sItem.songId : -1;
            selectedPath = sItem.filepath;
        }

        // C++ 模型移动单条，保持持久索引与选择状态。
        view.listModel.moveTrack(fromIdx, insertIdx);

        // move 后重新定位：优先按 songId，其次 filepath（本地歌 filepath 非空）
        var newPlayingIdx = -1;
        var newSelectedIdx = -1;
        for (var k = 0; k < view.listModel.count; k++) {
            var item = view.listModel.get(k);
            if (playingId > 0) {
                if (item.songId === playingId) newPlayingIdx = k;
            } else if (playingPath && playingPath.length > 0 && item.filepath === playingPath) {
                newPlayingIdx = k;
            }
            if (selectedId > 0) {
                if (item.songId === selectedId) newSelectedIdx = k;
            } else if (selectedPath && selectedPath.length > 0 && item.filepath === selectedPath) {
                newSelectedIdx = k;
            }
        }
        listView.playingIndex = newPlayingIdx;
        listView.itemSelected_index = newSelectedIdx;

        // 通知后台新的文件顺序
        var listCount = view.listModel.count;
        var itemsPath = [];
        for (var i = 0; i < listCount; i++) {
            itemsPath.push(view.listModel.get(i).filepath);
        }
        view.reorderCompleted(itemsPath)

        // 强制触发可视区域所有项的 fadeIn 动画
        listView.forceLayout();
        for (var vi = 0; vi < listCount; vi++) {
            var visibleItem = listView.itemAtIndex(vi);
            if (visibleItem)
                visibleItem.animateIn();
        }
    }


    MouseArea {
        id: dragController

        anchors.fill: parent
        z: listView.z + 20

        enabled: view.isMultiSelected && view.allowDragReorder && !view.searchActive
        hoverEnabled: false
        preventStealing: true
        acceptedButtons: Qt.LeftButton

        property bool pressedOnDragHandle: false

        onPressed: mouse => {
            pressedOnDragHandle = false;

            // 坐标映射到 ListView 内部再判断命中
            var posInList = mapToItem(listView, mouse.x, mouse.y);
            var sourceIndex = reorderController.dragHandleIndexAt(posInList.x, posInList.y);
            if (sourceIndex < 0) {
                mouse.accepted = false;
                return;
            }

            mouse.accepted = true;
            pressedOnDragHandle = true;

            var posInContainer = mapToItem(reorderController, mouse.x, mouse.y);
            reorderController.beginDrag(sourceIndex, posInContainer.y);
        }

        onPositionChanged: mouse => {
            if (!pressedOnDragHandle || !view.dragActive)
                return;

            var posInContainer = mapToItem(reorderController, mouse.x, mouse.y);

            view.dragHasMoved = true;
            reorderController.renderInsertLine(posInContainer.y);
        }

        onReleased: {
            if (!pressedOnDragHandle)
                return;

            pressedOnDragHandle = false;
            reorderController.endDrag(true);
        }

        onCanceled: {
            pressedOnDragHandle = false;
            reorderController.endDrag(false);
        }
    }
}
