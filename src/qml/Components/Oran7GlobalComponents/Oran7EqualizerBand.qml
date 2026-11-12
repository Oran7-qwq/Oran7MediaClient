import QtQuick
import QtQuick.Controls


// 单段均衡器竖条：值在上、滑轨居中、频率标签在下。
Item {
    id: root

    property real bandGain: 0
    property string bandLabel: ""
    property color ink: "#e4e6ed"
    property color accent: "#beb7ff"
    property color trackColor: "#434653"
    property string fontFamily: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
    signal gainEdited(real value)

    implicitWidth: 52
    implicitHeight: 248

    Column {
        anchors.fill: parent
        spacing: 2

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: Math.abs(root.bandGain) < 0.05 ? "0" : root.bandGain.toFixed(1)
            font.family: root.fontFamily
            font.pixelSize: 12
            color: root.ink
        }

        Item {
            width: parent.width
            height: parent.height - 32

            Slider {
                id: slider
                objectName: "equalizerSlider"
                anchors.fill: parent
                orientation: Qt.Vertical
                from: -12
                to: 12
                stepSize: 0.5
                value: root.bandGain
                onMoved: root.gainEdited(value)

                background: Rectangle {
                    x: (slider.width - width) / 2
                    y: slider.topPadding + 7
                    width: 3
                    height: Math.max(0, slider.availableHeight - 14)
                    radius: 1.5
                    color: root.trackColor

                    Rectangle {
                        x: 0
                        width: parent.width
                        radius: parent.radius
                        color: root.accent
                        // 0dB 在中间；正增益从中间向上，负增益向下。
                        height: Math.abs(slider.visualPosition - 0.5) * parent.height
                        y: Math.min(slider.visualPosition, 0.5) * parent.height
                    }
                }

                handle: Rectangle {
                    x: (slider.width - width) / 2
                    y: slider.topPadding + slider.visualPosition * (slider.availableHeight - height)
                    width: 24
                    height: 14
                    radius: 5
                    color: slider.hovered || slider.pressed
                           ? root.ink : root.accent
                    Rectangle { anchors.centerIn: parent; width: 12; height: 2; radius: 1; color: root.trackColor }
                }
            }
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.bandLabel
            font.family: root.fontFamily
            font.pixelSize: 12
            color: root.ink
        }
    }
}
