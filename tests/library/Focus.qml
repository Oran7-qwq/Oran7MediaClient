import QtQml
QtObject {
 id: root
 property var listModel: trackModels.local
 property QtObject playlistFlickable: QtObject { property int playingIndex: -1; property int itemSelected_index: -1 }
 readonly property int playing: playlistFlickable.playingIndex
 readonly property int selecting: playlistFlickable.itemSelected_index
 property Connections changes: Connections {
  target: root.listModel
  function onCountChanged() { Qt.callLater(root.syncIndicesToCurrentMedia) }
 }
function indexForSongId(songId) {
        if (!root.listModel || !songId || songId <= 0) return -1;
        for (var k = 0; k < root.listModel.count; k++) {
            if (root.listModel.get(k).songId === songId) return k;
        }
        return -1;
    }
function indexForFilepath(filepath) {
        if (!root.listModel || !filepath) return -1;
        for (var k = 0; k < root.listModel.count; k++) {
            var item = root.listModel.get(k);
            if (item.filepath === filepath || item.url === filepath) return k;
        }
        return -1;
    }
function indexForCurrentPlaying() {
        if (Client.playback.currentTrack.songId > 0) {
            var sidx = root.indexForSongId(Client.playback.currentTrack.songId);
            if (sidx >= 0)
                return sidx;
        }
        var fp = Client.playback.currentTrack.path;
        if (fp && fp.length > 0) {
            var fpidx = root.indexForFilepath(fp);
            if (fpidx >= 0)
                return fpidx;
        }
        return -1;
    }
function syncIndicesToCurrentMedia() {
        var idx = root.indexForCurrentPlaying();
        playlistFlickable.itemSelected_index = idx;
        playlistFlickable.playingIndex = BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_MusicPlayerIndex
            && Client.playback.active ? idx : -1;
    }
}
