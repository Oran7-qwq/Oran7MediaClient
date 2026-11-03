import QtQuick
import QtQuick.Controls
import Oran7UI.Impl
import "../../Settings/GlobalSettings"

ComboBox {
    id: control
    implicitWidth: 190
    implicitHeight: 38
    property string badge: "Aa"
    property string popupTitle: ""
    property bool previewFonts: false
    property bool popupStartsAtTop: false
    property var colorSamples: []
    property var subtitles: []
    property color accentColor: Oran7Theme.Oran7MainGUI.themeColor
    readonly property color ink: Oran7MainUiSetting.textColor
    readonly property color surface: Oran7MainUiSetting.itemBackColor
    property bool popupTracked: false
    font.family: Oran7MainUiSetting.fontFamily
    font.pixelSize: 13
    hoverEnabled: true
    leftPadding: 40
    rightPadding: 28

    function releasePopup() {
        if (!popupTracked) return
        popupTracked = false
        if (Oran7MainUiSetting.activeSettingsPopup === popup)
            Oran7MainUiSetting.activeSettingsPopup = null
        Oran7MainUiSetting.activeOtherItemCount = Math.max(0, Oran7MainUiSetting.activeOtherItemCount - 1)
    }
    onVisibleChanged: if (!visible) popup.close()
    Component.onDestruction: releasePopup()

    background: Rectangle {
        radius: 8
        color: control.hovered || control.popup.visible
            ? Qt.rgba(control.accentColor.r, control.accentColor.g, control.accentColor.b, 0.12) : control.surface
        border.width: 1
        border.color: control.activeFocus || control.popup.visible ? control.accentColor
            : Qt.rgba(control.ink.r, control.ink.g, control.ink.b, control.hovered ? 0.25 : 0.10)
        Behavior on color { ColorAnimation { duration: 140 } }
        Behavior on border.color { ColorAnimation { duration: 140 } }
        Rectangle {
            x: 7; anchors.verticalCenter: parent.verticalCenter
            width: 25; height: 25; radius: 6
            color: Qt.rgba(control.accentColor.r, control.accentColor.g, control.accentColor.b, 0.14)
            Text { anchors.centerIn: parent; text: control.badge; color: control.ink; font.pixelSize: 11 }
        }
    }
    contentItem: Text {
        text: control.displayText
        color: control.enabled ? control.ink : Qt.rgba(control.ink.r, control.ink.g, control.ink.b, 0.4)
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Item {
        x: control.width - width - 12; anchors.verticalCenter: parent.verticalCenter
        width: 10; height: 8
        rotation: control.popup.visible ? 180 : 0
        Behavior on rotation { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Rectangle { x: 0; y: 3; width: 6; height: 1.5; radius: 0.75; rotation: 45; color: control.ink }
        Rectangle { x: 4; y: 3; width: 6; height: 1.5; radius: 0.75; rotation: -45; color: control.ink }
    }
    ToolTip.visible: hovered && !popup.visible
    ToolTip.text: displayText

    delegate: ItemDelegate {
        id: option
        required property int index
        objectName: control.objectName + "Option" + index
        text: control.textAt(index)
        width: ListView.view ? ListView.view.width : control.width
        height: control.previewFonts || control.subtitles.length ? 52 : 40
        highlighted: control.highlightedIndex === index
        hoverEnabled: true
        padding: 9
        contentItem: Item {
            Rectangle {
                id: swatch
                visible: option.index < control.colorSamples.length
                width: 26; height: 26; radius: 6
                anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                color: visible ? control.colorSamples[option.index] : "transparent"
                border.width: 1; border.color: Qt.rgba(control.ink.r, control.ink.g, control.ink.b, 0.18)
            }
            Column {
                anchors.left: swatch.visible ? swatch.right : parent.left; anchors.leftMargin: swatch.visible ? 10 : 0; anchors.right: selectedMark.left; anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter; spacing: 3
                Text {
                    width: parent.width; text: control.textAt(option.index)
                    color: control.ink; font.family: control.font.family
                    font.pixelSize: 13; font.bold: control.currentIndex === option.index
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    visible: control.previewFonts || control.subtitles.length > 0
                    text: control.previewFonts ? qsTranslate("Oran7Ui", "中文预览 Aa 0123") : (control.subtitles[option.index] || "")
                    color: Qt.rgba(control.ink.r, control.ink.g, control.ink.b, 0.6)
                    font.family: control.previewFonts ? control.textAt(option.index) : control.font.family
                    font.pixelSize: 11; elide: Text.ElideRight
                }
            }
            Item {
                id: selectedMark
                width: 20; height: 20; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                visible: control.currentIndex === option.index
                Rectangle { x: 3; y: 10; width: 5; height: 2; radius: 1; rotation: 45; color: control.accentColor }
                Rectangle { x: 6; y: 8; width: 10; height: 2; radius: 1; rotation: -45; color: control.accentColor }
            }
        }
        background: Rectangle {
            radius: 7
            color: control.currentIndex === option.index || option.highlighted || option.hovered
                ? Qt.rgba(control.accentColor.r, control.accentColor.g, control.accentColor.b, control.currentIndex === option.index ? 0.18 : 0.09)
                : "transparent"
            Behavior on color { ColorAnimation { duration: 110 } }
        }
    }
    popup: Popup {
        id: menu
        objectName: control.objectName + "Popup"
        parent: Overlay.overlay
        padding: 8
        width: Math.min(control.previewFonts ? 290 : 248, parent ? parent.width - 16 : 248)
        height: Math.min(346, 44 + options.contentHeight + topPadding + bottomPadding,
                         parent ? parent.height - 24 : 346)
        x: parent ? Math.max(8, Math.min(control.mapToItem(parent, 0, 0).x, parent.width - width - 8)) : 0
        y: {
            if (!parent) return control.height + 6
            var below = control.mapToItem(parent, 0, control.height).y + 6
            return below + height <= parent.height - 8 ? below
                : Math.max(8, control.mapToItem(parent, 0, 0).y - height - 6)
        }
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        transformOrigin: Item.Top
        onAboutToShow: {
            if (Oran7MainUiSetting.activeSettingsPopup && Oran7MainUiSetting.activeSettingsPopup !== menu)
                Oran7MainUiSetting.activeSettingsPopup.close()
            Oran7MainUiSetting.activeSettingsPopup = menu
            if (!control.popupTracked) {
                control.popupTracked = true
                Oran7MainUiSetting.activeOtherItemCount++
            }
            options.positionViewAtIndex(control.popupStartsAtTop ? 0 : Math.max(0, control.currentIndex),
                                        control.popupStartsAtTop ? ListView.Beginning : ListView.Center)
            // ComboBox 随后的默认定位会滚到当前项；在下一次布局前恢复颜色菜单的首项。
            if (control.popupStartsAtTop) Qt.callLater(function() {
                if (menu.visible) options.positionViewAtIndex(0, ListView.Beginning)
            })
        }
        onClosed: control.releasePopup()
        background: Rectangle {
            radius: 11
            color: Oran7MainUiSetting.backColor
            border.width: 1
            border.color: Qt.rgba(control.ink.r, control.ink.g, control.ink.b, 0.18)
            Rectangle {
                x: 1; y: 1; width: parent.width - 2; height: parent.height - 2
                radius: 10; color: Qt.rgba(control.accentColor.r, control.accentColor.g, control.accentColor.b, 0.035)
            }
        }
        contentItem: Column {
            spacing: 6
            Item {
                width: parent.width; height: 30
                Text {
                    anchors.left: parent.left; anchors.leftMargin: 5; anchors.verticalCenter: parent.verticalCenter
                    text: control.popupTitle; color: control.ink; font.pixelSize: 12; font.bold: true
                }
                Text {
                    anchors.right: parent.right; anchors.rightMargin: 5; anchors.verticalCenter: parent.verticalCenter
                    text: control.count; color: Qt.rgba(control.ink.r, control.ink.g, control.ink.b, 0.45); font.pixelSize: 11
                }
            }
            ListView {
                id: options
                width: parent.width
                height: Math.max(0, menu.availableHeight - 36)
                clip: true
                model: control.delegateModel
                currentIndex: control.highlightedIndex
                spacing: 3
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            }
        }
        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 210; easing.type: Easing.OutCubic }
            }
        }
        exit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; to: 0; duration: 110; easing.type: Easing.InCubic }
                NumberAnimation { property: "scale"; to: 0.98; duration: 110; easing.type: Easing.InCubic }
            }
        }
    }
}
