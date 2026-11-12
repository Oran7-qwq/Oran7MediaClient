import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../Basic"
import "../../Components/Oran7GlobalComponents"

// 界面先接通；模型和命令由后续 QQ 音乐服务显式注入，不创建临时曲库或复用网易云模型。
Item {
    id: root
    property string pageName: "QQMusicPage"
    property bool serviceConnected: false
    property bool loggedIn: false
    property bool loginBusy: false
    property bool searchBusy: false
    property bool likedBusy: false
    property string userName: ""
    property string loginStatus: ""
    property string errorMessage: ""
    property var likedModel: null
    property var searchModel: null
    property color accentColor: "#31c27c"
    property color surfaceColor: "#fef2e8"
    property color textColor: "#2a1a22"
    readonly property var uiFont: typeof Oran7UiPreferences !== "undefined"
        ? Oran7UiPreferences.font : Qt.application.font
    readonly property bool isActive: StackView.status === StackView.Active
    readonly property int currentPageIndex: playlist.currentPageIndex
    property string _feedback: ""
    signal searchRequested(string keywords)
    signal likedRefreshRequested()
    signal loginRequested()
    signal loginCancelled()
    signal logoutRequested()
    signal trackPlayRequested(var model, int index)
    signal trackLikeRequested(var track)
    signal trackIntegrateRequested(var track)
    onVisibleChanged: { if (!visible) loginPopup.close() }

    function submitSearch() {
        var keywords = searchField.text.trim()
        if (!keywords.length) {
            root._feedback = qsTranslate("Oran7Ui", "请输入歌曲、歌手或专辑")
            return
        }
        if (root.searchBusy) return
        playlist.switchToPage(1)
        searchField.focus = false
        root._feedback = root.serviceConnected ? ""
            : qsTranslate("Oran7Ui", "搜索服务暂未连接")
        if (root.serviceConnected) root.searchRequested(keywords)
    }

    RowLayout {
        id: topBar
        anchors.top: parent.top
        anchors.topMargin: 21
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 33
        anchors.rightMargin: 16
        height: 38
        spacing: 10
        Label {
            text: qsTranslate("Oran7Ui", "QQ 音乐")
            visible: root.width >= 640
            font.family: root.uiFont.family
            font.pixelSize: 12
            font.italic: true
            color: "#e99763"
        }
        Item { Layout.fillWidth: true; visible: root.width >= 640 }
        Rectangle {
            Layout.fillWidth: true
            Layout.maximumWidth: 340
            Layout.minimumWidth: 100
            Layout.preferredWidth: 248
            Layout.fillHeight: true
            radius: height / 2
            color: root.surfaceColor
            border.color: searchField.activeFocus ? root.accentColor : Qt.alpha(root.textColor, 0.28)
            border.width: searchField.activeFocus ? 2 : 1
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                spacing: 6
                TextField {
                    id: searchField
                    objectName: "qqMusicSearchField"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    padding: 0
                    selectByMouse: true
                    maximumLength: 100
                    placeholderText: qsTranslate("Oran7Ui", "搜索 QQ 音乐")
                    placeholderTextColor: "#8b847f"
                    color: root.textColor
                    font.family: root.uiFont.family
                    font.pixelSize: 14
                    background: Item {}
                    onAccepted: root.submitSearch()
                    onActiveFocusChanged: {
                        if (activeFocus) BasicConfig.newTextAreaFocused(searchField)
                    }
                }
                Oran7StudioButton {
                    objectName: "qqMusicSearchButton"
                    implicitWidth: 30
                    implicitHeight: 30
                    leftPadding: 0; rightPadding: 0
                    enabled: !root.searchBusy
                    text: ""
                    surfaceColor: "transparent"
                    hoverColor: Qt.alpha(root.accentColor, 0.16)
                    borderColor: "transparent"
                    cornerRadius: 15
                    contentItem: Image {
                        source: "qrc:/image/search.png"
                        sourceSize: Qt.size(22, 22)
                        fillMode: Image.PreserveAspectFit
                    }
                    onClicked: root.submitSearch()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTranslate("Oran7Ui", "搜索")
                }
            }
        }
        Oran7StudioButton {
            objectName: "qqMusicAccountButton"
            Layout.preferredWidth: root.width < 520 ? 42 : 126
            Layout.fillHeight: true
            leftPadding: 8; rightPadding: 8
            cornerRadius: 19
            surfaceColor: root.surfaceColor
            hoverColor: Qt.tint(root.surfaceColor, Qt.alpha(root.accentColor, 0.14))
            borderColor: Qt.alpha(root.textColor, 0.18)
            contentItem: RowLayout {
                spacing: 6
                Image {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    source: "qrc:/image/oran7_qq_music.svg"
                    sourceSize: Qt.size(24, 24)
                }
                Label {
                    Layout.fillWidth: true
                    visible: root.width >= 520
                    text: root.loggedIn ? root.userName : qsTranslate("Oran7Ui", "扫码登录")
                    color: root.textColor
                    font.family: root.uiFont.family
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                }
            }
            onClicked: loginPopup.open()
            ToolTip.visible: hovered
            ToolTip.text: root.loggedIn ? root.userName : qsTranslate("Oran7Ui", "QQ 音乐登录")
        }
        Item { Layout.fillWidth: true; visible: root.width >= 640 }
    }

    Oran7MusicPlaylistView {
        id: playlist
        objectName: "qqMusicPlaylist"
        anchors.top: topBar.bottom
        anchors.topMargin: 7
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusRow.top
        anchors.bottomMargin: 6
        leftMargin: 33
        showAddButton: false
        showSearch: false
        showMultiSelect: root.serviceConnected && listModel !== null && listModel.count > 0
        showLoveButton: true
        showIntegrateButton: true
        // 云端排序策略接入前不提交拖拽，避免把本地排序误认为已保存到云端。
        allowDragReorder: false
        titleModel: [qsTranslate("Oran7Ui", "我喜欢的音乐"), qsTranslate("Oran7Ui", "搜索结果")]
        listModel: currentPageIndex === 0 ? root.likedModel : root.searchModel
        onCurrentPageIndexChanged: {
            isMultiSelected = false
            Qt.callLater(syncIndicesToCurrentMedia)
            root._feedback = ""
        }
        onItemClicked: function(index) { if (root.serviceConnected) root.trackPlayRequested(listModel, index) }
        onItemDoubleClicked: function(index) { if (root.serviceConnected) root.trackPlayRequested(listModel, index) }
        onItemLoved: function(index) { if (root.serviceConnected) root.trackLikeRequested(listModel.get(index)) }
        onItemIntegrated: function(index) { if (root.serviceConnected) root.trackIntegrateRequested(listModel.get(index)) }
    }
    Item {
        anchors.top: playlist.top
        anchors.topMargin: 82
        anchors.bottom: playlist.bottom
        anchors.left: playlist.left
        anchors.right: playlist.right
        anchors.leftMargin: 33
        anchors.rightMargin: 16
        visible: (!playlist.listModel || playlist.listModel.count === 0) && !playlist.isMultiSelected
        Rectangle {
            objectName: "qqMusicEmptyState"
            anchors.centerIn: parent
            width: Math.max(0, Math.min(360, parent.width - 24))
            height: emptyContent.implicitHeight + 36
            radius: 14
            color: Qt.alpha(root.textColor, 0.82)
            border.color: Qt.alpha(root.surfaceColor, 0.13)
            ColumnLayout {
                id: emptyContent
                anchors.centerIn: parent
                width: Math.max(0, parent.width - 36)
                spacing: 12
                Image {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    source: "qrc:/image/oran7_qq_music.svg"
                    sourceSize: Qt.size(40, 40)
                }
                Label {
                    Layout.fillWidth: true
                    text: playlist.currentPageIndex === 0
                        ? root.likedBusy ? qsTranslate("Oran7Ui", "正在加载喜欢的音乐…")
                            : root.loggedIn ? qsTranslate("Oran7Ui", "暂无喜欢的音乐")
                            : qsTranslate("Oran7Ui", "登录后查看喜欢的音乐")
                        : root.searchBusy ? qsTranslate("Oran7Ui", "正在搜索…")
                            : !root.serviceConnected ? qsTranslate("Oran7Ui", "搜索歌曲、歌手或专辑")
                            : qsTranslate("Oran7Ui", "暂无搜索结果")
                    font.family: root.uiFont.family
                    font.pixelSize: 16
                    font.bold: true
                    color: root.surfaceColor
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
                Label {
                    Layout.fillWidth: true
                    text: !root.serviceConnected ? qsTranslate("Oran7Ui", "QQ 音乐服务暂未连接")
                        : qsTranslate("Oran7Ui", "搜索歌曲、歌手或专辑")
                    color: Qt.alpha(root.surfaceColor, 0.65)
                    font.family: root.uiFont.family
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
                Oran7StudioButton {
                    objectName: "qqMusicEmptyLogin"
                    Layout.alignment: Qt.AlignHCenter
                    visible: playlist.currentPageIndex === 0 && !root.loggedIn
                    primary: true
                    accentColor: root.accentColor
                    text: qsTranslate("Oran7Ui", "扫码登录")
                    font.family: root.uiFont.family
                    onClicked: loginPopup.open()
                }
            }
        }
    }
    RowLayout {
        id: statusRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 33
        anchors.rightMargin: 16
        anchors.bottomMargin: 8
        height: 30
        spacing: 8
        Rectangle {
            width: 5; height: 5; radius: 2.5
            color: root.serviceConnected ? root.accentColor : "#e99763"
        }
        Label {
            objectName: "qqMusicPageStatus"
            Layout.fillWidth: true
            text: root.errorMessage || root._feedback
                || (!root.serviceConnected ? qsTranslate("Oran7Ui", "QQ 音乐服务暂未连接")
                    : root.loggedIn ? qsTranslate("Oran7Ui", "已登录 QQ 音乐")
                    : qsTranslate("Oran7Ui", "登录后同步喜欢的音乐"))
            font.family: root.uiFont.family
            font.pixelSize: 12
            color: "#d5d3d0"
            elide: Text.ElideRight
        }
        Oran7StudioButton {
            objectName: "qqMusicRefreshButton"
            implicitHeight: 28
            text: qsTranslate("Oran7Ui", "刷新列表")
            enabled: root.serviceConnected && root.loggedIn && !root.likedBusy && playlist.currentPageIndex === 0
            textColor: root.surfaceColor
            surfaceColor: Qt.alpha(root.textColor, 0.28)
            hoverColor: Qt.alpha(root.accentColor, 0.22)
            borderColor: Qt.alpha(root.surfaceColor, 0.18)
            onClicked: root.likedRefreshRequested()
        }
    }
    Oran7QQMusicLoginPopup {
        id: loginPopup
        serviceConnected: root.serviceConnected
        loggedIn: root.loggedIn
        loginBusy: root.loginBusy
        userName: root.userName
        statusText: root.loginStatus
        accentColor: root.accentColor
        surfaceColor: root.surfaceColor
        textColor: root.textColor
        onLoginRequested: root.loginRequested()
        onLoginCancelled: root.loginCancelled()
        onLogoutRequested: root.logoutRequested()
    }
    Connections {
        target: BasicConfig
        function onClickedOutside() { searchField.focus = false }
    }
}
