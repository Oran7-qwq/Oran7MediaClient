pragma Singleton
import QtQuick
QtObject {
    property bool isLoggedIn: false
    property string userName: ""
    property string cookieString: ""
    property string csrfToken: ""
    property bool online: false
    property bool running: false
    property bool loginInProgress: false
    property string statusText: ""
    property int loginState: 0
    enum State { Idle, Error, StartingBrowser, WaitingLogin, Captured }
    signal loginStatusChanged()
    signal loginSucceeded()
    signal loginFailed(string message)
    signal stateChanged()
    function start() {}
    function stop() {}
    function run(operation, arguments, done, failed) { return -1 }
    function reorderLikedSongs(ids) {}
}
