import QtQuick 2.15
import QtQuick.Shapes as Shapes
import QtQuick.Controls
import QtQuick.Dialogs
import Qt5Compat.GraphicalEffects
import "../../Basic"
import Client 1.0

import Oran7UI.Impl

Rectangle {
    required property var view
    required property var listView
    required property var reorderController
    required property var model
    required property int index

    id: playlistItem
    objectName: "playlistRow"

    height: view.matchesSearch(model.music_name, model.music_artist, model.music_album)
            ? view.elementRectangleHeight : 0
    visible: view.matchesSearch(model.music_name, model.music_artist, model.music_album)
    width: listView.width - 20
    color: "transparent"
    readonly property bool highlighted: view.isMultiSelected ? model.isSelected === true : itemIsSelected
    readonly property bool hovered: itemIsHovered && view.mouseInPlaylist && !view.isMultiSelected
    // 轮廓和图标共用同一悬停进度；中途移出直接从当前值回退。
    property real hoverProgress: 0
    onHoveredChanged: updateHover()
    function updateHover() {
        hoverAnimation.stop()
        if (listView.hoverSuspended) hoverProgress = hovered ? 1 : 0
        else {
            hoverAnimation.to = hovered ? 1 : 0
            hoverAnimation.duration = hovered ? 180 : 260
            hoverAnimation.restart()
        }
    }
    NumberAnimation {
        id: hoverAnimation
        target: playlistItem
        property: "hoverProgress"
        to: 0
        duration: 180
        easing.type: Easing.OutCubic
    }
    Connections {
        target: listView
        function onHoverSuspendedChanged() {
            playlistItem.updateHover()
            playlistItem.updateHighlight()
        }
    }
    // 底层使用 OutQuart 先铺开，顶层使用 InOutQuart 延后跟进，中段速度变化更明显。
    // 顶层首尾速度归零，增强加速感的同时保留平滑落位；指数可在 3～4 间微调。
    // 遮罩盖回强调色，仅留下窄斜边；两层错位形成先展开再收窄的拖尾，退场反向。
    readonly property bool highlightOpen: highlighted || hovered
    property real highlightProgress: 0
    readonly property int highlightEasingPower: 4
    readonly property real highlightBaseProgress: 1 - Math.pow(1 - highlightProgress, highlightEasingPower)
    readonly property real highlightAccentTime: Math.max(0, Math.min(1, (highlightProgress - 0.10) / 0.90))
    readonly property real highlightAccentProgress: highlightAccentTime < 0.5
        ? Math.pow(2 * highlightAccentTime, highlightEasingPower) / 2
        : 1 - Math.pow(2 * (1 - highlightAccentTime), highlightEasingPower) / 2
    readonly property real highlightEdgeWidth: 4
    onHighlightOpenChanged: updateHighlight()
    function updateHighlight() {
        highlightAnimation.stop()
        if (listView.hoverSuspended) {
            // 滚动不逐行播放进退场；实际命中行和选中行保持完整高亮。
            highlightProgress = highlightOpen ? 1 : 0
            return
        }
        highlightAnimation.to = highlightOpen ? 1 : 0
        // 中途反向只运行剩余路程，避免反复悬停时每次都用完整时长慢慢挪动。
        highlightAnimation.duration = Math.max(1, Math.round((highlightOpen ? 420 : 320)
            * Math.abs(highlightAnimation.to - highlightProgress)))
        highlightAnimation.restart()
    }
    NumberAnimation {
        id: highlightAnimation
        target: playlistItem
        property: "highlightProgress"
        to: 0
        duration: 420
        easing.type: Easing.Linear
    }
    function blendColor(from, to, amount) {
        return Qt.rgba(from.r + (to.r - from.r) * amount,
                       from.g + (to.g - from.g) * amount,
                       from.b + (to.b - from.b) * amount,
                       from.a + (to.a - from.a) * amount)
    }
    // 静态底图不再随 hover 每帧更新渐变、路径和离屏纹理。
    readonly property color surfaceColor: highlighted ? view.playlistSurfaceSelected : view.playlistSurface
    readonly property real actionsWidth: (view.showLoveButton ? 36 : 0) + (view.showIntegrateButton ? 36 : 0)
    readonly property real durationX: width - 72
    readonly property real actionsX: durationX - actionsWidth - 12
    readonly property real utilityPanelX: Math.max(118, actionsX - 24)
    // 背景明暗细节来自原版对比色，修改文字颜色不会顺带给卡片染色。
    readonly property color backgroundInk: view.playlistBackgroundInk
    // 仅通过 ColorOverlay 改变图标颜色；明亮色带上加深、暗色带上提亮。
    readonly property bool actionAccentIsLight: view.playlistAccent.r * 0.2126
        + view.playlistAccent.g * 0.7152 + view.playlistAccent.b * 0.0722 > 0.58
    readonly property color actionHoverColor: blendColor(view.playlistAccent,
        actionAccentIsLight ? Qt.rgba(0, 0, 0, 1) : Qt.rgba(1, 1, 1, 1),
        actionAccentIsLight ? 0.68 : 0.88)
    readonly property bool showAlbum: width >= 660
    readonly property real albumX: Math.max(290, width * 0.53)
    readonly property real titleEnd: showAlbum ? albumX - 24 : actionsX - 16
    readonly property real titleGuideEnd: Math.max(130, Math.min(titleEnd - 10, width * 0.48))
    readonly property string rowNumber: (itemIndex + 1 < 10 ? "0" : "") + (itemIndex + 1)
    // 控制器借用真实手柄几何，避免布局调整后命中区域仍停留在旧列坐标。
    readonly property alias dragHandleItem: dragIcon

    Item {
        id: staticBackdrop
        objectName: "playlistStaticBackdrop"
        anchors.fill: parent
        layer.enabled: true
        layer.smooth: true
        // 背景只绘制平面几何，右上角保持完整直角，不为每行建立模糊或阴影纹理。
        Shapes.Shape {
            anchors.fill: parent
            Shapes.ShapePath {
                strokeWidth: 0
                strokeColor: "transparent"
                // 同一自定义色在封面侧、正文区和操作区形成柔和层次，避免整块平涂。
                fillGradient: Shapes.LinearGradient {
                    x1: 0; y1: 0
                    x2: playlistItem.width; y2: playlistItem.height
                    GradientStop {
                        position: 0
                        color: Qt.tint(playlistItem.surfaceColor, Qt.alpha(playlistItem.backgroundInk, 0.06))
                    }
                    GradientStop {
                        position: 0.40
                        color: playlistItem.surfaceColor
                    }
                    GradientStop {
                        position: 0.76
                        color: Qt.tint(playlistItem.surfaceColor, Qt.alpha(playlistItem.backgroundInk, 0.04))
                    }
                    GradientStop {
                        position: 1
                        color: Qt.tint(playlistItem.surfaceColor, Qt.alpha(playlistItem.backgroundInk, 0.09))
                    }
                }
                startX: 0; startY: 0
                PathLine { x: playlistItem.width; y: 0 }
                PathLine { x: playlistItem.width; y: playlistItem.height }
                PathLine { x: 0; y: playlistItem.height }
                PathLine { x: 0; y: 0 }
            }
            // 右下分段轮廓与预览一致；仅作装饰，不改变内容和点击区域。
            Shapes.ShapePath {
                fillColor: "transparent"
                strokeColor: view.playlistDetail
                strokeWidth: 1
                startX: playlistItem.width - 170; startY: playlistItem.height - 1
                PathLine { x: playlistItem.width - 162; y: playlistItem.height - 7 }
                PathLine { x: playlistItem.width - 16; y: playlistItem.height - 7 }
                PathLine { x: playlistItem.width - 10; y: playlistItem.height - 1 }
            }
        }
        Shapes.Shape {
            // 封面底座与文字区错开，窄行高下也能看清结构。
            width: 102; height: parent.height
            Shapes.ShapePath {
                strokeColor: "transparent"
                fillColor: Qt.alpha(playlistItem.backgroundInk, 0.10)
                startX: 0; startY: 0
                PathLine { x: 102; y: 0 }
                PathLine { x: 90; y: playlistItem.height }
                PathLine { x: 0; y: playlistItem.height }
                PathLine { x: 0; y: 0 }
            }
        }
        Shapes.Shape {
            anchors.fill: parent
            // 操作区以反向斜切收边，中央正文区保持干净。
            Shapes.ShapePath {
                strokeColor: "transparent"
                fillColor: Qt.alpha(playlistItem.backgroundInk, 0.065)
                startX: playlistItem.utilityPanelX + 15; startY: 0
                PathLine { x: playlistItem.width; y: 0 }
                PathLine { x: playlistItem.width; y: playlistItem.height }
                PathLine { x: playlistItem.utilityPanelX - 3; y: playlistItem.height }
                PathLine { x: playlistItem.utilityPanelX + 15; y: 0 }
            }
            // 两条窄斜片形成结构过渡，避免大面积实心色块。
            Shapes.ShapePath {
                strokeColor: "transparent"
                fillColor: Qt.alpha(playlistItem.backgroundInk, 0.045)
                startX: playlistItem.utilityPanelX - 9; startY: 0
                PathLine { x: playlistItem.utilityPanelX - 3; y: 0 }
                PathLine { x: playlistItem.utilityPanelX - 21; y: playlistItem.height }
                PathLine { x: playlistItem.utilityPanelX - 27; y: playlistItem.height }
                PathLine { x: playlistItem.utilityPanelX - 9; y: 0 }
            }
        }
        Shapes.Shape {
            anchors.fill: parent
            // 断续轮廓与台阶线只沿上下边缘走线，正文不铺网格。
            Shapes.ShapePath {
                fillColor: "transparent"
                strokeColor: Qt.alpha(playlistItem.backgroundInk, 0.20)
                strokeWidth: 1
                startX: 110; startY: 2
                PathLine { x: 130; y: 2 }
                PathLine { x: 135; y: 5 }
                PathLine { x: playlistItem.titleGuideEnd - 18; y: 5 }
            }
            Shapes.ShapePath {
                fillColor: "transparent"
                strokeColor: Qt.alpha(playlistItem.backgroundInk, 0.14)
                strokeWidth: 1
                startX: 109; startY: playlistItem.height - 2
                PathLine { x: playlistItem.titleGuideEnd - 27; y: playlistItem.height - 2 }
                PathLine { x: playlistItem.titleGuideEnd - 21; y: playlistItem.height - 6 }
                PathLine { x: playlistItem.titleGuideEnd; y: playlistItem.height - 6 }
            }
            // 操作区顶部的折角色片，延续终末地式的分层工业面板。
            Shapes.ShapePath {
                strokeColor: "transparent"
                fillColor: Qt.alpha(view.playlistAccent, 0.15)
                startX: playlistItem.utilityPanelX + 17; startY: 1
                PathLine { x: playlistItem.utilityPanelX + 42; y: 1 }
                PathLine { x: playlistItem.utilityPanelX + 29; y: 12 }
                PathLine { x: playlistItem.utilityPanelX + 13; y: 12 }
                PathLine { x: playlistItem.utilityPanelX + 17; y: 1 }
            }
            // 右侧淡色折面留在时长后方，强化轮廓而不增加信息密度。
            Shapes.ShapePath {
                strokeColor: "transparent"
                fillColor: Qt.alpha(playlistItem.backgroundInk, 0.04)
                startX: playlistItem.width - 78; startY: 1
                PathLine { x: playlistItem.width - 60; y: 1 }
                PathLine { x: playlistItem.width - 36; y: playlistItem.height - 9 }
                PathLine { x: playlistItem.width - 54; y: playlistItem.height - 9 }
                PathLine { x: playlistItem.width - 78; y: 1 }
            }
        }
        Row {
            // 索引底边的装饰码条，不代表音质、音量或播放进度。
            x: 10; y: parent.height - 9
            height: 5; spacing: 3
            Repeater {
                model: 5
                Rectangle {
                    required property int index
                    width: index === 1 ? 3 : 1
                    height: index % 2 === 0 ? 5 : 3
                    anchors.bottom: parent.bottom
                    color: Qt.alpha(playlistItem.backgroundInk, 0.26)
                }
            }
        }
        Item {
            // 小块点阵置于操作区上边缘，避开按钮、时长和歌曲文字。
            x: playlistItem.width - 104; y: 4
            width: 19; height: 7
            visible: playlistItem.width >= 500
            Repeater {
                model: 12
                Rectangle {
                    required property int index
                    x: (index % 4) * 5; y: Math.floor(index / 4) * 3
                    width: 1; height: 1
                    color: Qt.alpha(playlistItem.backgroundInk, 0.32)
                }
            }
        }
        Row {
            // 刻线只放在操作区底边，不进入歌名、作者和按钮的阅读区域。
            x: playlistItem.utilityPanelX + 22
            y: parent.height - 6
            height: 3
            spacing: 4
            Repeater {
                model: 8
                Rectangle {
                    required property int index
                    width: index % 4 === 0 ? 6 : 2
                    height: index % 4 === 0 ? 3 : 1
                    anchors.bottom: parent.bottom
                    color: Qt.alpha(view.playlistAccent, 0.65)
                }
            }
        }
        // 新增强调色层沿边缘铺开，原有灰阶分区和装饰全部保留。
        Shapes.Shape {
            anchors.fill: parent
            // 折角旁补充一组实心短斜片，避开下方的操作按钮。
            Shapes.ShapePath {
                strokeColor: "transparent"
                fillColor: Qt.alpha(view.playlistAccent, 0.48)
                startX: playlistItem.utilityPanelX + 51; startY: 3
                PathLine { x: playlistItem.utilityPanelX + 55; y: 3 }
                PathLine { x: playlistItem.utilityPanelX + 51; y: 8 }
                PathLine { x: playlistItem.utilityPanelX + 47; y: 8 }
                PathLine { x: playlistItem.utilityPanelX + 51; y: 3 }
            }
            Shapes.ShapePath {
                strokeColor: "transparent"
                fillColor: Qt.alpha(view.playlistAccent, 0.30)
                startX: playlistItem.utilityPanelX + 59; startY: 3
                PathLine { x: playlistItem.utilityPanelX + 63; y: 3 }
                PathLine { x: playlistItem.utilityPanelX + 59; y: 8 }
                PathLine { x: playlistItem.utilityPanelX + 55; y: 8 }
                PathLine { x: playlistItem.utilityPanelX + 59; y: 3 }
            }
        }
        Row {
            x: 10; y: 3; spacing: 4
            Rectangle { width: 12; height: 2; color: Qt.alpha(view.playlistAccent, 0.65) }
            Rectangle { width: 4; height: 2; color: Qt.alpha(view.playlistAccent, 0.35) }
        }
        Rectangle {
            x: 112; y: parent.height - 4
            width: Math.max(0, playlistItem.titleGuideEnd - 148); height: 1
            color: Qt.alpha(view.playlistAccent, 0.38)
        }
        Rectangle {
            x: playlistItem.titleGuideEnd - 30; y: parent.height - 4
            width: 6; height: 1
            color: Qt.alpha(view.playlistAccent, 0.60)
        }
        Rectangle {
            x: playlistItem.titleGuideEnd - 12; y: parent.height - 9
            width: 3; height: 3; rotation: 45
            color: Qt.alpha(view.playlistAccent, 0.50)
        }
        Rectangle {
            x: parent.width - 5; y: 16
            width: 2; height: Math.max(0, parent.height - 34)
            color: Qt.alpha(view.playlistAccent, 0.35)
        }
        Rectangle {
            x: parent.width - 13; y: parent.height - 5
            width: 8; height: 2
            color: Qt.alpha(view.playlistAccent, 0.55)
        }
    }
    Item {
        id: highlightViewport
        objectName: "playlistHighlightViewport"
        x: playlistItem.utilityPanelX - 27
        width: Math.max(0, playlistItem.width - x)
        height: parent.height
        clip: true
        visible: playlistItem.highlightProgress > 0
        // 下层使用浅强调色，上层使用浓强调色；颜色各自成面，保持清晰的遮挡关系。
        // 两层只平移缓存纹理；文字、操作按钮与整行静态底图保持固定。
        Item {
            objectName: "playlistHighlightBase"
            width: parent.width; height: parent.height
            x: (width + 2) * (1 - playlistItem.highlightBaseProgress)
            layer.enabled: true
            layer.smooth: true
            Shapes.Shape {
                anchors.fill: parent
                // CurveRenderer 对斜边进行解析抗锯齿，缓存纹理平移使用线性采样。
                preferredRendererType: Shapes.Shape.CurveRenderer
                Shapes.ShapePath {
                    strokeColor: "transparent"
                    fillColor: Qt.tint(Qt.rgba(playlistItem.surfaceColor.r, playlistItem.surfaceColor.g,
                        playlistItem.surfaceColor.b, 1), Qt.alpha(view.playlistAccent,
                        view.playlistColorMode === "Light" ? (playlistItem.highlighted ? 0.50 : 0.30)
                                                          : (playlistItem.highlighted ? 0.36 : 0.22)))
                    startX: 30; startY: 0
                    PathLine { x: highlightViewport.width; y: 0 }
                    PathLine { x: highlightViewport.width; y: highlightViewport.height }
                    PathLine { x: 12; y: highlightViewport.height }
                    PathLine { x: 30; y: 0 }
                }
            }
        }
        Item {
            objectName: "playlistHighlightAccent"
            width: parent.width; height: parent.height
            x: (width + 2) * (1 - playlistItem.highlightAccentProgress) + playlistItem.highlightEdgeWidth
            layer.enabled: true
            layer.smooth: true
            Shapes.Shape {
                anchors.fill: parent
                preferredRendererType: Shapes.Shape.CurveRenderer
                Shapes.ShapePath {
                    strokeColor: "transparent"
                    // 上层保留完整强调色，下层的浅色只在错位拖尾与稳定窄边处露出。
                    fillColor: view.playlistAccent
                    startX: 30; startY: 0
                    PathLine { x: highlightViewport.width - playlistItem.highlightEdgeWidth; y: 0 }
                    PathLine { x: highlightViewport.width - playlistItem.highlightEdgeWidth; y: highlightViewport.height }
                    PathLine { x: 12; y: highlightViewport.height }
                    PathLine { x: 30; y: 0 }
                }
            }
        }
    }
    Shapes.Shape {
        anchors.fill: parent
        opacity: playlistItem.hoverProgress * 0.32
        visible: opacity > 0.001
        Shapes.ShapePath {
            fillColor: Qt.alpha(view.playlistAccent, 0.08)
            strokeColor: view.playlistAccent
            strokeWidth: 1
            startX: 1; startY: 1
            PathLine { x: playlistItem.width - 1; y: 1 }
            PathLine { x: playlistItem.width - 1; y: playlistItem.height - 1 }
            PathLine { x: 1; y: playlistItem.height - 1 }
            PathLine { x: 1; y: 1 }
        }
    }
    Rectangle {
        width: playlistItem.itemIsPlaying || playlistItem.highlighted ? 3 : 1
        height: parent.height - 12; y: 6
        color: playlistItem.itemIsPlaying || playlistItem.highlighted ? view.playlistAccent
            : playlistItem.blendColor(view.playlistDetail, view.playlistAccent, playlistItem.hoverProgress * 0.70)
    }
    Rectangle {
        x: 50; y: parent.height - 4
        width: playlistItem.itemIsPlaying ? 48 : 18; height: 2
        color: playlistItem.itemIsPlaying ? view.playlistAccent : Qt.alpha(view.playlistAccent, 0.60)
    }
    Rectangle {
        x: 74; y: parent.height - 4; width: 3; height: 2
        color: Qt.alpha(view.playlistAccent, 0.60)
        visible: !playlistItem.itemIsPlaying
    }
    Text {
        anchors.right: parent.right; anchors.rightMargin: 7
        anchors.bottom: parent.bottom; anchors.bottomMargin: -3
        text: playlistItem.rowNumber
        font.pixelSize: parent.height - 9; font.bold: true
        color: playlistItem.backgroundInk; opacity: 0.065 + playlistItem.hoverProgress * 0.025
    }

    property int itemIndex: model.index
    property string itemIcon: model.icon
    property string itemTitle: model.music_name
    property string itemArtist: model.music_artist
    property string itemAlbum: model.music_album
    property string itemDuration: model.timesize
    property string itemFilepath: model.filepath
    property bool itemIsVip: model.isVip === true
    property var itemSongId: model.songId ? model.songId : 0
    // 成员关系由 C++ 模型同步，无需逐行遍历其他列表。
    property bool itemIsLoved: model.isLiked === true
    property bool itemIsIntegrated: view.showIntegrateButton && model.isIntegrated === true
    property string itemUrl: model.url ? model.url : ""
    property bool needsAnimateIn: model.needsAnimateIn === true

    property bool itemIsSelected: model.index === listView.itemSelected_index //bind
    property bool itemIsHovered: model.index === listView.itemHovered_index //bind
    property bool itemIsPlaying: model.index === listView.playingIndex //bind

    // --- item opacity Animation ---
    property bool animationEnabled: true
    property bool isAnimating: false
    opacity: playlistItem.needsAnimateIn ? 0 : 1
    Component.onCompleted: {
        playlistItem.updateHighlight();
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
                view.listModel.setAnimationPending(index, false);
            }
        }
    }
    //-----------------------------  Connections -----------------------------//
    Connections {
        target: view
        function onAsk_dir_of_item_index(file_path) {
            // 网络歌 filepath 是临时地址、云端歌未解析音源时 filepath 为空，
            // 空 file_path 不能用 filepath 匹配（会误匹配所有空路径项），只按 songId 定位当前播放行
            if ((file_path && file_path.length > 0 && playlistItem.itemFilepath === file_path)
                || (Client.currentSongId() > 0 && playlistItem.itemSongId === Client.currentSongId())) {
                view.reponse_dir_of_item_index(playlistItem.itemIndex);
            }
        }
    }

    //----------------------------------  Ui  ----------------------------------//

    // --- 多选复选框 ---
    Oran7ESelectRect {
        id: checkBox
        width: 20
        anchors.left: parent.left
        anchors.leftMargin: listView.indexLeftMargin
        anchors.verticalCenter: parent.verticalCenter
        z: 2
        labelTextEnable: false
        checkedColor: view.playlistAccent
        border.color: view.playlistAccent
        visible: view.isMultiSelected

        checked: model.isSelected === true
        onClicked: {
            view.listModel.setSelected(model.index, !(model.isSelected === true));
        }
        HoverHandler{
            enabled: view.isMultiSelected
            acceptedDevices: PointerDevice.Mouse
            onHoveredChanged: {
                reorderController.cursorShape=  hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }
    }

    // 序号/播放图标
    Rectangle {
        id: indexRect
        width: listView.indexWidth
        height: width
        anchors.left: parent.left
        anchors.leftMargin: listView.indexLeftMargin
        anchors.verticalCenter: parent.verticalCenter
        color: "transparent"
        Label {
            id: indexLabel
            opacity: 1 - playlistItem.hoverProgress
            visible: !view.isMultiSelected && !playlistItem.itemIsSelected && opacity > 0.001
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 5
            text: model.index < 9 ? "0" + (model.index + 1) : (model.index + 1)
            font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
            font.bold: true
            font.pixelSize: Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize
            //Behavior on font.pixelSize{NumberAnimation{duration: Oran7Theme.Primary.durationMid}}
            color:view.playlistText
        }

        Image {
            id: playIcon
            opacity: playlistItem.hoverProgress
            visible: !view.isMultiSelected && opacity > 0.001
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
                color: !listView.hoverSuspended && playIconHoverHandler.hovered ? view.playlistText :
                                                      view.playlistSecondaryText
                Behavior on color { enabled: !listView.hoverSuspended; ColorAnimation { duration: 140; easing.type: Easing.OutCubic } }
            }
        }
        Oran7WaveBarChart {
            id: waveBarChart
            anchors.top: playIcon.top
            anchors.topMargin: 0
            anchors.horizontalCenter: playIcon.horizontalCenter

            opacity: 1 - playlistItem.hoverProgress
            visible: !view.isMultiSelected && playlistItem.itemIsSelected && opacity > 0.001

            animationEnabled: playlistItem.itemIsPlaying
            waveColor: view.playlistAccent
        }

        MouseArea {
            id:playIconBtnMouseArea
            anchors.fill: parent
            enabled: !view.isMultiSelected
            onClicked: {
                view.itemClicked(model.index);
                //console.log("clicked")
            }
        }
        HoverHandler{
            id:playIconHoverHandler
            enabled: !view.isMultiSelected && !listView.hoverSuspended
            acceptedDevices: PointerDevice.Mouse
            onHoveredChanged: {
                reorderController.cursorShape=  hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }
    }
    // 阴影只采样固定的圆角轮廓，不因封面下载、hover 或选中动画重复生成。
    Rectangle {
        id: coverShadowShape
        width: coverImage.width
        height: coverImage.height
        radius: coverImage.radius
        color: "black"
        visible: false
    }
    DropShadow {
        anchors.fill: coverImage
        source: coverShadowShape
        horizontalOffset: 0
        verticalOffset: 3
        radius: 7
        samples: 15
        spread: 0
        color: "#78000000"
        transparentBorder: true
        cached: true
    }
    // 专辑封面
    Oran7RoundedImage{
        id:coverImage
        objectName: "playlistCover"
        width: Math.min(40, playlistItem.height - 16)
        height: width
        anchors.left: parent.left
        anchors.leftMargin: 50
        anchors.verticalCenter: parent.verticalCenter
        radius: 7
        source:playlistItem.itemIcon
    }

    // 标题、作者与专辑各自限宽，窄窗口隐藏专辑但保留操作和时长。
    Column {
        id: titleColumn
        x: coverImage.x + coverImage.width + 14
        width: Math.max(0, playlistItem.titleEnd - x)
        anchors.verticalCenter: parent.verticalCenter
        spacing: 3
        Label {
            id: titleLabel
            objectName: "playlistSongTitle"
            width: parent.width
            text: playlistItem.itemTitle
            elide: Text.ElideRight
            color: view.playlistText
            font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
            font.pixelSize: view.playlistFontSize
            font.weight: Font.DemiBold
        }
        Row {
            width: parent.width; spacing: 6
            Label {
                id: vipLabel
                visible: playlistItem.itemIsVip
                text: "VIP"
                color: view.playlistAccent
                font.pixelSize: Math.max(10, view.playlistFontSize - 5)
                font.bold: true
            }
            Label {
                id: artistLabel
                width: Math.max(0, parent.width - (vipLabel.visible ? vipLabel.width + 6 : 0))
                text: playlistItem.itemArtist
                elide: Text.ElideRight
                color: view.playlistSecondaryText
                font.family: titleLabel.font.family
                font.pixelSize: Math.max(11, view.playlistFontSize - 3)
            }
        }
    }
    Column {
        x: playlistItem.albumX
        width: Math.max(0, playlistItem.actionsX - x - 20)
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        visible: playlistItem.showAlbum
        Label {
            text: qsTranslate("Oran7Ui", "专辑")
            color: view.playlistSecondaryText
            opacity: 0.65
            font.pixelSize: Math.max(10, view.playlistFontSize - 6)
            font.family: titleLabel.font.family
        }
        Label {
            id: albumLabel
            width: parent.width
            text: playlistItem.itemAlbum
            elide: Text.ElideRight
            color: view.playlistSecondaryText
            font.pixelSize: Math.max(11, view.playlistFontSize - 2)
            font.family: titleLabel.font.family
        }
    }
    Rectangle {
        x: playlistItem.durationX - 12
        anchors.verticalCenter: parent.verticalCenter
        width: 1; height: 22
        color: view.playlistDetail
    }
    Label {
        id: durationLabel
        x: playlistItem.durationX
        anchors.verticalCenter: parent.verticalCenter
        width: 56
        text: formatDuration(playlistItem.itemDuration)
        color: view.playlistText
        font.family: titleLabel.font.family
        font.pixelSize: Math.max(11, view.playlistFontSize - 2)
        font.weight: Font.DemiBold
        visible: !view.isMultiSelected && view.showDuration
    }

    // 拖拽图标（仅显示，不处理事件）
    Image {
        id: dragIcon
        anchors.left: parent.left
        anchors.leftMargin: playlistItem.durationX
        anchors.verticalCenter: parent.verticalCenter
        sourceSize.width: 40
        sourceSize.height: 40
        source: "qrc:/image/drag.png"
        visible: view.isMultiSelected && view.allowDragReorder

        property bool item_isDraging: view.dragActive && view.dragSourceIndex === playlistItem.itemIndex

        layer.enabled: true
        layer.effect: ColorOverlay {
            source: dragIcon
            color: dragIcon.item_isDraging ? view.playlistText :
                                             view.playlistText
        }
    }
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: false
        enabled: !view.isMultiSelected
        propagateComposedEvents: true

        // onClicked: {
        //     if (!view.isMultiSelected) {
        //         view.itemClicked(model.index);
        //     }
        // }

        // onPositionChanged: (mouse) =>{
        //     var posInParent = mapToItem(view, mouse.x, mouse.y)

        //     // console.log("父控件坐标:", posInParent.x, posInParent.y)
        //     // console.log("全局坐标:", mapToGlobal(mouse.x, mouse.y))
        //     // //相对坐标
        //     // var relativeX = mouse.x/* + dragImage.x*/
        //     // var relativeY = mouse.y/* + dragImage.y*/
        //     // console.log("相对坐标:", relativeX, relativeY)
        // }

        onDoubleClicked: {
            if (!view.isMultiSelected) {
                view.itemDoubleClicked(model.index);
            }
        }

        // onEntered: {
        //     if (!view.isMultiSelected) {
        //         listView.itemHovered_index = playlistItem.itemIndex;
        //     }
        // }
    }

    // 整合按钮（"我整合的音乐"合集；未整合/已整合两态图标，点击交给页面处理）
    Image {
        id: integrateImage
        anchors.left: parent.left
        anchors.leftMargin: playlistItem.actionsX
        anchors.verticalCenter: parent.verticalCenter
        width: 30
        height: 30
source: playlistItem.itemIsIntegrated ? "qrc:/image/ep_collection-tag.-no-middle-transparent.png" :
                "qrc:/image/ep_collection-tag.png"
        visible: view.showIntegrateButton
        smooth: true        // 平滑缩放
        mipmap: true        // mipmap 预过滤：大图小尺寸显示不再锯齿
        antialiasing: true  // 缩放时边缘抗锯齿

        property bool isPressed: false
        scale: isPressed ? 0.95 : 1.0

        layer.enabled: true
        layer.effect: ColorOverlay {
            source: integrateImage
            // 悬停仅变色，使用与强调色背景有明暗对比的图标色。
            color: !listView.hoverSuspended && integrateHoverHandler.hovered ? playlistItem.actionHoverColor :
                                                  view.playlistText
            Behavior on color { enabled: !listView.hoverSuspended; ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                view.itemIntegrated(playlistItem.itemIndex)
            }
            onPressed: parent.isPressed = true
            onReleased: parent.isPressed = false
        }
        // hover 高亮处理
        HoverHandler {
            id: integrateHoverHandler
            enabled: !view.isMultiSelected && !listView.hoverSuspended
            acceptedDevices: PointerDevice.Mouse
            onHoveredChanged: {
                reorderController.cursorShape = hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }
    }

    // 喜欢（收藏）按钮
    Image {
        id: loveImage
        anchors.left: parent.left
        anchors.leftMargin: playlistItem.actionsX + (view.showIntegrateButton ? 36 : 0)
        anchors.verticalCenter: parent.verticalCenter
        width: 26
        height: 26
        source: playlistItem.itemIsLoved ? "qrc:/image/love_NoMiddleTransparent.png" : "qrc:/image/love.png"
        visible: view.showLoveButton
        smooth: true        // 平滑缩放
        mipmap: true        // mipmap 预过滤：大图小尺寸显示不再锯齿
        antialiasing: true  // 缩放时边缘抗锯齿

        property bool isPressed: false
        scale: isPressed ? 0.95 : 1.0

        layer.enabled: true
        layer.effect: ColorOverlay {
            source: loveImage
            // 悬停仅变色，使用与强调色背景有明暗对比的图标色。
            color: !listView.hoverSuspended && loveHoverHandler.hovered ? playlistItem.actionHoverColor :
                                              view.playlistText
            Behavior on color { enabled: !listView.hoverSuspended; ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                view.itemLoved(playlistItem.itemIndex)
            }
            onPressed: parent.isPressed = true
            onReleased: parent.isPressed = false
        }
        // hover 高亮处理
        HoverHandler {
            id: loveHoverHandler
            enabled: !view.isMultiSelected && !listView.hoverSuspended
            acceptedDevices: PointerDevice.Mouse
            onHoveredChanged: {
                reorderController.cursorShape = hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }
    }

    function formatDuration(seconds) {
        var total = Math.max(0, parseInt(seconds) || 0);
        var minutes = Math.floor(total / 60);
        var secs = total % 60;
        return (minutes < 10 ? "0" : "") + minutes + ":" + (secs < 10 ? "0" : "") + secs;
    }
}

