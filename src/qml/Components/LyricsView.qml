import QtQuick 2.15
import QtQuick.Controls
import Client 1.0
import LyricsModel 1.0
import BasicConfig 1.0

import Oran7UI.Impl

Item {
    id: lyricsViewRoot

    // --- public ---
    property color hightLight_LyricsColor: "#fef2e8"
    property color deepLight_LyricsColor: "#8a8a8f"
    property int hightLight_LyricsFontSize: 25
    property int deepLight_LyricsFontSize: 20

    // --- private ---

    // 用户是否正在手动滚动/交互
    property bool userInteracting: false
    // 用户停止交互后，延迟恢复自动滚动的定时器
    property int autoScrollResumeDelay: 1500

    // 在线歌词搜索状态：0=默认(纯音乐请欣赏) 1=正在拉取 2=未找到
    property int _onlineSearchStatus: 0

    LyricsModel {
        id: lyricsModel
    }

    // 公共方法：立即跳转到当前活跃歌词行（无动画，用于窗口打开时）
    function scrollToCurrentLine() {
        lyricsListView.centerCurrentLine(false)
    }

    // 无歌词时显示占位文字
    Label {
        visible: !lyricsModel.hasLyrics
        text: {
            if (_onlineSearchStatus === 1) return qsTr("正在拉取歌词......")
            if (_onlineSearchStatus === 2) return qsTr("未找到歌词...")
            return qsTr("纯音乐，请欣赏")
        }
        anchors.centerIn: parent
        color: lyricsViewRoot.hightLight_LyricsColor
        font.pixelSize: lyricsViewRoot.hightLight_LyricsFontSize
        font.family: "微软雅黑"
        font.bold: true
        opacity: 1

        Behavior on opacity { NumberAnimation { duration: Oran7Theme.Primary.durationFast } }
    }

    // "未找到歌词" 显示 3 秒后恢复为 "纯音乐，请欣赏"
    Timer {
        id: notFoundResetTimer
        interval: 3000
        onTriggered: lyricsViewRoot._onlineSearchStatus = 0
    }

    // 歌词 ListView
    ListView {
        id: lyricsListView
        visible: lyricsModel.hasLyrics
        anchors.fill: parent
        anchors.leftMargin: 60
        anchors.rightMargin: 60
        clip: true
        interactive: true
        flickDeceleration: 3000

        model: lyricsModel

        // 固定行高
        property real lineSpacing: 14
        property real baseLineHeight: 40

        // 用 Flickable 的 margin 代替 header/footer 做上下留白
        // 让第一行和最后一行都能滚到 ListView 正中间
        topMargin: height / 2
        bottomMargin: height / 2

        // ── 滚动定位：统一函数，避免三处公式各算各的 ──

        function targetYForCurrentLine() {
            var lineH = baseLineHeight + lineSpacing
            var activeH = lineH * 1.5

            // 当前行中心点 - ListView 可视区域中心
            var targetY = lyricsModel.currentLine * lineH - height / 2 + activeH / 2

            // 使用 topMargin 后，contentY 允许是负数，不能再从 0 开始 clamp
            var minY = -topMargin
            var maxY = Math.max(minY, contentHeight - height + bottomMargin)

            return Math.max(minY, Math.min(targetY, maxY))
        }

        function centerCurrentLine(animated) {
            if (!lyricsModel.hasLyrics || lyricsModel.currentLine < 0) return
            if (height < 200) return

            var targetY = targetYForCurrentLine()

            scrollAnimation.stop()

            if (animated) {
                scrollAnimation.from = contentY
                scrollAnimation.to = targetY
                scrollAnimation.start()
            } else {
                contentY = targetY
            }
        }

        // 窗口展开/折叠时 height 变化，利用此时机定位到当前行
        onHeightChanged: {
            if (height > 200 && lyricsModel.hasLyrics && lyricsModel.currentLine >= 0 && !lyricsViewRoot.userInteracting) {
                centerCurrentLine(false)
            }
        }

        // 平滑滚动动画
        NumberAnimation {
            id: scrollAnimation
            target: lyricsListView
            property: "contentY"
            duration: 300
            easing.type: Easing.OutCubic
        }

        // 用户开始拖拽/滑动 → 禁止自动滚动
        onMovementStarted: {
            if (!scrollAnimation.running) {
                lyricsViewRoot.userInteracting = true
                scrollAnimation.stop()
                autoScrollResumeTimer.stop()
            }
        }

        // 用户停止拖拽/惯性滚动结束 → 延迟恢复自动滚动
        onMovementEnded: {
            if (lyricsViewRoot.userInteracting) {
                autoScrollResumeTimer.restart()
            }
        }

        // 监听当前行变化，自动滚动居中
        Connections {
            target: lyricsModel
            function onCurrentLineChanged() {
                if (lyricsViewRoot.userInteracting) return
                lyricsListView.centerCurrentLine(true)
            }
        }



        delegate: Item {
            id: lyricDelegate
            width: lyricsListView.width

            property int normalHeight: lyricsListView.baseLineHeight + lyricsListView.lineSpacing
            height: isActive ? normalHeight*1.5 : normalHeight

            Behavior on height {NumberAnimation { duration: Oran7Theme.Primary.durationMid; easing.type: Easing.InOutQuad }}

            property bool isActive: (model.lineIndex === lyricsModel.currentLine)

            // ── 根据到可视区域中心的距离自动调整透明度 ──
            readonly property real _viewCenterY: lyricsListView.contentY + lyricsListView.height / 2
            readonly property real _delegateCenterY: y + height / 2
            readonly property real _distFromCenter: Math.abs(_delegateCenterY - _viewCenterY)
            readonly property real _halfViewHeight: lyricsListView.height / 2
            // 归一化距离 0(中心)→1(边缘)，夹到 [0,1]
            readonly property real _normDist: Math.min(_distFromCenter / Math.max(_halfViewHeight, 1), 1.0)
            opacity: 1.0 - 0.6 * _normDist   // 中心 1.0，边缘 0.4

            Label {
                id: lyricTextLabel
                text: model.text
                width: parent.width
                height: lyricsListView.baseLineHeight
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                color: lyricDelegate.isActive ? lyricsViewRoot.hightLight_LyricsColor : lyricsViewRoot.deepLight_LyricsColor
                font.pixelSize: lyricDelegate.isActive ? lyricsViewRoot.hightLight_LyricsFontSize : lyricsViewRoot.deepLight_LyricsFontSize
                font.bold: lyricDelegate.isActive
                font.family: "微软雅黑"
                elide: Text.ElideRight

                // 平滑颜色过渡
                Behavior on color {
                    ColorAnimation { duration: Oran7Theme.Primary.durationMid; easing.type: Easing.InOutQuad }
                }
                // 平滑字号过渡
                Behavior on font.pixelSize {
                    NumberAnimation { duration: Oran7Theme.Primary.durationMid; easing.type: Easing.InOutQuad }
                }
                //活跃行轻微放大
            }
        }
    }

    // 用户停止交互后，延迟恢复自动滚动
    Timer {
        id: autoScrollResumeTimer
        interval: lyricsViewRoot.autoScrollResumeDelay
        onTriggered: {
            lyricsViewRoot.userInteracting = false
        }
    }

    // 连接 Client 信号 - 加载/清空歌词
    Connections {
        target: Client
        function onLyricsAvailable(lrcFilePath) {
            _onlineSearchStatus = 0
            notFoundResetTimer.stop()
            lyricsModel.loadFromFile(lrcFilePath)
        }
        function onLyricsUnavailable() {
            _onlineSearchStatus = 0
            notFoundResetTimer.stop()
            lyricsModel.clearLyrics()
        }
        function onOnlineLyricsSearching() {
            _onlineSearchStatus = 1
            notFoundResetTimer.stop()
        }
        function onOnlineLyricsNotFound() {
            _onlineSearchStatus = 2
            notFoundResetTimer.start()
        }
        function onPlayProgressUpdated(CurPos, CurTime) {
            if (BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_MusicPlayerIndex) {
                lyricsModel.updateTime(CurTime)
            }
        }
    }
}
