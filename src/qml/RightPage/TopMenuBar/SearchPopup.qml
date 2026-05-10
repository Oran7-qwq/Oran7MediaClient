import QtQuick 2.15
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

Popup{
    id:searchPopup
    // contentItem: Text { text: "This is a popup" }
    closePolicy: Popup.CloseOnPressOutside
    background: Rectangle{
        anchors.fill: parent
        radius: 12
        color:"#fef2e8"
    }
    clip: true
    //滚动条

    Flickable{
        id:searchPopupFlickable
        anchors.fill: parent
        contentHeight: 1180
        interactive: true // 保持可交互

        ScrollBar.vertical: ScrollBar{
            anchors.right: parent.right
            anchors.rightMargin: 0
            width: 10
            // contentItem: Rectangle{
            //     visible: parent.active
            // }
        }

        //搜索历史文本
        Item{
            id:searchHistoryItem
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top:parent.top
            Text{
                id:searchHistoryText
                text:"搜索历史"
                font.pixelSize: 16
                font.family: "微软雅黑 Light"
                font.bold: true
                color:"#adadb1"
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.top: parent.top
            }
            Rectangle{
                id:deleteImageRectangle
                width: 28
                height: width
                color:"transparent"
                anchors.top: searchHistoryText.top
                anchors.topMargin: -4
                anchors.left: searchHistoryText.right
                anchors.leftMargin: 4
                Image{
                    id:deleteImage
                    source: "/image/delete.png"
                    anchors.fill:parent
                    layer.enabled: false
                    layer.effect: ColorOverlay
                    {
                        source:deleteImage
                        color:"white"
                    }
                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled:true
                        onEntered: {
                            deleteImage.layer.enabled = true
                            cursorShape = Qt.PointingHandCursor
                        }
                        onExited: {
                            deleteImage.layer.enabled = false
                            cursorShape = Qt.ArrowCursor
                        }
                        onClicked: {
                            searchHistoryRepeaterModel.clear()
                        }
                    }
                }
            }
        }

        //搜索历史记录数据模型
        ListModel{
            id:searchHistoryRepeaterModel
            ListElement{singName:"Shelter"}
            ListElement{singName:"kawAIi (feat. Srezcat)"}
            ListElement{singName:"Effervescence"}
            ListElement{singName:"赴大荒"}
            ListElement{singName:"锦绣山河"}
            ListElement{singName:"Play"}
            ListElement{singName:"One Last Kiss"}
            ListElement{singName:"To the South"}
            ListElement{singName:"Kirara Magic"}
            ListElement{singName:"Beyond the way"}
            ListElement{singName:"祥风时雨"}
            ListElement{singName:"Alone"}
            ListElement{singName:"十年DJ"}
            ListElement{singName:"∧"}
        }
        //搜索历史记录网格，流布局
        Flow{
            id:searchHistoryFlow
            anchors.left: searchHistoryItem.left
            anchors.leftMargin: 10
            anchors.top: searchHistoryItem.bottom
            anchors.topMargin: 30
            anchors.right:searchHistoryItem.right
            spacing: 10
            Repeater{
                id:searchHistoryRepeater
                anchors.fill: parent
                model:searchHistoryRepeaterModel
                property bool showAll: false
                delegate: Rectangle{
                    width: searchHistoryDataLabel.implicitWidth + 20
                    height:30
                    border.width: 1
                    border.color: "#c36c7c"
                    color:"#c36c7c"
                    radius: 16
                    visible: searchHistoryRepeater.showAll ? true : index<=7
                    Label{
                        id:searchHistoryDataLabel
                        text: undefined === singName ? "" : (searchHistoryRepeater.showAll ? singName : (index===7 ? "∨" : singName))
                        verticalAlignment:Text.AlignVCenter
                        horizontalAlignment:Text.AlignHCenter
                        font.pixelSize: 14
                        anchors.centerIn: parent
                        color: "#f8c7c7"
                        font.family: "微软雅黑 Light"
                        font.bold: true
                        height: 25
                    }
                    BrightnessContrast{
                        id:searchHistoryDataLabelBrightness
                        source: searchHistoryDataLabel
                        anchors.fill: searchHistoryDataLabel
                        brightness:0.0
                        contrast: 0.0
                    }

                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            searchHistoryDataLabelBrightness.brightness = 1.0
                            parent.color = "#808087"
                            cursorShape = Qt.PointingHandCursor
                        }
                        onExited: {
                            searchHistoryDataLabelBrightness.brightness = 0.0
                            parent.color = "#c36c7c"
                            cursorShape = Qt.ArrowCursor
                        }
                        onClicked: {
                            if(searchHistoryRepeater.showAll === false && index === 7)
                            {
                                searchHistoryRepeater.showAll = true
                            }
                            else if(index === searchHistoryRepeaterModel.count-1)
                                searchHistoryRepeater.showAll = false
                        }
                    }
                }
            }
        }

        //猜你喜欢标签
        Label{
            id:gussAppreciteLable
            anchors.top: searchHistoryFlow.bottom
            anchors.topMargin: 10
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: parent.right
            text:"猜你喜欢"
            font.bold: true
            font.pixelSize: 16
            font.family: "微软雅黑 Light"
            color:"#adadb1"
        }

        //猜你喜欢数据模型
        ListModel{
            id:gussAppreciteRepeaterModel
            ListElement{singName:"Shelter"}
            ListElement{singName:"kawAIi (feat. Srezcat)"}
            ListElement{singName:"Effervescence"}
            ListElement{singName:"赴大荒"}
            ListElement{singName:"锦绣山河"}
            ListElement{singName:"Play"}
            ListElement{singName:"One Last Kiss"}
            ListElement{singName:"To the South"}
            ListElement{singName:"Kirara Magic"}
            ListElement{singName:"Beyond the way"}
            ListElement{singName:"祥风时雨"}
            ListElement{singName:"Alone"}
            ListElement{singName:"十年DJ"}
            ListElement{singName:"∧"}
        }
        //猜你喜欢网格，流布局
        Flow{
            id:gussAppreciteFlow
            anchors.left: gussAppreciteLable.left
            anchors.leftMargin: 0
            anchors.top: gussAppreciteLable.bottom
            anchors.topMargin: 10
            anchors.right:gussAppreciteLable.right
            spacing: 10
            Repeater{
                id:gussAppreciteRepeater
                anchors.fill: parent
                model:gussAppreciteRepeaterModel
                property bool showAll: true
                delegate: Rectangle{
                    width: gussAppreciteDataLabel.implicitWidth + 20
                    height:30
                    border.width: 1
                    border.color: "#c36c7c"
                    color:"#c36c7c"
                    radius: 16
                    visible: gussAppreciteRepeater.showAll ? true : index<=7
                    Label{
                        id:gussAppreciteDataLabel
                        text:gussAppreciteRepeater.showAll ? singName : (index===7 ? "∨" : singName)
                        verticalAlignment:Text.AlignVCenter
                        horizontalAlignment:Text.AlignHCenter
                        font.pixelSize: 14
                        anchors.centerIn: parent
                        color: "#adadb1"
                        font.family: "微软雅黑 Light"
                        font.bold: true
                        height: 25
                    }
                    BrightnessContrast{
                        id:gussAppreciteDataLabelBrightness
                        source: gussAppreciteDataLabel
                        anchors.fill: gussAppreciteDataLabel
                        brightness:0.0
                        contrast: 0.0
                    }

                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            gussAppreciteDataLabelBrightness.brightness = 1.0
                            parent.color = "#808087"
                            cursorShape = Qt.PointingHandCursor
                        }
                        onExited: {
                            gussAppreciteDataLabelBrightness.brightness = 0.0
                            parent.color = "#c36c7c"
                            cursorShape = Qt.ArrowCursor
                        }
                        onClicked: {
                            if(gussAppreciteRepeater.showAll === false && index === 7)
                            {
                                gussAppreciteRepeater.showAll = true
                            }
                            else if(index === gussAppreciteRepeaterModel.count-1)
                                gussAppreciteRepeater.showAll = false
                        }
                    }
                }
            }
        }

        //热搜榜列表数据模型
        ListModel{
            id:hotSearchListModel
            ListElement{number:"1";singName:"唯一";imageSource:""}
            ListElement{number:"2";singName:"你";imageSource:""}
            ListElement{number:"3";singName:"死疙瘩";imageSource:""}
            ListElement{number:"4";singName:"山楂树之恋";imageSource:""}
            ListElement{number:"5";singName:"有些";imageSource:""}
            ListElement{number:"6";singName:"我想要你的爱";imageSource:""}
            ListElement{number:"7";singName:"天赐的声音";imageSource:""}
            ListElement{number:"8";singName:"烟圈";imageSource:""}
            ListElement{number:"9";singName:"分离焦虑";imageSource:""}
        }
        //热搜榜hotSearchListRectangle
        Rectangle
        {
            id:hotSearchListRectangle
            width: 740
            height:180
            radius: 10
            anchors.top: gussAppreciteFlow.bottom
            anchors.topMargin: 10
            anchors.left: parent.left
            anchors.leftMargin: 10
            color:"#c36c7c"
            //热搜榜标签hotSearchListLabel
            Label{
                id:hotSearchListLabel
                text:"热搜榜"
                font.pixelSize: 18
                font.family: "黑体"
                font.bold: true
                color:"#adadb1"
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.top: parent.top
                anchors.topMargin: 10
            }
            //热搜榜快捷播放按钮
            Button{
                id:hotSearchListPlayButton
                height:25
                width: 60
                anchors.top: parent.top
                anchors.topMargin: 6
                anchors.left: hotSearchListLabel.right
                anchors.leftMargin: 10
                // icon.source: "qrc:/image/playBtn.png"
                // icon.width: 20
                // icon.height: 20
                // display: AbstractButton.TextBesideIcon
               Image {
                   id:hotSearchListPlayImage
                   anchors.left: parent.left
                   anchors.leftMargin: 4
                   anchors.verticalCenter: parent.verticalCenter
                   source: "qrc:/image/playBtn.png"
                   width: 20
                   height: 20
                   fillMode: Image.PreserveAspectFit
               }
               Label {
                   anchors.verticalCenter: parent.verticalCenter
                   anchors.left:hotSearchListPlayImage.right
                   text: "播放"
                   font.bold: true
                   font.family: "黑体"
                   font.pixelSize: 14
                   color: "#ececed"
               }
                background: Rectangle
                {
                    id:hotSearchListPlayButtonBackground
                    anchors.fill: parent
                    radius:16
                    border.color: "#c36c7c"
                    border.width: 1
                    // color: hotSearchListPlayButton.down ? "#34343e" : "#3f3f48"
                    color:"#c36c7c"
                }
                BrightnessContrast{
                    id:hotSearchListPlayButtonBrightness
                    source: hotSearchListPlayButton
                    anchors.fill: hotSearchListPlayButton
                    brightness: 0.0
                    contrast: 0.0
                }
                MouseArea{
                    id:hotSearchListPlayButtonMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true // 允许事件继续传递
                    onEntered: {
                        hotSearchListPlayButtonBrightness.brightness = 0.2
                        cursorShape = Qt.PointingHandCursor
                    }
                    onExited: {
                        hotSearchListPlayButtonBrightness.brightness = 0.0
                        cursorShape = Qt.ArrowCursor
                    }
                    onPressed: {
                        searchPopupFlickable.interactive = false
                        hotSearchListPlayButtonBrightness.brightness = 0.0
                        hotSearchListPlayButtonBackground.color = "#34343e"
                    }
                    onReleased: (mouse)=>{
                        if(hotSearchListPlayButtonMouseArea.containsMouse===true)
                            hotSearchListPlayButtonBrightness.brightness = 0.2
                        else hotSearchListPlayButtonBrightness.brightness = 0.0

                        hotSearchListPlayButtonBackground.color = "#3f3f48"
                        searchPopupFlickable.interactive = true
                    }
                }
            }
            //热搜榜列表网格布局
            GridView{
                anchors.top: hotSearchListLabel.bottom
                anchors.topMargin: 16
                anchors.left: hotSearchListLabel.left
                anchors.leftMargin: 8
                model:hotSearchListModel
                width:690
                height:120
                cellWidth:230
                cellHeight: 40
                layoutDirection:Qt.LeftToRight
                verticalLayoutDirection:GridView.TopToBottom
                flow:GridView.FlowTopToBottom
                delegate: Rectangle{
                    width: 230
                    height: 40
                    color: "transparent"
                    visible: true
                    Text{
                        id:numberText
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text:number
                        color: number<=3 ? "#fc3b5b" : "#d6d6d8"
                        font.pixelSize: 14
                        font.family: "微软雅黑"
                    }
                    Text{
                        anchors.left: numberText.right
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text:singName
                        color: "cyan"
                        font.bold: number<=3 ? true : false
                        font.pixelSize: 14
                        font.family: "微软雅黑"
                    }
                    Image {
                        anchors.left: numberText.left
                        anchors.verticalCenter: parent.verticalCenter
                        source:imageSource
                    }
                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            cursorShape  = Qt.PointingHandCursor
                        }
                        onExited: {
                            cursorShape = Qt.ArrowCursor
                        }
                    }
                }
            }
        }

        //说唱榜列表数据模型
        ListModel{
            id:rapListModel
            ListElement{number:"1";singName:"从前以后PT.1";imageSource:""}
            ListElement{number:"2";singName:"来电瑶";imageSource:""}
            ListElement{number:"3";singName:"Worldwide";imageSource:""}
            ListElement{number:"4";singName:"卑鄙LilBaby";imageSource:""}
            ListElement{number:"5";singName:"染缸";imageSource:""}
            ListElement{number:"6";singName:"NO HOOK FREESTYLE Pt.4";imageSource:""}
            ListElement{number:"7";singName:"初春";imageSource:""}
            ListElement{number:"8";singName:"20 Minutes Freestyle";imageSource:""}
            ListElement{number:"9";singName:"从前以后PT.2";imageSource:""}
        }
        //说唱榜rapListRectangle
        Rectangle
        {
            id:rapListRectangle
            width: 740
            height:180
            radius: 10
            anchors.top: hotSearchListRectangle.bottom
            anchors.topMargin: 10
            anchors.left: parent.left
            anchors.leftMargin: 10
            color:"#c36c7c"
            //说唱榜标签rapListLabel
            Label{
                id:rapListLabel
                text:"说唱榜"
                font.pixelSize: 18
                font.family: "黑体"
                font.bold: true
                color:"#adadb1"
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.top: parent.top
                anchors.topMargin: 10
            }
            //说唱榜快捷播放按钮
            Button{
                id:rapListPlayButton
                height:25
                width: 60
                anchors.top: parent.top
                anchors.topMargin: 6
                anchors.left: rapListLabel.right
                anchors.leftMargin: 10
               Image {
                   id:rapListPlayImage
                   anchors.left: parent.left
                   anchors.leftMargin: 4
                   anchors.verticalCenter: parent.verticalCenter
                   source: "qrc:/image/playBtn.png"
                   width: 20
                   height: 20
                   fillMode: Image.PreserveAspectFit
               }
               Label {
                   anchors.verticalCenter: parent.verticalCenter
                   anchors.left:rapListPlayImage.right
                   text: "播放"
                   font.bold: true
                   font.family: "黑体"
                   font.pixelSize: 14
                   color: "#ececed"
               }
                background: Rectangle
                {
                    id:rapListPlayButtonBackground
                    anchors.fill: parent
                    radius:16
                    border.color: "#c36c7c"
                    border.width: 1
                    color:rapListPlayButton.down ? "#3f3f48" : "#c36c7c"
                }
                BrightnessContrast{
                    id:rapListPlayButtonBrightness
                    source: rapListPlayButton
                    anchors.fill: rapListPlayButton
                    brightness: 0.0
                    contrast: 0.0
                }
                MouseArea{
                    id:rapListPlayButtonMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true // 允许事件继续传递
                    onEntered: {
                        rapListPlayButtonBrightness.brightness = 0.2
                        cursorShape = Qt.PointingHandCursor
                    }
                    onExited: {
                        rapListPlayButtonBrightness.brightness = 0.0
                        cursorShape = Qt.ArrowCursor
                    }
                    onPressed: {
                        searchPopupFlickable.interactive = false
                        rapListPlayButtonBrightness.brightness = 0.0
                        rapListPlayButtonBackground.color = "#34343e"
                    }
                    onReleased: (mouse)=>{
                        if(rapListPlayButtonMouseArea.containsMouse===true)
                            rapListPlayButtonBrightness.brightness = 0.2
                        else rapListPlayButtonBrightness.brightness = 0.0

                        rapListPlayButtonBackground.color = "#3f3f48"
                        searchPopupFlickable.interactive = true
                    }
                }
            }
            //说唱榜列表网格布局
            GridView{
                anchors.top: rapListLabel.bottom
                anchors.topMargin: 16
                anchors.left: rapListLabel.left
                anchors.leftMargin: 8
                model:rapListModel
                width:690
                height:120
                cellWidth:230
                cellHeight: 40
                layoutDirection:Qt.LeftToRight
                verticalLayoutDirection:GridView.TopToBottom
                flow:GridView.FlowTopToBottom
                delegate: Rectangle{
                    width: 230
                    height: 40
                    color: "transparent"
                    visible: true
                    Text{
                        id:rapListNumberText
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text:number
                        color: number<=3 ? "#fc3b5b" : "#d6d6d8"
                        font.pixelSize: 14
                        font.family: "微软雅黑"
                    }
                    Text{
                        anchors.left: rapListNumberText.right
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text:singName
                        color: "cyan"
                        font.bold: number<=3 ? true : false
                        font.pixelSize: 14
                        font.family: "微软雅黑"
                    }
                    Image {
                        anchors.left: rapListNumberText.left
                        anchors.verticalCenter: parent.verticalCenter
                        source:imageSource
                    }
                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            cursorShape  = Qt.PointingHandCursor
                        }
                        onExited: {
                            cursorShape = Qt.ArrowCursor
                        }
                    }
                }
            }
        }

        //民谣榜列表数据模型
        ListModel{
            id:folkSongChartModel
            ListElement{number:"1";singName:"远去的列车";imageSource:""}
            ListElement{number:"2";singName:"晚风";imageSource:""}
            ListElement{number:"3";singName:"爱在公园前";imageSource:""}
            ListElement{number:"4";singName:"公子不才";imageSource:""}
            ListElement{number:"5";singName:"我记得(Live版)";imageSource:""}
            ListElement{number:"6";singName:"关于理想的作文";imageSource:""}
            ListElement{number:"7";singName:"我走过一万年才和你遇见";imageSource:""}
            ListElement{number:"8";singName:"孤鸟";imageSource:""}
            ListElement{number:"9";singName:"小形迹(Live版)";imageSource:""}
        }
        //民谣榜folkSongChartRectangle
        Rectangle
        {
            id:folkSongChartRectangle
            width: 740
            height:180
            radius: 10
            anchors.top: rapListRectangle.bottom
            anchors.topMargin: 10
            anchors.left: parent.left
            anchors.leftMargin: 10
            color:"#c36c7c"
            //民谣榜标签folkSongChartLabel
            Label{
                id:folkSongChartLabel
                text:"民谣榜"
                font.pixelSize: 18
                font.family: "黑体"
                font.bold: true
                color:"#adadb1"
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.top: parent.top
                anchors.topMargin: 10
            }
            //民谣榜快捷播放按钮
            Button{
                id:folkSongChartPlayButton
                height:25
                width: 60
                anchors.top: parent.top
                anchors.topMargin: 6
                anchors.left: folkSongChartLabel.right
                anchors.leftMargin: 10
               Image {
                   id:folkSongChartPlayImage
                   anchors.left: parent.left
                   anchors.leftMargin: 4
                   anchors.verticalCenter: parent.verticalCenter
                   source: "qrc:/image/playBtn.png"
                   width: 20
                   height: 20
                   fillMode: Image.PreserveAspectFit
               }
               Label {
                   anchors.verticalCenter: parent.verticalCenter
                   anchors.left:folkSongChartPlayImage.right
                   text: "播放"
                   font.bold: true
                   font.family: "黑体"
                   font.pixelSize: 14
                   color: "#ececed"
               }
                background: Rectangle
                {
                    id:folkSongChartPlayButtonBackground
                    anchors.fill: parent
                    radius:16
                    border.color: "#c36c7c"
                    border.width: 1
                    color:folkSongChartPlayButton.down ? "#34343e" : "#c36c7c"
                }
                BrightnessContrast{
                    id:folkSongChartPlayButtonBrightness
                    source: folkSongChartPlayButton
                    anchors.fill: folkSongChartPlayButton
                    brightness: 0.0
                    contrast: 0.0
                }
                MouseArea{
                    id:folkSongChartPlayButtonMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true // 允许事件继续传递
                    onEntered: {
                        folkSongChartPlayButtonBrightness.brightness = 0.2
                        cursorShape = Qt.PointingHandCursor
                    }
                    onExited: {
                        folkSongChartPlayButtonBrightness.brightness = 0.0
                        cursorShape = Qt.ArrowCursor
                    }
                    onPressed: {
                        searchPopupFlickable.interactive = false
                        folkSongChartPlayButtonBrightness.brightness = 0.0
                        folkSongChartPlayButtonBackground.color = "#34343e"
                    }
                    onReleased: (mouse)=>{
                        if(folkSongChartPlayButtonMouseArea.containsMouse===true)
                            folkSongChartPlayButtonBrightness.brightness = 0.2
                        else folkSongChartPlayButtonBrightness.brightness = 0.0

                        folkSongChartPlayButtonBackground.color = "#3f3f48"
                        searchPopupFlickable.interactive = true
                    }
                }
            }
            //说唱榜列表网格布局
            GridView{
                anchors.top: folkSongChartLabel.bottom
                anchors.topMargin: 16
                anchors.left: folkSongChartLabel.left
                anchors.leftMargin: 8
                model:folkSongChartModel
                width:690
                height:120
                cellWidth:230
                cellHeight: 40
                layoutDirection:Qt.LeftToRight
                verticalLayoutDirection:GridView.TopToBottom
                flow:GridView.FlowTopToBottom
                delegate: Rectangle{
                    width: 230
                    height: 40
                    color: "transparent"
                    visible: true
                    Text{
                        id:folkSongChartNumberText
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text:number
                        color: number<=3 ? "#fc3b5b" : "#d6d6d8"
                        font.pixelSize: 14
                        font.family: "微软雅黑"
                    }
                    Text{
                        anchors.left: folkSongChartNumberText.right
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text:singName
                        color: "cyan"
                        font.bold: number<=3 ? true : false
                        font.pixelSize: 14
                        font.family: "微软雅黑"
                    }
                    Image {
                        anchors.left: folkSongChartNumberText.left
                        anchors.verticalCenter: parent.verticalCenter
                        source:imageSource
                    }
                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            cursorShape  = Qt.PointingHandCursor
                        }
                        onExited: {
                            cursorShape = Qt.ArrowCursor
                        }
                    }
                }
            }
        }

        //古风榜列表数据模型
        ListModel{
            id:ancientStyleChartModel
            ListElement{number:"1";singName:"知我";imageSource:""}
            ListElement{number:"2";singName:"诀别书";imageSource:""}
            ListElement{number:"3";singName:"牵丝戏";imageSource:""}
            ListElement{number:"4";singName:"一程山路";imageSource:""}
            ListElement{number:"5";singName:"弄舌";imageSource:""}
            ListElement{number:"6";singName:"揽山歌";imageSource:""}
            ListElement{number:"7";singName:"落(花开花落日升日没)";imageSource:""}
            ListElement{number:"8";singName:"西厢寻他";imageSource:""}
            ListElement{number:"9";singName:"武家坡2021";imageSource:""}
        }
        //古风榜ancientStyleChartRectangle
        Rectangle
        {
            id:ancientStyleChartRectangle
            width: 740
            height:180
            radius: 10
            anchors.top: folkSongChartRectangle.bottom
            anchors.topMargin: 10
            anchors.left: parent.left
            anchors.leftMargin: 10
            color:"#c36c7c"
            //古风榜标签ancientStyleChartLabel
            Label{
                id:ancientStyleChartLabel
                text:"古风榜"
                font.pixelSize: 18
                font.family: "黑体"
                font.bold: true
                color:"#adadb1"
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.top: parent.top
                anchors.topMargin: 10
            }
            //古风榜快捷播放按钮
            Button{
                id:ancientStyleChartPlayButton
                height:25
                width: 60
                anchors.top: parent.top
                anchors.topMargin: 6
                anchors.left: ancientStyleChartLabel.right
                anchors.leftMargin: 10
               Image {
                   id:ancientStyleChartPlayImage
                   anchors.left: parent.left
                   anchors.leftMargin: 4
                   anchors.verticalCenter: parent.verticalCenter
                   source: "qrc:/image/playBtn.png"
                   width: 20
                   height: 20
                   fillMode: Image.PreserveAspectFit
               }
               Label {
                   anchors.verticalCenter: parent.verticalCenter
                   anchors.left:ancientStyleChartPlayImage.right
                   text: "播放"
                   font.bold: true
                   font.family: "黑体"
                   font.pixelSize: 14
                   color: "#ececed"
               }
                background: Rectangle
                {
                    id:ancientStyleChartPlayButtonBackground
                    anchors.fill: parent
                    radius:16
                    border.color: "#c36c7c"
                    border.width: 1
                    color:ancientStyleChartPlayButton.down ? "#34343e" : "#c36c7c"
                }
                BrightnessContrast{
                    id:ancientStyleChartPlayButtonBrightness
                    source: ancientStyleChartPlayButton
                    anchors.fill: ancientStyleChartPlayButton
                    brightness: 0.0
                    contrast: 0.0
                }
                MouseArea{
                    id:ancientStyleChartPlayButtonMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true // 允许事件继续传递
                    onEntered: {
                        ancientStyleChartPlayButtonBrightness.brightness = 0.2
                        cursorShape = Qt.PointingHandCursor
                    }
                    onExited: {
                        ancientStyleChartPlayButtonBrightness.brightness = 0.0
                        cursorShape = Qt.ArrowCursor
                    }
                    onPressed: {
                        searchPopupFlickable.interactive = false
                        ancientStyleChartPlayButtonBrightness.brightness = 0.0
                        ancientStyleChartPlayButtonBackground.color = "#34343e"
                    }
                    onReleased: (mouse)=>{
                        if(ancientStyleChartPlayButtonMouseArea.containsMouse===true)
                            ancientStyleChartPlayButtonBrightness.brightness = 0.2
                        else ancientStyleChartPlayButtonBrightness.brightness = 0.0

                        ancientStyleChartPlayButtonBackground.color = "#3f3f48"
                        searchPopupFlickable.interactive = true
                    }
                }
            }
            //说唱榜列表网格布局
            GridView{
                anchors.top: ancientStyleChartLabel.bottom
                anchors.topMargin: 16
                anchors.left: ancientStyleChartLabel.left
                anchors.leftMargin: 8
                model:ancientStyleChartModel
                width:690
                height:120
                cellWidth:230
                cellHeight: 40
                layoutDirection:Qt.LeftToRight
                verticalLayoutDirection:GridView.TopToBottom
                flow:GridView.FlowTopToBottom
                delegate: Rectangle{
                    width: 230
                    height: 40
                    color: "transparent"
                    visible: true
                    Text{
                        id:ancientStyleChartNumberText
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text:number
                        color: number<=3 ? "#fc3b5b" : "#d6d6d8"
                        font.pixelSize: 14
                        font.family: "微软雅黑"
                    }
                    Text{
                        anchors.left: ancientStyleChartNumberText.right
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text:singName
                        color: "cyan"
                        font.bold: number<=3 ? true : false
                        font.pixelSize: 14
                        font.family: "微软雅黑"
                    }
                    Image {
                        anchors.left: ancientStyleChartNumberText.left
                        anchors.verticalCenter: parent.verticalCenter
                        source:imageSource
                    }
                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            cursorShape  = Qt.PointingHandCursor
                        }
                        onExited: {
                            cursorShape = Qt.ArrowCursor
                        }
                    }
                }
            }
        }

        //摇滚榜列表数据模型
        ListModel{
            id:rockChartModel
            ListElement{number:"1";singName:"高级动物";imageSource:""}
            ListElement{number:"2";singName:"Up From The Bottom";imageSource:""}
            ListElement{number:"3";singName:"烂苹果";imageSource:""}
            ListElement{number:"4";singName:"苍蝇";imageSource:""}
            ListElement{number:"5";singName:"我没有成为漫画家";imageSource:""}
            ListElement{number:"6";singName:"今天我们融为一体";imageSource:""}
            ListElement{number:"7";singName:"紧箍咒(Live)";imageSource:""}
            ListElement{number:"8";singName:"極樂金花";imageSource:""}
            ListElement{number:"9";singName:"This Can’t Be Us";imageSource:""}
        }
        //摇滚榜rockChartRectangle
        Rectangle
        {
            id:rockChartRectangle
            width: 740
            height:180
            radius: 10
            anchors.top: ancientStyleChartRectangle.bottom
            anchors.topMargin: 10
            anchors.left: parent.left
            anchors.leftMargin: 10
            color:"#c36c7c"
            //古风榜标签ancientStyleChartLabel
            Label{
                id:rockChartLabel
                text:"摇滚榜"
                font.pixelSize: 18
                font.family: "黑体"
                font.bold: true
                color:"#adadb1"
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.top: parent.top
                anchors.topMargin: 10
            }
            //古风榜快捷播放按钮
            Button{
                id:rockChartPlayButton
                height:25
                width: 60
                anchors.top: parent.top
                anchors.topMargin: 6
                anchors.left: rockChartLabel.right
                anchors.leftMargin: 10
               Image {
                   id:rockChartPlayImage
                   anchors.left: parent.left
                   anchors.leftMargin: 4
                   anchors.verticalCenter: parent.verticalCenter
                   source: "qrc:/image/playBtn.png"
                   width: 20
                   height: 20
                   fillMode: Image.PreserveAspectFit
               }
               Label {
                   anchors.verticalCenter: parent.verticalCenter
                   anchors.left:rockChartPlayImage.right
                   text: "播放"
                   font.bold: true
                   font.family: "黑体"
                   font.pixelSize: 14
                   color: "#ececed"
               }
                background: Rectangle
                {
                    id:rockChartPlayButtonBackground
                    anchors.fill: parent
                    radius:16
                    border.color: "#c36c7c"
                    border.width: 1
                    color:rockChartPlayButton.down ? "#34343e" : "#c36c7c"
                }
                BrightnessContrast{
                    id:rockChartPlayButtonBrightness
                    source: rockChartPlayButton
                    anchors.fill: rockChartPlayButton
                    brightness: 0.0
                    contrast: 0.0
                }
                MouseArea{
                    id:rockChartPlayButtonMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true // 允许事件继续传递
                    onEntered: {
                        rockChartPlayButtonBrightness.brightness = 0.2
                        cursorShape = Qt.PointingHandCursor
                    }
                    onExited: {
                        rockChartPlayButtonBrightness.brightness = 0.0
                        cursorShape = Qt.ArrowCursor
                    }
                    onPressed: {
                        searchPopupFlickable.interactive = false
                        rockChartPlayButtonBrightness.brightness = 0.0
                        rockChartPlayButtonBackground.color = "#34343e"
                    }
                    onReleased: (mouse)=>{
                        if(rockChartPlayButtonMouseArea.containsMouse===true)
                            rockChartPlayButtonBrightness.brightness = 0.2
                        else rockChartPlayButtonBrightness.brightness = 0.0

                        rockChartPlayButtonBackground.color = "#3f3f48"
                        searchPopupFlickable.interactive = true
                    }
                }
            }
            //说唱榜列表网格布局
            GridView{
                anchors.top: rockChartLabel.bottom
                anchors.topMargin: 16
                anchors.left: rockChartLabel.left
                anchors.leftMargin: 8
                model:rockChartModel
                width:690
                height:120
                cellWidth:230
                cellHeight: 40
                layoutDirection:Qt.LeftToRight
                verticalLayoutDirection:GridView.TopToBottom
                flow:GridView.FlowTopToBottom
                delegate: Rectangle{
                    width: 230
                    height: 40
                    color: "transparent"
                    visible: true
                    Text{
                        id:rockChartNumberText
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text:number
                        color: number<=3 ? "#fc3b5b" : "#d6d6d8"
                        font.pixelSize: 14
                        font.family: "微软雅黑"
                    }
                    Text{
                        anchors.left: rockChartNumberText.right
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text:singName
                        color: "cyan"
                        font.bold: number<=3 ? true : false
                        font.pixelSize: 14
                        font.family: "微软雅黑"
                    }
                    Image {
                        anchors.left: rockChartNumberText.left
                        anchors.verticalCenter: parent.verticalCenter
                        source:imageSource
                    }
                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            cursorShape  = Qt.PointingHandCursor
                        }
                        onExited: {
                            cursorShape = Qt.ArrowCursor
                        }
                    }
                }
            }
        }

    }
}
