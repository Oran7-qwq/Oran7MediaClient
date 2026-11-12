import QtQml
QtObject {
    property bool active: false
    property bool isConnected: false
    function start(room) { active = true; isConnected = true }
    function stop() { active = false; isConnected = false }
    signal danmakuReceived(string text, string uname, int color, double timestamp)
    signal superChatReceived(string text, string uname, int price, int color)
    signal errorOccurred(string message)
    signal connectionChanged(bool connected)
}
