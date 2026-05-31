//MainStackView.qml
import QtQuick 2.15
import QtQuick.Controls
import "../../Basic"
import Client 1.0

StackView{
    id:mainStackView
    background: Rectangle{
        color: "transparent"
        anchors.fill: parent
    }

    //在实例化后的主栈窗口中建立Client连接，待客户端数据传输信号响应后调用addMusicItem导入在BasicConfig中myFavorite的musicListModel音乐信息
    Connections{
        target:Client
        function onFavoriteMusicElementReady(icon_,music_name_,music_artist_,music_album_,timesize_,music_id_)
        {
            //console.log("push======MyFavoriteMusic==========");
            BasicConfig.addMyFavoriteMusicItem(icon_,music_name_,music_artist_,music_album_,timesize_,music_id_)
        }
        function onLocalMusicElementReady(icon_,music_name_,music_artist_,music_album_,timesize_,music_id_,music_filepath)
        {
            // console.log("push======LocalMusic==========");
            BasicConfig.addLocalMusicItem(icon_,music_name_,music_artist_,music_album_,timesize_,music_filepath)
            // console.log(music_filepath);
        }
        function onLocalMusicListCleared()
        {
            BasicConfig.localMusicListModel.clear();
        }
    }
}
