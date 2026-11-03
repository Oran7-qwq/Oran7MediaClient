// Oran7ValueSlider.qml
import QtQuick

import Oran7UI.Impl

Item {
    id: root

    width: parent ? parent.width * 0.8 : 100
    height: 20

    // =========================================================
    // API
    // =========================================================

    // 数值范围，可以是 -1 到 1，也可以是 0 到 100
    property real from: 0
    property real to: 100

    // 外部绑定的当前值
    // 例如：value: player.volume
    property real value: 0

    // 最大阈值，例如 65535
    // 鼠标松开时会根据滑块比例换算出 0 ~ thresholdMaximum 的整数位置
    property int thresholdMaximum: 65535

    // 步进值。0 表示不吸附
    // 例如 from: -1, to: 1, stepSize: 0.01
    property real stepSize: 0

    // 小数保留位数。-1 表示不主动处理
    property int valueDecimals: -1

    // 是否拖动时直接写回 root.value
    // 如果 value 是外部绑定，建议保持 false
    property bool liveUpdateValue: false

    // 鼠标松开时是否把最终值写回 root.value
    // 如果 value 是外部绑定，建议保持 false，由外部在 onCommitted 里更新数据源
    property bool writeBackOnRelease: false

    //应用相关鼠标事件
    property bool interactive: true

    // --- 外观 API ---
    property var sliderColor_themeItems: [
        { default_Color: "#00DDDD", sel_Color: "cyan" },
        { default_Color: "#c74054", sel_Color: "#fc3c55" }
    ]

    property int sliderColor_themeIndex: 1

    property color trackColor: "gray"
    property real trackOpacity: 0.3
    property real trackHeight: 10

    property int progressHandleWidth: 12
    property color progressHandleColor: "white"

    property bool handleVisibleWhenHover: true
    property int animationDuration: Oran7Theme.Primary.durationFast

    // --- 只读状态 ---
    readonly property bool pressed: mouseArea.pressed
    readonly property bool hovered: mouseArea.containsMouse

    // 当前显示值。拖动过程中会变化
    readonly property real visualValue: root._visualValue

    // 当前显示比例，范围 0 ~ 1
    readonly property real visualRatio: root._ratioFromValue(root._visualValue)

    // 当前阈值位置，范围 0 ~ thresholdMaximum
    readonly property int thresholdPosition: root._thresholdFromRatio(root.visualRatio)

    // =========================================================
    // Signals
    // =========================================================

    // 拖动过程中触发
    signal moved(real value, int thresholdPosition, real ratio)

    // 鼠标松开时触发
    signal committed(real value, int thresholdPosition, real ratio)
    signal positionChanged(real value, int thresholdPosition, real ratio)

    // =========================================================
    // Private state
    // =========================================================

    property real _visualValue: _normalizeValue(value)

    onValueChanged: {
        if (!mouseArea.pressed)
            root._visualValue = root._normalizeValue(root.value)
    }

    onFromChanged: {
        root._visualValue = root._normalizeValue(root.value)
    }

    onToChanged: {
        root._visualValue = root._normalizeValue(root.value)
    }

    onStepSizeChanged: {
        root._visualValue = root._normalizeValue(root.value)
    }

    function _themeItem() {
        if (!root.sliderColor_themeItems || root.sliderColor_themeItems.length <= 0)
            return { default_Color: "#c74054", sel_Color: "#fc3c55" }

        var index = Math.max(0, Math.min(root.sliderColor_themeIndex,
                                         root.sliderColor_themeItems.length - 1))
        return root.sliderColor_themeItems[index]
    }

    function _clamp01(v) {
        return Math.max(0, Math.min(1, v))
    }

    function _boundedValue(v) {
        if (isNaN(v))
            v = root.from

        var minValue = Math.min(root.from, root.to)
        var maxValue = Math.max(root.from, root.to)

        return Math.max(minValue, Math.min(maxValue, v))
    }

    function _normalizeValue(v) {
        v = root._boundedValue(v)

        if (root.stepSize > 0) {
            var steps = Math.round((v - root.from) / root.stepSize)
            v = root.from + steps * root.stepSize
            v = root._boundedValue(v)
        }

        if (root.valueDecimals >= 0) {
            var factor = Math.pow(10, root.valueDecimals)
            v = Math.round(v * factor) / factor
        }

        return v
    }

    function _valueFromRatio(ratio) {
        ratio = root._clamp01(ratio)
        return root.from + ratio * (root.to - root.from)
    }

    function _ratioFromValue(v) {
        if (root.to === root.from)
            return 0

        v = root._boundedValue(v)
        return root._clamp01((v - root.from) / (root.to - root.from))
    }

    function _thresholdFromRatio(ratio) {
        ratio = root._clamp01(ratio)
        return Math.round(ratio * Math.max(0, root.thresholdMaximum))
    }

    function valueFromThresholdPosition(position) {
        if (root.thresholdMaximum <= 0)
            return root._normalizeValue(root.from)

        var ratio = root._clamp01(position / root.thresholdMaximum)
        return root._normalizeValue(root._valueFromRatio(ratio))
    }

    function setVisualValue(v) {
        root._visualValue = root._normalizeValue(v)
    }

    function setVisualValueFromThresholdPosition(position) {
        root._visualValue = root.valueFromThresholdPosition(position)
    }

    function _setFromMouseX(mouseX, emitMovingSignal) {
        var ratio = root._clamp01(mouseX / Math.max(1, root.width))
        var newValue = root._normalizeValue(root._valueFromRatio(ratio))
        var newRatio = root._ratioFromValue(newValue)
        var newThresholdPosition = root._thresholdFromRatio(newRatio)

        root._visualValue = newValue

        if (root.liveUpdateValue)
            root.value = newValue

        if (emitMovingSignal) {
            root.moved(newValue, newThresholdPosition, newRatio)
            root.positionChanged(newValue, newThresholdPosition, newRatio)
        }
    }

    // =========================================================
    // UI
    // =========================================================

    Rectangle {
        id: track

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        height: root.trackHeight
        radius: height / 2
        color: root.trackColor
        opacity: root.trackOpacity
        clip: true

        Behavior on width {NumberAnimation {duration: root.animationDuration}}
    }

    Rectangle {
        id: progress

        anchors.left: track.left
        anchors.verticalCenter: track.verticalCenter

        height: track.height
        width: root.visualRatio * track.width
        radius: height / 2

        color: root.hovered || root.pressed
               ? root._themeItem().sel_Color
               : root._themeItem().default_Color

        Behavior on width {NumberAnimation {duration: root.animationDuration}}
    }

    Rectangle {
        id: handle

        width: root.progressHandleWidth
        height: width
        radius: width / 2
        color: root.progressHandleColor

        x: root.visualRatio * root.width - width / 2
        anchors.verticalCenter: track.verticalCenter

        visible: root.handleVisibleWhenHover
                 ? (root.hovered || root.pressed)
                 : true

        z: 2

        Behavior on x {NumberAnimation {duration: root.animationDuration}}
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent
        hoverEnabled: true
        enabled: root.interactive
        cursorShape: Qt.PointingHandCursor

        onPressed: mouse => {
            root._setFromMouseX(mouse.x, true)
        }

        onPositionChanged: mouse => {
            if (mouseArea.pressed)
                root._setFromMouseX(mouse.x, true)
        }

        onReleased: mouse => {
            root._setFromMouseX(mouse.x, false)

            var finalValue = root.visualValue
            var finalRatio = root.visualRatio
            var finalThresholdPosition = root.thresholdPosition

            if (root.writeBackOnRelease)
                root.value = finalValue

            root.committed(finalValue, finalThresholdPosition, finalRatio)
        }
    }
}
