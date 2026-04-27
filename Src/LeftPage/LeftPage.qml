    import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import QtQuick.Effects
import "../Basic"

Rectangle{
    id:root

    Label{
        id:logoLabel
        text: "Oran柒"
        anchors.left: parent.left
        anchors.leftMargin: 58
        anchors.top: parent.top
        anchors.topMargin: 18
        font.family: "华文琥珀"
        font.bold: true
        font.pixelSize: 22
        color: "#fef2e8"
    }
    Label{
        id:logoLabelname
        anchors.left: logoLabel.left
        anchors.top: logoLabel.bottom
        anchors.topMargin: 1
        text:"MediaClient"
        font.family: "华文琥珀"
        font.bold: true
        font.pixelSize: 22
        color: "#fef2e8"
    }

    //顶部列表数据模型
    ListModel{
        id:topListModel
        ListElement{iconImage:"qrc:/image/suggest.png";iconText:"VideoPlayer??";pageName:"VideoPlayerPage"}
        ListElement{iconImage:"qrc:/image/speSeleted.png";iconText:"ScreenCpature??";pageName:"ScreenCapturePage"}
        ListElement{iconImage:"qrc:/image/bok.png";iconText:"NAME??";pageName:"NAME?1"}
        ListElement{iconImage:"qrc:/image/gz.png";iconText:"NAME??";pageName:"NAME?2"}
    }
    //顶部的列表
    Column{
        id:topColumn
        anchors.top: logoLabel.bottom
        anchors.topMargin: 36
        anchors.left: parent.left
        anchors.leftMargin: 22
        anchors.right: parent.right
        spacing: 3
        Repeater{
            id:topColumnRepeater
            model:topListModel
            delegate: Rectangle{
                id:topColumnRepeaterElementRectangle
                height: 36
                width: 160
                radius: 10
                Connections{
                    target: BasicConfig
                    function onClearElementBackgroundColorInLeftPage()
                    {
                        if(topColumnRepeaterElementRectangle.isFocus === true)
                        {
                            topColumnRepeaterElementRectangle.color = "transparent"
                            topColumnRepeaterElementRectangle.isFocus = false
                        }
                    }

                    function onFocusCurrent_SelectedMenuModel(pageName_)
                    {
                        if(pageName === pageName_)
                        {
                            topColumnRepeaterElementRectangle.isFocus = true
                            topColumnRepeaterElementRectangle.color ="#fc3c4b"
                        }
                    }
                }

                color:index === 0 ? "#fc3c4b" : "transparent"
                property bool isFocus:index === 0 ? true : false
                Image {
                    id: topListImageIcon
                    source: iconImage
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    // layer.enabled: index === 0 ? true : false  //Discard
                    layer.enabled: true
                    layer.effect: ColorOverlay{
                        source: topListImageIcon
                        color: "white"
                    }
                }
                Text{
                    text: iconText
                    font.pixelSize: 14
                    font.family: "微软雅黑"
                    font.bold: true
                    color:"#fef2e8"
                    anchors.left: topListImageIcon.right
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                }
                MouseArea{
                    id:topColumnMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if(topColumnRepeaterElementRectangle.isFocus === false)
                        {
                            BasicConfig.clearElementBackgroundColorInLeftPage()
                            topColumnRepeaterElementRectangle.isFocus = true
                            topColumnRepeaterElementRectangle.color ="#fc3c4b"
                            // topListImageIcon.layer.enabled = true
                            switch(index)
                            {
                            case 0 :BasicConfig.pushVideoPlayerStackInto_RightPageMainStackView()
                                break;
                            case 1:BasicConfig.pushScreenCaptureStackInto_RightPageMainStackView()
                                break;
                            default:
                                break;
                            }
                        }
                    }
                    onEntered: {
                        if(topColumnRepeaterElementRectangle.isFocus === false)
                        {
                            topColumnRepeaterElementRectangle.color ="#FF818E"
                        }
                    }
                    onExited: {
                        if(topColumnRepeaterElementRectangle.isFocus === false)
                        {
                            topColumnRepeaterElementRectangle.color = "transparent"
                        }
                    }
                }
            }
        }
    }
    //分割线
    Rectangle{
        id:diveLine1
        anchors.top: topColumn.bottom
        anchors.left: topColumn.left
        anchors.leftMargin: 4
        anchors.topMargin: 10
        width: 154
        height: 1
        color: "#fef2e8"
    }
    //"我的"
    Label{
        id:myTextLabel
        text: "我的Music"
        anchors.top: diveLine1.bottom
        anchors.topMargin: 14
        anchors.left: topColumn.left
        anchors.leftMargin: 4
        font.pixelSize: 12
        font.family: "微软雅黑"
        color:"#fef2e8"
    }

    //中间列表数据模型
    ListModel{
        id:middleListModel
        ListElement{iconImage:"qrc:/image/loved.png";iconText:"我喜欢的音乐";pageName:"MyFavoriteMusicPage"}
        ListElement{iconImage:"qrc:/image/history.png";iconText:"最近播放";pageName:""}
        ListElement{iconImage:"qrc:/image/donwload.png";iconText:"下载管理";pageName:""}
        ListElement{iconImage:"qrc:/image/localmusic.png";iconText:"本地音乐";pageName:"LocalMusicPage"}
    }
    //中间的列表
    Column{
        id:middleColumn
        anchors.top: myTextLabel.bottom
        anchors.topMargin: 12
        anchors.left: topColumn.left
        anchors.leftMargin: 2
        Repeater{
            id:middleColumnRepeater
            model:middleListModel
            delegate: Rectangle{
                id:middleColumnRepeaterElementRectangle
                height: 36
                width: 160
                radius: 10
                Connections{
                    target: BasicConfig
                    function onClearElementBackgroundColorInLeftPage(){
                        if(middleColumnRepeaterElementRectangle.isFocus === true)
                        {
                            middleColumnRepeaterElementRectangle.color = "transparent"
                            middleColumnRepeaterElementRectangle.isFocus = false
                        }
                    }

                    function onFocusCurrent_SelectedMenuModel(pageName_)
                    {
                        if(pageName === pageName_)
                        {
                            middleColumnRepeaterElementRectangle.isFocus = true
                            middleColumnRepeaterElementRectangle.color ="#fc3c4b"
                        }
                    }
                }
                property bool isFocus: false
                color:"transparent"
                Image {
                    id: middleListImageIcon
                    source: iconImage
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    layer.enabled: true
                    layer.effect: ColorOverlay{
                        source: middleListImageIcon
                        color: "white"
                    }
                }
                Text{
                    text: iconText
                    font.pixelSize: 14
                    font.family: "微软雅黑"
                    font.bold: true
                    color:"#fef2e8"
                    anchors.left: middleListImageIcon.right
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                }
                MouseArea{
                    id:middleColumnMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if(middleColumnRepeaterElementRectangle.isFocus === false)
                        {
                            middleColumnRepeaterElementRectangle.isFocus = true
                            middleColumnRepeaterElementRectangle.color ="#fc3c4b"
                            switch(index)
                            {
                            case 0:BasicConfig.pushMyFavoriteMusicStackInto_RightPageMainStackView()
                                break;
                            case 3:
                                {
                                    BasicConfig.pushLocalMusicStackInto_RightPageMainStackView()
                                    //触检测当前stack是否有存在focused music ，存在的话就触发加载
                                    BasicConfig.focusCurrentMusicInDisplayList()
                                }
                                break;
                            default:
                                break;
                            }
                        }
                    }
                    onEntered: {
                        if(middleColumnRepeaterElementRectangle.isFocus === false)
                        {
                            middleColumnRepeaterElementRectangle.color ="#FF818E"
                        }
                    }
                    onExited: {
                        if(middleColumnRepeaterElementRectangle.isFocus === false)
                        {
                            middleColumnRepeaterElementRectangle.color = "transparent"
                        }
                    }
                }
            }
        }
    }
    //" ∨更多"TextLabel
    Label{
        id:moreLabel
        text: "∨  更多"
        anchors.top: middleColumn.bottom
        anchors.topMargin: 5
        anchors.left: middleColumn.left
        anchors.leftMargin: 2
        font.pixelSize: 13
        font.family: "微软雅黑"
        color:"#fef2e8"
    }
    //分割线
    Rectangle{
        id:diveLine2
        anchors.top: moreLabel.bottom
        anchors.left: topColumn.left
        anchors.leftMargin: 4
        anchors.topMargin: 14
        width: 154
        height: 1
        color: "#fef2e8"
    }
    // //" 创建的歌单  num  ∨"TextLabel
    // property int musicListNum: 0
    // Label{
    //     id:createdListLabel
    //     text: "创建的歌单  "+musicListNum+"  ∨"
    //     anchors.top: diveLine2.bottom
    //     anchors.topMargin: 15
    //     anchors.left: middleColumn.left
    //     anchors.leftMargin: 2
    //     font.pixelSize: 13
    //     font.bold: true
    //     font.family: "微软雅黑 Light"
    //     color:"#fef2e8"
    // }
    // //分割线
    // Rectangle{
    //     id:diveLine3
    //     anchors.top: createdListLabel.bottom
    //     anchors.left: topColumn.left
    //     anchors.leftMargin: 4
    //     anchors.topMargin: 15
    //     width: 154
    //     height: 1
    //     color: "#fef2e8"
    // }
}

