pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects

import "../GlobalSettings"
import "../../Components"
import "../../Components/Oran7SettingUiWindowItems"

Window {
    id: root
    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    modality: Qt.NonModal
    width: 252
    height: 600
    opacity: 0
    x: root.savedNormalX
    y: root.savedNormalY

    readonly property int savedNormalX: 40
    readonly property real savedNormalY: 20
    readonly property real savedNormalHeight: 600

    property point clickPos: Qt.point(0, 0)
    property bool mouseIsPressed: false

    Connections {
        target: Oran7MainUiSetting
        function onTriggleOpen_Oran7MainUiSetting_window() {
            delayTimer.delay(10).then(function () {
                if (root.visible === false) {
                    root.visible = true;
                    //only MainUiSettingWindow change it, a sign for extern
                    Oran7MainUiSetting.settingContent_visiable = true
                    window_openAnimation.restart();
                } else {
                    window_closeAnimation.restart();
                }
            });
        }
    }

    // --- tools Component ---
    Oran7DelayTimer {
        id: delayTimer
    }

    ParallelAnimation {
        id: window_openAnimation
        readonly property real aniDuration: Oran7MainUiSetting.toggleOpenAniDuration

        PropertyAnimation {
            target: root
            property: "opacity"
            from: 0
            to: 1
            duration: window_openAnimation.aniDuration
            easing.type: Easing.InOutCubic
        }

        PropertyAnimation {
            target: root
            property: "y"
            from: -root.savedNormalY
            to: root.savedNormalY
            duration: window_openAnimation.aniDuration
            easing.type: Easing.InOutCubic
        }

        PropertyAnimation {
            target: root
            property: "height"
            from: 0
            to: root.savedNormalHeight
            duration: window_openAnimation.aniDuration
            easing.type: Easing.InOutCubic
        }
    }

    ParallelAnimation {
        id: window_closeAnimation
        readonly property real aniDuration: Oran7MainUiSetting.toggleOpenAniDuration

        PropertyAnimation {
            target: root
            property: "opacity"
            from: 1
            to: 0
            duration: window_closeAnimation.aniDuration
            easing.type: Easing.InExpo
        }

        PropertyAnimation {
            target: root
            property: "y"
            from: root.savedNormalY
            to: -root.savedNormalY
            duration: window_closeAnimation.aniDuration
            easing.type: Easing.InExpo
        }

        PropertyAnimation {
            target: root
            property: "height"
            from: root.savedNormalHeight
            to: 0
            duration: window_openAnimation.aniDuration
            easing.type: Easing.InExpo
        }

        onStarted: {
            Oran7MainUiSetting.settingContent_visiable = false
        }

        onFinished: {
            root.visible = false;
        }
    }

    Rectangle {
        id: ui_root
        anchors.fill: parent
        color: "transparent"
        radius: 10

        // 阴影效果 - 添加在内容下方
        layer.enabled: true
        layer.effect: DropShadow {
            horizontalOffset: 0
            verticalOffset: 5
            radius: 12
            samples: 16
            spread: 0.7
            color: Oran7MainUiSetting.winShadowColor
            transparentBorder: true
        }

        Rectangle {
            id: ui_content
            anchors.fill: parent
            anchors.margins: 16  // 避开阴影边缘
            color: Oran7MainUiSetting.backColor
            radius: 10
            opacity: 1

            Oran7SetingTitleItem{
                id:topDragRect
                title:"Oran7MainUiSettings"
                anchors.top: parent.top
                anchors.margins: 2
            }

            //<--- ui content goes here --->
            Column {
                anchors.top: topDragRect.bottom
                anchors.topMargin: 8
                anchors.left: parent.left
                anchors.leftMargin: 4
                anchors.right: parent.right
                spacing: 2

                // --- item: isDarkMode ---
                Oran7SettingItem {
                    text: "Is Dark Mode:"
                    // 开关按钮
                    Oran7SwitchToggleItem {
                        checked: Oran7MainUiSetting.isDarkMode
                        onSwitchToggleChanged: function (checked) {
                            Oran7MainUiSetting.isDarkMode = checked;
                        }
                    }
                }

                // --- item: backgroundImagePath ---
                Oran7SettingItem {
                    text: "Background Image:"
                    Oran7OpenFolderItem {
                        id: background_imageOpen
                        fileDialog_selectReset: true
                        isMultiSelect: false
                        onReady: {
                            //console.log(background_imageOpen.singleton_filePath)
                            Oran7MainUiSetting.backgroundImagePath = background_imageOpen.filesArray[0];
                            background_image_textField.textField.text = Oran7MainUiSetting.backgroundImagePath
                            background_image_textField.tempText = Oran7MainUiSetting.backgroundImagePath
                        }
                    }
                }
                Oran7TextFieldItem {
                    id: background_image_textField
                    tempText: Oran7MainUiSetting.backgroundImagePath
                    placeholderText: "please input image path."
                    detectEnable: true
                    detectType: Oran7MainUiSetting.DetectionType.FileDetection
                    onEnterPressed: {
                        Oran7MainUiSetting.backgroundImagePath = tempText
                    }
                }

                // --- theme color set ---
                Oran7SettingItem {
                    text: "SettingWin Theme Color:"
                }
                Oran7TextFieldItem {
                    id: themeColor_textFiled
                    tempText: ""
                    placeholderText: tempText.length <=0 ? Oran7MainUiSetting.themeColor : ""
                    detectEnable: true
                    detectType: Oran7MainUiSetting.DetectionType.ColorDetection
                    onEnterPressed: {
                        Oran7MainUiSetting.themeColor = tempText
                    }
                }

                // ~~~~~ Oran7CaptionBar Settings ~~~~~
                // Oran7GroupDiveLine{ //Discard
                //     id:dive1
                // }
                Oran7SetingTitleItem{
                    id:oran7CaptionBarSettings
                    title:"CaptionBarSettings"
                }

                // --- SimpleCaptionBar ---
                Oran7SettingItem{
                    text:"SimpleCaptionBar"
                    Oran7SwitchToggleItem{
                        checked: Oran7MainUiSetting.captionBar_is_simpleMode
                        onSwitchToggleChanged: function (checked) {
                            Oran7MainUiSetting.captionBar_is_simpleMode = checked;
                        }
                    }
                }

            }
            //<--- ui content ends here --->
        }
    }

    // 拖动区域 - 整个窗口可拖动
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        onPressed: function(mouse) {
            root.mouseIsPressed = true;
            root.clickPos = Qt.point(mouse.x, mouse.y);

            // 检查是否在标题栏区域（用于拖动）
            let inTitleBar = mouse.y <= topDragRect.height + 16 && mouse.y >= 16;

            if (inTitleBar) {
                mouse.accepted = true; // 标题栏拖动事件被接受
            } else {
                mouse.accepted = false; // 其他区域事件传递给子组件
                // 立即触发 clickedOutSide，让 TextField 失去焦点\
                //console.log("clickedOutSide")
                Oran7MainUiSetting.clickedOutSide();
            }
        }
        onReleased: function(mouse) {
            root.mouseIsPressed = false;
            mouse.accepted = false;
        }
        onPositionChanged: function(mouse) {
            if (root.mouseIsPressed === false)
                return;
            if (root.clickPos.y > topDragRect.height + 16 || root.clickPos.y < 16)
                return;
            let delta = Qt.point(mouse.x - root.clickPos.x, mouse.y - root.clickPos.y);
            root.x += delta.x;
            root.y += delta.y;
        }
        onClicked: function(mouse) {
            mouse.accepted = false; // 允许事件继续传递给子组件
        }
    }
}
