import QtQuick 2.15
import QtQuick.Controls
import QtQuick.Dialogs
import Qt5Compat.GraphicalEffects
import "../../Basic"
import Client 1.0

import Oran7UI.Impl

Rectangle {
    required property var view
    required property var listView
    required property var reorderController
    required property var headerRow

    id: multiSelectMenu
    // 两种模式复用同一行的几何和底色，切换时不再发生高低、宽度或圆角变化。
    anchors.fill: headerRow
    radius: headerRow.radius
    color: headerRow.color
    visible: view.isMultiSelected

    property bool checked: false
    onCheckedChanged: {
        if (!view.listModel)
            return;
        if (checked)
        {
            for (var i = 0; i < view.listModel.count; i++)
            {
                if (view.searchActive) {
                    if (view.matchesSearch(view.listModel.get(i).music_name,
                                           view.listModel.get(i).music_artist,
                                           view.listModel.get(i).music_album))
                        view.listModel.setSelected(i, true);
                }
                else
                {
                    view.listModel.setSelected(i, true);
                }
            }
        }
        else
        {
            for (var j = 0; j < view.listModel.count; j++)
                view.listModel.setSelected(j, false);
        }
    }
    //全选
    Image {
        id: selectAllIcon
        width: Math.min(24, multiSelectMenu.height - 6)
        height: width
        sourceSize.width: 30
        sourceSize.height: 30
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.verticalCenter: parent.verticalCenter

        asynchronous: false
        smooth: true
        mipmap: true
        antialiasing: true

        source: "qrc:/image/selectAll.png"
        layer.enabled: true
        layer.effect: ColorOverlay {
            source: selectAllIcon
            color: multiSelectMenu.checked ? view.playlistText :
                        view.playlistText
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                multiSelectMenu.checked = !multiSelectMenu.checked;
            }
        }
        HoverHandler{
            enabled: view.isMultiSelected
            acceptedDevices: PointerDevice.Mouse
            onHoveredChanged: {
                reorderController.cursorShape=  hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }
    }

    Text {
        id: selectAllText
        objectName: "playlistSelectAllText"
        anchors.left: selectAllIcon.right
        anchors.leftMargin: 4
        anchors.verticalCenter: selectAllIcon.verticalCenter
        color: view.playlistText
        text: qsTranslate("Oran7Ui", "全选")
        font.pixelSize: view.playlistFontSize
        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
        font.bold: false
    }

    //批量整合按钮：把多选选中的元素成批加入"我整合的音乐"（哈希查重 + 分片异步插入）
    // 位置对齐表头"整合"列
    Image{
        id: batchIntegrateBtn
        width: Math.min(28, multiSelectMenu.height - 4)
        height: width
        sourceSize: Qt.size(512,512)
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: listView.width - 20 - 72 - 12
            - (view.showLoveButton ? 36 : 0) - (view.showIntegrateButton ? 36 : 0)
        // 实心=选中元素均已整合（点击移出）；空心=存在未整合（点击全部加入）
        source: batchIntegrateBtn.filled ? "qrc:/image/ep_collection-tag.-no-middle-transparent.png" :
                                       "qrc:/image/ep_collection-tag.png"
        visible: view.showIntegrateButton

        mipmap: true
        antialiasing: true

        // 空心=点击将整合所有选中项；实心=点击将取消整合所有选中项。
        // 状态仅随点击切换，不受勾选/全选/模型变化等任何其它行为影响
        property bool hovered: false
        property bool filled: false
        layer.enabled: true
        layer.effect:ColorOverlay{
            source:batchIntegrateBtn
            color:batchIntegrateBtn.hovered ? view.playlistText :
                                    view.playlistText
        }
        HoverHandler{
            acceptedDevices: PointerDevice.Mouse
            onHoveredChanged: {
                parent.hovered = hovered
                reorderController.cursorShape = reorderController.cursorShape=  hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }
        MouseArea{
            anchors.fill: parent
            onClicked: {
                if (!view.listModel) return;
                // 收集多选选中的元素（含 songId/url/filepath，source 按 songId 区分来源）
                var items = [];
                for (var i = 0; i < view.listModel.count; i++) {
                    var m = view.listModel.get(i);
                    if (m.isSelected === true) {
                        items.push({
                            icon: m.icon,
                            music_name: m.music_name,
                            music_artist: m.music_artist,
                            music_album: m.music_album,
                            timesize: m.timesize,
                            url: m.url || "",
                            filepath: m.filepath,
                            songId: m.songId || 0,
                            isVip: m.isVip === true,
                            source: (m.songId > 0) ? "netease" : "local"
                        });
                    }
                }
                if (items.length === 0) return;
                if (batchIntegrateBtn.filled) {
                    // 实心 → 取消整合所有选中项
                    for (var j = 0; j < items.length; j++) {
                        var it = items[j];
                        BasicConfig.removeIntegratedMusic(it.songId || 0, it.url || "", it.filepath);
                    }
                } else {
                    // 空心 → 整合所有选中项
                    BasicConfig.batchAddIntegratedItems(items);
                }
                // 状态仅随点击在空心/实心间切换
                batchIntegrateBtn.filled = !batchIntegrateBtn.filled;
            }
        }
    }

    //删除Selected  - items
    Image{
        id:deleteBtn
        width: Math.min(30, multiSelectMenu.height - 4)
        height: width
        sourceSize:Qt.size(512,512) //分辨率
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 32
        source: "qrc:/image/material-symbols_delete-outline-rounded.png"

        property bool hovered: false
        layer.enabled: true
        layer.effect:ColorOverlay{
            source:deleteBtn
            color:deleteBtn.hovered ? view.playlistText :
                                    view.playlistText
        }
        HoverHandler{
            acceptedDevices: PointerDevice.Mouse
            onHoveredChanged: {
                parent.hovered = hovered
                reorderController.cursorShape = reorderController.cursorShape=  hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }
        MouseArea{
            anchors.fill: parent
            onClicked: {
                if (!view.listModel) return;
                // 收集选中项（含 filepath + songId，云端/VIP 歌 filepath 为空，需 songId 精确移除）
                var selectedItems = [];
                for (var i = 0; i < view.listModel.count; i++) {
                    var item = view.listModel.get(i);
                    if (item.isSelected === true)
                        selectedItems.push({filepath: item.filepath, url: item.url, songId: item.songId});
                }
                // 文件删除异步完成后再发布快照，失败项保持可见。
                if (selectedItems.length > 0) view.removeItemOfFiles(selectedItems);
            }
        }
    }

    //<--multiSelectMenu END
}
