import QtQuick

// 面板只管理自身动画，容器统一决定何时展示/隐藏整个窗口。
Item {
    id: root
    property real savedNormalY: 40
    property real savedNormalHeight: 200
    property real animationDuration: 300
    property real revealProgress: 0
    property string transitionState: "Closed"
    readonly property bool isAnimating: transitionState === "Opening" || transitionState === "Closing"
    height: root.savedNormalHeight * root.revealProgress
    opacity: root.revealProgress
    visible: root.transitionState === "Opening" || root.transitionState === "Open" || root.transitionState === "Closing"
    y: 0

    signal opened()
    signal closed()

    // 高度始终绑定进度，动画不再直接接管 height 或在结束时恢复旧高度绑定。
    onRevealProgressChanged: root.y = root.savedNormalY * root.revealProgress

    function resetClosed() {
        opening.stop()
        closing.stop()
        root.transitionState = "Closed"
        root.revealProgress = 0
        root.y = 0
    }
    function prepareForOpen() {
        root.resetClosed()
        root.transitionState = "Prepared"
    }
    function startOpenAnimation() {
        closing.stop()
        root.transitionState = "Opening"
        opening.restart()
    }
    function startCloseAnimation() {
        opening.stop()
        if (root.revealProgress <= 0) {
            root.resetClosed()
            root.closed()
            return
        }
        root.transitionState = "Closing"
        closing.restart()
    }

    NumberAnimation {
        id: opening
        target: root
        property: "revealProgress"
        to: 1
        duration: root.animationDuration
        easing.type: Easing.OutCubic
        onFinished: {
            root.transitionState = "Open"
            root.y = root.savedNormalY
            root.opened()
        }
    }
    NumberAnimation {
        id: closing
        target: root
        property: "revealProgress"
        to: 0
        duration: root.animationDuration
        easing.type: Easing.InExpo
        onFinished: {
            root.resetClosed()
            root.closed()
        }
    }
}
