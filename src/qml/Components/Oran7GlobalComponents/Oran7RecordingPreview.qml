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
    property color panelColor: "#22252d"
    property color canvasColor: "#101216"
    property color textColor: "#e1e5ed"
    property color mutedTextColor: "#8d939f"
    property color accentColor: "#beb7ff"
    property color borderColor: "#424650"
    property color errorColor: "#e36a75"
    property string fontFamily: ""
    signal infoToggled(bool checked)
    signal scaleToggled(bool checked)
    readonly property bool recording: controller && controller.recordingState === "Recording"
    readonly property bool unavailable: controller && controller.previewState !== undefined && controller.previewState !== "Running"
    color: root.canvasColor
    radius: 12
    border.color: root.recording ? Qt.rgba(errorColor.r, errorColor.g, errorColor.b, 0.65) : root.borderColor
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
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 12; color: root.panelColor }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12; anchors.rightMargin: 12
                spacing: 10
                Rectangle {
                    Layout.preferredWidth: 32; Layout.minimumWidth: 32; Layout.maximumWidth: 32
                    Layout.preferredHeight: 32
                    radius: 9
                    color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.10)
                    Oran7RecordingIcon { anchors.centerIn: parent; glyph: "monitor"; ink: root.accentColor }
                }
                ColumnLayout {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    spacing: 3
                    Label {
                        Layout.fillWidth: true
                        text: root.sourceName || qsTranslate("Oran7Ui", "画面预览")
                        color: root.textColor; font.family: root.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: root.sourceDetails || qsTranslate("Oran7Ui", "画面预览")
                        color: root.mutedTextColor; font.family: root.fontFamily; font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }
                Rectangle {
                    objectName: "recordingPreviewBadge"
                    Layout.preferredWidth: badgeRow.implicitWidth + 16
                    Layout.minimumWidth: badgeRow.implicitWidth + 16
                    Layout.maximumWidth: badgeRow.implicitWidth + 16
                    Layout.preferredHeight: 25
                    radius: 12
                    color: root.recording ? Qt.rgba(root.errorColor.r, root.errorColor.g, root.errorColor.b, 0.13) : "#0cffffff"
                    Row {
                        id: badgeRow
                        anchors.centerIn: parent; spacing: 6
                        Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 6; height: 6; radius: 3; color: root.recording ? root.errorColor : root.unavailable ? root.mutedTextColor : root.accentColor }
                        Label { visible: root.width >= 420 || root.recording; text: root.recording ? qsTranslate("Oran7Ui", "正在录制") : qsTranslate("Oran7Ui", "画面预览"); color: root.recording ? root.errorColor : root.mutedTextColor; font.family: root.fontFamily; font.pixelSize: 10 }
                    }
                }
            }
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: root.borderColor; opacity: 0.5 }
        Item {
            id: canvas
            objectName: "recordingPreviewCanvas"
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0
            Loader {
                id: preview
                anchors.fill: parent; anchors.margins: 10
                sourceComponent: root.previewComponent
            }
            // 状态遮罩不销毁 Loader，确保首次挂载与自动恢复可以继续推进。
            Rectangle {
                anchors.fill: parent
                color: root.canvasColor
                visible: root.unavailable
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
        background: Rectangle {
            radius: 6
            color: action.checked ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.13) : action.hovered ? "#12ffffff" : "transparent"
            border.color: action.activeFocus ? root.accentColor : "transparent"
            Behavior on color { ColorAnimation { duration: 100 } }
        }
    }
}
