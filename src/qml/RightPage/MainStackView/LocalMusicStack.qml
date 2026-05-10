import QtQuick 2.15
import QtQuick.Controls
import QtCore
import QtQuick.Dialogs
import Qt5Compat.GraphicalEffects
import "../../Basic"
import "../../Components"
import Client 1.0

Item {
    id: root
    anchors.left: parent.left
    anchors.leftMargin: 0
    anchors.top: parent.top
    anchors.topMargin: 14
    clip: true

    //====================   define Properties    =====================//

    property string pageName: ""

    // 获取mainWindow
    function findMainWindow() {
        return BasicConfig.mainWindow;
    }

    property var mainWindow: null

    Component.onCompleted: {
        // 延迟执行，确保BasicConfig.mainWindow已设置
        Qt.callLater(function() {
            mainWindow = findMainWindow();

            // 检查blurSource状态
            if(localMusciStack_playlistView.enableListBlurEffect && !localMusciStack_playlistView.blurSource){
                console.warn("Warning: blurSource is null. mainWindow:", mainWindow);
            }
        });
    }

    //=======================  Ui  ====================//

    Oran7MusicPlaylistView {
        id: localMusciStack_playlistView
        anchors.fill: root

        leftMargin: 33

        enableListBlurEffect: false
        blurSource: root.mainWindow ? root.mainWindow.__Oran7WindowBackGround__ : null

        //初始化属性
        titleModel: ["本地音乐", "下载中歌曲"]
        listModel: BasicConfig.localMusicListModel
        isPlaying: BasicConfig.isPlaying //bind

        // ------------- innder signals -------------
        onItemDoubleClicked: function (index) {
            //console.log("double clicked:",index)
            localMusciStack_playlistView.dealClicked(index);
        }

        onItemClicked: function (index) {
            // console.log("clicked:",index)
            localMusciStack_playlistView.dealClicked(index);
        }

        onFocus_current_playlistItem: {
            localMusciStack_playlistView.ask_dir_of_item_index(BasicConfig.currentMediaFilePath);
        }

        // ------------- functions -------------

        function dealClicked(index) {
            //async globalPlayingFocus
            if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex) {
                BasicConfig.isPlaying = false;
                BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_MusicPlayerIndex;
            }

            var item = listModel.get(index);

            if (BasicConfig.isPlaying === false && curSelectingIndex === -1) {
                //--->first begain to play
                Client.qmlClickedReqPreparePlayMusic(item.filepath);
                BasicConfig.isPlaying = true;
                curPlayingIndex = index;

                //async bottom page info
                BasicConfig.currentMediaName = item.music_name;
                BasicConfig.currentMediaArtistAuthor = item.music_artist;
                BasicConfig.currentMediaFilePath = item.filepath;
                BasicConfig.currentMediaCoverFilePath = item.icon;//touch bottom page update
            } else if (BasicConfig.isPlaying === false && curSelectingIndex === index) {
                //-->pause statue,and the same file
                Client.qmlClickedReqPreparePlayMusic(item.filepath);
                BasicConfig.isPlaying = true;
                curPlayingIndex = index;
            } else if (BasicConfig.isPlaying === false && curSelectingIndex !== index) {
                //-->pause statue,and request to change file
                Client.qmlClickedReqPreparePlayMusic(item.filepath);
                BasicConfig.isPlaying = true;
                curPlayingIndex = index;

                //change file,async bottom page info
                BasicConfig.currentMediaName = item.music_name;
                BasicConfig.currentMediaArtistAuthor = item.music_artist;
                BasicConfig.currentMediaFilePath = item.filepath;
                BasicConfig.currentMediaCoverFilePath = item.icon;//touch bottom page update
            } else if (BasicConfig.isPlaying === true && curSelectingIndex !== index) {
                //--->is playing while change file
                Client.qmlClickedReqPreparePlayMusic(item.filepath);
                BasicConfig.isPlaying = true; //keep playing
                curPlayingIndex = index;

                //change file,async bottom page info
                BasicConfig.currentMediaName = item.music_name;
                BasicConfig.currentMediaArtistAuthor = item.music_artist;
                BasicConfig.currentMediaFilePath = item.filepath;
                BasicConfig.currentMediaCoverFilePath = item.icon;//touch bottom page update
            } else if (BasicConfig.isPlaying === true && curSelectingIndex === index) {
                //--->is playing ,the same of file for request to pause
                Client.qmlClickedReqPreparePlayMusic(item.filepath);
                BasicConfig.isPlaying = false;
                curPlayingIndex = -1;
            }

            //updata now selectingIndex
            curSelectingIndex = index;
        }

        function deal_outClickplayReponse_asyncUiStatue(index) {
            if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                return;
            if (BasicConfig.isPlaying === false) {
                curPlayingIndex = -1;
                //console.log(index)
            } else if (BasicConfig.isPlaying === true) {
                curPlayingIndex = index;
            }

            curSelectingIndex = index;
        }

        // -------------  Connections  -------------

        //-->signal from BasicConfig ,the player core playing statue is changed, now async the playlist Ui Of statues
        Connections {
            target: BasicConfig
            function onIsPlayingChanged() {
                if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex) {
                    localMusciStack_playlistView.curPlayingIndex = -1;
                    return;
                }

                localMusciStack_playlistView.ask_dir_of_item_index(BasicConfig.currentMediaFilePath);
            }
        }
        //--> signal from this for response to the file_path dir of item index
        Connections {
            target: localMusciStack_playlistView
            function onReponse_dir_of_item_index(index) {
                localMusciStack_playlistView.deal_outClickplayReponse_asyncUiStatue(index);
            }

            function onAddNewItemOfFiles(filesArray) {
                Client.addNewLocalMusic(filesArray);
            }
        }
        //-->signal from Client Of request to play next or last
        Connections {
            target: Client
            function onTriggerPlayNext() {
                if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                    return;
                localMusciStack_playlistView.dealClicked((localMusciStack_playlistView.curSelectingIndex + 1) % localMusciStack_playlistView.listModel.count);
            }
            function onTriggerPlayLast() {
                if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                    return;
                localMusciStack_playlistView.dealClicked(localMusciStack_playlistView.curSelectingIndex - 1 < 0 ? localMusciStack_playlistView.listModel.count - 1 : localMusciStack_playlistView.curSelectingIndex - 1);
            }
        }
    }
}
