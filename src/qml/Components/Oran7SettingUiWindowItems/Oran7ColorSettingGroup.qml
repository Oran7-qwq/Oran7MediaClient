import QtQuick

import "../../Settings/GlobalSettings"

Item {
    id:root
    property string title: "title"
    property bool textField_detectEnable: true
    readonly property string textFiled_text: item2.tempText  //bind

    property color checkedColor: "#ffffff" //bind Out
    property string colorToken: "-" //bind Out,must be initilized
    property string componentName: "-" //bind Out,must be initilized

    signal enterOfTextFiled(string text)
    signal colorReady(color seletedColor)

    anchors.left: parent.left
    anchors.right: parent.right
    height:item1.height +item2.height + color_Group.height
    Column{
        anchors.top: root.top
        anchors.left: parent.left
        anchors.right: parent.right
        //Title
        Oran7SettingItem{
            id:item1
            text:root.title
        }
        //TextField
        Oran7TextFieldItem{
            id:item2
            tempText: ""
            placeholderText:tempText.length <=0 ?
                                "\""+String(colorBox.color) + "\"" : ""
            detectEnable: root.textField_detectEnable
            detectType: Oran7MainUiSetting.DetectionType.ColorDetection
            onEnterPressed: {console.log(tempText);root.enterOfTextFiled()}
            anchors.rightMargin: Oran7MainUiSetting.itemHeight * 0.7 + 2
            Oran7ColorCheckBox{
                id:colorBox
                color:root.checkedColor
            }
        }
        //ColorSelectGroup
        Oran7ColorSelectItem{
            id:color_Group
            checkedColor:root.checkedColor
            colorTokenName:root.colorToken
            componentName: root.componentName
            open__: colorBox.checked
            onColorReady:function(seletedColor){
                root.colorReady(seletedColor)
                item2.textField.text = String(seletedColor)
            }
        }
    }
}
