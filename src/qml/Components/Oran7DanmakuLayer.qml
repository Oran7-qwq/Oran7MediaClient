import QtQuick
import QtQuick.Controls

// 弹幕显示层：铺满视频区域，从右向左匀速滚动，支持多轨道避让与醒目留言横幅。
// 用法：
//   layer.active = true                       // 激活（false 时自动清空并隐藏）
//   layer.spawn("文本", 0xffffff)             // 普通弹幕（颜色为 RGB 十进制）
//   layer.showSuperChat("文本", "用户", 30, 0xff5c6a)
Item {
    id: root

    property bool active: false
    property real speed: 140            // 弹幕滚动速度 px/s
    property int maxConcurrent: 1000    // 同时存在的最多弹幕条数（高并发房间不丢弹幕）
    property int danmakuFontSize: 26    // 普通弹幕字号
    property int laneHeight: 36         // 每条弹幕轨道高度

    clip: true
    visible: root.active

    onActiveChanged: {
        if (!root.active)
            root.clear()
    }

    // ============ 内部状态 ============
    property var activeDanmaku: []      // 所有正在滚动的弹幕
    property var lanes: []              // lanes[i] = 第 i 条轨道上的弹幕列表

    // 用于同步测量弹幕文本宽度（避免创建后再量）
    TextMetrics {
        id: metrics
        font.family: "微软雅黑"
        font.bold: true
    }

    // 弹幕条目：内部自带滚动动画
    Component {
        id: danmakuItemComponent
        Text {
            id: item
            property int lane: -1
            property var layerRoot: null

            color: "white"
            style: Text.Outline
            styleColor: "#B0000000"

            NumberAnimation {
                id: scrollAnim
                running: false
                duration: 6000
                easing.type: Easing.Linear
                target: item
                property: "x"
                onFinished: {
                    if (item.layerRoot)
                        item.layerRoot.onDanmakuFinished(item)
                }
            }

            function startScroll(fromX, toX, ms) {
                item.x = fromX
                scrollAnim.from = fromX
                scrollAnim.to = toX
                scrollAnim.duration = ms
                scrollAnim.start()
            }
        }
    }

    function laneCount() {
        return Math.max(1, Math.floor(root.height / root.laneHeight))
    }

    function ensureLanes() {
        var n = root.laneCount()
        while (lanes.length < n)
            lanes.push([])
    }

    // 轨道选择：
// 1) 优先复用"可用"的非空轨道（其最后一条弹幕右边缘已离开入口 → 新弹幕可紧跟其后不重叠），
//    选其中最前进（x 最小）的一条，让弹幕在轨道上排队而不是四散
// 2) 没有可用的非空轨道 → 用一条空轨道（避免与入口处弹幕重叠）
// 3) 全部轨道都忙着 → 勉强复用最前进的一条（接受轻微重叠）
function chooseLane() {
    var n = root.laneCount()
    ensureLanes()

    var bestAvail = -1
    var bestAvailX = Infinity
    for (var i = 0; i < n; i++) {
        var lane = lanes[i]
        if (lane.length === 0) continue
        var last = lane[lane.length - 1]
        if (last.x + last.width <= root.width) {
            if (last.x < bestAvailX) {
                bestAvailX = last.x
                bestAvail = i
            }
        }
    }
    if (bestAvail >= 0) return bestAvail

    for (var j = 0; j < n; j++) {
        if (lanes[j].length === 0) return j
    }

    var best = 0
    var bestX = Infinity
    for (var k = 0; k < n; k++) {
        var lk = lanes[k]
        if (lk.length === 0) continue
        if (lk[lk.length - 1].x < bestX) {
            bestX = lk[lk.length - 1].x
            best = k
        }
    }
    return best
}

    // 普通弹幕
    function spawn(text, color) {
        if (!root.active || !root.visible) return
        if (text === undefined || text === null || text.length === 0) return
        if (activeDanmaku.length >= root.maxConcurrent) return

        metrics.font.pixelSize = root.danmakuFontSize
        metrics.text = text
        var tw = Math.max(metrics.advanceWidth, 16)

        var lane = chooseLane()
        var item = danmakuItemComponent.createObject(laneLayer)
        item.layerRoot = root
        item.lane = lane
        item.text = text
        item.font.family = "微软雅黑"
        item.font.pixelSize = root.danmakuFontSize
        item.font.bold = true
        if (typeof color === "number" && color > 0) {
            item.color = Qt.rgba(((color >> 16) & 0xff) / 255,
                                 ((color >> 8) & 0xff) / 255,
                                 (color & 0xff) / 255, 1)
        }
        item.y = lane * root.laneHeight + (root.laneHeight - item.height) * 0.5

        activeDanmaku.push(item)
        lanes[lane].push(item)

        var dist = root.width + tw + 30
        var ms = dist / root.speed * 1000
        item.startScroll(root.width, -tw - 20, ms)
    }

    function onDanmakuFinished(item) {
        if (!item) return
        var idx = activeDanmaku.indexOf(item)
        if (idx >= 0) activeDanmaku.splice(idx, 1)
        if (item.lane >= 0 && item.lane < lanes.length) {
            var lane = lanes[item.lane]
            var li = lane.indexOf(item)
            if (li >= 0) lane.splice(li, 1)
        }
        item.destroy()
    }

    function clear() {
        for (var i = activeDanmaku.length - 1; i >= 0; i--) {
            var item = activeDanmaku[i]
            if (item) item.destroy()
        }
        activeDanmaku = []
        lanes = []
    }

    // ============ 弹幕条目容器 ============
    Item {
        id: laneLayer
        anchors.fill: parent
        z: 1
    }

    // ============ 醒目留言（顶部横幅） ============
    Rectangle {
        id: superChatBanner
        visible: false
        opacity: 0
        z: 5
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 10
        height: 46
        radius: 8
        color: "#FF5C6A"
        border.color: "#40FFFFFF"
        border.width: 1

        Row {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 10
            Label {
                id: superChatPrice
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 18
                font.bold: true
                font.family: "微软雅黑"
                color: "#FFF2B0"
            }
            Label {
                id: superChatText
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 16
                font.family: "微软雅黑"
                color: "white"
            }
        }

        Timer {
            id: superChatHideTimer
            interval: 8000
            repeat: false
            onTriggered: {
                if (superChatBanner.visible)
                    fadeOutSuperChat.start()
            }
        }
        NumberAnimation {
            id: fadeInSuperChat
            target: superChatBanner
            property: "opacity"
            from: 0
            to: 1
            duration: 250
        }
        NumberAnimation {
            id: fadeOutSuperChat
            target: superChatBanner
            property: "opacity"
            from: 1
            to: 0
            duration: 500
            onFinished: superChatBanner.visible = false
        }

        function showSuperChat(text, uname, price, color) {
            superChatText.text = (uname ? uname + "：" : "") + text
            superChatPrice.text = "¥" + price
            if (typeof color === "number" && color > 0) {
                superChatBanner.color = Qt.rgba(((color >> 16) & 0xff) / 255,
                                                ((color >> 8) & 0xff) / 255,
                                                (color & 0xff) / 255, 1)
            }
            var w = 30 + superChatPrice.implicitWidth + superChatText.implicitWidth
            superChatBanner.width = Math.min(w, root.width * 0.85)
            superChatBanner.visible = true
            fadeOutSuperChat.stop()
            fadeInSuperChat.start()
            superChatHideTimer.restart()
        }
    }
}