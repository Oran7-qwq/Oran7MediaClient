import QtQuick
import QtQuick.Window
import Qt5Compat.GraphicalEffects

Item {
    id: root

    property url source
    property real radius: 7
    property bool async: true
    property int decodeSize: 0
    readonly property real dpr: Math.max(1, Screen.devicePixelRatio)
    property bool tiled: false
    property real tileAreaWidth: 1600
    property bool autoPan: false
    property int panDuration: 30000

    readonly property url safeSource: {
        var s = String(root.source);
        if (!s) return "";
        if (s.indexOf("://") >= 0) return s;
        if (s.indexOf("qrc:") === 0) return s;
        return "file:///" + s.replace(/\\/g, "/");
    }

    Image {
        id: naturalMeasurer
        objectName: "roundedImageMeasurer"
        visible: false
        // 只有平铺需要原图尺寸；普通封面不再额外解码、持有一份全尺寸图片。
        source: root.tiled ? root.safeSource : ""
        asynchronous: root.async
        cache: true
    }
    readonly property real _natW: naturalMeasurer.implicitWidth
    readonly property real _natH: naturalMeasurer.implicitHeight
    readonly property int _tileCount: _natW > 0 ? Math.max(1, Math.ceil(root.tileAreaWidth / _natW) + 2) : 1
    // 左锚定：瓦片行贴容器左缘（x=0），宽度变化时左侧不动，右侧增减瓦片，不重新居中 → 不抖
    // 平移量有界，保证不露底：水平只向左（负），垂直以中心为基准上下
    readonly property real _panXMax: root.tiled ? Math.max(0, root._tileCount * _natW - 1100) : 0
    readonly property real _panYMax: root.tiled ? Math.max(0, (_natH - 60) / 2) : 0
    property real _panX: 0
    property real _panY: 0

    Item {
        id: viewport
        objectName: "roundedImageTiles"
        anchors.fill: parent
        clip: true
        visible: false
        Row {
            id: tileRow
            spacing: 0
            x: root._panX
            y: (viewport.height - tileRow.height) / 2 + root._panY
            Repeater {
                model: root.tiled ? root._tileCount : 0
                Image {
                    width: root._natW
                    height: root._natH
                    source: root.safeSource
                    asynchronous: root.async
                    smooth: true
                }
            }
        }
    }

    Image {
        id: img
        objectName: "roundedImageContent"
        anchors.fill: parent
        visible: false
        source: root.tiled ? "" : root.safeSource
        fillMode: Image.PreserveAspectCrop
        sourceSize: root.decodeSize > 0
            ? Qt.size(root.decodeSize, root.decodeSize)
            : Qt.size(Math.max(1, Math.ceil(root.width * root.dpr)), Math.max(1, Math.ceil(root.height * root.dpr)))
        asynchronous: root.async
        cache: true
        smooth: true
        mipmap: true
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        radius: root.radius
        color: "white"
        antialiasing: true
        visible: false
    }

    OpacityMask {
        anchors.fill: parent
        source: root.tiled ? viewport : img
        maskSource: mask
        cached: false
    }

    SequentialAnimation {
        id: panAnim
        running: root.autoPan && root.tiled && root.source.length > 0
        loops: Animation.Infinite
        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "_panX"
                to: -root._panXMax
                duration: root.panDuration
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                target: root
                property: "_panY"
                to: -root._panYMax
                duration: root.panDuration
                easing.type: Easing.InOutSine
            }
        }
        PauseAnimation { duration: 1200 }
        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "_panX"
                to: 0
                duration: root.panDuration
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                target: root
                property: "_panY"
                to: 0
                duration: root.panDuration
                easing.type: Easing.InOutSine
            }
        }
        PauseAnimation { duration: 1200 }
    }
}
