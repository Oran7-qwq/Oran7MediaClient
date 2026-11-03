import QtQuick 2.15
import QtQuick.Controls
import Client 1.0
import LyricsModel 1.0
import "../../Basic"
import Oran7UI.Impl

Oran7BlurCard {
    id: root

    width: targetWidth
    height: 52
    z: 999

    // =============== public properties =================== //
    property bool islandVisible: false     // 灵动岛可见性
    property bool locked: false              // 桌面歌词锁定（future）
    property bool dragable: false           // 桌面歌词可拖动（future）
    property int lyricsFontSize: 20          // 歌词字体大小

    // 悬浮窗模式：脱离主窗口、独立成窗（桌面歌词）。此时无法复用主窗口背景做 blur，
    // 改用下方半透明胶囊底色兜底，保证在任何桌面上歌词都可读。
    property bool floatingMode: false
    property real floatingOpacity: 0.9      // 悬浮模式胶囊底色不透明度（0=纯文字无底）
    property color _floatingBackdropColor: Oran7Theme.Oran7MusicLyricsIsland["floatingBackdropColor-6"]   // 悬浮模式胶囊底色（主题可配）
    property string backgroundImage: ""     // 悬浮模式背景图片（完整填充胶囊，圆角裁剪；空=不显示）
    property bool enableBackGroundImage: Oran7Theme.Oran7MusicLyricsIsland.backgroundImageEnabled

    // 胶囊宽度随歌词像素宽度动态伸缩
    property real islandMinWidth: 120      // 最小胶囊宽（极短/空歌词兜底）
    property real islandMaxWidth: 1000      // 最大胶囊宽（超长歌词截断）
    property real horizontalPadding: 34    // 胶囊左右单边留白（歌词到胶囊边缘）
    property real suffixSlotWidth: 26         // 后缀使用右侧留白，不改变歌词的居中位置和胶囊宽度
    property int lyricGradientColorIndex: 0 // 旧配色链（曾驱动 Oran7GradientMask.colorItems）；改用 Oran7ShaderTextMask 后已无人消费，保留备用

    // 后缀在右侧留白独立播放，不参与胶囊尺寸或文字定位。
    readonly property var suffixChars: [".", "o", "O", "〇", " ※ ", "？"]
    property int suffixStep: 15                    //setp前进总步数

    readonly property string suffixChar: cu_suffixStep < suffixStep ? suffixChars[cu_suffixStep % 6] : "" // 当前后缀字符引用
    property bool lyricSuffixActive: cu_suffixStep < suffixStep && lyricsModel.hasLyrics           // 后缀播放中 → 槽位预留
    property int cu_suffixStep : 0               // 当前后缀前进步index

    // ================ private =====================//

    function startSuffixAnimation() {
        suffixTimer.stop()
        cu_suffixStep = 0
        suffixTimer.start()
    }

    // 后缀循环定时器 300ms/step，suffixStep 步后自动停止
    Timer {
        id: suffixTimer
        interval: 300
        repeat: true
        onTriggered: {
            if (root.cu_suffixStep >= suffixStep) {
                stop()
                // 一句歌词加载完成 → 渐变色前进一格，下一句从上一句的下一个颜色续接
                root.lyricGradientColorIndex += 1
                return
            }
            root.cu_suffixStep++
        }
    }

    // ===== Oran7BlureCard Settings ===== //
    blurSource: floatingMode ? null : (BasicConfig.mainWindow ? BasicConfig.mainWindow.__Oran7WindowBackGround__ : null)
    blurEnabled: !floatingMode && Oran7Theme.Oran7MusicPlayControls.blurEffectEnabled
    saturation: Oran7Theme.Oran7MusicPlayControls.saturation
    brightness: Oran7Theme.Oran7MusicPlayControls.brightness
    contrast: Oran7Theme.Oran7MusicPlayControls.contrast
    themeColor: "#04FFFFFF"
    borderRadius: height / 2
    borderWidth: 1
    backGroundColor: root._floatingBackdropColor

    readonly property string titleText: presentation.titleText
    readonly property bool _displayLyrics: presentation.lyricsVisible
    // 统一交接进度，淡化与纵向微动同拍；不对文字做横向缩放。
    property real lyricsBlend: _displayLyrics ? 1 : 0
    Behavior on lyricsBlend { NumberAnimation { duration: 520; easing.type: Easing.InOutCubic } }
    readonly property int displayedLine: presentation.displayedLine
    property real currentLabelWidth: root._displayLyrics
        ? Math.min(Math.ceil(titleFontMetrics.advanceWidth(lyricsModel.textAt(root.displayedLine))) + 2, root.islandMaxWidth - root.horizontalPadding * 2)
        : titleFontMetrics.advanceWidth(root.titleText)

    Oran7LyricsIslandState {
        id: presentation
        model: lyricsModel
        songTitle: BasicConfig.currentMediaName
        artist: BasicConfig.currentMediaArtistAuthor
        trackKey: String(Client.playback.currentTrack.songId) + "|" + BasicConfig.currentMediaFilePath
        positionSeconds: BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_MusicPlayerIndex
            ? Client.playback.positionMs / 1000 : 0
        onLyricsVisibleChanged: {
            if (!lyricsVisible) {
                suffixTimer.stop()
                root.cu_suffixStep = root.suffixStep
            }
        }
    }

    // 目标尺寸只驱动胶囊动画；文字随实际宽度居中，避免目标变化时先跳位。
    readonly property real targetWidth: Math.max(
        islandMinWidth,
        Math.min(islandMaxWidth, currentLabelWidth + horizontalPadding * 2))

    // ---- islandVisible 控制 isLand整体 显隐动画 ----
    visible: opacity > 0.01
    opacity: islandVisible ? 1.0 : 0.0
    scale: islandVisible ? 1.0 : 0.8
    Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    // 宽度平滑动画（换句/歌曲名↔歌词过渡时仅胶囊外壳伸缩）。
    // 歌曲名宽度用 FontMetrics 纯同步计算，切歌时宽度目标稳定、不闪变，
    // 因此可以全程开动画：歌曲信息与歌词按播放时间切换并平滑调整宽度，衔接丝滑。
    Behavior on width { NumberAnimation { duration: 520; easing.type: Easing.InOutCubic } }

    // ----  islandVisible 开关控制 ----
    function toggle() { islandVisible = !islandVisible }
    function open() { islandVisible = true }
    function close() { islandVisible = false }

    // ---- 歌词 数据模型 ----
    LyricsModel { id: lyricsModel; skipEmptyLines: true }

    Rectangle {
        visible: root.floatingMode && root.floatingOpacity > 0
        anchors.fill: parent
        radius: height / 2
        color: "transparent"
        border.color: "transparent"
        border.width: 0
        z: 0
        Oran7RoundedImage {
            anchors.fill: parent
            source: root.backgroundImage
            radius: parent.height / 2
            tiled: true
            tileAreaWidth: 1600
            autoPan: true
            visible: root.enableBackGroundImage && root.backgroundImage.length > 0
        }
        Rectangle {
            anchors.fill: parent
            radius: root.height/2
            color: root.enableBackGroundImage && root.backgroundImage.length > 0 ? "transparent" : root._floatingBackdropColor
            opacity: root.floatingOpacity
        }
    }

    // === Lyrics Content ===
    Item {
        id: textContainer
        anchors.fill: parent
        clip: true
        z: 1

        // 固定排版画布：外壳动画只影响裁剪范围，不再让每行文字反复重排。
        Item {
            id: textStage
            objectName: "islandTextStage"
            width: root.islandMaxWidth
            height: parent.height
            anchors.centerIn: parent

        // 无曲目时显示默认标题；前奏、无歌词和换句空闲时显示「歌名 - 作者」。
        Oran7ShaderTextMask {
            anchors.fill: parent
            // 歌曲信息只在歌词区空闲时可见，其余时间不必跑 shader。
            // 用 opacity 而不是直接写条件，是为了让淡入淡出期间特效保持一致。
            effectEnabled: placeholderLabel.opacity > 0.01
                Text {
                id: placeholderLabel
                objectName: "islandIdleLabel"
                visible: opacity > 0.01
                opacity: Math.pow(1 - root.lyricsBlend, 2)
                transform: Translate {
                    y: -3 * root.lyricsBlend
                }
                // 标题也使用固定排版框；换歌只替换内容，不再改 Text 的 x/width。
                anchors.fill: parent
                text: root.titleText
                color: Oran7Theme.Oran7MusicPlayControls["iconColorBase-7"]
                font.pixelSize: root.lyricsFontSize
                font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        // 歌曲名/占位宽度用 FontMetrics 纯同步计算：隐藏 Text 的 implicitWidth 要到下一帧
        // 才更新，切歌瞬间宽度目标会经历"旧值→新值"两帧 → 即便关闭动画也会快速跳两次 → 抖动。
        FontMetrics {
            id: titleFontMetrics
            font.pixelSize: root.lyricsFontSize
            font.bold: true
            font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
        }

        // === 上下滚动歌词  === //
        ListView {
            id: lyricList
            objectName: "islandLyricList"
            visible: opacity > 0.01
            opacity: Math.pow(root.lyricsBlend, 2)
            transform: Translate {
                y: 3 * (1 - root.lyricsBlend)
            }
            anchors.fill: parent
            interactive: false            // 禁止滑动
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            model: lyricsModel

            // 行高略小于胶囊高度：给邻居行留出滑入/滑出的空间；
            // 邻居在边缘焦点≈0 → 完全透明，滚动完成后不残留（见 delegate 的 opacity 处理）
            property real lineH: 40
            topMargin: height / 2
            bottomMargin: height / 2

            // 当前行居中时的目标 contentY（与 Oran7LyricsView 同一套公式）
            function targetYForCurrentLine() {
                if (root.displayedLine < 0) return contentY
                var targetY = root.displayedLine * lineH - height / 2 + lineH / 2
                var minY = -topMargin
                var maxY = Math.max(minY, contentHeight - height + bottomMargin)
                return Math.max(minY, Math.min(targetY, maxY))
            }

            // 平滑滚动动画（OutCubic：先快后缓，滚动自然不僵硬）
            NumberAnimation {
                id: scrollAnim
                target: lyricList
                property: "contentY"
                duration: 500
                easing.type: Easing.OutCubic
            }

            // 当前行变化 → 平滑上滚到该行居中；跨行跳跃（启动/切歌/拖进度条）直接落位不滚动
            Connections {
                target: presentation
                function onLyricActivated() {
                    if (!lyricsModel.hasLyrics || root.displayedLine < 0) return
                    var targetY = lyricList.targetYForCurrentLine()
                    var delta = Math.abs(targetY - lyricList.contentY)
                    scrollAnim.stop()
                    if (root.lyricsBlend < 0.01 || delta > lyricList.lineH * 1.5) {
                        lyricList.contentY = targetY
                    } else {
                        scrollAnim.from = lyricList.contentY
                        scrollAnim.to = targetY
                        scrollAnim.start()
                    }
                }
            }

            // 歌词就位时若当前行已有效（如切歌后新建胶囊），直接落位不滚动
            Connections {
                target: lyricsModel
                function onHasLyricsChanged() {
                    if (lyricsModel.hasLyrics && root.displayedLine >= 0)
                        lyricList.contentY = lyricList.targetYForCurrentLine()
                }
            }
            Component.onCompleted: {
                if (lyricsModel.hasLyrics && root.displayedLine >= 0)
                    contentY = targetYForCurrentLine()
            }

            delegate: Item {
                id: lyricDelegate
                required property string text
                required property int lineIndex
                width: lyricList.width
                height: lyricList.lineH
                clip: true   // 裁剪到本行高度，渐变遮罩也不允许越界

                // 距可视中心距离 → 焦点值 _focus∈[0,1]（中心=1，边缘=0）
                // 用 scale+opacity 连续过渡（GPU 变换，不重新排版文字 → 滚动流畅不卡顿）：
                // 邻居进入时由小变大、由透明到全亮；离开时反向缩小淡出；
                // 停在边缘时焦点≈0 → 完全透明，滚动完成后不残留、不外溢
                readonly property real _viewCenter: lyricList.contentY + lyricList.height / 2
                readonly property real _selfCenter: y + height / 2
                readonly property real _normDist: Math.min(
                    Math.abs(_selfCenter - _viewCenter) / (lyricList.height / 2), 1.0)
                readonly property real _focus: 1.0 - _normDist
                scale: 0.6 + 0.4 * _focus
                opacity: Math.max(0.0, (_focus - 0.08) / 0.92)   // 边缘裁成完全透明
                transformOrigin: Item.Center

                // === 溶解过渡（逐元素操作字符串，单层无重叠） === //
                // displayText = 左侧已揭示的真实字符 + 右侧 $$$ 占位（占位宽度匹配剩余歌词像素宽度）
                // 每次成为当前行都会重放溶解（含 seek 回来）；节奏用 OutCubic（先快后慢）
                property real revealProgress: 0.0   // 溶解进度 0..1（由动画驱动）
                property string displayText: ""     // 最终显示字符串
                // 同步测量并封顶到固定画布；不受胶囊动画、后缀或逐字溶解影响。
                // 向上取整并留 2px，避免亚像素笔画被裁掉。
                readonly property real _textWidth: Math.min(Math.ceil(lyricFontMetrics.advanceWidth(text)) + 2,
                    root.islandMaxWidth - root.horizontalPadding * 2)

                // 新行加载（delegate 复用）→ 重置为全 $$$ 占位；若本行即当前行则立即开始溶解
                onTextChanged: {
                    revealAnim.stop()
                    revealProgress = 0
                    updateDisplayText()
                    if (lineIndex === root.displayedLine)
                        startReveal()
                }
                // 行索引就位（与 onTextChanged 顺序不定，双保险，避免误判）
                onLineIndexChanged: {
                    revealAnim.stop()
                    revealProgress = 0
                    updateDisplayText()
                    if (lineIndex === root.displayedLine)
                        startReveal()
                }

                // 溶解进度变化 → 重建显示字符串（已揭示字符数 = floor(进度 × 总字符数)）
                onRevealProgressChanged: updateDisplayText()

                // 生成：已揭示真实字符 + 右侧 $$$ 占位（富文本，逐个 $ 的透明度从 1 渐降到 0.4）
                // 用 floor：字符铺满整个溶解时长；OutCubic 开头起步快，首字符等待可忽略
                function updateDisplayText() {
                    var chars = Math.min(text.length, Math.floor(revealProgress * text.length))
                    var revealed = text.substring(0, chars)
                    var remaining = text.substring(chars)
                    // 富文本转义已揭示部分（含 & < >）
                    var escapedRevealed = revealed.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
                    displayText = escapedRevealed + dollarSkeletonRich(remaining)
                }

                // 生成像素宽度匹配的 $$$ 串：主渐变 1→0.4（前 count-3 个），末尾 3 个 0.4→0.2→0 平滑收尾
                // 用富文本 <font color="#AARRGGBB"> 控制逐字符 alpha；渐变遮罩 shader 只取文字 alpha
                function dollarSkeletonRich(str)
                {
                    if (!str || str.length === 0) return ""
                    var w = lyricFontMetrics.advanceWidth(str)
                    var dw = Math.max(lyricFontMetrics.advanceWidth("$"), 1)
                    var count = Math.max(1, Math.ceil(w / dw))
                    var s = ""
                    for (var i = 0; i < count; i++) {
                        var alpha
                        if (count <= 2) {
                            // 占位太短：1→0.4，不淡到 0（否则整段不可见）
                            alpha = 1.0 - 0.6 * (i / Math.max(1, count - 1))
                        } else {
                            var tailStart = count - 3   // 末尾 3 个做平滑淡出
                            if (i >= tailStart) {
                                // 末尾 3 个：0.4 → 0.2 → 0（线性收尾，与主渐变 0.4 无缝衔接）
                                alpha = 0.4 * (count - 1 - i) / 2
                            } else {
                                // 主渐变 1 → 0.4（前 count-3 个）
                                var t = i / Math.max(1, tailStart - 1)
                                alpha = 1.0 - 0.6 * t
                            }
                        }
                        var aHex = Math.round(alpha * 255).toString(16)
                        if (aHex.length < 2) aHex = "0" + aHex
                        s += '<font color="#' + aHex + '000000">$</font>'
                    }
                    return s
                }

                // 每次成为当前行都重放溶解：重置为 $$$ 占位再按 OutCubic 逐字揭示（seek 回来也会再溶解）
                function startReveal() {
                    revealAnim.stop()
                    // 从歌曲信息切入时只做一次整句交接，避免滚动/溶解/缩放同时叠加。
                    if (root.lyricsBlend < 0.01) {
                        revealProgress = 1
                        return
                    }
                    revealAnim.duration = Math.min(2400,Math.max(500, text.length * 50))   // 时长随字符数伸缩
                    revealProgress = 0
                    revealAnim.start()
                }

                // 溶解动画：OutCubic 节奏（先快后慢，等价 QEasingCurve::OutCubic）
                // 注意：不要换成 InOutQuint/InOutCubic 这类"两端缓"的曲线——它们开头平缓会空等、中间陡峭溶解飞快
                NumberAnimation {
                    id: revealAnim
                    target: lyricDelegate
                    property: "revealProgress"
                    from: 0
                    to: 1
                    duration: 400//default value
                    easing.type: Easing.OutQuad
                }

                // 当前行变化 → 本行成为当前行时触发溶解
                Connections {
                    target: presentation
                    function onLyricActivated() {
                        if (lyricDelegate.lineIndex === root.displayedLine)
                            lyricDelegate.startReveal()
                    }
                }

                // 字体度量：用于生成像素宽度匹配的 $$$ 占位串
                FontMetrics {
                    id: lyricFontMetrics
                    font.pixelSize: root.lyricsFontSize
                    font.bold: true
                    font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                }
                // 歌词文字 shader 特效（Oran7ShaderTextMask；遮罩源只取 alpha，黑/白均可）
                // 换特效只需替换 shaders/<name>.frag，颜色不再由 QML 决定。
                // 注：root.lyricGradientColorIndex 那条旧配色链已无人消费（保留备用）。
                Oran7ShaderTextMask {
                    anchors.fill: parent
                    // 只为「当前行 ±1」这 3 行开特效。
                    // ListView 的 cacheBuffer 默认约 320px，而 lineH=40 → 上下各缓存 8 行，
                    // 约 18 个 delegate 会同时存在；每个只要开着 layer 就是一个离屏 FBO，
                    // 加上每帧一次的完整 shader 运算（这个 fractal 是 19 次迭代）—— 全是白烧。
                    // 胶囊可视区其实只有约 1.3 行，其余行根本看不见。
                    //
                    // ±1 的提前量是必要的：某行在成为 current-1 时就已经开启，
                    // 等它滚进可视区时特效早已就位，不会有「先黑一下再上色」的跳变。
                    effectEnabled: Math.abs(lineIndex - root.displayedLine) <= 1
                    Label {
                        id: mainLyricLabel
                        objectName: "islandLyricText"
                        readonly property int lyricLine: lyricDelegate.lineIndex
                        // 每行各自居中；后缀在右侧留白独立播放，换句时旧行也不会被重新挤动。
                        x: (lyricDelegate.width - lyricDelegate._textWidth) / 2
                        width: lyricDelegate._textWidth
                        height: parent.height
                        text: lyricDelegate.displayText
                        textFormat: Text.RichText   // 逐字符 alpha 需要富文本（<font color="#AARRGGBB">）
                        color: "black"   // 遮罩源
                        font.pixelSize: root.lyricsFontSize
                        font.bold: true
                        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideNone   // 不显示省略号；超出部分靠 clip 裁掉（避免末尾变 ...）
                        clip: true
                    }
                    // 后缀加载符号：当前行显示 root 统一管理的后缀字符；位于槽位内、槽位内居中
                    Label {
                        id: suffixLabel
                        x: mainLyricLabel.x + lyricDelegate._textWidth + (root.suffixSlotWidth - width) / 2
                        y: (parent.height - height) / 2
                        text: (lyricDelegate.lineIndex === root.displayedLine) ? root.suffixChar : ""
                        color: "black"   // 遮罩源
                        font.pixelSize: root.lyricsFontSize
                        font.bold: true
                        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        }
    }

    // === 歌词数据驱动 === //
    Connections {
        target: Client
        function onLyricsAvailable(lrcFilePath) {
            lyricsModel.loadFromFile(lrcFilePath)
        }
        function onLyricsUnavailable() {
            lyricsModel.clearLyrics()
        }

    }

    // === 当前行变化 → 播放后缀加载动画 === //
    Connections {
        target: presentation
        function onLyricActivated() {
            if (root.displayedLine >= 0)
                root.startSuffixAnimation()
        }
    }
}
