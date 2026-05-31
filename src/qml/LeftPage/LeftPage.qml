import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import QtQuick.Effects
import "../Basic"

import Oran7UI.Impl

Rectangle {
    id: root

    objectName: "__CaptionBar__"

    property real captionIconSize: 32

    property bool simpleMode: false
    readonly property real defaultWidth: 204
    readonly property real simpleModeWidth: 70

    property int itemSelected_index: 0
    property int itemHovered_index: -1
    property bool mouseIsInCaptionBar: false
    property bool enableCentering: root.simpleMode  // 控制导航栏图标是否居中

    Label {
        id: logoLabel
        text: "Oran柒"
        anchors.left: parent.left
        anchors.leftMargin: 58
        anchors.top: parent.top
        anchors.topMargin: 18
        font.family: "华文琥珀"
        font.bold: true
        font.pixelSize: 22
        color: "#fef2e8"

        visible: !root.simpleMode
    }
    Label {
        id: logoLabelname
        anchors.left: logoLabel.left
        anchors.top: logoLabel.bottom
        anchors.topMargin: 1
        text: "MediaClient"
        font.family: "华文琥珀"
        font.bold: true
        font.pixelSize: 22
        color: "#fef2e8"

        visible: !root.simpleMode
    }

    //顶部列表数据模型
    ListModel {
        id: topListModel
        ListElement {
            iconImage: "qrc:/image/hugeicons_ai-video.png"
            iconText: "VideoPlayer??"
            pageName: "VideoPlayerPage"
        }
        ListElement {
            iconImage: "qrc:/image/hugeicons_video-camera-ai.png"
            iconText: "ScreenCpature??"
            pageName: "ScreenCapturePage"
        }
        ListElement {
            iconImage: "qrc:/image/arcticons_star-music-tag-editor.png"
            iconText: "NAME??"
            pageName: "NAME?1"
        }
        ListElement {
            iconImage: "qrc:/image/arcticons_star-music-tag-editor.png"
            iconText: "NAME??"
            pageName: "NAME?2"
        }
    }
    //顶部的列表
    Column {
        id: topColumn
        anchors.top: logoLabel.bottom
        anchors.topMargin: 36
        anchors.horizontalCenter: root.horizontalCenter
        spacing: 4

        HoverHandler{
            id:topCaptionBarHoverHandler
            acceptedDevices: PointerDevice.Mouse
            onHoveredChanged: {
                root.mouseIsInCaptionBar = topCaptionBarHoverHandler.hovered
                if(!topCaptionBarHoverHandler.hovered){
                    root.itemHovered_index = -1
                }
            }
        }
        Repeater {
            id: topColumnRepeater
            model: topListModel
            delegate: Rectangle {
                id: topColumnRepeaterElementRectangle
                height: 42
                width: root.simpleMode ? 42 : 160
                radius: 10
                color: root.itemSelected_index === model.index ? Oran7Theme.Oran7CaptionBar.selectedColor :
                                    root.itemHovered_index === model.index ? Oran7Theme.Oran7CaptionBar.hoveredColor : "transparent"
                Behavior on x { NumberAnimation { duration: Oran7Theme.Primary.durationMid } }
                Behavior on width { NumberAnimation { duration: Oran7Theme.Primary.durationMid } }
                Behavior on color{
                    PropertyAnimation{
                        duration: Oran7Theme.Primary.durationMid
                        easing.type:Easing.OutCubic
                    }
                }

                Connections {
                    target: BasicConfig
                    function onFocusCurrent_SelectedMenuModel(pageName_) {
                        if (model.pageName === pageName_) {
                            root.itemSelected_index = model.index
                        }
                    }
                }

                Image {
                    id: topListImageIcon
                    source: iconImage

                    anchors.verticalCenter: parent.verticalCenter

                    // 动态布局策略 - 用x属性替代anchors，避免尺寸压缩
                    x: root.enableCentering ? parent.width/2 - width/2 : 7

                    // 固定图标尺寸，在simpleMode时适当缩小
                    width: root.simpleMode ? 28 : root.captionIconSize
                    height: root.simpleMode ? 28 : root.captionIconSize
                    sourceSize.width: width
                    sourceSize.height: height

                    // 尺寸和位置变化动画
                    Behavior on width { NumberAnimation { duration: Oran7Theme.Primary.durationMid } }
                    Behavior on height { NumberAnimation { duration: Oran7Theme.Primary.durationMid } }
                    Behavior on x { NumberAnimation { duration: Oran7Theme.Primary.durationMid } }

                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        source: topListImageIcon
                        color: Oran7Theme.Oran7CaptionBar["iconColorBase-6"]
                        Behavior on color{PropertyAnimation{duration:Oran7Theme.Primary.durationMid}}
                    }
                }
                Text {
                    text: iconText
                    font.pixelSize: 14
                    font.family: "微软雅黑"
                    font.bold: true
                    color: Oran7Theme.Oran7CaptionBar["textColorBase-6"]
                    anchors.left: topListImageIcon.right
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter

                    visible: !root.simpleMode
                }
                MouseArea {
                    id: topColumnMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        root.itemSelected_index = model.index
                        switch (index) {
                        case 0:
                            BasicConfig.pushVideoPlayerStackInto_RightPageMainStackView();
                            break;
                        case 1:
                            BasicConfig.pushScreenCaptureStackInto_RightPageMainStackView();
                            break;
                        default:
                            break;
                        }
                    }
                    onEntered: {
                        root.itemHovered_index = model.index
                    }
                }
            }
        }
    }
    //分割线 1
    Rectangle {
        id: diveLine1
        anchors.top: topColumn.bottom
        anchors.left: topColumn.left
        anchors.topMargin: 10
        width: root.simpleMode ? 50 : 154
        height: 1
        color: Oran7Theme.Oran7CaptionBar["textColorBase-6"]

        Behavior on width { NumberAnimation { duration: Oran7Theme.Primary.durationMid } }
    }
    //"我的"
    Label {
        id: myTextLabel
        text: "Music"
        anchors.top: diveLine1.bottom
        anchors.topMargin: 14
        anchors.left: topColumn.left
        anchors.leftMargin: 4
        font.pixelSize: 12
        font.family: "微软雅黑"
        color: Oran7Theme.Oran7CaptionBar["textColorBase-6"]
    }

    //中间列表数据模型
    ListModel {
        id: middleListModel
        ListElement {
            iconImage: "qrc:/image/icon-park-outline_like.png"
            iconText: "我喜欢的音乐"
            pageName: "MyFavoriteMusicPage"
        }
        ListElement {
            iconImage: "qrc:/image/arcticons_star-music-tag-editor.png"
            iconText: "本地音乐"
            pageName: "LocalMusicPage"
        }
    }
    //中间的列表
    Column {
        id: middleColumn
        anchors.top: myTextLabel.bottom
        anchors.topMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 4
        HoverHandler{
            id:middleCaptionBarHoverHandler
            acceptedDevices: PointerDevice.Mouse
            onHoveredChanged: {
                root.mouseIsInCaptionBar = middleCaptionBarHoverHandler.hovered
                if(!middleCaptionBarHoverHandler.hovered){
                    root.itemHovered_index = -1
                }
            }
        }
        Repeater {
            id: middleColumnRepeater
            model: middleListModel
            delegate: Rectangle {
                id: middleColumnRepeaterElementRectangle
                height: 42
                width: root.simpleMode ? 42 : 160
                radius: 10
                color: root.itemSelected_index - topListModel.count === model.index ? Oran7Theme.Oran7CaptionBar.selectedColor :
                                    root.itemHovered_index - topListModel.count === model.index ? Oran7Theme.Oran7CaptionBar.hoveredColor : "transparent"
                Behavior on x { NumberAnimation { duration: 150 } }
                Behavior on width { NumberAnimation { duration: 150 } }
                Behavior on color{
                    PropertyAnimation{
                        duration: Oran7Theme.Primary.durationMid
                        easing.type:Easing.OutCubic
                    }
                }

                Connections {
                    target: BasicConfig
                    function onFocusCurrent_SelectedMenuModel(pageName_) {
                        if (pageName === pageName_) {
                            root.itemSelected_index = model.index + topListModel.count
                        }
                    }
                }

                Image {
                    id: middleListImageIcon
                    source: iconImage

                    anchors.verticalCenter: parent.verticalCenter

                    x: root.enableCentering ? parent.width/2 - width/2 : 7

                    width: root.simpleMode ? 28 : root.captionIconSize
                    height: root.simpleMode ? 28 : root.captionIconSize

                    sourceSize.width: width
                    sourceSize.height: height

                    // 尺寸和位置变化动画
                    Behavior on width { NumberAnimation { duration: Oran7Theme.Primary.durationMid } }
                    Behavior on height { NumberAnimation { duration: Oran7Theme.Primary.durationMid } }
                    Behavior on x { NumberAnimation { duration: Oran7Theme.Primary.durationMid } }

                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        source: middleListImageIcon
                        color: Oran7Theme.Oran7CaptionBar["iconColorBase-6"]
                        Behavior on color{PropertyAnimation{duration:Oran7Theme.Primary.durationMid}}
                    }
                }
                Text {
                    text: iconText
                    font.pixelSize: 14
                    font.family: "微软雅黑"
                    font.bold: true
                    color: Oran7Theme.Oran7CaptionBar["textColorBase-6"]
                    anchors.left: middleListImageIcon.right
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter

                    visible: !root.simpleMode
                }
                MouseArea {
                    id: middleColumnMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        root.itemSelected_index = model.index + topListModel.count
                        switch (index) {
                        case 0:
                            BasicConfig.pushMyFavoriteMusicStackInto_RightPageMainStackView();
                            break;
                        case 1:
                            BasicConfig.pushLocalMusicStackInto_RightPageMainStackView();
                            break;
                        default:
                            break;
                        }
                    }
                    onEntered: {
                        root.itemHovered_index = model.index + topListModel.count
                    }
                }
            }
        }
    }
    //" ∨更多"TextLabel
    Label {
        id: moreLabel
        text: "∨  更多"
        anchors.top: middleColumn.bottom
        anchors.topMargin: 5
        anchors.left: middleColumn.left
        anchors.leftMargin: 2
        font.pixelSize: 13
        font.family: "微软雅黑"
        color: Oran7Theme.Oran7CaptionBar["textColorBase-6"]
    }
    //分割线 2
    Rectangle {
        id: diveLine2
        anchors.top: moreLabel.bottom
        anchors.left: topColumn.left
        anchors.topMargin: 14
        width: root.simpleMode ? 50 : 154
        height: 1
        color: Oran7Theme.Oran7CaptionBar["textColorBase-6"]

        Behavior on width { NumberAnimation { duration: Oran7Theme.Primary.durationMid } }
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

    //Oran7MainUiSetting_OpenImage
    Image {
        id: oran7MainUiSetting_openImage

        x: root.enableCentering ? parent.width/2 - width/2 : 7
        Behavior on x { NumberAnimation { duration: Oran7Theme.Primary.durationMid } }

        anchors.bottom: root.bottom
        anchors.margins: 7
        sourceSize.width: 40
        sourceSize.height: 40
        source: "qrc:/image/hugeicons_ai-setting.png"

        mipmap: true
        antialiasing: true
        asynchronous: false

        layer.enabled: true
        layer.effect: ColorOverlay {
            source: oran7MainUiSetting_openImage
            color: Oran7Theme.Oran7CaptionBar["iconColorBase-7"]
            Behavior on color{PropertyAnimation{duration:Oran7Theme.Primary.durationMid}}
        }
        Behavior on opacity {NumberAnimation{duration:Oran7Theme.Primary.durationMid}}
        Behavior on scale {NumberAnimation{duration: Oran7Theme.Primary.durationVeryFast}}
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                Oran7MainUiSetting.callOpenSettingWindow()
            }
            onEntered: {
                cursorShape = Qt.PointingHandCursor
                oran7MainUiSetting_openImage.scale = 1.05
            }
            onExited: {
                cursorShape = Qt.ArrowCursor
                oran7MainUiSetting_openImage.scale = 1
            }
            onPressed: oran7MainUiSetting_openImage.scale = 0.95
            onReleased: oran7MainUiSetting_openImage.scale = 1
        }
    }
}
