// Oran7BlurCard
import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Item {
    id: root

    property Item blurSource
    property real blurAmount: 0.65
    //property bool dragable: false

    property real blurMax: 48
    property real borderRadius: 7
    property color borderColor: "#44FFFFFF"
    property real borderWidth: 1
    property color themeColor: "#24FFFFFF"

    property bool blurEnabled: true

    // 给 blur 预留额外采样区域
    property real blurPadding: Math.ceil(root.blurMax * root.blurAmount) + 4

    default property alias content: contentItem.data

    width: 300
    height: 200

    // MouseArea {
    //     anchors.fill: parent
    //     drag.target: root
    //     drag.axis: Drag.XAndYAxis
    //     enabled: root.dragable
    // }

    // 捕获背景内容：注意这里比 root 本身大一圈
    ShaderEffectSource {
        id: effectSource

        x: -root.blurPadding
        y: -root.blurPadding
        width: root.width + root.blurPadding * 2
        height: root.height + root.blurPadding * 2

        sourceItem: root.blurSource

        sourceRect: {
            if (!root.blurSource)
                return Qt.rect(0, 0, effectSource.width, effectSource.height)

            // 强制 sourceRect 在窗口 resize、anchor 改变、root 移动时刷新
            const forceUpdate =
                root.x + root.y + root.width + root.height +
                effectSource.width + effectSource.height +
                (root.parent ? root.parent.width + root.parent.height : 0) +
                root.blurSource.width + root.blurSource.height

            var pos = root.mapToItem(
                root.blurSource,
                -root.blurPadding,
                -root.blurPadding
            )

            return Qt.rect(
                pos.x,
                pos.y,
                effectSource.width,
                effectSource.height
            )
        }

        live: root.visible && root.blurEnabled && root.blurSource !== null
        recursive: false
        visible: false
    }

    // 遮罩也要和扩大后的 effectSource 对齐
    Item {
        id: maskItem

        x: -root.blurPadding
        y: -root.blurPadding
        width: root.width + root.blurPadding * 2
        height: root.height + root.blurPadding * 2

        layer.enabled: true
        layer.smooth: true
        visible: false

        Rectangle {
            x: root.blurPadding
            y: root.blurPadding
            width: root.width
            height: root.height
            radius: root.borderRadius
            color: "white"
        }
    }

    MultiEffect {
        x: -root.blurPadding
        y: -root.blurPadding
        width: root.width + root.blurPadding * 2
        height: root.height + root.blurPadding * 2

        source: effectSource

        autoPaddingEnabled: false

        blurEnabled: root.blurEnabled
        blurMax: root.blurMax
        blur: root.blurAmount

        maskEnabled: true
        maskSource: maskItem

        saturation: 0.3
        brightness: 0.1
        contrast: 0.3
        colorization: 0.0

        visible: root.blurEnabled && root.blurSource !== null
    }

    // 玻璃底色
    Rectangle {
        anchors.fill: parent
        radius: root.borderRadius
        color: root.themeColor
        z: 1
        border.color: root.borderColor
        border.width: root.borderWidth
        visible: root.blurEnabled
    }

    // 顶部高光线，增强玻璃质感
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        radius: root.borderRadius
        color: "#22FFFFFF"
        opacity: 0.5
        z: 2
        visible: root.blurEnabled
    }

    Item {
        id: contentItem
        anchors.fill: parent
        clip: false
        z: 3
    }
}
