import QtQuick
import Qt5Compat.GraphicalEffects

import Oran7UI.Impl 1.0

import "../Oran7SettingUiWindowItems"
import "../../Settings/GlobalSettings"
import "../"

Item {
    id:root
    anchors.left: parent.left
    anchors.right: parent.right
    height:root.open__ ? line.height : 0 //bind
    visible: root.open__ //__initilization

    property bool open__: false
    property bool showTag: false

    property color checkedColor: "#000000" //bind
    property string componentName: "Oran7MainGUI"
    property string colorTokenName: "colorPrimaryBase"//bind Out,must be initilized
    property real toggleOpenAniDuration: Oran7Theme.Primary.durationMid
    property int displayAmount: 10

    signal colorReady(color seletedColor)

    onOpen__Changed: {
        if(root.open__ == true)
            root.visible = true
        else
        {
            delayTimer.delay(root.toggleOpenAniDuration).then(function(){
                root.visible = false
            });
        }
    }

    Behavior on height{NumberAnimation{
            duration: root.toggleOpenAniDuration
            easing.type:Easing.OutCubic
        }
    }

    /*
    *@notice-Link : contene_column.height -> line.height ~> root.height
    **/

    function pharseColor(tokenName, index) {
        var key = tokenName + "-" + index
        var val = Oran7Theme[root.componentName][key]
        if (val === undefined) {
            console.log("pharseColor: key=" + key + " undefined. Map keys:", Object.keys(Oran7Theme[root.componentName]))
        }
        return val
    }

    //--- LeftColumeLine ---
    Rectangle{
        id:line
        width: 2
        height: contene_column.implicitHeight
        color:Oran7MainUiSetting.textColor
        opacity: root.open__ ? 1 : 0
        Behavior on opacity {
            NumberAnimation{
                duration: root.toggleOpenAniDuration
                easing.type:Easing.OutCubic
            }
        }
    }

    Loader {
        id: contentLoader
        active: root.open__
        asynchronous: true
        anchors.left: parent.left
        anchors.right: parent.right
        Column{
            id:contene_column
            anchors.left: parent.left
            anchors.right: parent.right
            opacity: root.open__ ? 1 : 0
            Behavior on opacity {
                NumberAnimation{
                    duration: root.toggleOpenAniDuration
                    easing.type:Easing.OutCubic
                }
            }
            spacing: 0
            Repeater{
                model: root.displayAmount
                delegate: Oran7SettingItem {
                    id: element_root

                    required property int index

                    property color currentColor: pharseColor(root.colorTokenName, index + 1)

                    property real elementLeftMargin__: Oran7MainUiSetting.itemHeight * 0.75

                    anchors.rightMargin: elementLeftMargin__
                    showTag: false
                    radius: 4
                    fontBold: true
                    backColor: currentColor
                    text: formatColorText(currentColor)
                    textColor: {
                        var c = Qt.color(currentColor)
                        var brightness = c.r * 0.299 + c.g * 0.587 + c.b * 0.114
                        return brightness > 0.5 ? "black" : "white"
                    }
                    textHAlign: Text.AlignHCenter

                    Behavior on elementLeftMargin__ {
                        NumberAnimation {
                            duration: 150
                            easing.type: Easing.OutCubic
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: element_root.elementLeftMargin__ = 0
                        onExited: element_root.elementLeftMargin__ = Oran7MainUiSetting.itemHeight * 0.75
                        onClicked: root.colorReady(element_root.currentColor)
                    }
                }
            }
            // 当前颜色显示项
            Oran7SettingItem {
                id: currentColorDisplay
                anchors.left: parent.left
                anchors.right: parent.right
                height: Oran7MainUiSetting.itemHeight
                radius: 4
                text: "AutoDefine:"
                showTag: false

                // 颜色选择器按钮
                Rectangle {
                    id: colorPickerButton
                    width: parent.width * 0.45
                    height: Oran7MainUiSetting.itemHeight * 0.8
                    radius: 4
                    anchors.right: parent.right
                    anchors.rightMargin: Oran7MainUiSetting.itemHeight * 0.75
                    anchors.verticalCenter: parent.verticalCenter
                    color: Oran7MainUiSetting.itemBackColor
                    border.width: 1
                    border.color: getBorderColor()

                    // 按钮内容
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: Oran7MainUiSetting.itemHeight * 0.133
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        // 颜色预览方块
                        Rectangle {
                            width: Oran7MainUiSetting.itemHeight * 0.5
                            height: width
                            anchors.verticalCenter: parent.verticalCenter
                            radius: 2
                            color: root.checkedColor
                        }

                        // 颜色文本
                        Text {
                            id: colorText
                            text: formatColorText(root.checkedColor)
                            anchors.verticalCenter: parent.verticalCenter
                            font.pixelSize: Oran7MainUiSetting.textPixelSize
                            font.family: Oran7MainUiSetting.fontFamily
                            color: Oran7MainUiSetting.isDarkMode ? "white" : "black"
                            font.bold: false
                        }
                    }

                    // 鼠标交互区域
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered:
                            root.buttonHovered = true
                        onExited:
                            root.buttonHovered = false

                        onPressed: root.openColorPicker()
                    }
                }
            }
        }
    }

    // --- Tools ---
    Oran7DelayTimer{
        id:delayTimer
    }

    // 辅助函数：格式化颜色文本
    function formatColorText(color) {
        if (!color || color.a <= 0) {
            return "#------";
        }
        var r = Math.round(color.r * 255).toString(16).padStart(2, '0').toUpperCase()
        var g = Math.round(color.g * 255).toString(16).padStart(2, '0').toUpperCase()
        var b = Math.round(color.b * 255).toString(16).padStart(2, '0').toUpperCase()
        return "#" + r + g + b
    }

    // 辅助函数：获取边框颜色
    property bool buttonHovered: false
    function getBorderColor() {
        return root.buttonHovered ? root.checkedColor :
               (Oran7MainUiSetting.isDarkMode ? "#3e3e3e" : "#c8c8c8")
    }

    function openColorPicker() {
        var component = Qt.createComponent("qrc:/src/qml/Components/Oran7ColorPickerWindow.qml")
        if (component.status !== Component.Ready) {
            console.log(component.errorString())
            return
        }

        var parentWindow = root.Window.window

        var picker = component.createObject(
            parentWindow ? parentWindow : null,
            {
                selectedColor: root.checkedColor
            }
        )

        if (!picker) {
            return
        }

        var globalPos = root.mapToGlobal(0, 0)
        picker.x = globalPos.x + root.width / 2 - picker.width / 2
        picker.y = globalPos.y + root.height

        picker.showPicker()

        picker.colorSelected.connect(function(selectedColor) {
            root.colorReady(selectedColor)

            var hexString = typeof picker.colorToHexString === "function"
                ? picker.colorToHexString(selectedColor)
                : formatColorText(selectedColor)

            colorText.text = "#" + hexString
            picker.destroy()
        })

        picker.colorPickerCanceled.connect(function() {
            picker.destroy()
        })
    }
}
