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

    // 选中与悬停各共用一个滑块；悬停结束只淡出，保留最后位置以便连续滑动。
    property int navigationHoverIndex: 0
    readonly property color navigationAccent: Oran7Theme.Oran7CaptionBar.selectedColor
    property bool navigationAnimationReady: false

    onItemHovered_indexChanged: {
        if (itemHovered_index >= 0)
            navigationHoverIndex = itemHovered_index
    }
    Component.onCompleted: Qt.callLater(function() { root.navigationAnimationReady = true })
    onVisibleChanged: {
        if (!visible) {
            itemHovered_index = -1
            mouseIsInCaptionBar = false
        }
    }

    component Oran7NavigationSlider: Rectangle {
        id: slider
        property int targetIndex: 0
        property bool selected: false
        property bool engaged: true
        readonly property color accentColor: selected ? Qt.darker(root.navigationAccent, 1.12)
                                                     : Oran7Theme.Oran7CaptionBar.hoveredColor
        anchors.horizontalCenter: parent.horizontalCenter
        width: topColumn.width
        height: 42
        radius: 9
        visible: targetIndex >= 0 && targetIndex < topListModel.count + middleListModel.count
        opacity: engaged ? 1 : 0
        y: targetIndex < topListModel.count
           ? topColumn.y + targetIndex * (height + topColumn.spacing)
           : middleColumn.y + (targetIndex - topListModel.count) * (height + middleColumn.spacing)
        border.width: 1
        border.color: Qt.alpha(accentColor, selected ? 0.84 : 0.50)
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0
                color: Qt.alpha(slider.accentColor, slider.selected ? 0.82 : 0.52)
                Behavior on color { ColorAnimation { duration: 180 } }
            }
            GradientStop {
                position: 0.65
                color: Qt.alpha(slider.accentColor, slider.selected ? 0.56 : 0.30)
                Behavior on color { ColorAnimation { duration: 180 } }
            }
            GradientStop { position: 1; color: Qt.alpha(slider.accentColor, slider.selected ? 0.32 : 0.16) }
        }

        Behavior on y {
            enabled: root.navigationAnimationReady && root.visible
            NumberAnimation { duration: 340; easing.type: Easing.OutQuint }
        }
        Behavior on border.color { ColorAnimation { duration: 180 } }
        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

        // 左侧定位灯与边缘刻线随整个滑块移动，不在委托里叠加背景。
        Rectangle {
            x: 0
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: slider.selected ? 22 : 14
            radius: 1.5
            color: slider.accentColor
            Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        }
        Rectangle {
            x: 12; y: 1
            width: Math.max(0, parent.width - 24)
            height: 1
            color: Qt.alpha("#ffffff", 0.14)
        }
        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 5
            width: Math.max(0, parent.width - 65)
            height: 1
            color: Qt.alpha(slider.accentColor, 0.32)
            visible: !root.simpleMode
        }
        Column {
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3
            Repeater {
                model: 3
                Rectangle {
                    width: 3
                    height: 1
                    color: Qt.alpha(slider.accentColor, index === 1 ? 0.9 : 0.45)
                }
            }
        }
    }

    Oran7NavigationSlider {
        objectName: "navigationHoverHighlight"
        targetIndex: root.navigationHoverIndex
        engaged: root.itemHovered_index >= 0 && root.itemHovered_index !== root.itemSelected_index
    }
    Oran7NavigationSlider {
        objectName: "navigationSelectedHighlight"
        targetIndex: root.itemSelected_index
        selected: true
    }

    Label {
        id: logoLabel
        text: "Oran柒"
        anchors.left: parent.left
        anchors.leftMargin: 58
        anchors.top: parent.top
        anchors.topMargin: 18
        font.family: "华文琥珀" // 品牌标志固定字体，不跟随主题或界面字体。
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
        font.family: "华文琥珀" // 品牌标志固定字体，不跟随主题或界面字体。
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
            iconText: "视频播放器"
            pageName: "VideoPlayerPage"
        }
        ListElement {
            iconImage: "qrc:/image/hugeicons_video-camera-ai.png"
            iconText: "直播工作台"
            pageName: "ScreenCapturePage"
        }
        ListElement {
            iconImage: "qrc:/image/arcticons_star-music-tag-editor.png"
            iconText: "终末地 • 帝江号"
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

        Repeater {
            id: topColumnRepeater
            model: topListModel
            delegate: Item {
                id: topColumnRepeaterElementRectangle
                objectName: "navigationItem" + model.index
                height: 42
                width: root.simpleMode ? 42 : 160
                Behavior on x { NumberAnimation { duration: Oran7Theme.Primary.durationMid } }
                Behavior on width { NumberAnimation { duration: Oran7Theme.Primary.durationMid } }

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
                    text: qsTranslate("Oran7Ui", iconText)
                    font.pixelSize: 14
                    font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
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
                    cursorShape: Qt.PointingHandCursor
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
                        root.mouseIsInCaptionBar = true
                    }
                    onExited: {
                        if (root.itemHovered_index === model.index) {
                            root.itemHovered_index = -1
                            root.mouseIsInCaptionBar = false
                        }
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
        text: qsTranslate("Oran7Ui", "音乐")
        anchors.top: diveLine1.bottom
        anchors.topMargin: 14
        anchors.left: topColumn.left
        anchors.leftMargin: 4
        font.pixelSize: 12
        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
        color: Oran7Theme.Oran7CaptionBar["textColorBase-6"]
    }

    //中间列表数据模型
    ListModel {
        id: middleListModel
        ListElement {
            iconImage: "qrc:/image/ep_collection-tag.-no-middle-transparent.png"
            iconText: "我整合的音乐"
            pageName: "LocalMusicPage"
        }
        ListElement {
            iconImage: "qrc:/image/thesvg_netease-cloud-music.png"
            iconText: "从网易云搜索"
            pageName: "SearchMusicPage"
        }
    }
    //中间的列表
    Column {
        id: middleColumn
        anchors.top: myTextLabel.bottom
        anchors.topMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 4
        Repeater {
            id: middleColumnRepeater
            model: middleListModel
            delegate: Item {
                id: middleColumnRepeaterElementRectangle
                objectName: "navigationItem" + (model.index + topListModel.count)
                height: 42
                width: root.simpleMode ? 42 : 160
                Behavior on x { NumberAnimation { duration: 150 } }
                Behavior on width { NumberAnimation { duration: 150 } }

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
                    text: qsTranslate("Oran7Ui", iconText)
                    font.pixelSize: 14
                    font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
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
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.itemSelected_index = model.index + topListModel.count
                        switch (index) {
                        case 0:
                            BasicConfig.pushLocalMusicStackInto_RightPageMainStackView();
                            break;
                        case 1:
                            BasicConfig.pushSearchMusicStackInto_RightPageMainStackView();
                            break;
                        default:
                            break;
                        }
                    }
                    onEntered: {
                        root.itemHovered_index = model.index + topListModel.count
                        root.mouseIsInCaptionBar = true
                    }
                    onExited: {
                        if (root.itemHovered_index === model.index + topListModel.count) {
                            root.itemHovered_index = -1
                            root.mouseIsInCaptionBar = false
                        }
                    }
                }
            }
        }
    }
    //" ∨更多"TextLabel
    Label {
        id: moreLabel
        text: qsTranslate("Oran7Ui", "∨  更多")
        anchors.top: middleColumn.bottom
        anchors.topMargin: 5
        anchors.left: middleColumn.left
        anchors.leftMargin: 2
        font.pixelSize: 13
        font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
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
    //     font.family: (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family
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
