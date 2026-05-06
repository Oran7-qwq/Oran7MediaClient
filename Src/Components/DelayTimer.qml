import QtQuick

QtObject {
    // 异步延时函数 - 推荐使用
    function delay(ms) {
        return new Promise(function(resolve) {
            var timer = Qt.createQmlObject('import QtQuick; Timer {interval: ' + ms + '; onTriggered: parent._resolve()}', root);
            timer.start();
            root._resolve = resolve;
        });
    }

    // 同步延时函数（通过回调）
    function delayCallback(ms, callback) {
        var timer = Qt.createQmlObject('import QtQuick; Timer {interval: ' + ms + '; onTriggered: { if(parent._callback) parent._callback() } }', root);
        timer.start();
        root._callback = callback;
    }

    property var _resolve: null
    property var _callback: null
}