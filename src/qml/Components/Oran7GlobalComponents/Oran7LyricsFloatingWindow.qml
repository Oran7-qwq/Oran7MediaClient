// Oran7LyricsFloatingWindow.qml
// 桌面歌词悬浮窗：独立于 mainWindow 的顶层透明窗口。
// 复用 Oran7LyricsIsland 的歌词渲染核心（溶解/滚动/渐变/后缀动画），仅将其置于
// 一个无边框、置顶、不进任务栏的 Window 中，并提供拖动/锁定/穿透/不透明度控制。
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects

import Client 1.0
import "../../Basic"
import "."

import Oran7UI.Impl

Window {
    id: root

    // ============ 运行状态（受 Oran7MusicLyricsIsland 主题驱动） ============
    // 直接绑定主题 map：saveComponentToken → installComponentToken → reloadComponentThemeFile
    // 会 emit `<Component>Changed` 信号，QML 绑定实时刷新；
    // （旧写法 onCompleted 一次性读取，点击开关只写配置、不更新本地值，导致窗口不显示）
    property bool enabled: Oran7Theme.Oran7MusicLyricsIsland.floatingEnabled === true    // 悬浮窗显示/隐藏
    property bool locked: Oran7Theme.Oran7MusicLyricsIsland.floatingLocked === true      // 锁定位置（禁止拖动）
    property bool clickThrough: Oran7Theme.Oran7MusicLyricsIsland.floatingClickThrough === true  // 鼠标穿透
    property bool alwaysOnTop: Oran7Theme.Oran7MusicLyricsIsland.floatingAlwaysOnTop !== false   // 置顶
    property real windowOpacity: typeof Oran7Theme.Oran7MusicLyricsIsland.floatingOpacity === "number"
        ? Oran7Theme.Oran7MusicLyricsIsland.floatingOpacity : 0.9                       // 胶囊底色不透明度
    property int lyricFontSize: typeof Oran7Theme.Oran7MusicLyricsIsland.lyricsFontSize === "number"
        ? Oran7Theme.Oran7MusicLyricsIsland.lyricsFontSize : 20                          // 歌词字号
    readonly property string backgroundImage: Oran7Theme.Oran7MusicLyricsIsland.backgroundImage_url
    readonly property bool useBackgroundImage: windowOpacity > 0 && backgroundImage.length > 0
        && Oran7Theme.Oran7MusicLyricsIsland.backgroundImageEnabled

    property int lyricsHeight: 52
    // 歌名和歌词共用用户设定的高度，切歌等待新歌词时也保持窗口尺寸。
    readonly property int lyricsAreaHeight: Math.max(40, Math.min(96, lyricsHeight))
    readonly property int controlsHeight: 40
    readonly property int controlsWidth: 284
    readonly property int controlsExtent: Math.max(0, Math.round(controlsHeight * controlsReveal))
    readonly property bool musicFocused:
        BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_MusicPlayerIndex
    readonly property bool hoverAvailable: visible && !clickThrough
    property bool controlsExpanded: false
    // 展开过半再显示控件；收起时等淡出完成才缩回面板。
    property real controlsReveal: hoverAvailable && (controlsExpanded || controlsOpacity > 0) ? 1 : 0
    property real controlsOpacity: hoverAvailable && controlsExpanded && controlsReveal >= 0.55 ? 1 : 0
    Behavior on controlsReveal {
        enabled: root.hoverAvailable
        NumberAnimation {
            duration: 260
            easing.type: root.controlsExpanded ? Easing.OutBack : Easing.InOutCubic
            easing.overshoot: 1.1
        }
    }
    Behavior on controlsOpacity {
        enabled: root.hoverAvailable
        NumberAnimation { duration: 135; easing.type: Easing.InOutQuad }
    }
    onHoverAvailableChanged: if (!hoverAvailable) controlsExpanded = false


    // 透明留白（胶囊周围）
    readonly property real padX: 12
    readonly property real padY: 12
    readonly property int collapsedHeight: lyricsAreaHeight + padY * 2

    // 无边框 + Tool（不进任务栏）；按配置动态叠加 置顶 / 鼠标穿透
    flags: {
        var f = Qt.FramelessWindowHint | Qt.Tool;
        if (root.alwaysOnTop) f |= Qt.WindowStaysOnTopHint;
        if (root.clickThrough) f |= Qt.WindowTransparentForInput;
        return f;
    }
    color: "transparent"
    // 原生宽度固定，歌词文字不会随胶囊宽度重排。
    width: lyricsIsland.islandMaxWidth + padX * 2
    height: collapsedHeight + controlsExtent
    property bool displayAllowed: true
    property bool placementReady: false
    property var desktopAreas: Oran7Api.screenGeometries
    property rect workArea: Qt.rect(0, 0, 1024, 768)
    visible: root.enabled && root.displayAllowed && root.placementReady

    // 不依附 mainWindow：声明在 main.qml 里默认会成为其 transient parent（owned 窗口），
    // 主窗口最小化时系统会把 owned 窗口一起最小化。置空后成为完全独立顶层窗口 → 不受影响。
    transientParent: null

    // ============ 对外接口 ============
    function toggleEnabled() {
        var cur = Oran7Theme.Oran7MusicLyricsIsland.floatingEnabled === true;
        Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland", "floatingEnabled", !cur);
    }
    function setEnabled(b) {
        if (Oran7Theme.Oran7MusicLyricsIsland.floatingEnabled !== b)
            Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland", "floatingEnabled", b);
    }

    // 位置只在启动时读一次，运行期由拖动写入（避免与保存形成循环绑定）；
    // 其余运行状态已直接绑定主题 map，设置窗口/播放栏改动会实时生效。

    function workAreaForPoint(px, py) {
        var nearest = workArea
        var distance = Infinity
        for (var i = 0; i < desktopAreas.length; ++i) {
            var area = desktopAreas[i]
            if (area.width <= 0 || area.height <= 0) continue
            var dx = Math.max(area.x - px, 0, px - area.x - area.width)
            var dy = Math.max(area.y - py, 0, py - area.y - area.height)
            var candidate = dx * dx + dy * dy
            if (candidate < distance) {
                nearest = area
                distance = candidate
            }
        }
        return nearest
    }
    function desktopContainsPoint(px, py) {
        for (var i = 0; i < desktopAreas.length; ++i) {
            var area = desktopAreas[i]
            if (px >= area.x && px < area.x + area.width && py >= area.y && py < area.y + area.height)
                return true
        }
        return false
    }
    // 仅启用时检查原生悬浮窗左上角；窗口其余部分允许超出屏幕，任务栏也属于桌面。
    function recoverPositionIfOutside() {
        if (!desktopAreas.length || desktopContainsPoint(x, y)) return
        workArea = workAreaForPoint(x, y)
        root.x = Math.round(Math.max(workArea.x, Math.min(root.x, workArea.x + workArea.width - 1)))
        root.y = Math.round(Math.max(workArea.y, Math.min(root.y, workArea.y + workArea.height - 1)))
    }
    // 重置到默认位置（后续可接设置窗口/右键菜单）
    function resetPosition() {
        root.x = workArea.x + (workArea.width - root.width) / 2
        root.y = workArea.y + workArea.height - collapsedHeight - 40
        posSaveTimer.restart()
    }

    function restorePosition() {
        var th = Oran7Theme.Oran7MusicLyricsIsland;
        var modern = th.floatingPositionVersion === 2
        var hasX = typeof (modern ? th.floatingCenterX : th.floatingX) === "number"
            && isFinite(modern ? th.floatingCenterX : th.floatingX)
            && (modern || th.floatingX !== -1)
        var hasY = typeof th.floatingY === "number" && isFinite(th.floatingY) && (modern || th.floatingY !== -1)
        var center = hasX ? (modern ? th.floatingCenterX : th.floatingX + contentMaskRect.width / 2) : NaN
        workArea = hasX || hasY ? workAreaForPoint(hasX ? center : workArea.x + workArea.width / 2,
                                                   hasY ? th.floatingY + height / 2 : workArea.y + workArea.height / 2)
                               : (desktopAreas.length ? desktopAreas[0] : workArea)
        // 先确定所属工作区和画布宽度，再恢复中心；不依赖启动时正在变化的胶囊左边缘。
        root.x = hasX ? center - width / 2 : workArea.x + (workArea.width - width) / 2
        root.y = hasY ? th.floatingY : workArea.y + workArea.height - collapsedHeight - 40
        placementReady = true
        root.updateContentMask()
    }
    Component.onCompleted: {
        restorePosition()
        if (enabled) recoverPositionIfOutside()
    }
    // 普通显隐、拖动结束、分辨率变化、歌词伸缩均不触发位置修正。
    onEnabledChanged: if (placementReady && enabled) recoverPositionIfOutside()

    // 位置防抖保存（停止拖动 200ms 后落盘）
    // 只有用户拖动才落盘；floatingX 仍保存可见胶囊区域左边缘，兼容旧版配置。
    property bool posMovedByUser: false
    function savePosition() {
        posMovedByUser = false
        Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland", "floatingX", root.x + (root.width - lyricsIsland.width) / 2 - padX)
        Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland", "floatingY", root.y)
        Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland", "floatingCenterX", root.x + root.width / 2)
        Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland", "floatingPositionVersion", 2)
    }
    Timer {
        id: posSaveTimer
        interval: 200
        onTriggered: {
            if (dragArea.mouseIsPressed) { restart(); return }
            root.savePosition()
        }
    }
    onXChanged: if (posMovedByUser) posSaveTimer.restart()
    onYChanged: if (posMovedByUser) posSaveTimer.restart()

    // 内容宽度动画只发生在同一渲染表面内部。
    // 遮罩跟随胶囊和阴影留白，透明宿主区域不拦截桌面鼠标。
    readonly property real contentWidth: Math.max(lyricsIsland.width, controlsWidth * Math.min(1, controlsReveal / 0.55))
    readonly property real contentLeft: (width - contentWidth) / 2 - padX
    readonly property rect contentMaskRect: Qt.rect(contentLeft, 0, contentWidth + padX * 2, height)
    function updateContentMask() { Oran7Api.setWindowMaskRect(root, root.contentMaskRect) }
    onContentMaskRectChanged: updateContentMask()

    function updateControlsHover() {
        if (!hoverAvailable)
            return
        var pointer = capsuleHover.point.scenePosition
        var pointerX = x + pointer.x
        var pointerY = y + pointer.y
        // 窗口重现或几何变化会重新派发 hover，桌面外的缓存坐标不能触发展开。
        var hovered = capsuleHover.hovered && (!desktopAreas.length || desktopContainsPoint(pointerX, pointerY))
        if (hovered || dragArea.pressed || progress.pressed) {
            collapseControlsTimer.stop()
            if (hovered && !dragArea.pressed)
                controlsExpanded = true
        } else if (controlsExpanded) {
            collapseControlsTimer.restart()
        }
    }

    Timer {
        id: collapseControlsTimer
        interval: 180
        onTriggered: root.controlsExpanded = false
    }

    Item {
        x: root.contentLeft
        width: root.contentMaskRect.width
        height: root.contentMaskRect.height
        z: capsuleSurface.z + 2
        HoverHandler {
            id: capsuleHover
            blocking: false
            enabled: root.hoverAvailable
            onHoveredChanged: root.updateControlsHover()
        }
    }

    // 主窗口最小化时，Tool 子窗口会被系统跟着最小化/隐藏；
    // 桌面歌词要求常驻屏幕：延时到系统最小化完成后重新 show + raise，保持置顶可见。
    //
    // ⚠ 信号名必须是 visibilityChanged。Window（QQuickWindow/QWindow）没有
    // minimizedChanged —— 最小化是通过 visibility 属性表达的（QWindow::Visibility）。
    // 名字写错不会报错，只会静默失效：处理函数永远不被调用，这段恢复逻辑等于没写。
    Connections {
        target: BasicConfig.mainWindow
        function onVisibilityChanged() {
            if (root.enabled && BasicConfig.mainWindow.visibility === Window.Minimized)
                restoreTimer.restart()
        }
    }
    Timer {
        id: restoreTimer
        interval: 120
        onTriggered: {
            if (!root.enabled || BasicConfig.mainWindow.visibility !== Window.Minimized)
                return
            root.showNormal()   // 处于最小化状态的窗口 show() 无效，须用 showNormal() 取消最小化
            root.raise()
        }
    }

    // 鼠标区域坐标固定，命中范围交给原生遮罩；避免拖动时被胶囊伸缩带偏。
    // 与主窗口/播放栏同一套拖拽写法：pressPos 记录按下点，移动时按增量挪窗，
    // 窗口随鼠标移动后 mouse.x 相对坐标自会抵消上一帧位移，无需记起始窗口坐标。
    Item {
        y: root.padY
        width: parent.width
        height: root.lyricsAreaHeight
        z: capsuleSurface.z + 1
        MouseArea {
            id: dragArea
            anchors.fill: parent
            enabled: !root.locked && !root.clickThrough && root.enabled
            cursorShape: Qt.SizeAllCursor

            property point pressPos: Qt.point(0, 0)
            readonly property bool mouseIsPressed: pressed
            onPressedChanged: root.updateControlsHover()

            onPressed: (mouse) => {
                pressPos = Qt.point(mouse.x, mouse.y)
            }
            onReleased: {
                if (posMovedByUser)
                    posSaveTimer.restart()   // 仅实际拖动过才落盘位置（复用位置防抖保存）
            }
            onPositionChanged: (mouse) => {
                if (!mouseIsPressed)
                    return
                posMovedByUser = true
                let delta = Qt.point(mouse.x - pressPos.x, mouse.y - pressPos.y)
                // 自由拖动和跨屏；松开只保存，不吸附或限制边界。
                root.x += delta.x
                root.y += delta.y
            }
        }
    }

    // 阴影和可见背景使用相同的圆角及展开尺寸，留白用于容纳模糊边缘。
    Rectangle {
        id: shadowSilhouette
        objectName: "floatingShadowSilhouette"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.alignWhenCentered: false
        y: capsuleSurface.y
        width: root.windowOpacity > 0 ? capsuleSurface.width : lyricsIsland.width
        height: root.windowOpacity > 0 ? capsuleSurface.height : lyricsIsland.height
        radius: capsuleBackdrop.radius
        antialiasing: true
        color: Qt.rgba(0, 0, 0, root.useBackgroundImage ? 1 : root.windowOpacity * capsuleBackdrop.color.a)
        visible: false
    }

    DropShadow {
        anchors.fill: shadowSilhouette
        source: root.windowOpacity > 0 ? shadowSilhouette : lyricsIsland
        z: capsuleSurface.z - 1
        samples: 17
        radius: 8
        verticalOffset: 2
        color: "#78000000"
    }

    Item {
        id: capsuleSurface
        objectName: "floatingCapsuleSurface"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.alignWhenCentered: false
        y: root.padY
        width: root.contentWidth
        height: root.height - root.padY * 2
        z: 999

        // 歌词与控制行共享同一张背景，原生窗口增高时直接拉长这块底色或图片。
        Rectangle {
            id: capsuleBackdrop
            anchors.fill: parent
            visible: !root.useBackgroundImage && root.windowOpacity > 0
            radius: lyricsIsland.height / 2
            antialiasing: true
            color: Oran7Theme.Oran7MusicLyricsIsland["floatingBackdropColor-6"]
            opacity: root.windowOpacity
        }
        Oran7RoundedImage {
            anchors.fill: parent
            visible: root.useBackgroundImage
            source: root.useBackgroundImage ? root.backgroundImage : ""
            radius: lyricsIsland.height / 2
            tiled: true
            autoPan: true
        }

        Oran7LyricsIsland {
            id: lyricsIsland
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.alignWhenCentered: false
            height: root.lyricsAreaHeight
            blurEnabled: false
            floatingMode: true
            islandVisible: true
            lyricGradientColorIndex: 12
            lyricsFontSize: root.lyricFontSize
            borderWidth: 0
            borderRadius: 0
        }

        // 控制行是同一胶囊的内容，歌词仍固定在上半部分。
        Item {
            id: controlsArea
            objectName: "floatingPlaybackControls"
            anchors.horizontalCenter: parent.horizontalCenter
            y: lyricsIsland.height
            width: Math.min(root.controlsWidth, parent.width)
            height: root.controlsExtent
            clip: true
            visible: height > 0
            enabled: root.hoverAvailable && root.controlsExpanded && root.controlsOpacity >= 0.95

            Item {
                id: controlsContent
                objectName: "floatingPlaybackContent"
                width: parent.width - 24
                height: root.controlsHeight
                anchors.horizontalCenter: parent.horizontalCenter
                opacity: root.controlsOpacity

                Oran7ShaderTextMask {
                    id: progressMask
                    anchors.left: transportRow.right
                    anchors.leftMargin: 14
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    height: 18
                    shaderName: "clouds"
                    effectEnabled: root.controlsOpacity > 0

                    Oran7ValueSlider {
                        id: progress
                        objectName: "floatingPlaybackProgress"
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.max(0, parent.width - progressHandleWidth)
                        height: parent.height
                        to: root.musicFocused ? Client.playback.durationMs / 1000 : 0
                        value: root.musicFocused ? Client.playback.positionMs / 1000 : 0
                        interactive: to > 0
                        trackHeight: 4
                        trackColor: "white"
                        progressHandleWidth: 8
                        animationDuration: pressed ? 0 : 100
                        sliderColor_themeIndex: 0
                        sliderColor_themeItems: [{default_Color: "white", sel_Color: "white"}]
                        readonly property string trackKey:
                            String(Client.playback.currentTrack.songId) + "|" + BasicConfig.currentMediaFilePath
                        property string pressedTrackKey: ""
                        onPressedChanged: {
                            if (pressed)
                                pressedTrackKey = trackKey
                            root.updateControlsHover()
                        }
                        onCommitted: (seconds) => {
                            if (interactive && pressedTrackKey === trackKey)
                                Client.seekTo(Math.floor(seconds))
                        }
                    }
                }

                Row {
                    id: transportRow
                    objectName: "floatingTransportRow"
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Oran7FloatingTransportButton {
                        text: qsTranslate("Oran7Ui", "上一首")
                        glyphPath: "M 3 3 H 4.6 V 13 H 3 Z M 12.2 2.9 Q 13 2.5 13 3.4 V 12.6 Q 13 13.5 12.2 13.1 L 5.8 8.7 Q 4.8 8 5.8 7.3 Z"
                        onClicked: Client.playPrevious()
                    }
                    Oran7FloatingTransportButton {
                        readonly property bool playing: root.musicFocused && Client.playback.active
                        text: playing ? qsTranslate("Oran7Ui", "暂停") : qsTranslate("Oran7Ui", "播放")
                        primary: true
                        glyphPath: playing
                            ? "M 4 3 H 6.5 V 13 H 4 Z M 9.5 3 H 12 V 13 H 9.5 Z"
                            : "M 5 3.3 Q 5 2.5 5.8 2.9 L 13 7.4 Q 14 8 13 8.6 L 5.8 13.1 Q 5 13.5 5 12.7 Z"
                        onClicked: Client.musicQueue.toggleCurrent()
                    }
                    Oran7FloatingTransportButton {
                        text: qsTranslate("Oran7Ui", "下一首")
                        glyphPath: "M 11.4 3 H 13 V 13 H 11.4 Z M 3.8 2.9 Q 3 2.5 3 3.4 V 12.6 Q 3 13.5 3.8 13.1 L 10.2 8.7 Q 11.2 8 10.2 7.3 Z"
                        onClicked: Client.playNext()
                    }
                }
            }
        }
    }

    component Oran7FloatingTransportButton: Button {
        id: transportButton
        required property string glyphPath
        property bool primary: false
        width: 30
        height: width
        padding: 0
        hoverEnabled: true
        focusPolicy: Qt.NoFocus
        Accessible.name: text
        background: Item {
            DropShadow {
                anchors.fill: disc
                source: disc
                radius: 3
                samples: 7
                verticalOffset: 1
                color: "#58000000"
            }
            Rectangle {
                id: disc
                anchors.centerIn: parent
                width: transportButton.primary ? parent.width : parent.width - 4
                height: width
                radius: width / 2
                antialiasing: true
                color: transportButton.down ? "#38ffffff" : transportButton.hovered ? "#26ffffff"
                    : transportButton.primary ? "#1affffff" : "#0dffffff"
                border.width: 1
                border.color: transportButton.hovered || transportButton.primary ? "#66ffffff" : "#25ffffff"
                Behavior on color { ColorAnimation { duration: 120 } }
            }
        }
        contentItem: Item {
            Shape {
                anchors.centerIn: parent
                width: 16
                height: 16
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeWidth: -1
                    fillColor: transportButton.primary || transportButton.hovered ? "#ffffff" : "#deffffff"
                    PathSvg { path: transportButton.glyphPath }
                }
            }
        }
    }
}
