import QtQuick
import QtQuick.Controls

import "../Settings/GlobalSettings"

Item {
    id: root
    width: 600
    height: 400

    // 打开颜色选择器的函数
    function openColorPicker() {
        var picker = Qt.createComponent("qrc:/qml/Components/Oran7ColorPickerWindow.qml").createObject(root)
        picker.selectedColor = currentColorDisplay.color
        picker.colorSelected.connect(function(selectedColor) {
            currentColorDisplay.color = selectedColor
            colorText.text = "# + Oran7ColorPickerWindow.colorToHexString(selectedColor)"
            picker.destroy()
        })
        picker.colorPickerCanceled.connect(function() {
            picker.destroy()
        })
    }

    // 当前选中的颜色显示
    Rectangle {
        id: currentColorDisplay
        anchors.centerIn: parent
        width: 150
        height: 150
        radius: 12
        border.width: 2
        border.color: Oran7MainUiSetting.itemBackColor
        color: "#4CAF50"

        // 阴影效果
        layer.enabled: true
        layer.effect: DropShadow {
            horizontalOffset: 0
            verticalOffset: 4
            radius: 12
            samples: 25
            spread: 0.3
            color: "#40000000"
            transparentBorder: true
        }

        Text {
            anchors.centerIn: parent
            text: "点击下方按钮\n打开颜色选择器"
            font.pixelSize: 14
            font.family: Oran7MainUiSetting.fontFamily
            color: "white"
            textFormat: Text.PlainText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // 颜色HEX值显示
    Text {
        id: colorText
        anchors.top: currentColorDisplay.bottom
        anchors.topMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        text: "#4CAF50"
        font.pixelSize: 18
        font.family: Oran7MainUiSetting.fontFamily
        color: Oran7MainUiSetting.textColor
        font.bold: true
    }

    // 打开颜色选择器按钮
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40
        anchors.horizontalCenter: parent.horizontalCenter
        width: 180
        height: 48
        radius: 8
        color: "#2196F3"

        Text {
            anchors.centerIn: parent
            text: "选择颜色"
            font.pixelSize: 16
            font.family: Oran7MainUiSetting.fontFamily
            color: "white"
            font.bold: true
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            hoverEnabled: true
            onPressed: parent.color = "#1E5CB1"
            onReleased: parent.color = "#2196F3"
            onClicked: root.openColorPicker()
        }
    }
}