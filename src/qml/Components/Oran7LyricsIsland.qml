import QtQuick 2.15
import QtQuick.Controls
import Client 1.0
import LyricsModel 1.0
import "../Basic"
import Oran7UI.Impl

Oran7BlurCard {
    id: root

    width: Math.max(islandMinWidth, Math.min(islandMaxWidth, currentLabelWidth + horizontalPadding * 2))
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
    property real suffixSlotWidth: 26         // 后缀槽位宽度：歌词右侧的后缀区域；使（歌词+后缀）作为整体块居中、两侧 padding 对称
    property int lyricGradientColorIndex: 0 // 旧配色链（曾驱动 Oran7GradientMask.colorItems）；改用 Oran7ShaderTextMask 后已无人消费，保留备用

    //后缀加载动画 // 新歌词加载到中间时，末尾依次显示 . o O 〇 ⁒ ? 循环 suffixStep步；播完后后缀槽位自动收回
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

    // ---- 显示状态机：空闲显示 "Oran7MediaClient"；切歌先显示歌曲名 1s，再显示歌词；无歌词则一直显示歌曲名 ----
    //  introActive  切歌后歌曲名介绍（1s）进行中
    //  titleText    非歌词状态显示的文字（当前歌曲名，无歌曲则默认占位）
    //  _displayLyrics  是否显示歌词（= 介绍结束 && 有歌词）
    property bool introActive: false
    property string titleText: BasicConfig.currentMediaName.length > 0
        ? BasicConfig.currentMediaName : "Oran7MediaClient"
    property bool _displayLyrics: !root.introActive && lyricsModel.hasLyrics
    property real introTargetWidth: 0   // 介绍期间的目标宽度

    Connections {
        target: BasicConfig
        function onCurrentMediaNameChanged() {
            if (BasicConfig.currentMediaName.length > 0) {
                root.introTargetWidth = titleFontMetrics.advanceWidth(BasicConfig.currentMediaName)
                root.introActive = true
                introTimer.restart()
            }
            else
            {
                root.introActive = false
                introTimer.stop()
            }
        }
    }

    Timer {
        id: introTimer
        interval: 1000
        onTriggered: { root.introActive = false }
    }

    Component.onCompleted: {
        // 启动时若有已恢复的播放歌曲：进入歌曲名介绍 → 1s 后显示歌词
        if (BasicConfig.currentMediaName.length > 0) {
            root.introTargetWidth = titleFontMetrics.advanceWidth(BasicConfig.currentMediaName)
            root.introActive = true
            introTimer.start()
        }
    }

    property real currentLabelWidth: root.introActive
        ? root.introTargetWidth
        : (root._displayLyrics
            ? Math.min(widthMeasurer.implicitWidth + (root.lyricSuffixActive ? root.suffixSlotWidth : 0), root.islandMaxWidth - root.horizontalPadding * 2)
            : titleFontMetrics.advanceWidth(root.titleText))

    // 胶囊目标宽度（不含动画，随 currentLabelWidth 即时变化）。
    // 歌词 Label 用它算稳态 x，避免用动画中的实际 width 与瞬间后缀项打架 → 发抖。
    readonly property real targetWidth: Math.max(
        islandMinWidth,
        Math.min(islandMaxWidth, currentLabelWidth + horizontalPadding * 2))

    // ---- islandVisible 控制 isLand整体 显隐动画 ----
    visible: opacity > 0.01
    opacity: islandVisible ? 1.0 : 0.0
    scale: islandVisible ? 1.0 : 0.8
    Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    // 宽度平滑动画（换句/后缀槽位增删/歌曲名↔歌词过渡时胶囊整体平滑伸缩）。
    // 歌曲名宽度用 FontMetrics 纯同步计算，切歌时宽度目标稳定、不闪变，
    // 因此可以全程开动画：切歌平滑缩到歌曲名宽，1s 后平滑展开到歌词宽，衔接丝滑。
    Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutQuad } }

    // ----  islandVisible 开关控制 ----
    function toggle() { islandVisible = !islandVisible }
    function open() { islandVisible = true }
    function close() { islandVisible = false }

    // ---- 歌词 数据模型 ----
    LyricsModel { id: lyricsModel }

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

        // === 歌曲名/默认占位（非歌词状态下显示：空闲="Oran7MediaClient"，播放=当前歌曲名） === //
        Oran7ShaderTextMask {
            anchors.fill: parent
            // 占位字只在「无歌词且非切歌介绍」时可见，其余时间不必跑 shader。
            // 用 opacity 而不是直接写条件，是为了让淡入淡出期间特效保持一致。
            effectEnabled: placeholderLabel.opacity > 0.01
                Text {
                id: placeholderLabel
                visible: opacity > 0.01
                opacity: (!root.introActive && !lyricsModel.hasLyrics) ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                // 与歌词 Label 同一套稳态定位：用胶囊“目标宽度”targetWidth 算 x，
                x: (root.targetWidth - placeholderLabel.width) / 2
                y: (parent.height - placeholderLabel.height) / 2
                width: titleFontMetrics.advanceWidth(root.titleText)
                text: root.titleText
                color: Oran7Theme.Oran7MusicPlayControls["iconColorBase-7"]
                font.pixelSize: root.lyricsFontSize
                font.family: "微软雅黑"
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        // === 隐藏的宽度测量 Label：胶囊宽度随当前显示文字（歌词行 / 歌曲名 / 占位）像素宽度伸缩 === //
        Text {
            id: widthMeasurer
            visible: false
            text: root._displayLyrics ? lyricsModel.currentText : root.titleText
            font.pixelSize: root.lyricsFontSize   // 与 delegate 歌词字号一致，保证胶囊宽度精确匹配
            font.family: "微软雅黑"
            font.bold: true
        }

        // 歌曲名/占位宽度用 FontMetrics 纯同步计算：隐藏 Text 的 implicitWidth 要到下一帧
        // 才更新，切歌瞬间宽度目标会经历"旧值→新值"两帧 → 即便关闭动画也会快速跳两次 → 抖动。
        FontMetrics {
            id: titleFontMetrics
            font.pixelSize: root.lyricsFontSize
            font.bold: true
            font.family: "微软雅黑"
        }

        // === 上下滚动歌词  === //
        ListView {
            id: lyricList
            visible: opacity > 0.01
            opacity: root._displayLyrics ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
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
                if (lyricsModel.currentLine < 0) return contentY
                var targetY = lyricsModel.currentLine * lineH - height / 2 + lineH / 2
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
                target: lyricsModel
                function onCurrentLineChanged() {
                    if (!lyricsModel.hasLyrics || lyricsModel.currentLine < 0) return
                    var targetY = lyricList.targetYForCurrentLine()
                    var delta = Math.abs(targetY - lyricList.contentY)
                    scrollAnim.stop()
                    if (delta > lyricList.lineH * 1.5) {
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
                    if (lyricsModel.hasLyrics && lyricsModel.currentLine >= 0)
                        lyricList.contentY = lyricList.targetYForCurrentLine()
                }
            }
            Component.onCompleted: {
                if (lyricsModel.hasLyrics && lyricsModel.currentLine >= 0)
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
                // 真实歌词完整宽度（隐藏 Text 的 implicitWidth，与 Label 渲染精确一致；封顶胶囊内宽）
                // 固定标签宽度，防止溶解时整体左右窜动；也不用 FontMetrics（与渲染有亚像素差会导致末尾被 elide 成 ...）
                readonly property real _textWidth: Math.min(lyricTextMeasurer.implicitWidth, width)

                // 新行加载（delegate 复用）→ 重置为全 $$$ 占位；若本行即当前行则立即开始溶解
                onTextChanged: {
                    revealAnim.stop()
                    revealProgress = 0
                    updateDisplayText()
                    if (lineIndex === lyricsModel.currentLine)
                        startReveal()
                }
                // 行索引就位（与 onTextChanged 顺序不定，双保险，避免误判）
                onLineIndexChanged: {
                    revealAnim.stop()
                    revealProgress = 0
                    updateDisplayText()
                    if (lineIndex === lyricsModel.currentLine)
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
                    target: lyricsModel
                    function onCurrentLineChanged() {
                        if (lyricDelegate.lineIndex === lyricsModel.currentLine)
                            lyricDelegate.startReveal()
                    }
                }

                // 字体度量：用于生成像素宽度匹配的 $$$ 占位串
                FontMetrics {
                    id: lyricFontMetrics
                    font.pixelSize: root.lyricsFontSize
                    font.bold: true
                    font.family: "微软雅黑"
                }
                // 隐藏的测量 Text：implicitWidth 与 Label 实际渲染宽度一致，用于标签宽度（避免末尾被 elide 成 ...）
                Text {
                    id: lyricTextMeasurer
                    visible: false
                    text: lyricDelegate.text
                    font.pixelSize: root.lyricsFontSize
                    font.bold: true
                    font.family: "微软雅黑"
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
                    effectEnabled: Math.abs(lineIndex - lyricsModel.currentLine) <= 1
                    Label {
                        id: mainLyricLabel
                        // （歌词+当前行后缀槽位）作为整体块居中，歌词锚定在块左侧。
                        // 槽位随后缀播放动态预留/收回，但歌词 x 用“目标宽度”算稳态位置（恒为左留白 P）
                        // → 不随溶解/后缀窜动；若用动画中的 parent.width 算，宽度动画与瞬间后缀项
                        // 不一致会导致歌词左右发抖。
                        x: (root.targetWidth - lyricDelegate._textWidth
                             - (lyricDelegate.lineIndex === lyricsModel.currentLine && root.lyricSuffixActive ? root.suffixSlotWidth : 0)) / 2
                        width: lyricDelegate._textWidth
                        height: parent.height
                        text: lyricDelegate.displayText
                        textFormat: Text.RichText   // 逐字符 alpha 需要富文本（<font color="#AARRGGBB">）
                        color: "black"   // 遮罩源
                        font.pixelSize: root.lyricsFontSize
                        font.bold: true
                        font.family: "微软雅黑"
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
                        text: (lyricDelegate.lineIndex === lyricsModel.currentLine) ? root.suffixChar : ""
                        color: "black"   // 遮罩源
                        font.pixelSize: root.lyricsFontSize
                        font.bold: true
                        font.family: "微软雅黑"
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        // === 切歌歌曲名介绍覆盖层：1s 内盖在歌词上方显示歌曲名（声明在最后 = 最上层） === //
        // 与占位/歌词一致，走 Oran7ShaderTextMask；纯 Text + FontMetrics 同步宽度，不抖
        Oran7ShaderTextMask {
            anchors.fill: parent
            // 切歌介绍层只在 introActive 的 1 秒里可见，其余时间不必跑 shader
            effectEnabled: introLabel.opacity > 0.01
            z: 10
            Text {
                id: introLabel
                visible: opacity > 0.01
                opacity: root.introActive ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                // 与歌词 Label 同一套稳态定位：用胶囊“目标宽度”targetWidth 算 x，
                // 宽度动画期间位置恒定 → 不抖；而非 anchors 依赖实时父宽度
                x: (root.targetWidth - introLabel.width) / 2
                y: (parent.height - introLabel.height) / 2
                width: titleFontMetrics.advanceWidth(root.titleText)
                text: BasicConfig.currentMediaName
                color: Oran7Theme.Oran7MusicPlayControls["iconColorBase-7"]
                font.pixelSize: root.lyricsFontSize
                font.family: "微软雅黑"
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
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
        function onPlayProgressUpdated(CurPos, CurTime) {
            if (BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_MusicPlayerIndex)
                lyricsModel.updateTime(CurTime)
        }
    }

    // === 当前行变化 → 播放后缀加载动画 === //
    Connections {
        target: lyricsModel
        function onCurrentLineChanged() {
            if (lyricsModel.currentLine >= 0)
                root.startSuffixAnimation()
        }
    }
}
