import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 预览容器只负责布局/反馈，视频仍由原 Loader 内的生产渲染组件消费。
Rectangle {
    id: root
    required property var controller
    required property var pageState
    property Component previewComponent
    property string sourceName: ""
    property string sourceDetails: ""
    property bool showInfo: false
    property bool fillPreview: false
    property bool violetSkin: false
    property real materialStrength: 0.025
    property color paperColor: "#d5dfec"
    property color paperInkColor: "#263447"
    property color titleSurfaceColor: "#161e29"
    property color panelColor: "#1e252f"
    property color canvasColor: "#0f141c"
    property color textColor: "#e7edf5"
    property color mutedTextColor: "#a3afbf"
    readonly property color standbyColor: violetSkin ? "#a6dce6f8" : canvasColor
    property color accentColor: "#80a9db"
    property color recordingColor: "#e7a399"
    property color streamingColor: "#6dcbb7"
    property color borderColor: "#334154"
    property color errorColor: "#ed8795"
    property string fontFamily: ""
    signal infoToggled(bool checked)
    signal scaleToggled(bool checked)
    readonly property bool recording: controller && controller.recordingState === "Recording"
    readonly property bool streaming: !!controller && !!controller.liveSession && controller.liveSession.state === "Streaming"
    readonly property bool unavailable: controller && controller.previewState !== undefined && controller.previewState !== "Running"
    color: root.violetSkin ? "transparent" : root.canvasColor
    radius: 9
    border.color: root.recording ? Qt.rgba(recordingColor.r, recordingColor.g, recordingColor.b, 0.65) : root.borderColor
    clip: true
    Behavior on border.color { ColorAnimation { duration: 160 } }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            Layout.minimumWidth: 0
            color: root.panelColor
            radius: 11
            bottomLeftRadius: root.violetSkin ? 0 : radius
            bottomRightRadius: bottomLeftRadius
            Rectangle { visible: !root.violetSkin; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 12; color: root.panelColor }
            Oran7StudioMaterial { anchors.fill: parent; anchors.margins: 2; grainOpacity: root.materialStrength }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12; anchors.rightMargin: 12
                spacing: 10
                Rectangle {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    Layout.preferredHeight: 40
                    radius: 5; color: root.titleSurfaceColor
                    border.color: Qt.rgba(root.borderColor.r,root.borderColor.g,root.borderColor.b,.55)
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 10
                        spacing: 9
                        Oran7RecordingIcon {
                            Layout.preferredWidth: 22; Layout.preferredHeight: 22
                            glyph: "monitor"; ink: root.accentColor
                        }
                        Rectangle { width: 1; Layout.preferredHeight: 24; color: Qt.rgba(root.borderColor.r,root.borderColor.g,root.borderColor.b,.8) }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 2
                            Label {
                                Layout.fillWidth: true
                                text: "02 / " + (root.sourceName || qsTranslate("Oran7Ui", "画面预览"))
                                color: root.textColor; font.family: root.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: root.sourceDetails || qsTranslate("Oran7Ui", "画面预览")
                                color: root.mutedTextColor
                                font.family: root.fontFamily; font.pixelSize: 10; elide: Text.ElideRight
                            }
                        }
                    }
                }
                Rectangle {
                    objectName: "recordingPreviewBadge"
                    Layout.preferredWidth: badgeRow.implicitWidth + 16
                    Layout.minimumWidth: badgeRow.implicitWidth + 16
                    Layout.maximumWidth: badgeRow.implicitWidth + 16
                    Layout.preferredHeight: 25
                    radius: 12
                    color: root.recording ? Qt.rgba(root.recordingColor.r, root.recordingColor.g, root.recordingColor.b, 0.13) : "#0cffffff"
                    Row {
                        id: badgeRow
                        anchors.centerIn: parent; spacing: 6
                        Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 6; height: 6; radius: 3; color: root.recording ? root.recordingColor : root.streaming ? root.streamingColor : root.unavailable ? root.mutedTextColor : root.accentColor }
                        Label { visible: root.width >= 420 || root.recording || root.streaming; text: root.recording && root.streaming ? "REC + LIVE" : root.recording ? qsTranslate("Oran7Ui", "正在录制") : root.streaming ? qsTranslate("Oran7Ui", "推流中") : qsTranslate("Oran7Ui", "画面预览"); color: root.recording ? root.recordingColor : root.streaming ? root.streamingColor : root.mutedTextColor; font.family: root.fontFamily; font.pixelSize: 10 }
                    }
                }
            }
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: root.borderColor; opacity: 0.5 }
        Item {
            id: canvas
            objectName: "recordingPreviewCanvas"
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0
            Rectangle {
                anchors.fill: parent
                visible: root.violetSkin && !root.unavailable
                color: root.canvasColor
                bottomLeftRadius: 8
                bottomRightRadius: 8
            }
            Loader {
                id: preview
                anchors.fill: parent; anchors.margins: 10
                sourceComponent: root.previewComponent
                opacity: root.violetSkin && root.unavailable ? 0 : 1
            }
            Rectangle {
                anchors.fill: parent; anchors.margins: 6
                color: "transparent"; border.color: "#14ffffff"; radius: 3
            }
            // 状态遮罩不销毁 Loader，确保首次挂载与自动恢复可以继续推进。
            Rectangle {
                objectName: "recordingPreviewStandby"
                anchors.fill: parent
                color: root.standbyColor
                visible: root.unavailable
                bottomLeftRadius: root.violetSkin ? 8 : 0
                bottomRightRadius: bottomLeftRadius
                // 紫色皮肤的待机遮罩保留磨砂背景，开启采集后由预览画布遮住。
                Item {
                    anchors.fill: parent
                    clip: true
                    visible: !root.violetSkin
                    Oran7StudioMaterial { anchors.fill: parent; grainOpacity: root.materialStrength }
                    Oran7StudioMotif {
                        width: Math.min(280, parent.width * 0.55, parent.height * 0.75)
                        height: width; anchors.centerIn: parent
                        visible: !root.violetSkin
                        ink: root.mutedTextColor; opacity: 0.055
                    }
                }
                Column {
                    anchors.centerIn: parent
                    width: Math.max(0, parent.width - 32)
                    spacing: 12
                    Oran7RecordingIcon { anchors.horizontalCenter: parent.horizontalCenter; width: 32; height: 32; ink: root.mutedTextColor }
                    Label {
                        width: parent.width
                        text: root.pageState.previewHint || (root.controller && root.controller.previewError) || qsTranslate("Oran7Ui", "预览尚未启动")
                        horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                        color: root.mutedTextColor; font.family: root.fontFamily; font.pixelSize: 12
                    }
                }
            }
            Rectangle {
                id: tools
                objectName: "recordingPreviewTools"
                anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 14
                width: toolRow.implicitWidth + 8; height: 36; radius: 10
                color: Qt.rgba(root.panelColor.r, root.panelColor.g, root.panelColor.b, 0.94)
                border.color: Qt.rgba(root.borderColor.r, root.borderColor.g, root.borderColor.b, 0.8)
                opacity: toolHover.hovered || infoButton.activeFocus || scaleButton.activeFocus ? 1 : 0.8
                Behavior on opacity { NumberAnimation { duration: 140 } }
                Rectangle { z: -1; x: 0; y: 3; width: parent.width; height: parent.height; radius: 10; color: "#28000000" }
                HoverHandler { id: toolHover }
                Row {
                    id: toolRow
                    anchors.centerIn: parent; spacing: 3
                    Oran7PreviewAction {
                        id: infoButton
                        objectName: "recordingPreviewInfo"
                        glyph: "info"; text: qsTranslate("Oran7Ui", "画面信息")
                        checked: root.showInfo
                        onToggled: root.infoToggled(checked)
                    }
                    Rectangle { width: 1; height: 14; anchors.verticalCenter: parent.verticalCenter; color: root.borderColor }
                    Oran7PreviewAction {
                        id: scaleButton
                        objectName: "recordingPreviewScale"
                        glyph: "fit"; text: root.fillPreview ? qsTranslate("Oran7Ui", "填充") : qsTranslate("Oran7Ui", "适应")
                        checked: root.fillPreview
                        onToggled: root.scaleToggled(checked)
                    }
                }
            }
        }
    }
    // 两路状态独立，切换时不改变舞台或工具区的尺寸。
    Row {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.bottomMargin: 3
        spacing: 4; height: 3
        Rectangle {
            width: (parent.width - 4) / 2; height: 3; radius: 1
            color: root.recording ? root.recordingColor : "#233b454d"
            Behavior on color { ColorAnimation { duration: 240 } }
        }
        Rectangle {
            width: (parent.width - 4) / 2; height: 3; radius: 1
            color: root.streaming ? root.streamingColor : "#233b454d"
            Behavior on color { ColorAnimation { duration: 240 } }
        }
    }

    component Oran7PreviewAction: Button {
        id: action
        property string glyph
        checkable: true
        implicitWidth: row.implicitWidth + 16; implicitHeight: 28
        hoverEnabled: true
        contentItem: Row {
            id: row
            spacing: 5
            Oran7RecordingIcon { anchors.verticalCenter: parent.verticalCenter; width: 15; height: 15; glyph: action.glyph; ink: action.checked ? root.accentColor : root.textColor }
            Label { text: action.text; anchors.verticalCenter: parent.verticalCenter; color: action.checked ? root.accentColor : root.textColor; font.family: root.fontFamily; font.pixelSize: 11 }
        }
        background: Oran7StudioControlSurface {
            polished: root.violetSkin; tintColor: root.accentColor; highlighted: action.checked || action.hovered
            radius: 6
            color: action.checked || (root.violetSkin && action.hovered) ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.13) : action.hovered ? "#12ffffff" : "transparent"
            border.color: action.activeFocus ? root.accentColor : "transparent"
            Behavior on color { ColorAnimation { duration: 100 } }
        }
    }
}
