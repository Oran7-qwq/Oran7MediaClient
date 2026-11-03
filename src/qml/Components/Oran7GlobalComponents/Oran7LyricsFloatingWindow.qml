// Oran7LyricsFloatingWindow.qml
// 桌面歌词悬浮窗：独立于 mainWindow 的顶层透明窗口。
// 复用 Oran7LyricsIsland 的歌词渲染核心（溶解/滚动/渐变/后缀动画），仅将其置于
// 一个无边框、置顶、不进任务栏的 Window 中，并提供拖动/锁定/穿透/不透明度控制。
import QtQuick 2.15
import QtQuick.Window
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

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


    // 透明留白（胶囊周围）
    readonly property real padX: 12
    readonly property real padY: 12

    // 无边框 + Tool（不进任务栏）；按配置动态叠加 置顶 / 鼠标穿透
    flags: {
        var f = Qt.FramelessWindowHint | Qt.Tool;
        if (root.alwaysOnTop) f |= Qt.WindowStaysOnTopHint;
        if (root.clickThrough) f |= Qt.WindowTransparentForInput;
        return f;
    }
    color: "transparent"
    // 宿主尺寸不随歌词动画变化，避免原生 move/resize 与 Qt 帧提交不同步。
    width: lyricsIsland.islandMaxWidth + padX * 2
    height: lyricsIsland.height + padY * 2
    property bool displayAllowed: true
    visible: root.enabled && root.displayAllowed

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

    // 把窗口位置限制在工作区（Screen.desktopAvailable* 已排除任务栏）内，
    // 防止被拖到任务栏下面/屏幕外后拿不回来。
    function clampToWorkArea() {
        var minX = -root.contentLeft
        var maxX = Math.max(minX, Screen.desktopAvailableWidth - root.contentRight)
        var maxY = Math.max(0, Screen.desktopAvailableHeight - root.height)
        root.x = Math.max(minX, Math.min(root.x, maxX))
        root.y = Math.max(0, Math.min(root.y, maxY))
    }
    // 重置到默认位置（后续可接设置窗口/右键菜单）
    function resetPosition() {
        root.x = (Screen.desktopAvailableWidth - root.width) / 2
        root.y = Screen.desktopAvailableHeight - root.height - 40
        clampToWorkArea()
        posSaveTimer.restart()
    }

    Component.onCompleted: {
        var th = Oran7Theme.Oran7MusicLyricsIsland;
        if (typeof th.floatingX === "number" && th.floatingX >= 0) root.x = th.floatingX - root.contentLeft;
        if (typeof th.floatingY === "number" && th.floatingY >= 0) root.y = th.floatingY;
        else {
            root.x = (Screen.desktopAvailableWidth - root.width) / 2   // 默认水平居中（与主窗口灵动岛一致）
            root.y = Screen.desktopAvailableHeight - root.height - 40  // 默认底部居中偏上
        }
        clampToWorkArea()   // 上次被拖到任务栏下面等越界坐标，启动即拉回工作区
        root.updateContentMask()
    }

    // 每次重新显示时自愈：越界（如意外卡到任务栏下）自动拉回工作区
    onVisibleChanged: {
        if (root.visible)
            clampToWorkArea()
    }

    // 位置防抖保存（停止拖动 200ms 后落盘）
    // 只有用户拖动才落盘；floatingX 仍保存可见胶囊区域左边缘，兼容旧版配置。
    property bool posMovedByUser: false
    Timer {
        id: posSaveTimer
        interval: 200
        onTriggered: {
            posMovedByUser = false
            Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland", "floatingX", root.x + root.contentLeft);
            Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland", "floatingY", root.y);
        }
    }
    onXChanged: if (posMovedByUser) posSaveTimer.restart()
    onYChanged: if (posMovedByUser) posSaveTimer.restart()

    // 原生窗口保持固定尺寸/位置；内容宽度动画只发生在同一渲染表面内部。
    // 遮罩跟随胶囊和阴影留白，透明宿主区域不拦截桌面鼠标。
    readonly property real contentLeft: lyricsIsland.x - root.padX
    readonly property real contentRight: lyricsIsland.x + lyricsIsland.width + root.padX
    readonly property rect contentMaskRect: Qt.rect(contentLeft, 0, contentRight - contentLeft, root.height)
    function updateContentMask() { Oran7Api.setWindowMaskRect(root, root.contentMaskRect) }
    onContentMaskRectChanged: updateContentMask()

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
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        z: 2
        MouseArea {
            id: dragArea
            anchors.fill: parent
            enabled: !root.locked && !root.clickThrough && root.enabled
            cursorShape: Qt.SizeAllCursor

            property point pressPos: Qt.point(0, 0)
            property bool mouseIsPressed: false

            onPressed: (mouse) => {
                mouseIsPressed = true
                pressPos = Qt.point(mouse.x, mouse.y)
            }
            onReleased: {
                mouseIsPressed = false
                if (posMovedByUser)
                    posSaveTimer.restart()   // 仅实际拖动过才落盘位置（复用位置防抖保存）
            }
            onCanceled: mouseIsPressed = false
            onPositionChanged: (mouse) => {
                if (!mouseIsPressed)
                    return
                posMovedByUser = true
                let delta = Qt.point(mouse.x - pressPos.x, mouse.y - pressPos.y)
                root.x = Math.max(-root.contentLeft, Math.min(root.x + delta.x, Screen.desktopAvailableWidth - root.contentRight))
                root.y = Math.max(0, Math.min(root.y + delta.y, Screen.height - root.height + 10))
            }
        }
    }

    // 歌词胶囊本体（悬浮模式：独立底色，不复用主窗口背景 blur）
    Oran7LyricsIsland {
        id: lyricsIsland
        anchors.horizontalCenter: parent.horizontalCenter
        y: padY
        blurEnabled:false
        floatingMode: true
        floatingOpacity: root.windowOpacity
        backgroundImage: root.backgroundImage
        islandVisible: true
        lyricGradientColorIndex: 12
        lyricsFontSize: root.lyricFontSize
        borderWidth: 0
        borderRadius: 0
        layer.enabled: true
        layer.effect: DropShadow {
            id: windowContentCloseBtnDropShadow
            anchors.fill: lyricsIsland
            source: lyricsIsland
            z: lyricsIsland.z
            spread: 0.01
            samples: 15
            radius: lyricsIsland.height / 4
            horizontalOffset: 0
            verticalOffset: 0
            color: "gray"
            SequentialAnimation on color {
                id: shadowAnimation_inner
                running: false
                loops: Animation.Infinite
                property real transDuration: 2000

                PropertyAnimation { to: "#FFA500"; duration: shadowAnimation_inner.transDuration }
                PropertyAnimation { to: "#FFFF00"; duration: shadowAnimation_inner.transDuration }
                PropertyAnimation { to: "#00FF00"; duration: shadowAnimation_inner.transDuration }
                PropertyAnimation { to: "#00FFFF"; duration: shadowAnimation_inner.transDuration }
                PropertyAnimation { to: "#0000FF"; duration: shadowAnimation_inner.transDuration }
                PropertyAnimation { to: "#800080"; duration: shadowAnimation_inner.transDuration }
                PropertyAnimation { to: "#FF0000"; duration: shadowAnimation_inner.transDuration }
            }
        }
    }
}
