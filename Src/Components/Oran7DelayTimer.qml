import QtQuick

QtObject {
    id: root

    // 异步延时函数 -可以直接在JS中调用
    function delay(ms) {
        return new Promise(function(resolve) {
            var timer = Qt.createQmlObject(
                'import QtQuick; Timer {interval: ' + ms + '; repeat: false; running: true; onTriggered: { if(parent._resolve) parent._resolve(); this.destroy() } }',
                root,
                "delayTimer"
            );
            root._resolve = resolve;
        });
    }

    property var _resolve: null
}
