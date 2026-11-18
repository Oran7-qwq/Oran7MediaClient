import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../Oran7SettingUiWindowItems"
import Oran7UI.Impl

Pane {
    id: root
    padding: 10
    readonly property real sourcePanelWidth: 230
    readonly property real outputPanelWidth: 260
    readonly property real minimumCenterWidth: 480
    readonly property real outputCardHeight: 96
    font.family: root.fontFamily
    font.pixelSize: root.textPixelSize
    required property var service
    required property var controller
    property int processCpuPercent: -1
    property int processGpuPercent: -1
    readonly property bool liveSupported: !!controller && typeof controller.startLive === "function"
    property var liveDraft: ({})
    property bool audioDirty: false
    property bool liveDirty: false
    property bool flushingPendingSettings: false
    property string liveMessage: ""
    property bool audioMonitoringActive: visible && (!Window.window ||
        (Window.window.visible && Window.window.visibility !== Window.Minimized && Window.window.opacity > 0))
    property bool audioMonitoringMounted: false
    function syncAudioMonitoring() {
        if (audioMonitoringMounted && controller && typeof controller.setAudioMonitoringEnabled === "function")
            controller.setAudioMonitoringEnabled(audioMonitoringActive)
    }
    onAudioMonitoringActiveChanged: syncAudioMonitoring()
    Component.onCompleted: {
        if (liveSupported) liveDraft = Object.assign({}, controller.liveOptions)
        audioMonitoringMounted = true
        syncAudioMonitoring()
    }
    Component.onDestruction: {
        if (audioMonitoringMounted && controller && typeof controller.setAudioMonitoringEnabled === "function")
            controller.setAudioMonitoringEnabled(false)
    }
    function editLive(key,value) {
        var next = Object.assign({},liveDraft); next[key] = value
        liveDraft = next; liveDirty = true; liveSave.restart()
    }
    function audioValues(source) {
        return {audioEnabled:!!source.audioEnabled, microphoneEnabled:!!source.microphoneEnabled,
            systemVolume:source.systemVolume === undefined ? 100 : source.systemVolume,
            microphoneVolume:source.microphoneVolume === undefined ? 100 : source.microphoneVolume}
    }
    function editLiveAudio(key, value) {
        var next = Object.assign({}, liveDraft); next[key] = value
        var result = controller.previewAudioSettings(audioValues(next))
        if (!result.ok) { pageState.message = result.msg || ""; return }
        liveDraft = next; audioDirty = true; audioSave.restart()
    }
    function saveLiveAudio() {
        audioSave.stop()
        if (!audioDirty) return true
        var result = controller.applyAudioSettings(audioValues(liveDraft))
        if (!result.ok) {
            pageState.message = result.msg || ""
            var saved = audioValues(controller.audioOptions)
            controller.previewAudioSettings(saved)
            liveDraft = Object.assign({}, liveDraft, saved)
        }
        if (result.ok) liveDraft = Object.assign({}, liveDraft, audioValues(controller.audioOptions))
        audioDirty = false
        return result.ok
    }
    Connections {
        target: root.controller
        ignoreUnknownSignals: true
        function onLiveSettingsChanged() {
            if (!root.audioDirty && root.controller.audioOptions)
                root.liveDraft = Object.assign({}, root.liveDraft, root.audioValues(root.controller.audioOptions))
        }
    }
    Timer { id: audioSave; interval: 2000; onTriggered: root.saveLiveAudio() }
    function saveLive() {
        liveSave.stop()
        if (!root.saveLiveAudio()) return false
        if (!liveDirty) return true
        var result = controller.applyLiveSettings(liveDraft)
        liveMessage = result.msg || ""
        if (!result.ok) {
            pageState.message = liveMessage
            console.warn("[RTMP] " + liveMessage)
        }

        if (result.ok) { liveDirty = false; liveDraft = Object.assign({},controller.liveOptions) }
        return result.ok
    }
    Timer { id: liveSave; interval: 2000; onTriggered: root.saveLive() }
    function flushPendingSettings() {
        if (flushingPendingSettings) return true
        flushingPendingSettings = true
        audioSave.stop()
        liveSave.stop()
        // 三组独立提交；无效的直播地址不能阻止有效的音量和本地输出设置保存。
        var audioOk = saveLiveAudio()
        var outputOk = pageState.flushOutputForExit()
        var liveOk = !liveDirty || saveLive()
        flushingPendingSettings = false
        var ok = audioOk && outputOk && liveOk
        if (!ok) console.warn("[RecordingSettings] 退出配置补交未全部成功，保留最后成功保存的配置")
        return ok
    }
    Connections {
        target: root.Window.window
        function onClosing(close) { root.flushPendingSettings() }
    }
    Connections {
        target: root.controller
        ignoreUnknownSignals: true
        function onSettingsFlushRequested() { root.flushPendingSettings() }
    }
    readonly property bool violetSkin: Oran7Theme.Oran7ScreenCapture.workspaceSkin === "violet"
    property Component previewComponent
    // 工作台自行管理样式，调用方可覆盖；不借用下拉设置窗口的样式单例。
    property color panelColor: root.violetSkin ? "#9de5ebfa" : "#252830"
    property color baseColor: root.violetSkin ? "#bbc9e6" : "#191b21"
    property color textColor: root.violetSkin ? "#24304d" : "#e8ebf2"
    property color accentColor: root.violetSkin ? "#514887" : "#89adfa"
    property color selectionColor: root.violetSkin ? Qt.alpha(root.accentColor, 0.12) : "#3459a9"
    property color actionColor: root.violetSkin ? root.groupColor : "#363b47"
    property real materialStrength: root.violetSkin ? 0.018 : 0.006
    property real motifStrength: 0.035
    property real avatarParallaxPadding: 30
    property bool avatarParallaxEnabled: true
    property real avatarOpacity: root.violetSkin ? 0.52 : 0.28
    property color avatarTintColor: root.violetSkin ? "#51436f" : root.decorationColor
    property color decorationColor: root.violetSkin ? "#b4bbdb" : "#a0a9ba"
    property color paperColor: root.violetSkin ? "#c7c4e7" : "#3e4553"
    property color paperInkColor: root.violetSkin ? "#3c3a65" : "#e0e5ef"
    property color recordingAccentColor: root.violetSkin ? "#85517f" : "#f0878f"
    property color liveAccentColor: root.violetSkin ? "#466d85" : "#62cd8c"
    property color hoverColor: root.violetSkin ? "#cbd7ef" : "#444b5b"
    property color groupColor: root.violetSkin ? "#d2d4ed" : "#2d313a"
    property color inputColor: root.violetSkin ? "#edf2fc" : "#1b1e25"
    property color borderColor: root.violetSkin ? "#a1a9ca" : "#3d4350"
    property color mutedTextColor: root.violetSkin ? "#4a5576" : "#aeb7c7"
    property real controlHeight: 34
    property real controlPixelSize: 13
    // 这些标志只控制自定义输入区的展开，不复制设置草稿。
    property bool customCaptureFps: false
    property bool customOutputFps: false
    property bool customOutputSize: false
    property bool customBitrate: false
    readonly property var bitratePresets: [
        {kbps: 4000, title: qsTranslate("Oran7Ui", "4 Mbps · 节省空间"), description: qsTranslate("Oran7Ui", "文件较小，适合静态演示；快速运动细节可能较少")},
        {kbps: 8000, title: qsTranslate("Oran7Ui", "8 Mbps · 日常均衡"), description: qsTranslate("Oran7Ui", "日常桌面与课程录制，兼顾清晰度和文件体积")},
        {kbps: 12000, title: qsTranslate("Oran7Ui", "12 Mbps · 清晰优先"), description: qsTranslate("Oran7Ui", "为文字与动态画面保留更多细节，文件体积适中")},
        {kbps: 20000, title: qsTranslate("Oran7Ui", "20 Mbps · 高画质"), description: qsTranslate("Oran7Ui", "适合游戏与复杂画面，细节余量更多，文件较大")},
        {kbps: 40000, title: qsTranslate("Oran7Ui", "40 Mbps · 精细录制"), description: qsTranslate("Oran7Ui", "适合高分辨率或复杂动态画面，磁盘占用较高")},
        {kbps: 60000, title: qsTranslate("Oran7Ui", "60 Mbps · 高码率"), description: qsTranslate("Oran7Ui", "为复杂细节保留更多余量，文件很大，画质提升可能有限")},
        {kbps: null, title: qsTranslate("Oran7Ui", "自定义码率"), description: qsTranslate("Oran7Ui", "手动输入 0.1～200 Mbps，保留精确设置")}
    ]
    readonly property int bitratePresetIndex: {
        if (!customBitrate)
            for (var i = 0; i < bitratePresets.length - 1; ++i)
                if (bitratePresets[i].kbps === root.pageState.outputDraft.bitrateKbps) return i
        return bitratePresets.length - 1
    }
    readonly property string bitrateStorageEstimate: {
        var kbps = root.pageState.outputDraft.bitrateKbps
        if (typeof kbps !== "number" || !isFinite(kbps) || kbps <= 0) return qsTranslate("Oran7Ui", "请输入有效码率以估算文件体积")
        var gb = kbps * 0.00045
        return qsTranslate("Oran7Ui", "约 ") + (gb >= 1 ? gb.toFixed(1) + " GB" : Math.round(kbps * 0.45) + " MB") + qsTranslate("Oran7Ui", "/小时 · 仅视频估算")
    }
    readonly property int outputSizeIndex: {
        if (customOutputSize) return 3
        var w = root.pageState.outputDraft.width, h = root.pageState.outputDraft.height
        if (w === 0 && h === 0) return 0
        if (w === 1920 && h === 1080) return 1
        if (w === 1280 && h === 720) return 2
        return 3
    }
    property color previewBackgroundColor: root.violetSkin ? "#27324a" : "#101216"
    property color previewTextColor: root.textColor
    property color errorColor: root.violetSkin ? "#a13658" : "#f38d98"
    property color toggleOffColor: root.violetSkin ? root.paperColor : root.inputColor
    property color toggleBorderColor: root.violetSkin ? "#8d7ca7" : root.borderColor
    property color toggleThumbColor: root.violetSkin ? root.accentColor : root.textColor
    property real toggleHeight: 20
    property string fontFamily: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
    property real textPixelSize: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).pixelSize > 0 ? (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).pixelSize : 16
    property alias pageState: state
    readonly property var previewSource: {
        var list = state.monitors
        for (var i = 0; i < list.length; ++i)
            if (list[i].outputIndex === state.captureSaved.outputIndex) return list[i]
        return ({})
    }
    property bool showScreenInfo: false
    property bool fillPreview: Oran7Theme.Oran7ScreenCapture.ScaleMode === "Fill"
    signal previewScaleChanged(bool fill)
    palette.window: root.panelColor
    palette.base: root.baseColor
    palette.text: root.textColor
    palette.windowText: root.textColor
    palette.buttonText: root.textColor
    palette.button: root.panelColor
    palette.highlight: root.violetSkin ? root.accentColor : root.selectionColor
    background: Rectangle {
        id: workspaceBackground
        radius: 14
        color: root.violetSkin ? "#ffffff" : root.baseColor
        gradient: root.violetSkin ? null : defaultBackgroundGradient
        Gradient {
            id: defaultBackgroundGradient
            GradientStop { position: 0; color: Qt.lighter(root.baseColor, 1.12) }
            GradientStop { position: 0.4; color: root.baseColor }
            GradientStop { position: 1; color: Qt.darker(root.baseColor, 1.12) }
        }
        border.color: Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.07)
        // 构图都在底板内裁切，不增加标题栏高度或改变网格。
        Item {
            anchors.fill: parent
            clip: true
            Oran7StudioMaterial {
                anchors.fill: parent
                visible: !root.violetSkin
                grainOpacity: root.materialStrength
            }
            Image {
                id: landscape
                objectName: "recordingVioletLandscape"
                anchors.fill: parent
                // 原图共用于圆角底板与四块毛玻璃，自身不直接绘制以免覆盖圆角。
                visible: false
                source: root.violetSkin ? Qt.resolvedUrl("../../../../image/skins/violet/reference-snowfield.png") : ""
                fillMode: Image.PreserveAspectCrop
            }
            Canvas {
                objectName: "recordingRoundedSnowfield"
                anchors.fill: parent
                visible: root.violetSkin
                antialiasing: true
                smooth: true
                readonly property url source: landscape.source
                readonly property size paintedSize: Qt.size(landscape.paintedWidth, landscape.paintedHeight)
                readonly property real radius: workspaceBackground.radius
                readonly property int imageStatus: landscape.status

                function loadSnowfield() {
                    if (!available || !visible || String(source).length === 0)
                        return
                    // 再次启用时素材可能已经缓存，不会再发 imageLoaded，需主动重绘。
                    if (isImageLoaded(source))
                        requestPaint()
                    else
                        loadImage(source)
                }

                onAvailableChanged: loadSnowfield()
                onVisibleChanged: loadSnowfield()
                onSourceChanged: loadSnowfield()
                onImageLoaded: requestPaint()
                onPaintedSizeChanged: if (visible) requestPaint()
                onRadiusChanged: if (visible) requestPaint()
                onImageStatusChanged: if (visible) requestPaint()
                onCanvasSizeChanged: if (visible) requestPaint()
                onPaint: {
                    const c = getContext("2d")
                    c.reset()
                    c.clearRect(0, 0, width, height)
                    if (imageStatus !== Image.Ready || !isImageLoaded(source))
                        return

                    c.beginPath()
                    c.roundedRect(0, 0, width, height, radius, radius)
                    c.clip()
                    c.translate((width - paintedSize.width) / 2, (height - paintedSize.height) / 2)
                    c.scale(paintedSize.width / landscape.implicitWidth, paintedSize.height / landscape.implicitHeight)
                    c.drawImage(source, 0, 0)
                }
            }
            Rectangle {
                visible: root.violetSkin
                anchors.fill: parent
                radius: workspaceBackground.radius
                gradient: Gradient {
                    GradientStop { position: 0; color: "#8ce8effc" }
                    GradientStop { position: 0.14; color: "#18e8effc" }
                    GradientStop { position: 0.65; color: "transparent" }
                    GradientStop { position: 1; color: "#42b6bae3" }
                }
            }
            Oran7PointerParallax {
                id: avatarPointer
                objectName: "recordingAvatarParallax"
                anchors.fill: parent
                active: root.avatarParallaxEnabled && root.audioMonitoringActive
                maximumOffset: root.avatarParallaxPadding
            }
            // 视差只作用于图片坐标，裁切容器与四个板块保持固定。
            Item {
                id: avatar
                objectName: "recordingStudioAvatar"
                width: 440; height: 440
                readonly property url source: Qt.resolvedUrl("../../../../image/oran7_studio_avatar_outline_v2.png")
                property real parallaxX: avatarPointer.offset.x
                property real parallaxY: avatarPointer.offset.y
                Behavior on parallaxX { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                Behavior on parallaxY { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                x: parent.width * 0.65 - width / 2 + parallaxX
                y: -141 + parallaxY
                opacity: root.avatarOpacity
                Image {
                    anchors.fill: parent
                    visible: !root.violetSkin
                    source: root.violetSkin ? "" : avatar.source
                    sourceSize: Qt.size(512, 512)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }
                Canvas {
                    anchors.fill: parent
                    visible: root.violetSkin
                    smooth: true
                    readonly property size outlineSize: Qt.size(512, 512)
                    readonly property color tint: root.avatarTintColor

                    function loadOutline() {
                        if (!available || !visible)
                            return
                        if (isImageLoaded(avatar.source))
                            requestPaint()
                        else
                            loadImage(avatar.source, outlineSize)
                    }

                    onAvailableChanged: loadOutline()
                    onVisibleChanged: loadOutline()
                    onImageLoaded: requestPaint()
                    onTintChanged: if (visible) requestPaint()
                    onPaint: {
                        const c = getContext("2d")
                        c.reset()
                        c.clearRect(0, 0, width, height)
                        if (!isImageLoaded(avatar.source))
                            return
                        c.save()
                        c.scale(width / outlineSize.width, height / outlineSize.height)
                        c.drawImage(avatar.source, 0, 0)
                        c.restore()
                        // 只染原图的线条，透明区域继续透出工作台底色。
                        c.globalCompositeOperation = "source-in"
                        c.fillStyle = tint
                        c.fillRect(0, 0, width, height)
                    }
                }
            }
        }
        Rectangle {
            anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width * 0.6; height: 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "transparent" }
                GradientStop { position: 0.5; color: root.violetSkin ? "#35ffffff" : "#16ffffff" }
                GradientStop { position: 1; color: "transparent" }
            }
        }
    }

    Oran7RecordingPageState { id: state; service: root.service; controller: root.controller }

    // 标签、空白和预览通常不接收焦点，点击这些区域时主动结束输入。
    // 被动监听按下，不拦截按钮、输入框或滚动区域原有的鼠标操作。
    Item {
        anchors.fill: parent
        z: 1000
        PointHandler {
            objectName: "workspaceOutsideClick"
            acceptedButtons: Qt.LeftButton
            onActiveChanged: {
                if (!active || !root.Window.window) return
                var editor = root.Window.window.activeFocusItem
                while (editor && !editor.recordingWorkspaceEditor) editor = editor.parent
                if (!editor) return
                var position = root.mapToItem(editor, point.position.x, point.position.y)
                if (position.x < 0 || position.y < 0 || position.x >= editor.width || position.y >= editor.height) {
                    editor.focus = false
                    root.forceActiveFocus()
                }
            }
        }
    }

    component Oran7StudioPanel: Rectangle {
        id: panel
        default property alias contents: panelColumn.data
        property string title
        property string statusText
        property string sectionId: "01"
        property bool modified: false
        property Component footer
        property real minimumFooterHeight: 0
        // 为标题、输出按钮和最小设置视口留足空间，短窗口交给外层滚动。
        readonly property real minimumPanelHeight: 60 + 120 + 12 + minimumFooterHeight
        color: root.panelColor
        radius: 9
        border.color: root.borderColor
        clip: true
        Item {
            anchors.fill: parent
            clip: true
            Oran7StudioMaterial { anchors.fill: parent; grainOpacity: root.materialStrength }
            Oran7StudioMotif {
                visible: !root.violetSkin
                width: 225; height: 225
                x: panel.width - width * 0.7
                y: Math.max(100, panel.height - footerArea.height - height - 8)
                rotation: panel.sectionId === "01" ? -18 : 18
                ink: root.decorationColor; opacity: root.motifStrength
                energized: panel.sectionId === "04" && !!root.controller && (root.controller.recordingBusy === true || root.controller.liveBusy === true)
                signalColor: root.liveAccentColor
            }
            Item {
                visible: root.violetSkin && panel.sectionId === "01"
                x: 16; y: Math.max(120, panel.height - footerArea.height - 58)
                width: panel.width - 32; height: 36
                Text { y: 20; text: "01 / SOURCE"; color: root.mutedTextColor; opacity: 0.7; font.pixelSize: 9; font.letterSpacing: 1.5 }
                Repeater {
                    model: 18
                    Rectangle {
                        required property int index
                        x: parent.width - 48 + index % 6 * 8; y: Math.floor(index / 6) * 8
                        width: 1; height: 1; color: root.accentColor; opacity: 0.4
                    }
                }
            }
            Text {
                x: 16; y: Math.max(110, panel.height - footerArea.height - height - 12)
                text: panel.sectionId
                visible: !root.violetSkin
                font.pixelSize: 66; font.bold: true; font.letterSpacing: -4
                color: root.decorationColor; opacity: 0.035
            }
        }
        Rectangle {
            visible: !root.violetSkin
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 }
            height: panelHeading.y + panelHeading.height + 8
            topLeftRadius: panel.radius - 1
            topRightRadius: topLeftRadius
            color: root.groupColor
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.borderColor }
        }
        RowLayout {
            id: panelHeading
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
            spacing: 6
            Label {
                text: panel.sectionId; color: root.paperInkColor; font.pixelSize: 11; font.bold: true
                leftPadding: 4; rightPadding: 4
                background: Rectangle {
                    color: root.paperColor; radius: root.violetSkin ? 3 : 2
                    Rectangle { visible: root.violetSkin; x: 0; y: parent.height + 3; width: parent.width; height: 2; radius: 1; color: root.accentColor; opacity: 0.6 }
                }
            }
            Label { text: panel.title; color: root.textColor; font.pixelSize: 15; font.bold: true; Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight }
            Label {
                objectName: panel.objectName + "Status"
                text: panel.statusText
                color: panel.modified ? root.accentColor : root.mutedTextColor
                font.pixelSize: 10
            }
        }
        Rectangle {
            visible: root.violetSkin
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: panelHeading.bottom
            anchors.leftMargin: 14; anchors.rightMargin: 14; anchors.topMargin: 9
            height: 1; color: root.borderColor
            Rectangle { y: -1; width: 18; height: 2; radius: 1; color: root.recordingAccentColor; opacity: 0.75 }
        }
        ScrollView {
            id: settingsScroll
            objectName: panel.objectName + "Scroll"
            anchors { left: parent.left; right: parent.right; top: panelHeading.bottom; bottom: footerArea.top; leftMargin: 12; rightMargin: 12; topMargin: 14; bottomMargin: 12 }
            clip: true
            contentWidth: availableWidth
            contentHeight: panelColumn.implicitHeight
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            Column { id: panelColumn; width: settingsScroll.availableWidth; spacing: 12 }
        }
        Item {
            id: footerArea
            objectName: panel.objectName + "Footer"
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: panel.footer ? footerLoader.height + 25 : 0
            visible: !!panel.footer
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: root.borderColor }
            Loader {
                id: footerLoader
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 12 }
                sourceComponent: panel.footer
            }
        }
    }
    component Oran7SettingsGroup: Rectangle {
        default property alias contents: groupColumn.data
        property string title
        width: parent.width
        implicitHeight: groupColumn.implicitHeight + 22
        color: root.groupColor
        radius: 6
        border.color: "transparent"
        Rectangle {
            x: 10; y: 0; width: parent.width - 20; height: 1
            color: root.violetSkin ? root.borderColor : "#0cffffff"
            Rectangle { visible: root.violetSkin; width: 16; height: 2; y: -1; radius: 1; color: root.recordingAccentColor; opacity: 0.6 }
        }
        Column {
            id: groupColumn
            x: 10; y: 11
            width: parent.width - 20
            spacing: 10
            Label { text: parent.parent.title; color: root.textColor; font.pixelSize: 13; font.bold: true }
        }
    }
    component Oran7Caption: Label {
        width: parent.width
        color: root.mutedTextColor
        wrapMode: Text.Wrap
        font.pixelSize: 11
        lineHeight: 1.15
    }
    component Oran7FieldLabel: Label {
        width: parent.width
        color: root.textColor
        font.pixelSize: root.controlPixelSize
    }
    component Oran7Input: TextField {
        id: editor
        readonly property bool recordingWorkspaceEditor: true
        property bool editingHadFocus: false
        signal commitRequested()
        // 文本绑定回写可能影响 TextInput 的 editingFinished 判定，
        // 因此空闲提交和失焦提交不依赖该信号是否发出。
        onTextChanged: {
            if (activeFocus) editSaveTimer.restart()
        }
        onEditingFinished: {
            editSaveTimer.stop()
            commitRequested()
        }
        onActiveFocusChanged: {
            if (activeFocus) {
                editingHadFocus = true
            } else if (editingHadFocus) {
                editingHadFocus = false
                editSaveTimer.stop()
                commitRequested()
            }
        }
        Timer {
            id: editSaveTimer
            interval: 2000
            onTriggered: editor.commitRequested()
        }
        width: parent.width
        implicitHeight: root.controlHeight
        selectByMouse: true
        color: root.textColor
        placeholderTextColor: root.mutedTextColor
        selectionColor: root.violetSkin ? root.accentColor : root.selectionColor
        selectedTextColor: root.violetSkin ? root.inputColor : root.textColor
        font.pixelSize: root.controlPixelSize
        padding: 8
        background: Oran7StudioControlSurface {
            polished: root.violetSkin; tintColor: root.accentColor; highlighted: editor.activeFocus
            radius: 6
            color: root.inputColor
            border.width: 1
            border.color: parent.activeFocus ? root.accentColor : root.borderColor
        }
    }
    component Oran7Dimension: Oran7Input {
        validator: IntValidator { bottom: 0; top: 16384 }
    }
    component Oran7Chevron: Canvas {
        width: 12; height: 8
        property color strokeColor: root.mutedTextColor
        onStrokeColorChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = strokeColor
            ctx.lineWidth = 1.5
            ctx.beginPath(); ctx.moveTo(1, 1); ctx.lineTo(6, 6); ctx.lineTo(11, 1); ctx.stroke()
        }
    }
    component Oran7Choice: ComboBox {
        id: choice
        property int selectedRow: 0
        property string fieldGlyph: "info"
        ToolTip.visible: hovered && !popup.visible
        ToolTip.delay: 600
        ToolTip.text: displayText
        property real popupWidth: width
        property real maximumPopupHeight: 250
        // 等模型内部完成初始选中，再提交草稿索引，避免被默认的第 0 项覆盖。
        Binding {
            target: choice; property: "currentIndex"; value: choice.selectedRow
            when: choice.count > 0
            delayed: true
        }
        width: parent.width
        implicitHeight: root.controlHeight
        font.pixelSize: root.controlPixelSize
        leftPadding: root.violetSkin ? 30 : 9
        rightPadding: root.violetSkin ? 30 : 24
        contentItem: Text {
            text: choice.displayText
            color: choice.enabled ? root.textColor : root.mutedTextColor
            font: choice.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        indicator: Oran7Chevron {
            x: choice.width - width - 9; anchors.verticalCenter: parent.verticalCenter
        }
        background: Oran7StudioControlSurface {
            polished: root.violetSkin; tintColor: root.accentColor; highlighted: choice.activeFocus || choice.popup.visible
            radius: 6; color: root.inputColor
            border.color: choice.activeFocus ? root.accentColor : root.borderColor
            Oran7RecordingIcon {
                visible: root.violetSkin; x: 9; anchors.verticalCenter: parent.verticalCenter
                width: 14; height: 14; glyph: choice.fieldGlyph; ink: root.mutedTextColor
            }
            Rectangle {
                visible: root.violetSkin; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
                width: 26; radius: 5; color: "#08ffffff"
                Rectangle { width: 1; height: parent.height - 10; y: 5; color: root.borderColor; opacity: 0.7 }
            }
        }
        delegate: ItemDelegate {
            required property int index
            width: choice.popupWidth
            height: 34
            highlighted: choice.highlightedIndex === index
            contentItem: Text { text: choice.textAt(index); color: root.textColor; font.pixelSize: root.controlPixelSize; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            background: Rectangle { radius: 4; color: parent.highlighted ? (root.violetSkin ? root.hoverColor : root.selectionColor) : "transparent" }
        }
        popup: Popup {
            y: choice.height + 4
            margins: 8
            width: choice.popupWidth
            padding: 4
            implicitHeight: Math.min(contentItem.implicitHeight + 8, choice.maximumPopupHeight)
            contentItem: ListView {
                clip: true; implicitHeight: contentHeight
                model: choice.popup.visible ? choice.delegateModel : null
                currentIndex: choice.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }
            background: Rectangle { color: root.panelColor; radius: 7; border.color: root.borderColor }
        }
    }
    component Oran7QuickChoices: Row {
        id: choices
        property var options: []
        property var optionEnabled: []
        property int selectedIndex: -1
        signal selected(int index)
        width: parent.width
        height: root.controlHeight
        spacing: 4
        Repeater {
            model: choices.options
            delegate: Button {
                required property int index
                required property var modelData
                objectName: choices.objectName + "Option" + index
                width: (choices.width - (choices.options.length - 1) * choices.spacing) / choices.options.length
                height: root.controlHeight
                text: String(modelData)
                enabled: choices.optionEnabled.length === 0 || !!choices.optionEnabled[index]
                opacity: enabled ? 1 : 0.35
                padding: 2
                contentItem: Text {
                    text: parent.text; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    color: root.violetSkin && choices.selectedIndex === index ? root.accentColor : root.textColor
                }
                background: Oran7StudioControlSurface {
                    polished: root.violetSkin; tintColor: root.accentColor
                    highlighted: choices.selectedIndex === index || parent.hovered; depressed: parent.down
                    radius: 5
                    color: choices.selectedIndex === index ? root.selectionColor
                        : parent.hovered ? root.hoverColor : root.inputColor
                    border.color: parent.activeFocus || choices.selectedIndex === index ? root.accentColor : root.borderColor
                }
                onClicked: if (enabled) choices.selected(index)
            }
        }
    }
    component Oran7Action: Oran7StudioButton {
        polished: root.violetSkin
        accentColor: root.accentColor
        textColor: root.textColor
        surfaceColor: root.actionColor
        hoverColor: root.hoverColor
        borderColor: root.borderColor
        dangerColor: root.errorColor
        font.family: root.fontFamily
    }
    component Oran7WorkspaceSwitch: Oran7StudioSwitch {
        polished: root.violetSkin
        onColor: root.accentColor
        offColor: root.toggleOffColor
        borderColor: root.toggleBorderColor
        thumbColor: root.toggleThumbColor
        controlHeight: root.toggleHeight
    }

    component Oran7LoadReading: Row {
        id: reading
        required property string caption
        required property int percent
        readonly property int highLoadThreshold: 80
        spacing: 4

        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: reading.caption
            font.pixelSize: 11
            color: root.mutedTextColor
        }
        TextMetrics { id: loadValueMetrics; font: loadValue.font; text: "100%" }
        Label {
            id: loadValue
            objectName: reading.objectName + "Value"
            anchors.verticalCenter: parent.verticalCenter
            width: Math.ceil(loadValueMetrics.advanceWidth)
            horizontalAlignment: Text.AlignRight
            text: reading.percent < 0 ? "—" : reading.percent + "%"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            color: reading.percent >= reading.highLoadThreshold ? root.errorColor : root.textColor
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Column {
                spacing: 2
                Label { text: "ORAN7 / CONTROL ROOM"; color: root.accentColor; font.pixelSize: 9; font.letterSpacing: 1.5 }
                Label { text: qsTranslate("Oran7Ui", "直播工作台"); color: root.textColor; font.pixelSize: 20; font.bold: true }
            }
            Rectangle {
                implicitWidth: formatBadge.implicitWidth + 16; implicitHeight: 24
                radius: 5; color: root.groupColor; border.color: root.borderColor
                Label { id: formatBadge; anchors.centerIn: parent; text: root.controller && root.controller.liveBusy === true ? "RTMP" : root.pageState.outputDraft.format === "flv" ? qsTranslate("Oran7Ui", "本地 FLV") : qsTranslate("Oran7Ui", "本地 MP4"); color: root.mutedTextColor; font.pixelSize: 11 }
            }
            Item { Layout.fillWidth: true }
            Oran7StudioControlSurface {
                objectName: "recordingProcessLoad"
                implicitWidth: readings.implicitWidth + 16
                implicitHeight: 28
                radius: 7
                polished: root.violetSkin
                color: root.violetSkin ? root.panelColor : Qt.alpha(root.groupColor, 0.88)
                border.color: root.borderColor
                ToolTip.visible: loadHover.hovered
                ToolTip.delay: 500
                ToolTip.text: qsTranslate("Oran7Ui", "Oran7MediaClient 进程占用率")

                HoverHandler { id: loadHover }
                Row {
                    id: readings
                    anchors.centerIn: parent
                    spacing: root.width < 720 ? 8 : 12
                    Oran7LoadReading {
                        objectName: "recordingCpuLoad"
                        caption: "CPU"
                        percent: root.processCpuPercent
                    }
                    Oran7LoadReading {
                        objectName: "recordingGpuLoad"
                        caption: "GPU"
                        percent: root.processGpuPercent
                    }
                }
            }
            Oran7Action { objectName: "recordingRestoreSettings"; text: qsTranslate("Oran7Ui", "还原设置"); enabled: !root.pageState.busy; onClicked: {
                root.forceActiveFocus()
                root.customCaptureFps = false; root.customOutputFps = false; root.customOutputSize = false; root.customBitrate = false
                root.pageState.reset()
            } }
        }
        ScrollView {
            id: workspaceScroll
            objectName: "recordingWorkspaceScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            Layout.preferredHeight: 0
            // 始终保留左右工具栏；空间不足时滚动整行，不改变预览与工具栏的位置关系。
            clip: true
            contentWidth: Math.max(availableWidth, root.sourcePanelWidth + root.minimumCenterWidth + root.outputPanelWidth + 20)
            contentHeight: Math.max(200 + recordingToolbar.preferredToolbarHeight + 10,
                                   sourcePanel.minimumPanelHeight, outputPanel.minimumPanelHeight,
                                   height)
            ScrollBar.horizontal: ScrollBar {
                objectName: "recordingWorkspaceHorizontalScrollBar"
                parent: workspaceScroll
                x: 0; y: workspaceScroll.height - height
                width: workspaceScroll.width; height: 8
                z: 5
                policy: workspaceScroll.contentWidth > workspaceScroll.availableWidth ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                implicitHeight: 8
                minimumSize: 0.15
                contentItem: Rectangle {
                    implicitHeight: 6; radius: 3
                    color: root.accentColor; opacity: parent.hovered || parent.pressed ? 0.65 : 0.4
                }
                background: Rectangle { color: root.panelColor; radius: 4 }
            }
            // 滚动范围单向分配给内容项，避免布局测量反向写回滚动容器的尺寸。
            Item {
                id: studioContent
                width: workspaceScroll.contentWidth
                height: workspaceScroll.contentHeight

                Canvas {
                    id: frost
                    objectName: "recordingFrostedCards"
                    anchors.fill: parent
                    visible: root.violetSkin
                    smooth: true
                    antialiasing: true

                    // 四块玻璃共用一份降采样雪景。按原图取景对齐，只在尺寸、位置或素材变化时重绘。
                    readonly property url landscapeUrl: landscape.source
                    readonly property size sampleSize: Qt.size(96, 54)
                    readonly property var cards: [sourcePanel, previewPanel, recordingToolbar, outputPanel]
                    readonly property var cardBounds: cards.map(card => {
                        const column = card.parent === grid ? null : card.parent
                        return {
                            rect: Qt.rect(card.x + (column ? column.x : 0), card.y + (column ? column.y : 0),
                                          card.width, card.height),
                            radius: card.radius
                        }
                    })
                    readonly property rect backdropRect: {
                        const viewport = workspaceScroll.contentItem
                        const scrolled = viewport ? viewport.contentItem : null
                        const left = root.background.x + (landscape.width - landscape.paintedWidth) / 2
                        const top = root.background.y + (landscape.height - landscape.paintedHeight) / 2
                        return Qt.rect(
                            left - root.contentItem.x - workspaceScroll.x - (viewport ? viewport.x : 0)
                                - (scrolled ? scrolled.x : 0) - studioContent.x,
                            top - root.contentItem.y - workspaceScroll.y - (viewport ? viewport.y : 0)
                                - (scrolled ? scrolled.y : 0) - studioContent.y,
                            landscape.paintedWidth, landscape.paintedHeight)
                    }

                    function loadBackdrop() {
                        if (available && String(landscapeUrl).length > 0)
                            loadImage(landscapeUrl, sampleSize)
                    }

                    onAvailableChanged: loadBackdrop()
                    onLandscapeUrlChanged: loadBackdrop()
                    onImageLoaded: requestPaint()
                    onCardBoundsChanged: if (visible) requestPaint()
                    onBackdropRectChanged: if (visible) requestPaint()
                    onCanvasSizeChanged: if (visible) requestPaint()
                    onPaint: {
                        const c = getContext("2d")
                        c.reset()
                        c.clearRect(0, 0, width, height)
                        if (!isImageLoaded(landscapeUrl))
                            return

                        c.beginPath()
                        for (const card of cardBounds) {
                            const r = card.rect
                            if (r.width > 0 && r.height > 0)
                                c.roundedRect(r.x, r.y, r.width, r.height, card.radius, card.radius)
                        }
                        c.clip()
                        const r = backdropRect
                        // 在画笔坐标中放大，使用平滑插值；直接传目标尺寸会先做近邻放大。
                        c.translate(r.x, r.y)
                        c.scale(r.width / sampleSize.width, r.height / sampleSize.height)
                        c.drawImage(landscapeUrl, 0, 0)
                    }
                }

                GridLayout {
                    id: grid
                    objectName: "recordingStudioGrid"
                    anchors.fill: parent
                    columns: 3
                    columnSpacing: 10; rowSpacing: 10
                    ColumnLayout {
                        objectName: "recordingCenterColumn"
                        Layout.row: 0
                        Layout.column: 1
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 500
                        Layout.minimumWidth: root.minimumCenterWidth
                        Layout.preferredHeight: 380
                        spacing: 10
                        Oran7RecordingPreview {
                            id: previewPanel
                            objectName: "recordingPreviewPanel"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 200
                            controller: root.controller
                            pageState: state
                            previewComponent: root.previewComponent
                            sourceName: root.previewSource.name || ""
                            sourceDetails: (root.previewSource.details ? root.previewSource.details + " · " : "")
                                + qsTranslate("Oran7Ui", "采集上限 %1 FPS").arg(state.captureSaved.fps || 60)
                            violetSkin: root.violetSkin
                            materialStrength: root.materialStrength
                            paperColor: root.paperColor; paperInkColor: root.paperInkColor
                            titleSurfaceColor: root.violetSkin ? root.inputColor : root.groupColor
                            panelColor: root.panelColor
                            canvasColor: root.previewBackgroundColor
                            textColor: root.previewTextColor
                            mutedTextColor: root.mutedTextColor
                            accentColor: root.accentColor
                            streamingColor: root.liveAccentColor
                            recordingColor: root.recordingAccentColor
                            borderColor: root.borderColor
                            errorColor: root.errorColor
                            fontFamily: root.fontFamily
                            showInfo: root.showScreenInfo
                            fillPreview: root.fillPreview
                            onInfoToggled: function(checked) { root.showScreenInfo = checked }
                            onScaleToggled: function(checked) { root.fillPreview = checked; root.previewScaleChanged(checked) }
                        }
                        Oran7RecordingToolbar {
                            liveCommit: function() { return root.saveLive() }
                            audioCommit: function() { return root.saveLiveAudio() }
                            audioOptions: root.liveDraft
                            audioEdit: function(key,value) { root.editLiveAudio(key,value) }
                            violetSkin: root.violetSkin
                            materialStrength: root.materialStrength
                            paperColor: root.paperColor; paperInkColor: root.paperInkColor
                            id: recordingToolbar
                            recordingColor: root.recordingAccentColor
                            objectName: "recordingCenterToolbar"
                            Layout.fillWidth: true
                            pageState: state
                            controller: root.controller
                            panelColor: root.violetSkin ? "#76d9e0f6" : root.panelColor
                            textColor: root.textColor
                            errorColor: root.errorColor
                            accentColor: root.accentColor
                            mutedTextColor: root.mutedTextColor
                            surfaceColor: root.groupColor
                            hoverColor: root.hoverColor
                            borderColor: root.borderColor
                            toggleOffColor: root.toggleOffColor
                            toggleBorderColor: root.toggleBorderColor
                            toggleThumbColor: root.toggleThumbColor
                            toggleHeight: root.toggleHeight
                            fontFamily: root.fontFamily
                            textPixelSize: root.textPixelSize
                            meterNormalColor: root.violetSkin ? root.accentColor : "#4bcb76"
                            meterWarningColor: root.violetSkin ? "#936728" : "#f1c454"
                            meterPeakColor: root.errorColor
                        }
                    }
                    Oran7StudioPanel {
                        id: sourcePanel
                        objectName: "recordingSourcePanel"
                        Layout.minimumHeight: minimumPanelHeight
                        minimumFooterHeight: 36 + 25
                        title: qsTranslate("Oran7Ui", "画面来源")
                        statusText: root.pageState.applying ? qsTranslate("Oran7Ui", "应用中") : root.pageState.captureDirty ? qsTranslate("Oran7Ui", "待保存") : qsTranslate("Oran7Ui", "已保存")
                        modified: root.pageState.captureDirty
                        Layout.row: 0; Layout.column: 0
                        Layout.fillHeight: true
                        Layout.minimumWidth: root.sourcePanelWidth; Layout.preferredWidth: root.sourcePanelWidth; Layout.maximumWidth: root.sourcePanelWidth
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredHeight: 380
                        footer: Component {
                            Column {
                                spacing: 6
                                Oran7Action {
                                    objectName: "restartCapturePreview"
                                    width: parent.width
                                    text: qsTranslate("Oran7Ui", "重连预览")
                                    enabled: !root.pageState.busy
                                    onClicked: {
                                        root.forceActiveFocus()
                                        root.pageState.message = root.controller.restartPreview()
                                            ? "正在重新连接预览…" : "当前无法重连预览"
                                    }
                                }
                            }
                        }
                        Column {
                            width: parent.width; spacing: 12
                            enabled: !root.pageState.busy
                            opacity: enabled ? 1 : 0.5
                            Oran7SettingsGroup {
                                title: qsTranslate("Oran7Ui", "显示器")
                                Row {
                                    width: parent.width; spacing: 6
                                    Oran7Choice {
                                        id: monitorSelector
                                        objectName: "recordingMonitor"
                                        fieldGlyph: "monitor"
                                        width: parent.width - refreshMonitor.width - parent.spacing
                                        model: root.pageState.monitors
                                        textRole: "name"
                                        selectedRow: root.pageState.monitorRow
                                        popupWidth: Math.max(width, 280)
                                        ToolTip.visible: hovered && displayText.length > 0
                                        ToolTip.text: displayText
                                        delegate: ItemDelegate {
                                            required property var modelData
                                            required property int index
                                            width: monitorSelector.popup.width - 8
                                            height: 54
                                            enabled: modelData.attached
                                            highlighted: monitorSelector.highlightedIndex === index
                                            contentItem: Column {
                                                spacing: 3
                                                Text { width: parent.width; text: modelData.name; color: root.textColor; font.pixelSize: 13; elide: Text.ElideRight }
                                                Text { width: parent.width; text: modelData.details; color: root.mutedTextColor; font.pixelSize: 10; elide: Text.ElideRight }
                                            }
                                            background: Rectangle { radius: 4; color: parent.highlighted ? (root.violetSkin ? root.hoverColor : root.selectionColor) : "transparent" }
                                        }
                                        onActivated: function(index) {
                                            if (root.pageState.monitors[index].attached) root.pageState.editCapture("outputIndex", root.pageState.monitors[index].outputIndex)
                                            else root.pageState.message = "此显示器未连接"
                                        }
                                    }
                                    ToolButton {
                                        id: refreshMonitor
                                        objectName: "refreshRecordingMonitors"
                                        width: 30; height: root.controlHeight
                                        icon.source: "qrc:/image/mage_refresh-reverse.png"
                                        icon.width: 18; icon.height: 18; icon.color: root.mutedTextColor
                                        background: Oran7StudioControlSurface {
                                            polished: root.violetSkin
                                            tintColor: root.accentColor; highlighted: parent.hovered || parent.activeFocus; depressed: parent.down
                                            radius: 5; color: parent.hovered ? root.hoverColor : "transparent"
                                        }
                                        ToolTip.visible: hovered; ToolTip.text: qsTranslate("Oran7Ui", "刷新显示器")
                                        Accessible.name: qsTranslate("Oran7Ui", "刷新显示器")
                                        onClicked: root.pageState.refreshMonitors()
                                    }
                                }
                                Oran7Caption { text: root.pageState.monitorRow >= 0 ? root.pageState.monitors[root.pageState.monitorRow].details : qsTranslate("Oran7Ui", "请选择已连接的显示器") }
                            }
                            Oran7SettingsGroup {
                                title: qsTranslate("Oran7Ui", "采集设置")
                                Oran7FieldLabel { text: qsTranslate("Oran7Ui", "采集帧率 · FPS") }
                                Oran7QuickChoices {
                                    objectName: "captureFpsChoices"
                                    options: ["30", "60", "120", qsTranslate("Oran7Ui", "自定义")]
                                    optionEnabled: [root.pageState.minimumCaptureFps <= 30, root.pageState.minimumCaptureFps <= 60, root.pageState.minimumCaptureFps <= 120, true]
                                    selectedIndex: root.customCaptureFps ? 3 : [30, 60, 120].indexOf(root.pageState.captureDraft.fps) < 0 ? 3 : [30, 60, 120].indexOf(root.pageState.captureDraft.fps)
                                    onSelected: function(index) {
                                        root.customCaptureFps = index === 3
                                        if (index < 3) root.pageState.editCapture("fps", [30, 60, 120][index])
                                    }
                                }
                                Oran7Input {
                                    objectName: "captureCustomFps"
                                    visible: root.customCaptureFps || [30, 60, 120].indexOf(root.pageState.captureDraft.fps) < 0
                                    validator: IntValidator { bottom: root.pageState.minimumCaptureFps; top: 240 }
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    text: root.pageState.captureDraft.fps === null ? "" : String(root.pageState.captureDraft.fps)
                                    placeholderText: root.pageState.minimumCaptureFps + "–240 FPS"
                                    onTextEdited: root.pageState.editCapture("fps", text.length ? Number(text) : null, true)
                                    onCommitRequested: root.pageState.commitCapture()
                                }
                                Item {
                                    width: parent.width; height: 34
                                    Label { anchors.verticalCenter: parent.verticalCenter; text: qsTranslate("Oran7Ui", "显示鼠标"); color: root.textColor; font.pixelSize: 13 }
                                    Oran7WorkspaceSwitch {
                                        id: mouseToggle
                                        objectName: "recordingMouse"
                                        onSwitchToggleChanged: function(checked) { root.pageState.editCapture("drawMouse", checked) }
                                    }
                                    Binding { target: mouseToggle; property: "checked"; value: !!root.pageState.captureDraft.drawMouse }
                                }
                                Oran7Caption { text: qsTranslate("Oran7Ui", "至少 ") + root.pageState.minimumCaptureFps + qsTranslate("Oran7Ui", " FPS，不低于录制帧率。\n修改后自动保存并更新预览。") }
                            }
                            Column {
                                width: parent.width; spacing: 8
                                Button {
                                    id: advancedToggle
                                    objectName: "recordingAdvancedToggle"
                                    width: parent.width; height: 30
                                    checkable: true
                                    contentItem: Item {
                                        Text { anchors.verticalCenter: parent.verticalCenter; text: qsTranslate("Oran7Ui", "高级设置"); color: root.mutedTextColor; font.pixelSize: 12 }
                                        Oran7Chevron { x: 60; anchors.verticalCenter: parent.verticalCenter; rotation: advancedToggle.checked ? 180 : 0 }
                                    }
                                    background: Oran7StudioControlSurface {
                                        polished: root.violetSkin
                                        tintColor: root.accentColor; highlighted: parent.hovered || parent.checked || parent.activeFocus; depressed: parent.down
                                        radius: 5; color: parent.hovered ? root.hoverColor : "transparent"
                                    }
                                }
                                Oran7SettingsGroup {
                                    objectName: "recordingAdvancedGroup"
                                    visible: advancedToggle.checked
                                    title: qsTranslate("Oran7Ui", "诊断")
                                    Item {
                                        width: parent.width; height: 34
                                        Label { anchors.verticalCenter: parent.verticalCenter; text: qsTranslate("Oran7Ui", "调试日志"); color: root.textColor; font.pixelSize: 13 }
                                        Oran7WorkspaceSwitch {
                                            id: debugToggle
                                            objectName: "recordingDebugLog"
                                            onSwitchToggleChanged: function(checked) { root.pageState.editCapture("debugLog", checked) }
                                        }
                                        Binding { target: debugToggle; property: "checked"; value: !!root.pageState.captureDraft.debugLog }
                                    }
                                    Oran7Caption { text: qsTranslate("Oran7Ui", "排查采集问题时开启。") }
                                }
                            }
                        }
                    }
                    Oran7StudioPanel {
                        id: outputPanel
                        objectName: "recordingOutputPanel"
                        Layout.minimumHeight: minimumPanelHeight
                        minimumFooterHeight: root.outputCardHeight * (root.liveSupported ? 2 : 1) + (root.liveSupported ? 8 : 0) + 25
                        title: qsTranslate("Oran7Ui", "输出控制")
                        sectionId: "04"
                        statusText: root.pageState.outputDirty || root.liveDirty || root.audioDirty ? qsTranslate("Oran7Ui", "待保存") : qsTranslate("Oran7Ui", "已保存")
                        modified: root.pageState.outputDirty || root.liveDirty || root.audioDirty
                        Layout.row: 0; Layout.column: 2
                        Layout.fillHeight: true
                        Layout.minimumWidth: root.outputPanelWidth; Layout.preferredWidth: root.outputPanelWidth; Layout.maximumWidth: root.outputPanelWidth
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredHeight: 380
                        footer: Component {
                            Column {
                                spacing: 8
                                Oran7RecordingOutputCard {
                                    objectName: "recordingLocalOutputCard"
                                    implicitHeight: root.outputCardHeight
                                    width: parent.width
                                    violetSkin: root.violetSkin
                                    materialStrength: root.materialStrength
                                    paperColor: root.paperColor; paperInkColor: root.paperInkColor
                                    controller: root.controller; pageState: root.pageState
                                    commitAudio: function() { return root.saveLiveAudio() }
                                    accentColor: root.recordingAccentColor; textColor: root.textColor; mutedTextColor: root.mutedTextColor
                                    actionColor: root.violetSkin ? root.inputColor : root.actionColor
                                    surfaceColor: root.inputColor; hoverColor: root.hoverColor; borderColor: root.borderColor; errorColor: root.errorColor
                                }
                                Oran7RecordingOutputCard {
                                    objectName: "recordingLiveOutputCard"
                                    implicitHeight: root.outputCardHeight
                                    width: parent.width; visible: root.liveSupported; live: true
                                    violetSkin: root.violetSkin
                                    materialStrength: root.materialStrength
                                    paperColor: root.paperColor; paperInkColor: root.paperInkColor
                                    controller: root.controller; pageState: root.pageState
                                    commitAudio: function() { return root.saveLiveAudio() }
                                    commitLive: function() { return root.saveLive() }
                                    accentColor: root.liveAccentColor; textColor: root.textColor; mutedTextColor: root.mutedTextColor
                                    actionColor: root.violetSkin ? root.inputColor : root.actionColor
                                    surfaceColor: root.inputColor; hoverColor: root.hoverColor; borderColor: root.borderColor; errorColor: root.errorColor
                                }
                            }
                        }
                        Column {
                            width: parent.width; spacing: 12
                            enabled: root.controller && root.controller.previewBusy !== true && !root.pageState.applying
                            opacity: enabled ? 1 : 0.5
                            Oran7SettingsGroup {
                                enabled: !root.controller.recordingBusy
                                opacity: enabled ? 1 : 0.5
                                title: qsTranslate("Oran7Ui", "画质设置")
                                Oran7FieldLabel { text: qsTranslate("Oran7Ui", "录制格式") }
                                Oran7Choice {
                                    objectName: "recordingFileFormat"
                                        fieldGlyph: "folder"
                                    model: ["MP4", "FLV"]
                                    selectedRow: root.pageState.outputDraft.format === "flv" ? 1 : 0
                                    onActivated: function(index) {
                                        root.pageState.editOutput("format", index === 1 ? "flv" : "mp4")
                                    }
                                }
                                Oran7FieldLabel { text: qsTranslate("Oran7Ui", "输出尺寸") }
                                Oran7Choice {
                                    objectName: "recordingSizePreset"
                                        fieldGlyph: "fit"
                                    model: [qsTranslate("Oran7Ui", "原始尺寸"), "1080p · 1920 × 1080", "720p · 1280 × 720", qsTranslate("Oran7Ui", "自定义尺寸")]
                                    selectedRow: root.outputSizeIndex
                                    onActivated: function(index) {
                                        root.customOutputSize = index === 3
                                        if (index < 3) {
                                            var sizes = [[0, 0], [1920, 1080], [1280, 720]]
                                            root.pageState.outputDraft = Object.assign({}, root.pageState.outputDraft, {width: sizes[index][0], height: sizes[index][1]})
                                            root.pageState.commitOutput()
                                        }
                                    }
                                }
                                RowLayout {
                                    objectName: "recordingCustomSize"
                                    width: parent.width; spacing: 6
                                    visible: root.outputSizeIndex === 3
                                    Oran7Dimension {
                                        objectName: "recordingWidth"
                                        Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 1
                                        placeholderText: qsTranslate("Oran7Ui", "宽")
                                        text: root.pageState.outputDraft.width === null ? "" : String(root.pageState.outputDraft.width || 0)
                                        onTextEdited: root.pageState.editOutput("width", text.length ? Number(text) : null, true)
                                        onCommitRequested: root.pageState.commitOutput()
                                    }
                                    Label { text: "×"; color: root.mutedTextColor }
                                    Oran7Dimension {
                                        objectName: "recordingHeight"
                                        Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 1
                                        placeholderText: qsTranslate("Oran7Ui", "高")
                                        text: root.pageState.outputDraft.height === null ? "" : String(root.pageState.outputDraft.height || 0)
                                        onTextEdited: root.pageState.editOutput("height", text.length ? Number(text) : null, true)
                                        onCommitRequested: root.pageState.commitOutput()
                                    }
                                }
                                Oran7Caption { visible: root.outputSizeIndex === 3; text: qsTranslate("Oran7Ui", "宽高须为不小于 16 的偶数。") }
                                Oran7FieldLabel { text: qsTranslate("Oran7Ui", "录制帧率上限 · FPS") }
                                Oran7QuickChoices {
                                    objectName: "outputFpsChoices"
                                    options: ["30", "60", qsTranslate("Oran7Ui", "自定义")]
                                    optionEnabled: [root.pageState.maximumOutputFps >= 30, root.pageState.maximumOutputFps >= 60, true]
                                    selectedIndex: root.customOutputFps ? 2 : [30, 60].indexOf(root.pageState.outputDraft.fps) < 0 ? 2 : [30, 60].indexOf(root.pageState.outputDraft.fps)
                                    onSelected: function(index) {
                                        root.customOutputFps = index === 2
                                        if (index < 2) root.pageState.editOutput("fps", [30, 60][index])
                                    }
                                }
                                Oran7Input {
                                    objectName: "recordingFps"
                                    visible: root.customOutputFps || [30, 60].indexOf(root.pageState.outputDraft.fps) < 0
                                    validator: IntValidator { bottom: 1; top: root.pageState.maximumOutputFps }
                                    text: root.pageState.outputDraft.fps === null ? "" : String(root.pageState.outputDraft.fps)
                                    placeholderText: "1–" + root.pageState.maximumOutputFps + " FPS"
                                    onTextEdited: root.pageState.editOutput("fps", text.length ? Number(text) : null, true)
                                    onCommitRequested: root.pageState.commitOutput()
                                }
                                Oran7Caption { text: qsTranslate("Oran7Ui", "上限 ") + root.pageState.maximumOutputFps + qsTranslate("Oran7Ui", " FPS，不得超过左侧采集帧率。") }
                                Oran7FieldLabel { text: qsTranslate("Oran7Ui", "视频码率") }
                                Oran7Choice {
                                    id: bitrateChoice
                                    objectName: "recordingBitratePreset"
                                        fieldGlyph: "info"
                                    model: root.bitratePresets
                                    textRole: "title"
                                    selectedRow: root.bitratePresetIndex
                                    popupWidth: Math.max(width, 350)
                                    maximumPopupHeight: 400
                                    delegate: ItemDelegate {
                                        required property var modelData
                                        required property int index
                                        width: bitrateChoice.popup.width - 8
                                        height: bitrateOption.implicitHeight + 20
                                        highlighted: bitrateChoice.highlightedIndex === index
                                        contentItem: Column {
                                            id: bitrateOption
                                            spacing: 5
                                            Text {
                                                width: parent.width
                                                text: modelData.title
                                                color: index === root.bitratePresetIndex ? root.accentColor : root.textColor
                                                font.pixelSize: 13; font.bold: true
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                width: parent.width
                                                text: modelData.description
                                                color: root.mutedTextColor
                                                font.pixelSize: 11
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                        background: Rectangle { radius: 4; color: parent.highlighted ? root.hoverColor : "transparent" }
                                    }
                                    onActivated: function(index) {
                                        var preset = root.bitratePresets[index]
                                        root.customBitrate = preset.kbps === null
                                        if (!root.customBitrate) root.pageState.editOutput("bitrateKbps", preset.kbps)
                                    }
                                }
                                Oran7Input {
                                    objectName: "recordingBitrate"
                                    visible: root.bitratePresetIndex === root.bitratePresets.length - 1
                                    rightPadding: 50
                                    validator: DoubleValidator { bottom: 0.1; top: 200; decimals: 3; notation: DoubleValidator.StandardNotation; locale: "C" }
                                    text: root.pageState.outputDraft.bitrateKbps === null ? "" : String(Number(root.pageState.outputDraft.bitrateKbps) / 1000)
                                    onTextEdited: root.pageState.editOutput("bitrateKbps", text.length ? Math.round(Number(text) * 1000) : null, true)
                                    onCommitRequested: root.pageState.commitOutput()
                                    Label { anchors { right: parent.right; rightMargin: 9; verticalCenter: parent.verticalCenter } text: "Mbps"; color: root.mutedTextColor; font.pixelSize: 11 }
                                }
                                Oran7Caption {
                                    text: root.bitratePresets[root.bitratePresetIndex].description
                                }
                                Oran7Caption {
                                    objectName: "recordingBitrateStorage"
                                    text: root.bitrateStorageEstimate
                                    color: root.accentColor
                                }
                                Oran7Caption { text: qsTranslate("Oran7Ui", "同一码率下，提高帧率不会增加估算体积，每帧可分配的数据会减少。编码负载还取决于分辨率、帧率和编码器；实际体积会浮动。") }
                            }
                            Oran7SettingsGroup {
                                enabled: !root.controller.recordingBusy
                                opacity: enabled ? 1 : 0.5
                                title: qsTranslate("Oran7Ui", "文件保存")
                                Oran7FieldLabel { text: qsTranslate("Oran7Ui", "保存目录") }
                                Oran7Input {
                                    id: directoryField
                                    objectName: "recordingDirectory"
                                    rightPadding: 34
                                    placeholderText: qsTranslate("Oran7Ui", "系统视频目录")
                                    text: root.pageState.outputDraft.outputDirectory || ""
                                    color: activeFocus ? root.textColor : "transparent"
                                    onTextEdited: root.pageState.editOutput("outputDirectory", text, true)
                                    onCommitRequested: root.pageState.commitOutput()
                                    Label {
                                        objectName: "recordingDirectoryDisplay"
                                        anchors { left: parent.left; right: directoryButton.left; verticalCenter: parent.verticalCenter; leftMargin: 8; rightMargin: 4 }
                                        visible: !directoryField.activeFocus && directoryField.text.length > 0
                                        text: directoryField.text; color: root.textColor; font: directoryField.font; elide: Text.ElideMiddle
                                    }
                                    ToolButton {
                                        id: directoryButton
                                        objectName: "browseRecordingDirectory"
                                        anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 2 }
                                        width: 30; height: 30
                                        icon.source: "qrc:/image/formkit_file.png"
                                        icon.width: 16; icon.height: 16; icon.color: root.mutedTextColor
                                        background: Oran7StudioControlSurface {
                                            polished: root.violetSkin
                                            tintColor: root.accentColor; highlighted: parent.hovered || parent.activeFocus; depressed: parent.down
                                            radius: 4; color: parent.hovered ? root.hoverColor : "transparent"
                                        }
                                        ToolTip.visible: hovered; ToolTip.text: qsTranslate("Oran7Ui", "选择保存文件夹")
                                        Accessible.name: qsTranslate("Oran7Ui", "选择保存文件夹")
                                        onClicked: directoryDialog.open()
                                    }
                                    ToolTip.visible: directoryHover.hovered && text.length > 0
                                    ToolTip.text: text
                                    HoverHandler { id: directoryHover }
                                }
                                Oran7FieldLabel { text: qsTranslate("Oran7Ui", "文件名前缀") }
                                Oran7Input {
                                    objectName: "recordingFilePrefix"
                                    text: root.pageState.outputDraft.filePrefix || ""
                                    onTextEdited: root.pageState.editOutput("filePrefix", text, true)
                                    onCommitRequested: root.pageState.commitOutput()
                                }
                                Oran7Caption { text: qsTranslate("Oran7Ui", "自动追加后缀，避免覆盖。") }
                            }
                            Oran7SettingsGroup {
                                visible: root.liveSupported
                                objectName: "liveSettingsGroup"
                                enabled: !root.controller.liveBusy
                                opacity: enabled ? 1 : 0.5
                                title: qsTranslate("Oran7Ui", "直播推流")
                                Oran7Choice {
                                    model: [qsTranslate("Oran7Ui", "B站直播"), qsTranslate("Oran7Ui", "自定义 RTMP")]
                                    selectedRow: root.liveDraft.platform === "custom" ? 1 : 0
                                    onActivated: function(index) { root.editLive("platform", index ? "custom" : "bilibili") }
                                }
                                Oran7FieldLabel { text: qsTranslate("Oran7Ui", "推流服务器地址") }
                                Oran7Input {
                                    objectName: "liveServer"
                                    text: root.liveDraft.server || ""
                                    placeholderText: "rtmp://server/live"
                                    maximumLength: 8192
                                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                                    onTextEdited: root.editLive("server",text)
                                    onCommitRequested: root.saveLive()
                                }
                                Oran7FieldLabel { text: qsTranslate("Oran7Ui", "推流码") }
                                Oran7Input {
                                    objectName: "liveStreamKey"
                                    text: root.liveDraft.streamKey || ""
                                    maximumLength: 8192
                                    echoMode: liveKeyVisible.checked ? TextInput.Normal : TextInput.Password
                                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                                    onTextEdited: root.editLive("streamKey",text)
                                    onCommitRequested: root.saveLive()
                                }
                                Oran7Action {
                                    id: liveKeyVisible
                                    checkable: true
                                    text: checked ? qsTranslate("Oran7Ui", "隐藏推流码") : qsTranslate("Oran7Ui", "显示推流码")
                                }
                                Oran7Action {
                                    text: qsTranslate("Oran7Ui", "清除推流凭据")
                                    onClicked: { root.editLive("server", ""); root.editLive("streamKey", ""); root.saveLive() }
                                }
                                Oran7FieldLabel { text: qsTranslate("Oran7Ui", "直播画质") }
                                Oran7Choice {
                                    model: ["720p30", "720p60", "1080p30", "1080p60", "1440p60"]
                                    selectedRow: root.liveDraft.width === 1280 ? (root.liveDraft.fps === 60 ? 1 : 0)
                                        : root.liveDraft.width === 2560 ? 4 : root.liveDraft.fps === 30 ? 2 : 3
                                    onActivated: function(index) {
                                        var sizes = [[1280,720,30],[1280,720,60],[1920,1080,30],[1920,1080,60],[2560,1440,60]]
                                        root.editLive("width", sizes[index][0]); root.editLive("height", sizes[index][1]); root.editLive("fps", sizes[index][2])
                                    }
                                }
                                Oran7FieldLabel { text: qsTranslate("Oran7Ui", "直播码率 · kbps") }
                                Oran7Input {
                                    objectName: "liveBitrate"
                                    text: String(root.liveDraft.bitrateKbps || "")
                                    validator: IntValidator { bottom: 100; top: 50000 }
                                    onTextEdited: root.editLive("bitrateKbps", Number(text))
                                    onCommitRequested: root.saveLive()
                                }
                                Oran7Action {
                                    checkable: true
                                    checked: !!root.liveDraft.audioEnabled
                                    text: qsTranslate("Oran7Ui", "直播系统声音")
                                    onClicked: root.editLiveAudio("audioEnabled", checked)
                                }
                                Oran7Caption { text: qsTranslate("Oran7Ui", "可与本地录制同时运行；直播帧率不能超过采集帧率。") }
                                Oran7Caption { text: qsTranslate("Oran7Ui", root.liveMessage); visible: text.length > 0 }
                            }
                        }
                    }
                }
            }
        }
    }
    FolderDialog {
        id: directoryDialog
        title: qsTranslate("Oran7Ui", "选择录像保存目录")
        onAccepted: {
            if (root.pageState.busy) return
            var value = selectedFolder.toString()
            var path = value.indexOf("file:///") === 0 ? value.slice(8) : "//" + value.slice(7)
            root.pageState.editOutput("outputDirectory", decodeURIComponent(path))
        }
    }
}
