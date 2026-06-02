import QtQuick

import "../../Settings/GlobalSettings"

Item {
    id:root
    anchors.left: parent.left
    anchors.right: parent.right
    height:contentColumn.height

    // --- Public: -----
    property string title: "title"
    property bool textField_detectEnable: true
    readonly property string textFiled_text: item2.tempText  //bind

    property color checkedColor: "#ffffff" //bind Out
    property string colorToken: "-" //bind Out,must be initilized
    property string componentName: "-" //bind Out,must be initilized
    property int displayAmount: 10

    signal enterOfTextFiled(string text)
    signal colorReady(color seletedColor)

    // --- Private ---

    //property bool contentVisible: false

    // --- Ui ---
    Column{
        id:contentColumn
        anchors.top: root.top
        anchors.left: parent.left
        anchors.right: parent.right
        //Title
        Oran7SettingItem{
            id:item1
            text:root.title
            showTag: false
            // MouseArea{
            //     anchors.fill: parent
            //     acceptedButtons: Qt.RightButton
            //     onClicked: root.contentVisible = !root.contentVisible
            // }
        }
        //TextField
        Oran7TextFieldItem{
            id:item2
            // height: root.contentVisible ? Oran7MainUiSetting.itemHeight : 0
            // Behavior on height{NumberAnimation{duration: Oran7MainUiSetting.toggleOpenAniDuration - 50;easing.type:Easing.OutCubic}}
            // opacity: root.contentVisible ? 1 : 0
            // Behavior on opacity {NumberAnimation{duration: Oran7MainUiSetting.toggleOpenAniDuration - 50;easing.type:Easing.OutCubic}}
            // visible: opacity !== 0? true : false
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
            displayAmount: root.displayAmount
            onColorReady:function(seletedColor){
                root.colorReady(seletedColor)
                item2.textField.text = String(seletedColor)
            }
        }
    }
}
