import QtQuick 2.15
import QtQuick.Controls
import QtQuick.Dialogs
import Qt5Compat.GraphicalEffects
import "../../Basic"
import Client 1.0

import Oran7UI.Impl

Item {
    required property var view
    required property var listView
    required property var reorderController
    property alias currentPageIndex: titleRepeater.focIndex
    height: titleFlow.height
    // ===== 标题栏 =====
    Flow {
        id: titleFlow
        objectName: "playlistHeaderTabs"
        anchors.top: parent.top
        anchors.left: parent.left
        spacing: 16

        Repeater {
            id: titleRepeater
            model: view.titleModel ? view.titleModel : [qsTranslate("Oran7Ui", "列表")]
            anchors.fill: parent
            property int focIndex: 0
            // 页签切换：切到对应页签时把 listModel 换成该页的模型，
            // 并同步播放/选中索引（当前播放歌曲在该页签则聚焦正确索引，否则重置 -1）
            onFocIndexChanged: {
                if (view.pageListModels && view.pageListModels.length > 0
                        && focIndex >= 0 && focIndex < view.pageListModels.length) {
                    view.listModel = view.pageListModels[focIndex];
                    var idx = view.indexForCurrentPlaying();
                    listView.itemSelected_index = idx;
                    listView.playingIndex = BasicConfig.isPlaying ? idx : -1;
                    view.isMultiSelected = false;
                    // 切页签后把当前选中/播放项滚动到可见区域最顶部
                    view.scrollToCurrentIndex();
                }
            }
            delegate: Label {
                objectName: "playlistTabLabel" + index
                text: modelData
                font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
                font.bold: true
                font.pixelSize: 24
                color: index === titleRepeater.focIndex ? Oran7Theme.Oran7MusicPlaylistView["colorPrimaryBase-8"] :
                                     Oran7Theme.Oran7MusicPlaylistView["colorPrimaryBase-5"]

                Rectangle {
                    id: underline
                    width: parent.implicitWidth - 24
                    height: 3
                    radius: 1
                    anchors.top: parent.bottom
                    anchors.topMargin: 3
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: index === titleRepeater.focIndex ? Oran7Theme.Oran7MusicPlaylistView["colorPrimaryBase-7"] : "transparent"
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        titleRepeater.focIndex = index;
                    }
                }
            }
        }
    }

    // ===== 搜索框 =====
    Rectangle {
        id: searchRect
        objectName: "playlistHeaderSearchRect"
        anchors.right: multiSelectRect.left
        anchors.rightMargin: 10
        anchors.verticalCenter: titleFlow.verticalCenter
        anchors.top: parent.top

        property int initWidth: view.showSearch ? 60 : 0
        width: searchRect.initWidth
        height: 30
        color: "#fef2e8"
        radius: height / 2
        border.width: 1
        border.color: "#4d4d56"
        visible: view.showSearch

        TextField {
            id: searchField
            objectName: "playlistHeaderSearchField"
            anchors.top: parent.top
            anchors.topMargin: 2
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 2
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.right: parent.right
            anchors.rightMargin: 8
            color: view.playlistText
            font.pixelSize: 16
            font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
            background: Rectangle {
                color: "transparent"
            }

            onFocusChanged: {
                if (focus) {
                    BasicConfig.newTextAreaFocused(searchField);
                    searchRectParallelAnimation_maxsize.restart();
                }
                else{
                    if(searchField.text.length === 0)
                        searchRectParallelAnimation_minisize.restart();
                }

            }

            onTextChanged: {
                var text = searchField.text.trim();
                if (text.length === 0) {
                    view.searchActive = false;
                    view.searchQuery = "";
                } else {
                    if (!view.searchActive) {
                        view._savedPlayingPath = listView.playingIndex >= 0 ?
                            view.listModel.get(listView.playingIndex).filepath : "";
                        view._savedSelectedPath = listView.itemSelected_index >= 0 ?
                            view.listModel.get(listView.itemSelected_index).filepath : "";
                    }
                    view.searchActive = true;
                    view.searchQuery = text;
                }
            }
        }
        Connections {
            target: BasicConfig
            function onClickedOutside() {
                if (searchField.focus === true)
                    searchField.focus = false;
            }
        }

        ParallelAnimation {
            id: searchRectParallelAnimation_maxsize
            PropertyAnimation {
                target: searchRect
                property: "width"
                from: searchRect.initWidth
                to: searchRect.initWidth * 2
                easing.type: Easing.OutCubic
            }
        }
        ParallelAnimation {
            id: searchRectParallelAnimation_minisize
            PropertyAnimation {
                target: searchRect
                property: "width"
                from: searchRect.width
                to: searchRect.initWidth
                easing.type: Easing.OutCubic
            }
        }
        HoverHandler{
            enabled: true
            acceptedDevices: PointerDevice.Mouse
            onHoveredChanged: {
                reorderController.cursorShape=  hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }
    }

    // ===== 多选按钮 =====
    Rectangle {
        id: multiSelectRect
        objectName: "playlistHeaderMultiSelect"
        width: 40
        height: width
        anchors.top: parent.top
        anchors.topMargin: 4
        anchors.right: parent.right
        anchors.rightMargin: 30
        // 盖过 dragController(z=20) 等，保证始终可点击
        z: 100

        color: "transparent"
        visible: view.showMultiSelect

        Image {
            id: multiSelectIcon
            anchors.fill: parent
            source: "qrc:/image/selectmore.png"
            sourceSize.width: parent.width
            sourceSize.height: parent.height

            asynchronous: true
            cache: false
            mipmap: true

            layer.enabled: true
            layer.effect: ColorOverlay {
                source: multiSelectIcon
                color: multiSelectRect_HoverHandler.hovered ? view.playlistText :
                                                           view.playlistText
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    view.isMultiSelected = !view.isMultiSelected;
                }
            }
            HoverHandler{
                id:multiSelectRect_HoverHandler
                enabled: true
                acceptedDevices: PointerDevice.Mouse
                onHoveredChanged: {
                    reorderController.cursorShape=  hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
        }
    }


}
