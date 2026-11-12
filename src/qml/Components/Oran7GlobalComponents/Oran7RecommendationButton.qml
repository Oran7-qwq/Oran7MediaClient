import QtQuick
import QtQuick.Controls

Button {
    id: control
    property var queue
    property int mode: 0
    property color ink: "#fef2e8"
    property color accent: "#ff9c6e"
    property color surface: "#242936"
    readonly property var modes: [0, 3, 4, 5, 6]
    readonly property var titles: [qsTranslate("Oran7Ui", "关闭推荐"), qsTranslate("Oran7Ui", "心动模式"),
        qsTranslate("Oran7Ui", "私人漫游"), qsTranslate("Oran7Ui", "熟悉模式"), qsTranslate("Oran7Ui", "探索模式")]
    readonly property var descriptions: [qsTranslate("Oran7Ui", "回到原列表，沿用播放顺序"),
        qsTranslate("Oran7Ui", "以当前网易云歌曲为种子，延续相近风格"),
        qsTranslate("Oran7Ui", "按个人口味持续推荐，无需先选择歌曲"),
        qsTranslate("Oran7Ui", "偏向熟悉的音乐，重温合口味的旋律"),
        qsTranslate("Oran7Ui", "探索更多音乐，不保证全部为未听歌曲")]
    objectName: "recommendationButton"
    implicitWidth: 104
    implicitHeight: 32
    hoverEnabled: true
    text: mode >= 3 ? titles[modes.indexOf(mode)] : qsTranslate("Oran7Ui", "网易云推荐")
    onClicked: menu.open()
    onVisibleChanged: if (!visible) menu.close()
    background: Rectangle {
        radius: 8
        color: Qt.alpha(control.accent, control.mode >= 3 ? 0.22 : control.hovered ? 0.12 : 0.05)
        border.width: 1
        border.color: Qt.alpha(control.accent, control.mode >= 3 ? 0.65 : 0.25)
        Behavior on color { ColorAnimation { duration: 160 } }
    }
    contentItem: Text {
        text: control.text; rightPadding: 10; color: control.mode >= 3 ? control.accent : control.ink
        font.pixelSize: 12; font.bold: control.mode >= 3
        verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter
    }
    Item {
        anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
        width: 8; height: 6; rotation: menu.visible ? 180 : 0
        Behavior on rotation { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Rectangle { x: 0; y: 2; width: 5; height: 1.5; radius: 0.75; rotation: -45; color: control.ink }
        Rectangle { x: 3; y: 2; width: 5; height: 1.5; radius: 0.75; rotation: 45; color: control.ink }
    }
    ToolTip.visible: hovered && !menu.visible
    ToolTip.text: queue && queue.recommendationError ? qsTranslate("Oran7Ui", queue.recommendationError) : control.text
    Popup {
        id: menu
        objectName: "recommendationPopup"
        parent: Overlay.overlay
        width: Math.min(326, parent ? parent.width - 16 : 326)
        height: Math.min(402, parent ? parent.height - 16 : 402)
        x: parent ? Math.max(8, Math.min(control.mapToItem(parent, 0, 0).x, parent.width - width - 8)) : 0
        y: parent ? Math.max(8, control.mapToItem(parent, 0, 0).y - height - 8) : 0
        padding: 10
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        transformOrigin: Item.Bottom
        background: Rectangle {
            radius: 12; color: control.surface
            border.width: 1; border.color: Qt.alpha(control.ink, 0.18)
        }
        contentItem: Column {
            spacing: 8
            Text { text: qsTranslate("Oran7Ui", "网易云推荐"); color: control.ink; font.bold: true; font.pixelSize: 14; height: 24 }
            ListView {
                width: parent.width; height: Math.max(0, menu.availableHeight - 78)
                clip: true; model: control.modes; spacing: 4
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                delegate: ItemDelegate {
                    id: option
                    required property int index
                    objectName: "recommendationOption" + control.modes[index]
                    width: ListView.view.width; height: 52
                    hoverEnabled: true
                    onClicked: {
                        if (control.queue && control.queue.selectRecommendation(control.modes[index])) menu.close()
                    }
                    background: Rectangle {
                        radius: 8
                        color: Qt.alpha(control.accent, control.mode === control.modes[option.index] ? 0.20 : option.hovered ? 0.10 : 0)
                        Behavior on color { ColorAnimation { duration: 130 } }
                    }
                    contentItem: Column {
                        spacing: 4
                        Text {
                            width: parent.width; text: control.titles[option.index]
                            color: control.mode === control.modes[option.index] ? control.accent : control.ink
                            font.pixelSize: 13; font.bold: true; elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width; text: control.descriptions[option.index]
                            color: Qt.alpha(control.ink, 0.68); font.pixelSize: 11; elide: Text.ElideRight
                        }
                    }
                }
            }
            Text {
                width: parent.width; height: 38; wrapMode: Text.Wrap
                text: !control.queue ? "" : control.queue.recommendationError
                    ? qsTranslate("Oran7Ui", control.queue.recommendationError)
                    : control.queue.recommendationBusy ? qsTranslate("Oran7Ui", "正在加载推荐…")
                    : qsTranslate("Oran7Ui", "点击模式开始推荐，可在推荐队列中选歌")
                color: Qt.alpha(control.ink, 0.7); font.pixelSize: 11
            }
        }
        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160 }
                NumberAnimation { property: "scale"; from: 0.95; to: 1; duration: 210; easing.type: Easing.OutCubic }
            }
        }
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: 110 } }
    }
}
