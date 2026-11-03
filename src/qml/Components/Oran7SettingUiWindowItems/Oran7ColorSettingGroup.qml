import QtQuick
import "../../Settings/GlobalSettings"

Item {
    id: root
    anchors.left: parent.left
    anchors.right: parent.right
    height: contentColumn.height
    property string title: "title"
    property bool textField_detectEnable: true
    readonly property string textFiled_text: input.tempText
    property color checkedColor: "#ffffff"
    property string colorToken: "-"
    property string componentName: "-"
    property int displayAmount: 10
    property alias pickerLabel: colorMenu.pickerLabel
    property alias presetColors: colorMenu.presetColors
    property alias presetNames: colorMenu.presetNames
    signal enterOfTextFiled(string text)
    signal colorReady(color selectedColor)

    Column {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        Oran7SettingItem {
            text: root.title
            showTag: false
        }
        Oran7TextFieldItem {
            id: input
            tempText: ""
            placeholderText: tempText.length <= 0 ? '"' + String(root.checkedColor) + '"' : ""
            detectEnable: root.textField_detectEnable
            detectType: Oran7MainUiSetting.DetectionType.ColorDetection
            onEnterPressed: root.enterOfTextFiled(tempText)
            anchors.rightMargin: colorMenu.width + 4
            Oran7ColorSelectItem {
                id: colorMenu
                objectName: root.objectName + "ColorChoice"
                title: root.title
                checkedColor: root.checkedColor
                colorTokenName: root.colorToken
                componentName: root.componentName
                displayAmount: root.displayAmount
                onColorReady: function(color) {
                    root.colorReady(color)
                    input.textField.text = String(color)
                }
            }
        }
    }
}
