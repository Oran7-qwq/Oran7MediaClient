import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects

// 视频空间：当前会话主卡片、分类记录与快速入口；数据与命令由页面传入。
Rectangle {
    id: root
    objectName: "videoLibraryPage"
    property color surfaceColor: "#dd1e222a"
    property color textColor: "#f1f2f5"
    property color secondaryTextColor: "#a8aebb"
    property color accentColor: "#ebc286"
    property string title: ""
    property string statusText: ""
    property string roomNumber: ""
    property bool hasSession: false
    property bool resolving: false
    property bool loggedIn: false
    property int selectedTab: 0
    property var historyModel: null
    property var latestRecord: ({})
    property int historyCount: 0
    readonly property bool hasHistory: latestRecord.mediaKind !== undefined
    readonly property string bannerThumbnail: hasHistory && (!hasSession || title === latestRecord.title)
        ? (latestRecord.thumbnail || "") : ""
    property string fontFamily: typeof Oran7UiPreferences !== "undefined"
        ? Oran7UiPreferences.font.family : Qt.application.font.family
    signal openFileRequested()
    signal playFileRequested(string path)
    signal playRoomRequested(string roomNumber)
    signal resumeRequested()
    signal loginRequested()
    signal historyRequested(string kind, string target)
    signal removeHistoryRequested(string kind, string target)
    function timeText(milliseconds) {
        var seconds = Math.max(0, Math.floor(milliseconds / 1000))
        var hours = Math.floor(seconds / 3600)
        var minutes = Math.floor(seconds % 3600 / 60)
        return (hours ? hours + ":" + String(minutes).padStart(2,"0") : minutes)
            + ":" + String(seconds % 60).padStart(2,"0")
    }
    color: surfaceColor
    radius: 12
    gradient: Gradient {
        GradientStop { position: 0; color: Qt.lighter(root.surfaceColor, 1.08) }
        GradientStop { position: 1; color: Qt.darker(root.surfaceColor, 1.08) }
    }

    component Oran7VideoEntryButton: Oran7StudioButton {
        accentColor: root.accentColor
        textColor: root.textColor
        surfaceColor: "#303641"
        hoverColor: "#414956"
        borderColor: "#46505e"
        font.family: root.fontFamily
    }
    component Oran7VideoEntryField: TextField {
        color: root.textColor
        placeholderTextColor: root.secondaryTextColor
        font.family: root.fontFamily
        font.pixelSize: 13
        selectByMouse: true
        leftPadding: 12
        rightPadding: 12
        implicitHeight: 38
        background: Rectangle {
            radius: 7
            color: "#95171b22"
            border.color: parent.activeFocus ? root.accentColor : "#414957"
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.width < 650 ? 16 : 20
        spacing: 14
        RowLayout {
            Layout.fillWidth: true
            Rectangle {
                Layout.preferredWidth: 4
                Layout.preferredHeight: 34
                radius: 2
                color: root.accentColor
            }
            Column {
                spacing: 3
                Label {
                    text: qsTranslate("Oran7Ui", "视频空间")
                    color: root.textColor
                    font.family: root.fontFamily
                    font.pixelSize: 22
                    font.bold: true
                }
                Label {
                    text: "VIDEO SPACE / 01"
                    color: root.secondaryTextColor
                    font.pixelSize: 10
                    font.letterSpacing: 1.5
                }
            }
            Item { Layout.fillWidth: true }
            Oran7VideoEntryButton {
                objectName: "videoLoginButton"
                visible: root.width >= 560
                text: root.loggedIn ? qsTranslate("Oran7Ui", "B站已登录") : qsTranslate("Oran7Ui", "B站登录")
                onClicked: root.loginRequested()
            }
            Oran7VideoEntryButton {
                objectName: "videoOpenFileButton"
                text: root.selectedTab === 0 ? qsTranslate("Oran7Ui", "＋ 打开视频") : qsTranslate("Oran7Ui", "进入直播间")
                onClicked: {
                    if (root.selectedTab === 0) root.openFileRequested()
                    else {
                        roomField.forceActiveFocus()
                        scroll.contentItem.contentY = Math.max(0, scroll.contentItem.contentHeight - scroll.availableHeight)
                    }
                }
            }
        }
        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: scroll.availableWidth
                spacing: 18
                Rectangle {
                    id: sessionCard
                    objectName: "videoResumeBanner"
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.height < 450 ? 174 : 196
                    radius: 14
                    color: "#252d39"
                    border.color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.2)
                    clip: true
                    // 独立封面保持原来的缩略图形式，不铺成卡片背景。
                    Rectangle {
                        id: sessionArt
                        anchors.left: parent.left
                        anchors.leftMargin: 28
                        anchors.verticalCenter: parent.verticalCenter
                        width: root.width >= 700 ? 156 : 110
                        height: width * 9 / 16
                        radius: 10
                        color: "#161b23"
                        visible: root.width >= 580
                        layer.enabled: visible
                        layer.effect: DropShadow {
                            horizontalOffset: 0
                            verticalOffset: 7
                            radius: 12
                            samples: 25
                            spread: 0
                            color: "#70000000"
                            transparentBorder: true
                        }
                        Oran7RoundedImage {
                            anchors.fill: parent
                            source: root.bannerThumbnail
                            radius: parent.radius
                            visible: root.bannerThumbnail.length > 0
                        }
                        Shape {
                            anchors.centerIn: parent
                            width: 18; height: 20
                            visible: root.bannerThumbnail.length === 0
                            preferredRendererType: Shape.CurveRenderer
                            ShapePath {
                                fillColor: root.accentColor
                                strokeWidth: -1
                                PathSvg { path: "M 3 2 Q 3 0.5 4.5 1.5 L 16 9 Q 17.5 10 16 11 L 4.5 18.5 Q 3 19.5 3 18 Z" }
                            }
                        }
                    }
                    Rectangle {
                        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                        anchors.margins: 20
                        width: 2
                        color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.35)
                    }
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: root.height < 450 ? 16 : 22
                        anchors.leftMargin: sessionArt.visible ? sessionArt.x + sessionArt.width + 24 : 34
                        width: parent.width - anchors.leftMargin - 24
                        spacing: 7
                        RowLayout {
                            spacing: 8
                            Rectangle { width: 6; height: 6; radius: 3; color: root.accentColor }
                            Label {
                                text: root.hasSession ? qsTranslate("Oran7Ui", "当前会话")
                                    : root.hasHistory ? qsTranslate("Oran7Ui", "最近观看") : qsTranslate("Oran7Ui", "从一段视频开始")
                                color: root.accentColor
                                font.family: root.fontFamily
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.hasSession ? root.title : root.hasHistory
                                ? (root.latestRecord.mediaKind === "live"
                                    ? (root.latestRecord.anchorName || qsTranslate("Oran7Ui", "直播间")) + " · " + root.latestRecord.title
                                    : root.latestRecord.title)
                                : qsTranslate("Oran7Ui", "打开本地影片，或进入一个直播间")
                            textFormat: Text.PlainText
                            color: root.textColor
                            font.family: root.fontFamily
                            font.pixelSize: root.width < 600 ? 20 : 24
                            font.bold: true
                            elide: Text.ElideMiddle
                        }
                        Label {
                            Layout.fillWidth: true
                            text: !root.hasSession && root.latestRecord.durationMs > 0
                                ? (root.latestRecord.completed ? qsTranslate("Oran7Ui", "已看完")
                                    : root.timeText(root.latestRecord.positionMs) + " / " + root.timeText(root.latestRecord.durationMs))
                                : qsTranslate("Oran7Ui", "画面、声音与弹幕，在这里汇合")
                            color: root.secondaryTextColor
                            font.family: root.fontFamily
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                        Item { Layout.fillHeight: true }
                        Oran7VideoEntryButton {
                            id: resumeButton
                            objectName: "videoResumeButton"
                            Layout.preferredWidth: Math.min(220, parent.width)
                            Layout.preferredHeight: 52
                            primary: true
                            cornerRadius: 10
                            font.pixelSize: 15
                            font.bold: true
                            text: root.hasSession ? qsTranslate("Oran7Ui", "返回播放")
                                : root.hasHistory ? qsTranslate("Oran7Ui", "快捷播放") : qsTranslate("Oran7Ui", "打开文件")
                            contentItem: Row {
                                spacing: 12
                                Item {
                                    width: 16; height: parent.height
                                    Shape {
                                        anchors.centerIn: parent
                                        width: 14; height: 16
                                        preferredRendererType: Shape.CurveRenderer
                                        ShapePath {
                                            fillColor: "#171a20"
                                            strokeWidth: -1
                                            PathSvg { path: "M 1 1 L 13 8 L 1 15 Z" }
                                        }
                                    }
                                }
                                Text {
                                    height: parent.height
                                    text: resumeButton.text
                                    font: resumeButton.font
                                    color: "#171a20"
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            onClicked: {
                                if (root.hasSession) root.resumeRequested()
                                else if (root.hasHistory) root.historyRequested(root.latestRecord.mediaKind, root.latestRecord.target)
                                else root.openFileRequested()
                            }
                        }
                    }
                    Rectangle {
                        anchors.right: parent.right; anchors.bottom: parent.bottom
                        anchors.margins: 22
                        width: 42; height: 3; radius: 1.5
                        color: root.accentColor
                        opacity: 0.65
                        visible: sessionArt.visible
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: [qsTranslate("Oran7Ui", "本地视频"), qsTranslate("Oran7Ui", "直播房间")]
                        delegate: Button {
                            id: tab
                            required property int index
                            required property string modelData
                            text: modelData
                            leftPadding: 16; rightPadding: 16
                            implicitHeight: 38
                            contentItem: Label {
                                text: tab.text
                                color: root.selectedTab === tab.index ? root.textColor : root.secondaryTextColor
                                font.family: root.fontFamily
                                font.pixelSize: 14
                                font.bold: root.selectedTab === tab.index
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 9
                                color: root.selectedTab === tab.index
                                    ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.14)
                                    : tab.hovered ? "#153c495e" : "transparent"
                                border.color: root.selectedTab === tab.index
                                    ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.35) : "transparent"
                                Behavior on color { ColorAnimation { duration: 140 } }
                            }
                            onClicked: root.selectedTab = index
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: root.historyCount > 0 ? qsTranslate("Oran7Ui", "观看记录") + " · " + root.historyCount
                            : root.selectedTab === 0 ? "LOCAL / FILE" : "LIVE / BILIBILI"
                        color: root.secondaryTextColor
                        font.pixelSize: 10
                        visible: root.width > 430
                    }
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: root.width >= 960 ? 2 : 1
                    columnSpacing: 12
                    rowSpacing: 10
                    Repeater {
                        model: root.historyModel
                        delegate: Rectangle {
                            id: historyRow
                            required property string mediaKind
                            required property string target
                            required property string title
                            required property string lastOpenedAt
                            required property string thumbnail
                            required property double positionMs
                            required property double durationMs
                            required property bool completed
                            required property string anchorName
                            required property string avatarUrl
                            required property int liveStatus
                            required property int roomInfoState
                            Layout.fillWidth: true
                            implicitHeight: 100
                            visible: mediaKind === (root.selectedTab === 0 ? "local" : "live")
                            radius: 8
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0; color: rowMouse.containsMouse ? "#b03a4554" : "#90313a47" }
                                GradientStop { position: 1; color: rowMouse.containsMouse ? "#90303a48" : "#60212a35" }
                            }
                            border.color: rowMouse.containsMouse
                                ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.45) : "#304e5b6c"
                            Behavior on border.color { ColorAnimation { duration: 160 } }
                            Rectangle {
                                anchors.right: parent.right; anchors.top: parent.top
                                anchors.rightMargin: 16; anchors.topMargin: 8
                                width: 32; height: 2
                                color: root.accentColor
                                opacity: rowMouse.containsMouse ? 0.65 : 0.18
                                Behavior on opacity { NumberAnimation { duration: 160 } }
                            }
                            function loadThumbnail() {
                                if (visible && mediaKind === "local") root.historyModel.requestThumbnail(target)
                                else if (visible && mediaKind === "live") root.historyModel.requestRoomInfo(target)
                            }
                            Component.onCompleted: loadThumbnail()
                            onVisibleChanged: loadThumbnail()
                            MouseArea {
                                id: rowMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.historyRequested(historyRow.mediaKind, historyRow.target)
                            }
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 12
                                Rectangle { Layout.preferredWidth: 3; Layout.preferredHeight: 24; color: root.accentColor }
                                Rectangle {
                                    Layout.preferredWidth: historyRow.width >= 600 ? 110 : 88
                                    Layout.preferredHeight: width * 9 / 16
                                    radius: 10
                                    color: "#161b23"
                                    layer.enabled: historyRow.visible
                                    layer.effect: DropShadow {
                                        horizontalOffset: 0
                                        verticalOffset: 7
                                        radius: 12
                                        samples: 25
                                        spread: 0
                                        color: "#70000000"
                                        transparentBorder: true
                                    }
                                    Oran7RoundedImage {
                                        objectName: "videoHistoryThumbnail"
                                        anchors.fill: parent
                                        source: historyRow.thumbnail
                                        radius: parent.radius
                                    }
                                    Label {
                                        anchors.centerIn: parent
                                        visible: !historyRow.thumbnail.length
                                        text: historyRow.mediaKind === "live" ? "LIVE" : "VIDEO"
                                        color: root.secondaryTextColor
                                        font.pixelSize: 10
                                    }
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.bottom: parent.bottom
                                        anchors.leftMargin: 6
                                        anchors.bottomMargin: 4
                                        visible: historyRow.mediaKind === "local"
                                        width: historyRow.durationMs > 0
                                            ? Math.max(0, parent.width - 12) * Math.min(1,historyRow.positionMs / historyRow.durationMs) : 0
                                        height: 3
                                        radius: 1.5
                                        color: root.accentColor
                                    }
                                }
                                Column {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Label {
                                        width: parent.width
                                        text: historyRow.mediaKind === "live"
                                            ? (historyRow.title === historyRow.target ? qsTranslate("Oran7Ui", "直播间") + " " + historyRow.target : historyRow.title)
                                            : historyRow.title
                                        textFormat: Text.PlainText
                                        color: root.textColor
                                        font.family: root.fontFamily
                                        font.pixelSize: 14
                                        elide: Text.ElideMiddle
                                    }
                                    Label {
                                        visible: historyRow.mediaKind === "local" && historyRow.durationMs > 0
                                        text: historyRow.completed ? qsTranslate("Oran7Ui", "已看完")
                                            : root.timeText(historyRow.positionMs) + " / " + root.timeText(historyRow.durationMs)
                                        color: root.accentColor
                                        font.family: root.fontFamily
                                        font.pixelSize: 11
                                    }
                                    Row {
                                        visible: historyRow.mediaKind === "live"
                                        width: parent.width
                                        spacing: 6
                                        Image {
                                            width: 16; height: 16
                                            source: historyRow.avatarUrl
                                            visible: source.toString().length > 0
                                            asynchronous: true
                                            cache: true
                                        }
                                        Label {
                                            width: parent.width - (historyRow.avatarUrl.length ? 22 : 0)
                                            elide: Text.ElideRight
                                            text: historyRow.anchorName || qsTranslate("Oran7Ui", "主播信息待更新")
                                            textFormat: Text.PlainText
                                            color: root.secondaryTextColor
                                            font.family: root.fontFamily
                                            font.pixelSize: 11
                                        }
                                    }
                                    Label {
                                        width: parent.width
                                        text: historyRow.mediaKind === "live"
                                            ? qsTranslate("Oran7Ui", "直播间") + " " + historyRow.target + " · "
                                                + (historyRow.roomInfoState === 1 ? qsTranslate("Oran7Ui", "查询中")
                                                    : historyRow.liveStatus === 1 ? qsTranslate("Oran7Ui", "直播中")
                                                    : historyRow.liveStatus === 0 ? qsTranslate("Oran7Ui", "未开播")
                                                    : historyRow.liveStatus === 2 ? qsTranslate("Oran7Ui", "轮播")
                                                    : qsTranslate("Oran7Ui", "状态未知"))
                                            : historyRow.target
                                        color: root.secondaryTextColor
                                        font.family: root.fontFamily
                                        font.pixelSize: 11
                                        elide: Text.ElideMiddle
                                    }
                                }
                                Label {
                                    visible: historyRow.width > 620
                                    text: Qt.formatDateTime(new Date(historyRow.lastOpenedAt), "MM-dd hh:mm")
                                    color: root.secondaryTextColor
                                    font.pixelSize: 11
                                }
                                Oran7VideoEntryButton {
                                    visible: historyRow.mediaKind === "live"
                                    text: qsTranslate("Oran7Ui", "刷新")
                                    implicitWidth: 64
                                    enabled: historyRow.roomInfoState !== 1
                                    onClicked: root.historyModel.requestRoomInfo(historyRow.target,true)
                                }
                                Oran7VideoEntryButton {
                                    objectName: "videoHistoryRemoveButton"
                                    text: qsTranslate("Oran7Ui", "移除")
                                    implicitWidth: 78
                                    onClicked: root.removeHistoryRequested(historyRow.mediaKind, historyRow.target)
                                }
                            }
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: entryColumn.implicitHeight + 32
                    radius: 10
                    color: "#502f3540"
                    border.color: "#394451"
                    ColumnLayout {
                        id: entryColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 16
                        spacing: 12
                        Label {
                            text: root.selectedTab === 0 ? qsTranslate("Oran7Ui", "打开本地视频") : qsTranslate("Oran7Ui", "进入直播间")
                            color: root.textColor
                            font.family: root.fontFamily
                            font.pixelSize: 16
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.selectedTab === 0 ? qsTranslate("Oran7Ui", "选择文件，或粘贴本地视频路径")
                                : qsTranslate("Oran7Ui", "输入 B站房间号，播放时获取可用直播流")
                            color: root.secondaryTextColor
                            font.family: root.fontFamily
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Oran7VideoEntryField {
                                id: localField
                                objectName: "videoPathField"
                                Layout.fillWidth: true
                                visible: root.selectedTab === 0
                                placeholderText: qsTranslate("Oran7Ui", "本地视频路径")
                                onAccepted: if (text.trim()) root.playFileRequested(text.trim())
                            }
                            Oran7VideoEntryField {
                                id: roomField
                                objectName: "videoRoomField"
                                Layout.fillWidth: true
                                visible: root.selectedTab === 1
                                placeholderText: qsTranslate("Oran7Ui", "B站房间号")
                                text: root.roomNumber
                                validator: RegularExpressionValidator { regularExpression: /[0-9]{0,12}/ }
                                onAccepted: if (text.length) root.playRoomRequested(text)
                            }
                            Oran7VideoEntryButton {
                                objectName: "videoStartButton"
                                text: root.resolving ? qsTranslate("Oran7Ui", "重新连接") : qsTranslate("Oran7Ui", "播放")
                                enabled: root.selectedTab === 0 ? localField.text.trim().length > 0 : roomField.text.length > 0
                                onClicked: {
                                    if (root.selectedTab === 0) root.playFileRequested(localField.text.trim())
                                    else root.playRoomRequested(roomField.text)
                                }
                            }
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: root.statusText.length > 0
                    text: root.statusText
                    color: root.accentColor
                    font.family: root.fontFamily
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
                Label {
                    Layout.fillWidth: true
                    visible: root.historyModel && root.historyModel.errorString.length > 0
                    text: root.historyModel ? root.historyModel.errorString : ""
                    color: root.accentColor
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
