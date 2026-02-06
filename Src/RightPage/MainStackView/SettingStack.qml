import QtQuick 2.15
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import "../../Basic"

Item {
    id:root_parent
    property string pageName: ""
    Item{
        id:root
        anchors.fill: parent
        anchors.left: parent.left
        anchors.leftMargin: 40
        anchors.top: parent.top
        clip: true
        //顶部的"设置"Text
        Text {
            id:topSettingText
            anchors.left: parent.left
            anchors.top: parent.top
            color:"#2a1a22"
            font.pixelSize: 24
            font.family: "微软雅黑"
            font.bold: true
            text: qsTr("设置")
        }
        //顶部设置小标题列表数据模型
        ListModel{
            id:settingTopListModel
            ListElement{titleName:"账号"}
            ListElement{titleName:"常规"}
            ListElement{titleName:"系统"}
            ListElement{titleName:"播放"}
            ListElement{titleName:"消息与隐私"}
            ListElement{titleName:"快捷键"}
            ListElement{titleName:"音质与下载"}
            ListElement{titleName:"桌面与歌词"}
            ListElement{titleName:"工具"}
            ListElement{titleName:"关于Oran柒云音乐"}
        }

        //settingTopTitleFlow
        Flow{
            id:settingTopTitleFlow
            anchors.left:parent.left
            anchors.top:topSettingText.bottom
            anchors.topMargin: 20
            spacing: 14
            Repeater{
                id:settingTopTitleFlowRepeater
                anchors.fill: parent
                model:settingTopListModel
                delegate: Label{
                    id:settingTopTitleFlowRepeaterElementLabel
                    text:titleName
                    font.family:"微软雅黑 Light"
                    font.bold: true
                    font.pixelSize: 18
                    color:index === 0 ? "#FF7381" : "#2a1a22"
                    Connections{
                        target: BasicConfig
                        function onClearElementSubTitleMaxiHightLight_InSettingStack()
                        {
                            if(settingTopTitleFlowRepeaterElementLabel.isFocus === true)
                            {
                                settingTopTitleFlowRepeaterElementLabel.color = "#2a1a22"
                                settingTopTitleFlowRepeaterElementLabel.isFocus = false
                                underlineElementRectangle.color = "transparent"
                            }
                        }
                    }

                    property bool isFocus: index === 0 ? true : false
                    Rectangle{
                        id:underlineElementRectangle
                        width: parent.implicitWidth-4
                        height: 3
                        radius: 1
                        anchors.top: parent.bottom
                        anchors.topMargin: 3
                        anchors.horizontalCenter: parent.horizontalCenter
                        color:index === 0 ? "#ff3a3a" : "transparent"
                    }
                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            if(settingTopTitleFlowRepeaterElementLabel.isFocus === false)
                                settingTopTitleFlowRepeaterElementLabel.color = "#616161"
                            cursorShape = Qt.PointingHandCursor
                        }
                        onExited: {
                            if(settingTopTitleFlowRepeaterElementLabel.isFocus === false)
                                settingTopTitleFlowRepeaterElementLabel.color = "#2a1a22"
                            cursorShape = Qt.ArrowCursor
                        }
                        onClicked: {
                            if(settingTopTitleFlowRepeaterElementLabel.isFocus === false)
                            {
                                BasicConfig.clearElementSubTitleMaxiHightLight_InSettingStack()
                                settingTopTitleFlowRepeaterElementLabel.isFocus = true
                                settingTopTitleFlowRepeaterElementLabel.color = "#FF7381"
                                underlineElementRectangle.color = "#ff3a3a"
                            }
                        }
                    }
                }
            }
        }

        //分割线1
        Rectangle{
            id:diveRectangle1
            height: 1
            width: parent.width-20
            color: "#47474c"
            anchors.top: settingTopTitleFlow.bottom
            anchors.topMargin: 18
            anchors.left: settingTopTitleFlow.left
        }
        //setting小标题行正下方的大型可滑动区域
        Rectangle{
            id:detialFlickableRectangle
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top:diveRectangle1.bottom
            color:"transparent"
            clip:true
            Flickable{
                id:detialFlickable
                anchors.fill:detialFlickableRectangle
                contentHeight: 800
                clip: true
                ScrollBar.vertical: ScrollBar{
                    anchors.right: parent.right
                    anchors.rightMargin: 0
                    width: 10
                }

                //账号
                Rectangle{
                    id:accountSettingRectangle
                    height: 180
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.topMargin: 30
                    anchors.right: parent.right
                    color:"transparent"
                    Label{
                        id:accountTextLabel
                        text: "账号"
                        font.family:"微软雅黑"
                        font.bold: true
                        font.pixelSize: 18
                        color: "#2a1a22"
                        anchors.left: parent.left
                        anchors.top: parent.top
                    }
                    //三种登录方式行布局
                    Row{
                        id:loginWaysRow
                        width: 200
                        height:44
                        anchors.left:accountTextLabel.right
                        anchors.leftMargin: 90
                        spacing: 25
                        Repeater{
                            id:loginWaysRectangleRepeater
                            model:["qrc:/image/wechat.png",
                                        "qrc:/image/qq.png",
                                        "qrc:/image/wb.png"]
                            delegate:Rectangle{
                                id:wayRectangle
                                height: loginWaysRow.height
                                width: height
                                radius: height/2
                                anchors.verticalCenter: loginWaysRow.verticalCenter
                                border.width: 1
                                border.color: "#5c5c61"
                                color: "#fef2e8"
                                Image {
                                    id:wayImage
                                    anchors.fill: parent
                                    scale:0.7
                                    source: modelData
                                    anchors.centerIn: parent
                                    layer.enabled: false
                                    layer.effect:ColorOverlay{
                                        source: wayImage
                                        anchors.fill: wayImage
                                        color: index === 0 ? "#4d9c3c" :
                                                    (index=== 1 ? "#328dc4":
                                                    (index === 2 ? "#af172b" :""))
                                    }
                                }

                                MouseArea{
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onEntered: {
                                        wayImage.layer.enabled = true
                                        wayRectangle.border.color = index === 0 ? "#427e36" :
                                                                                  (index=== 1 ? "#2d739f":
                                                                                  (index === 2 ? "#af172b" :""))
                                        wayRectangle.color = index === 0 ? "#4FDC3E" :
                                                                           (index=== 1 ? "#44C3F3":
                                                                           (index === 2 ? "#E82B2B" :""))
                                        cursorShape = Qt.PointingHandCursor
                                    }
                                    onExited: {
                                        wayImage.layer.enabled = false
                                        wayRectangle.border.color = "#5c5c61"
                                        wayRectangle.color = "#fef2e8"
                                        cursorShape = Qt.ArrowCursor
                                    }
                                    onClicked: {
                                        //---
                                    }
                                }
                            }
                        }
                    }
                    //”绑定账号>“Label
                    Label{
                        id:linkAccountTextLabel
                        text:"绑定账号 >"
                        anchors.left: loginWaysRow.right
                        anchors.leftMargin: 10
                        anchors.verticalCenter: loginWaysRow.verticalCenter
                        font.family: "微软雅黑"
                        font.pixelSize: 15
                        color: "#2a1a22"
                        MouseArea{
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: {
                                linkAccountTextLabel.color = "white"
                                cursorShape = Qt.PointingHandCursor
                            }
                            onExited: {
                                linkAccountTextLabel.color = "#2a1a22"
                                cursorShape = Qt.ArrowCursor
                            }
                            onClicked: {
                                //---
                            }
                        }
                    }
                    //”修改个人信息“TextRectangle
                    Rectangle{
                        id:changePersonInfoRectangle
                        anchors.left: loginWaysRow.left
                        anchors.top: loginWaysRow.bottom
                        anchors.topMargin: 20
                        width: 110
                        height: 26
                        radius: height/2
                        border.width: 1
                        border.color: "#89898d"
                        color: "transparent"
                        Label{
                            text: "修改个人信息"
                            font.family: "微软雅黑"
                            font.pixelSize: 14
                            color: "#2a1a22"
                            anchors.centerIn: parent
                        }
                        MouseArea{
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: {
                                changePersonInfoRectangle.color = "#fef2e8"
                                cursorShape = Qt.PointingHandCursor
                            }
                            onExited:{
                                changePersonInfoRectangle.color = "transparent"
                                cursorShape = Qt.ArrowCursor
                            }
                            onClicked: {
                                //---
                            }
                        }
                    }
                    //快速登录Label
                    Label{
                        id:fastLoadTextLabel
                        text:"快速登录"
                        font.family: "微软雅黑 Light"
                        font.bold: true
                        font.pixelSize: 14
                        color: "#2a1a22"
                        anchors.left:loginWaysRow.left
                        anchors.top: changePersonInfoRectangle.bottom
                        anchors.topMargin: 28
                    }
                    //开启快速登录CheckBox
                    Rectangle{
                        id:fastloginRectangle
                        width: 18
                        height: width
                        radius: 4
                        color: "transparent"
                        border.color: "#76767c"
                        border.width: 1
                        anchors.left: fastLoadTextLabel.left
                        anchors.top: fastLoadTextLabel.bottom
                        anchors.topMargin: 10
                        clip: true
                        Image {
                            id: sureLoginImage
                            source: "qrc:/image/sure.png"
                            anchors.fill: parent
                            scale: 1.3
                            opacity: 0
                        }
                        MouseArea{
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: {
                                if(sureLoginImage.opacity === 0)
                                    fastloginRectangle.border.color = "white"
                            }
                            onExited: {
                                if(sureLoginImage.opacity === 0)
                                    fastloginRectangle.border.color = "#76767c"
                            }
                            onClicked: {
                                if(sureLoginImage.opacity === 0)
                                {
                                    sureLoginImage.opacity = 1
                                    fastloginRectangle.border.color = "#ff3a3a"
                                    fastloginRectangle.color = "#ff3a3a"
                                }
                                else
                                {
                                    sureLoginImage.opacity = 0
                                    fastloginRectangle.border.color = "white"
                                    fastloginRectangle.color = "transparent"
                                }
                            }
                        }
                    }
                    //开启快速登录TextLabel
                    Label{
                        text:"开启快速登录"
                        font.family: "微软雅黑 Light"
                        font.bold: true
                        font.pixelSize: 14
                        color: "#2a1a22"
                        anchors.left:fastloginRectangle.right
                        anchors.leftMargin: 6
                        anchors.verticalCenter: fastloginRectangle.verticalCenter
                    }
                }
                //分割线2
                Rectangle{
                    id:diveRectangle2
                    height: 1
                    width: parent.width-20
                    color: "#47474c"
                    anchors.top: accountSettingRectangle.bottom
                    anchors.topMargin: 18
                    anchors.left: accountSettingRectangle.left
                }
                //常规
                Rectangle{
                    id:normalSettingRectangle
                    height: 180
                    anchors.left: parent.left
                    anchors.top: diveRectangle2.bottom
                    anchors.topMargin: 30
                    anchors.right: parent.right
                    color:"transparent"
                    Label{
                        id:normalTextLabel
                        text: "常规"
                        font.family:"微软雅黑"
                        font.bold: true
                        font.pixelSize: 18
                        color: "#2a1a22"
                        anchors.left: parent.left
                        anchors.top: parent.top
                    }
                    //字体选择TextLabel
                    Label{
                        id:choseFontFamilyTextLabel
                        anchors.left: normalTextLabel.right
                        anchors.verticalCenter: normalTextLabel.verticalCenter
                        anchors.leftMargin: 90
                        text: "字体选择"
                        font.family: "微软雅黑"
                        font.pixelSize: 15
                        color: "#2a1a22"
                    }
                    Label{
                        anchors.left: choseFontFamilyTextLabel.right
                        anchors.verticalCenter: choseFontFamilyTextLabel.verticalCenter
                        text: "(更换字体要修改全局变量容器，由于时间关系这里字体暂时只能选择默认，尚未开发完全)"
                        font.family: "微软雅黑"
                        font.pixelSize: 13
                        color: "#89898d"
                    }
                    ListModel{
                        id:fontSelectorComboBoxListModel
                        ListElement{fontName:"默认"}
                        ListElement{fontName:"微软雅黑"}
                        ListElement{fontName:"黑体"}
                        ListElement{fontName:"华文宋体"}
                        ListElement{fontName:"..."}
                        ListElement{fontName:"..."}
                        ListElement{fontName:"..."}
                        ListElement{fontName:"..."}
                        ListElement{fontName:"..."}
                        ListElement{fontName:"..."}
                        ListElement{fontName:"..."}
                        ListElement{fontName:"..."}
                        ListElement{fontName:"..."}
                    }
                    //字体选择的ComboBox
                    ComboBox {
                        id: fontSelectorComboBox
                        width: 120
                        height: 30
                        anchors.top: choseFontFamilyTextLabel.bottom
                        anchors.topMargin: 10
                        anchors.left: choseFontFamilyTextLabel.left
                        model: fontSelectorComboBoxListModel
                        background: Rectangle {
                            id: choseFontFamilyRectangle
                            anchors.fill: parent
                            radius: height / 2
                            border.color: "#4f4f54"
                            border.width: 1
                            color: "#fef2e8"
                        }
                        indicator: Label {
                            id: choseStateTextLabel
                            text: "∨"
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            font.family: "微软雅黑"
                            font.pixelSize: 12
                            color: "#2a1a22"
                        }
                        contentItem: Text {
                            id:contentItemText
                            text: "默认"
                            font.pixelSize: 14
                            font.family: "微软雅黑"
                            color: "#2a1a22"
                            width: parent.width - 20
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                        }
                        popup: Popup {
                            id: selectFontPopup
                            y: fontSelectorComboBox.height
                            width: fontSelectorComboBox.width
                            height: 280
                            parent: fontSelectorComboBox
                            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                            background: Rectangle {
                                anchors.fill: parent
                                radius: 10
                                color: "#2d2d38"
                                border.color: "#42424c"
                                border.width: 1
                            }
                            ListView {
                                anchors.fill: parent
                                model: fontSelectorComboBoxListModel
                                delegate: ItemDelegate {
                                    background: Rectangle{
                                        anchors.fill: parent
                                        color:"transparent"
                                        MouseArea{
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onEntered: {
                                                parent.color="#393943"
                                            }
                                            onExited: {
                                                parent.color="transparent"
                                            }
                                        }
                                    }
                                    width: parent.width
                                    contentItem: Text {
                                            text: modelData
                                            color: "white"
                                            font.pixelSize: 14
                                            font.family: "微软雅黑"
                                        }
                                    onClicked: {
                                        fontSelectorComboBox.currentIndex = index
                                        contentItemText.text=modelData
                                        selectFontPopup.close()
                                    }
                                }
                            }
                        }
                    }

                }
            }
        }

    }
}
