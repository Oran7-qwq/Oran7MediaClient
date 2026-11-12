import QtQml
QtObject {
    property var avliStrAdr: ["https://fixture.invalid/stream"]
    property string selectedCodecName: "avc"
    property string selectedFormatName: "flv"
    property string requestedRoom: ""
    function getRoomInfo(room) { requestedRoom = room }
    signal urlsReady()
    signal urlsError()
}
