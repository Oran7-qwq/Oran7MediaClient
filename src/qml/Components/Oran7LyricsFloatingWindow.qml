// Oran7LyricsFloatingWindow.qml
// 桌面歌词悬浮窗：独立于 mainWindow 的顶层透明窗口。
// 复用 Oran7LyricsIsland 的歌词渲染核心（溶解/滚动/渐变/后缀动画），仅将其置于
// 一个无边框、置顶、不进任务栏的 Window 中，并提供拖动/锁定/穿透/不透明度控制。
import QtQuick 2.15
import QtQuick.Window
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

import "../Basic"
import "../Components"

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
    width: lyricsIsland.width + padX * 2
    height: lyricsIsland.height + padY * 2
    visible: root.enabled

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
    // 同时把浮点中心 _centerX 对齐到钳位后的实际 x（用户拖动/启动定位等 int 操作后的权威值）。
    function clampToWorkArea() {
        var maxX = Math.max(0, Screen.desktopAvailableWidth - root.width)
        var maxY = Math.max(0, Screen.desktopAvailableHeight - root.height)
        root.x = Math.max(0, Math.min(root.x, maxX))
        root.y = Math.max(0, Math.min(root.y, maxY))
        _centerX = root.x + root.width / 2
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
        if (typeof th.floatingX === "number" && th.floatingX >= 0) root.x = th.floatingX;
        if (typeof th.floatingY === "number" && th.floatingY >= 0) root.y = th.floatingY;
        else {
            root.x = (Screen.desktopAvailableWidth - root.width) / 2   // 默认水平居中（与主窗口灵动岛一致）
            root.y = Screen.desktopAvailableHeight - root.height - 40  // 默认底部居中偏上
        }
        clampToWorkArea()   // 上次被拖到任务栏下面等越界坐标，启动即拉回工作区
        root.prevWidth = root.width   // 以当前宽度为基准：首个歌词宽度变化不被“首次就位”跳过，
                                      // 保证从一开始就以窗口中心为锚左右对称伸缩
    }

    // 每次重新显示时自愈：越界（如意外卡到任务栏下）自动拉回工作区
    onVisibleChanged: {
        if (root.visible)
            clampToWorkArea()
    }

    // 位置防抖保存（停止拖动 200ms 后落盘）
    // 只有“用户实际拖动过”才允许落盘：歌词随内容变宽/变窄引起的自动居中 x 平移、
    // 启动/显示时的越界钳位都会改 x/y，但都不该写配置——
    // 否则每次歌词宽度变化都会 saveComponentToken → 同步读写一次 profile 文件。
    property bool posMovedByUser: false
    Timer {
        id: posSaveTimer
        interval: 200
        onTriggered: {
            posMovedByUser = false
            Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland", "floatingX", root.x);
            Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland", "floatingY", root.y);
        }
    }
    onXChanged: if (posMovedByUser) posSaveTimer.restart()
    onYChanged: if (posMovedByUser) posSaveTimer.restart()

    // 居中延申：胶囊随歌词变宽/变窄时，保持窗口水平中心不动，左右对称扩展
    // （胶囊本身在窗口内即水平居中：x=padX，两侧留白相等）
    // _centerX 是窗口中心的浮点权威值（仅在用户拖动/启动定位/越界钳位时重新对齐），
    // 宽度变化时保持它不动、直接由它反推 x —— 中心即锚点，向两边对称伸缩。
    property real prevWidth: 0      // 上一次宽度（0 = 尚未初始化）
    property real _centerX: 0.0     // 窗口水平中心（浮点权威值）
    onWidthChanged: {
        var old = root.prevWidth
        root.prevWidth = root.width
        if (old <= 0) return        // 首次宽度就位，不当作变化
        var dw = root.width - old
        if (dw !== 0)
            _applyCenteredX()
    }
    // 以固定中心为锚反推窗口 x（round）：
    // 注意绝不能再对 _centerX 累加 dw/2 —— x = _centerX - width/2 本身已随宽度变化
    // 自动平移 dw/2，若再累加会造成双重修正 → x 整体平移 dw（右缘钉死、整体向左延申）。
    function _applyCenteredX() {
        root.x = Math.round(_centerX - root.width / 2)
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

    // 拖动区：覆盖整窗，未锁定且未穿透时用鼠标拖动窗口
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
                // 拖拽同样钳制在工作区内，杜绝拖进任务栏下面
                root.x = Math.max(0, Math.min(root.x + delta.x, Screen.desktopAvailableWidth - root.width))
                root.y = Math.max(0, Math.min(root.y + delta.y, Screen.desktopAvailableHeight - root.height))
                _centerX = root.x + root.width / 2   // 拖动后以实际 int x 为权威，重新对齐浮点中心
            }
        }
    }

// 歌词胶囊本体（悬浮模式：独立底色，不复用主窗口背景 blur）
    Oran7LyricsIsland {
        id: lyricsIsland
        x: root._centerX - root.x - (root.width - padX * 2) / 2
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
        layer.effect:DropShadow{
            id:windowContentCloseBtnDropShadow
            anchors.fill: lyricsIsland
            source: lyricsIsland
            z:lyricsIsland.z
            spread: 0.01
            samples: 15
            radius:lyricsIsland.height / 4
            horizontalOffset: 0
            verticalOffset: 0
            color:"gray"
            SequentialAnimation on color{
                id: shadowAnimation_inner
                running: false
                loops: Animation.Infinite
                property real transDuration: 2000

                PropertyAnimation { to: "#FFA500"; duration: shadowAnimation_inner.transDuration}
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
