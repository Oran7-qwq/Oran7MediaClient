import QtQuick 2.15
import QtQuick.Controls
import QtCore
import QtQuick.Dialogs
import Qt5Compat.GraphicalEffects
import "../../Basic"
import "../../Components"
import Client 1.0

Item {
        id:root
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.top: parent.top
        anchors.topMargin: 14
        clip: true

        //====================   define Properties    =====================//

        property string pageName: ""

        property var stackView_rootItemContainer: root //包含此组件的外部作为stackViewItem的根项

        // ==================== StackView 活动状态检测 ====================//
        property bool isActive: false
        property var _myStackView: null

        function findAndSetupStackView() {
            if (StackView.view) {
                _myStackView = StackView.view;
            } else {
                var parent = container.parent;
                var depth = 0;
                while (parent && depth < 20) {
                    if (typeof parent.push === "function" && typeof parent.pop === "function") {
                        _myStackView = parent;
                        break;
                    }
                    parent = parent.parent;
                    depth++;
                }
            }

            if (_myStackView) {
                _myStackView.currentItemChanged.connect(updateActiveState);
                updateActiveState();
            }
        }

        function updateActiveState() {
            if (!_myStackView || !_myStackView.currentItem) {
                root.isActive = false;
                return;
            }

            root.isActive = (_myStackView.currentItem === root.stackView_rootItemContainer);
            if (root.isActive) {
                //Page被激活 -> touch 业务
                root.activeResponseTask()
            }
        }

        //Page被激活 -> touch 业务
        function activeResponseTask(){
            //-->Focus item
            localMusciStack_playlistView.focus_current_playlistItem()
            localMusciStack_playlistView.animateItemsFromIndex(0)
        }

        Component.onCompleted: {
            Qt.callLater(findAndSetupStackView);
        }

        Component.onDestruction: {
            if (_myStackView) {
                _myStackView.currentItemChanged.disconnect(updateActiveState);
            }
        }

        //=======================  Ui  ====================//

        Oran7MusicPlaylistView{
            id:localMusciStack_playlistView
            anchors.fill: root

            leftMargin: 33

            //初始化属性
            titleModel: ["本地音乐","下载中歌曲"]
            listModel: BasicConfig.localMusicListModel
            isPlaying: BasicConfig.isPlaying //bind

            // ------------- innder signals -------------
            onItemDoubleClicked: function(index){
                //console.log("double clicked:",index)
                localMusciStack_playlistView.dealClicked(index)
            }

            onItemClicked: function(index){
                // console.log("clicked:",index)
                localMusciStack_playlistView.dealClicked(index)
            }
            onFocus_current_playlistItem: {
                localMusciStack_playlistView.ask_dir_of_item_index(BasicConfig.currentMediaFilePath)
            }

            // ------------- functions -------------

            function dealClicked(index){
                //async globalPlayingFocus
                if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                {
                    BasicConfig.isPlaying =false
                    BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_MusicPlayerIndex
                }

                var item = listModel.get(index)

                if(BasicConfig.isPlaying === false && curSelectingIndex === -1)
                {
                    //--->first begain to play
                    Client.qmlClickedReqPreparePlayMusic(item.filepath)
                    BasicConfig.isPlaying = true
                    curPlayingIndex = index

                    //async bottom page info
                    BasicConfig.currentMediaName = item.music_name
                    BasicConfig.currentMediaArtistAuthor = item.music_artist
                    BasicConfig.currentMediaFilePath = item.filepath
                    BasicConfig.currentMediaCoverFilePath = item.icon//touch bottom page update
                }
                else if(BasicConfig.isPlaying === false && curSelectingIndex === index){
                    //-->pause statue,and the same file
                    Client.qmlClickedReqPreparePlayMusic(item.filepath)
                    BasicConfig.isPlaying = true
                    curPlayingIndex = index
                }
                else if(BasicConfig.isPlaying === false && curSelectingIndex !== index){
                    //-->pause statue,and request to change file
                    Client.qmlClickedReqPreparePlayMusic(item.filepath)
                    BasicConfig.isPlaying = true
                    curPlayingIndex = index

                    //change file,async bottom page info
                    BasicConfig.currentMediaName = item.music_name
                    BasicConfig.currentMediaArtistAuthor = item.music_artist
                    BasicConfig.currentMediaFilePath = item.filepath
                    BasicConfig.currentMediaCoverFilePath = item.icon//touch bottom page update
                }
                else if(BasicConfig.isPlaying === true && curSelectingIndex !== index){
                    //--->is playing while change file
                    Client.qmlClickedReqPreparePlayMusic(item.filepath)
                    BasicConfig.isPlaying = true //keep playing
                    curPlayingIndex = index

                    //change file,async bottom page info
                    BasicConfig.currentMediaName = item.music_name
                    BasicConfig.currentMediaArtistAuthor = item.music_artist
                    BasicConfig.currentMediaFilePath = item.filepath
                    BasicConfig.currentMediaCoverFilePath = item.icon//touch bottom page update
                }
                else if(BasicConfig.isPlaying === true && curSelectingIndex === index){
                    //--->is playing ,the same of file for request to pause
                    Client.qmlClickedReqPreparePlayMusic(item.filepath)
                    BasicConfig.isPlaying = false
                    curPlayingIndex = -1
                }

                //updata now selectingIndex
                curSelectingIndex = index
            }

            function deal_outClickplayReponse_asyncUiStatue(index){
                if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                    return;
                if(BasicConfig.isPlaying === false){
                    curPlayingIndex = -1
                    //console.log(index)
                }
                else if(BasicConfig.isPlaying === true){
                    curPlayingIndex = index
                }

                curSelectingIndex = index
            }

            // -------------  Connections  -------------

            //-->signal from BasicConfig ,the player core playing statue is changed, now async the playlist Ui Of statues
            Connections{
                target: BasicConfig
                function onIsPlayingChanged()
                {
                    if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                    {
                        localMusciStack_playlistView.curPlayingIndex = -1;
                        return;
                    }

                    localMusciStack_playlistView.ask_dir_of_item_index(BasicConfig.currentMediaFilePath)
                }
            }
            //--> signal from this for response to the file_path dir of item index
            Connections{
                target: localMusciStack_playlistView
                function onReponse_dir_of_item_index(index)
                {
                    localMusciStack_playlistView.deal_outClickplayReponse_asyncUiStatue(index)
                }

                function onAddNewItemOfFiles(filesArray){
                    Client.addNewLocalMusic(filesArray)
                }
            }
            //-->signal from Client Of request to play next or last
            Connections{
                target: Client
                function onTriggerPlayNext(){
                    localMusciStack_playlistView.dealClicked(
                                (localMusciStack_playlistView.curSelectingIndex+1)%localMusciStack_playlistView.listModel.count)
                }
                function onTriggerPlayLast(){
                    localMusciStack_playlistView.dealClicked(
                                localMusciStack_playlistView.curSelectingIndex-1<0 ?
                                    localMusciStack_playlistView.listModel.count - 1 :
                                    localMusciStack_playlistView.curSelectingIndex-1)
                }
            }

        }
}
