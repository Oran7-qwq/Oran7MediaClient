import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../Oran7SettingUiWindowItems"
import Oran7UI.Impl

Pane {
    id: root
    padding: 10
    font.family: root.fontFamily
    font.pixelSize: root.textPixelSize
    required property var service
    required property var controller
    property Component previewComponent
    // 工作台自行管理样式，调用方可覆盖；不借用下拉设置窗口的样式单例。
    property color panelColor: "#22252d"
    property color baseColor: "#171a20"
    property color textColor: "#e1e5ed"
    property color accentColor: "#beb7ff"
    property color hoverColor: "#363b47"
    property color groupColor: Qt.tint(root.panelColor, "#09ffffff")
    property color inputColor: root.baseColor
    property color borderColor: Qt.tint(root.panelColor, "#20ffffff")
    property color mutedTextColor: Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.58)
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
    property color previewBackgroundColor: "#101216"
    property color previewTextColor: "#dddddd"
    property color errorColor: "#e36a75"
    property color toggleOffColor: root.inputColor
    property color toggleBorderColor: root.accentColor
    property color toggleThumbColor: "white"
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
    palette.highlight: root.accentColor
    background: Rectangle { color: "transparent" }

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
        property bool modified: false
        property Component footer
        color: root.panelColor
        radius: 10
        clip: true
        RowLayout {
            id: panelHeading
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
            spacing: 6
            Rectangle { width: 3; height: 15; radius: 1.5; color: root.accentColor }
            Label { text: panel.title; color: root.textColor; font.pixelSize: 15; font.bold: true; Layout.fillWidth: true }
            Label {
                objectName: panel.objectName + "Status"
                text: panel.statusText
                color: panel.modified ? root.accentColor : root.mutedTextColor
                font.pixelSize: 10
            }
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
        radius: 8
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
        enabled: !root.pageState.busy
        color: root.textColor
        placeholderTextColor: root.mutedTextColor
        selectionColor: root.accentColor
        selectedTextColor: root.inputColor
        font.pixelSize: root.controlPixelSize
        padding: 8
        background: Rectangle {
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
        leftPadding: 9
        rightPadding: 24
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
        background: Rectangle {
            radius: 6; color: root.inputColor
            border.color: choice.activeFocus ? root.accentColor : root.borderColor
        }
        delegate: ItemDelegate {
            required property int index
            width: choice.popupWidth
            height: 34
            highlighted: choice.highlightedIndex === index
            contentItem: Text { text: choice.textAt(index); color: root.textColor; font.pixelSize: root.controlPixelSize; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            background: Rectangle { radius: 4; color: parent.highlighted ? root.hoverColor : "transparent" }
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
                    color: choices.selectedIndex === index ? root.accentColor : root.textColor
                }
                background: Rectangle {
                    radius: 5
                    color: choices.selectedIndex === index ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.12)
                        : parent.hovered ? root.hoverColor : root.inputColor
                    border.color: parent.activeFocus || choices.selectedIndex === index ? root.accentColor : root.borderColor
                }
                onClicked: if (enabled) choices.selected(index)
            }
        }
    }
    component Oran7Action: Oran7StudioButton {
        accentColor: root.accentColor
        textColor: root.textColor
        surfaceColor: root.groupColor
        hoverColor: root.hoverColor
        borderColor: root.borderColor
        dangerColor: root.errorColor
        font.family: root.fontFamily
    }
    component Oran7WorkspaceSwitch: Oran7SwitchToggleItem {
        onColor: root.accentColor
        offColor: root.toggleOffColor
        borderColor: root.toggleBorderColor
        thumbColor: root.toggleThumbColor
        controlHeight: root.toggleHeight
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTranslate("Oran7Ui", "直播工作台"); color: root.textColor; font.pixelSize: 19; font.bold: true }
            Rectangle {
                implicitWidth: formatBadge.implicitWidth + 16; implicitHeight: 24
                radius: 5; color: root.groupColor; border.color: root.borderColor
                Label { id: formatBadge; anchors.centerIn: parent; text: root.pageState.outputDraft.format === "flv" ? qsTranslate("Oran7Ui", "本地 FLV") : qsTranslate("Oran7Ui", "本地 MP4"); color: root.mutedTextColor; font.pixelSize: 11 }
            }
            Item { Layout.fillWidth: true }
            Oran7Action { text: qsTranslate("Oran7Ui", "还原设置"); enabled: !root.pageState.busy; onClicked: {
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
            // 始终保留左右工具栏；空间不足时滚动整行，不改变预览与工具栏的位置关系。
            contentWidth: Math.max(availableWidth, 790)
            ScrollBar.horizontal.policy: ScrollBar.AsNeeded
            GridLayout {
                id: grid
                objectName: "recordingStudioGrid"
                width: workspaceScroll.contentWidth
                height: Math.max(200 + recordingToolbar.implicitHeight + 10, workspaceScroll.availableHeight)
                columns: 3
                columnSpacing: 10; rowSpacing: 10
                ColumnLayout {
                    objectName: "recordingCenterColumn"
                    Layout.row: 0
                    Layout.column: 1
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 500
                    Layout.minimumWidth: 280
                    Layout.preferredHeight: 380
                    spacing: 10
                    Oran7RecordingPreview {
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
                        panelColor: root.panelColor
                        canvasColor: root.previewBackgroundColor
                        textColor: root.previewTextColor
                        mutedTextColor: root.mutedTextColor
                        accentColor: root.accentColor
                        borderColor: root.borderColor
                        errorColor: root.errorColor
                        fontFamily: root.fontFamily
                        showInfo: root.showScreenInfo
                        fillPreview: root.fillPreview
                        onInfoToggled: function(checked) { root.showScreenInfo = checked }
                        onScaleToggled: function(checked) { root.fillPreview = checked; root.previewScaleChanged(checked) }
                    }
                    Oran7RecordingToolbar {
                        id: recordingToolbar
                        objectName: "recordingCenterToolbar"
                        Layout.fillWidth: true
                        pageState: state
                        controller: root.controller
                        panelColor: root.panelColor
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
                    }
                }
                Oran7StudioPanel {
                    objectName: "recordingSourcePanel"
                    title: qsTranslate("Oran7Ui", "画面来源")
                    statusText: root.pageState.applying ? qsTranslate("Oran7Ui", "应用中") : root.pageState.captureDirty ? qsTranslate("Oran7Ui", "待保存") : qsTranslate("Oran7Ui", "已保存")
                    modified: root.pageState.captureDirty
                    Layout.row: 0; Layout.column: 0
                    Layout.fillHeight: true
                    Layout.minimumWidth: 230; Layout.preferredWidth: 230; Layout.maximumWidth: 230
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
                                        background: Rectangle { radius: 4; color: parent.highlighted ? root.hoverColor : "transparent" }
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
                                    background: Rectangle { radius: 5; color: parent.hovered ? root.hoverColor : "transparent" }
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
                                background: Rectangle { radius: 5; color: parent.hovered ? root.hoverColor : "transparent" }
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
                    objectName: "recordingOutputPanel"
                    title: qsTranslate("Oran7Ui", "录制输出")
                    statusText: root.pageState.outputDirty ? qsTranslate("Oran7Ui", "待保存") : qsTranslate("Oran7Ui", "已保存")
                    modified: root.pageState.outputDirty
                    Layout.row: 0; Layout.column: 2
                    Layout.fillHeight: true
                    Layout.minimumWidth: 260; Layout.preferredWidth: 260; Layout.maximumWidth: 260
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredHeight: 380
                    Column {
                        width: parent.width; spacing: 12
                        enabled: !root.pageState.busy
                        opacity: enabled ? 1 : 0.5
                        Oran7SettingsGroup {
                            title: qsTranslate("Oran7Ui", "画质设置")
                            Oran7FieldLabel { text: qsTranslate("Oran7Ui", "录制格式") }
                            Oran7Choice {
                                objectName: "recordingFileFormat"
                                model: ["MP4", "FLV"]
                                selectedRow: root.pageState.outputDraft.format === "flv" ? 1 : 0
                                onActivated: function(index) {
                                    root.pageState.editOutput("format", index === 1 ? "flv" : "mp4")
                                }
                            }
                            Oran7FieldLabel { text: qsTranslate("Oran7Ui", "输出尺寸") }
                            Oran7Choice {
                                objectName: "recordingSizePreset"
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
                                    background: Rectangle { radius: 4; color: parent.hovered ? root.hoverColor : "transparent" }
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
