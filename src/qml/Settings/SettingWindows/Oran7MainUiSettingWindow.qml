import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects

import "../GlobalSettings"
import "../../Components"
import "../../Components/Oran7SettingUiWindowItems"

import Oran7UI.Impl

Item {
    id: root
    visible: false
    width: Oran7Theme.Oran7MainGUI.settingWinItemDefalutWidth
    height: 0
    opacity: 0
    x: root.savedNormalX
    y: 0

    property int  winIndex : 0

    property int savedNormalX: 80 + Oran7Theme.Oran7MainGUI.settingWinItemDefalutWidth * root.winIndex
    property real savedNormalY: 40
    property real savedNormalHeight: 700 // 初始默认值，会根据内容动态调整

    property point clickPos: Qt.point(0, 0)
    property bool mouseIsPressed: false
    property bool isAnimating: false  // 动画期间禁用 Behavior，防止与显式动画冲突导致抖动

    function prepareForOpen() {
        window_closeAnimation.stop();
        root.isAnimating = true;
        root.opacity = 0;
        root.y = 0;
        root.visible = true;
        root.savedNormalHeight = Math.max(contene_column.implicitHeight + topDragRect.height + 50, 200);
        //root.height = root.savedNormalHeight;
        root.height = 0;
    }

    function startOpenAnimation() {
        window_openAnimation.restart();
    }

    function startCloseAnimation() {
        window_openAnimation.stop();
        root.isAnimating = true;
        window_closeAnimation.restart();
    }

    Timer {
        id: openStartedTimer
        interval: 20
        repeat: false
        onTriggered: {
            Oran7MainUiSetting.settingWindow_isOpening_Or_isClosing = true
            Oran7Theme.installComponentToken("Oran7MainGUI","OpenSettingWin",true)
            Oran7Theme.installComponentToken("Oran7MainGUI","settingContent_visiable",true)
            settingSound.playOpen();
        }
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
            easing.type: Easing.OutCubic
        }

        PropertyAnimation {
            target: root
            property: "y"
            from: 0
            to: root.savedNormalY
            duration: window_openAnimation.aniDuration
            easing.type: Easing.OutCubic
        }

        PropertyAnimation {
            target: root
            property: "height"
            from: 0
            to: root.savedNormalHeight
            duration: window_openAnimation.aniDuration
            easing.type: Easing.OutCubic
        }

        onStarted: {
            openStartedTimer.start();
        }

        onFinished: {
            root.isAnimating = false;
            root.height = Qt.binding(function() { return root.savedNormalHeight; });
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
            to: 0
            duration: window_closeAnimation.aniDuration
            easing.type: Easing.InExpo
        }

        PropertyAnimation {
            target: root
            property: "height"
            from: root.savedNormalHeight
            to: 0
            duration: window_closeAnimation.aniDuration
            easing.type: Easing.InExpo
        }

        onStarted: {
             //first index of settingWindow
            Oran7Theme.installComponentToken("Oran7MainGUI","settingContent_visiable",false)
            Oran7MainUiSetting.settingWindow_isOpening_Or_isClosing = true
        }

        onFinished: {
            root.visible = false;
            root.opacity = 0
            root.y = 0;
            root.height = 0;
            root.isAnimating = false;

            //console.log("finised close,opacity:",root.opacity)

             //first index of settingWindow
            Oran7MainUiSetting.clickedOutSide()
            Oran7Theme.installComponentToken("Oran7MainGUI","OpenSettingWin",false)
            settingSound.playClose();
        }
    }

    Rectangle {
        id: ui_root
        anchors.fill: parent
        color: "transparent"
        radius: 10

        // 阴影效果 - 延迟到动画完成后再启用，避免首次渲染卡顿
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
            Behavior on color{PropertyAnimation{duration: Oran7Theme.Primary.durationMid}}
            radius: 10
            clip: true
            opacity: 1

            Oran7SetingTitleItem{
                id:topDragRect
                title:"Oran7MainUiSettings"
                anchors.top: parent.top
                anchors.margins: 2
            }

            //<=== ui content goes here ===>
            Column {
                id:contene_column
                anchors.top: topDragRect.bottom
                anchors.topMargin: 8
                anchors.left: parent.left
                anchors.leftMargin: 1
                anchors.right: parent.right
                spacing: 2

                // ~~~~~ Oran7SettingWindow ~~~~~
                Oran7SettingItem{
                    id:settingWindowSettings
                    index: 0
                    text:"SettingWindows"
                    fontBold:true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand:true
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: settingWindowSettings.expand
                    Column{
                        // --- item: isDarkMode ---
                        Oran7SettingItem {
                            text: "Is Dark Mode:"
                            // 开关按钮
                            Oran7SwitchToggleItem {
                                checked: Oran7Theme.Oran7MainGUI.isDarkMode
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.saveComponentToken("Oran7MainGUI","isDarkMode",checked)
                                }
                            }
                        }
                        // --- theme color set ---
                        Oran7ColorSettingGroup{
                            title:"SettingWin Theme Color:"
                            checkedColor:Oran7Theme.Oran7MainGUI.themeColor
                            componentName: "Oran7MainGUI"
                            colorToken:"colorPrimaryBase"
                            onEnterOfTextFiled: function(text){
                                Oran7Theme.saveComponentToken(componentName,"themeColor",text)
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${text})`)
                            }
                            onColorReady: function(seletedColor){
                                Oran7Theme.saveComponentToken(componentName,"themeColor",String(seletedColor).toLowerCase())
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${seletedColor})`)
                            }
                        }
                        // --- soundEffectVolume ---
                        Oran7NumSettingGroup{
                            value: Oran7Theme.Oran7MainGUI.soundEffectVolume
                            title:"SoundEffectVolume："
                            property string componentName: "Oran7MainGUI"
                            property string tokenName: "soundEffectVolume"
                            sliderValueFrom: 0.0
                            sliderValueTo: 1.0
                            stepSize:0.1
                            valueDecimals:2
                            onCommitted: (value, thresholdPosition, ratio) =>{
                                Oran7Theme.saveComponentToken(componentName,tokenName,value)
                            }
                        }
                    }
                }

                // ~~~~~ MainWindowBackGround ~~~~~
                Oran7SettingItem{
                    id:mainWindowBackGroundSettings
                    text:"MainBackGround"
                    index: 1
                    fontBold: true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand:true
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: mainWindowBackGroundSettings.expand
                    Column{
                        // --- item: backgroundImagePath ---
                        Oran7SettingItem {
                            text: "Background Image:"
                            showTag: false
                        }
                        Oran7TextFieldItem {
                            id: background_image_textField
                            tempText: Oran7Theme.Oran7MainGUI.backgroundImage
                            placeholderText: "please input image path."
                            detectEnable: true
                            detectType: Oran7MainUiSetting.DetectionType.FileDetection
                            onEnterPressed: {
                                Oran7Theme.saveComponentToken("Oran7MainGUI","backgroundImage",tempText)
                            }
                            anchors.rightMargin: background_imageOpen.height + 2
                            Oran7OpenFolderItem {
                                id: background_imageOpen
                                fileDialog_selectReset: true
                                isMultiSelect: false
                                anchors.left: parent.right
                                anchors.leftMargin: 2
                                onReady: {
                                    //console.log(background_imageOpen.singleton_filePath)

                                    Oran7Theme.saveComponentToken("Oran7MainGUI","backgroundImage",background_imageOpen.filesArray[0]);
                                    background_image_textField.textField.text = Oran7Theme.Oran7MainGUI.backgroundImage
                                    background_image_textField.tempText = Oran7Theme.Oran7MainGUI.backgroundImage
                                }
                            }
                        }
                    }
                }

                // ~~~~~ Oran7CaptionBar Settings ~~~~~
                Oran7SettingItem{
                    id:captionBarSettings
                    index: 2
                    text:"CaptionBarSettings"
                    fontBold:true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand:true
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: captionBarSettings.expand
                    Column{
                        // --- SimpleCaptionBar ---
                        Oran7SettingItem{
                            text:"SimpleCaptionBar"
                            Oran7SwitchToggleItem{
                                checked: Oran7Theme.Oran7CaptionBar.isSimpleMode
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.saveComponentToken("Oran7CaptionBar","isSimpleMode",checked);
                                }
                            }
                        }

                        // --- CaptionSelectedColor ---
                        Oran7ColorSettingGroup{
                            title:"CaptionSelectedColor"
                            checkedColor: Oran7Theme.Oran7CaptionBar.selectedColor
                            componentName: "Oran7CaptionBar"
                            colorToken:"colorPrimaryBase"
                            onEnterOfTextFiled: function(text){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${text})`)
                            }
                            onColorReady: function(seletedColor){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${seletedColor})`)
                            }
                        }

                        // --- CaptionHoveredColor ---
                        Oran7ColorSettingGroup{
                            title:"CaptionHoveredColor:"
                            checkedColor: Oran7Theme.Oran7CaptionBar.hoveredColor
                            componentName: "Oran7CaptionBar"
                            colorToken:"colorPrimaryBase"
                            onEnterOfTextFiled: function(text){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${text})`)
                            }
                            onColorReady: function(seletedColor){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${seletedColor})`)
                            }
                        }

                        // --- CaptionIconColor ---
                        Oran7ColorSettingGroup{
                            title:"CaptionIconColor:"
                            checkedColor: Oran7Theme.Oran7CaptionBar[colorToken+"-6"]
                            componentName: "Oran7CaptionBar"
                            colorToken:"iconColorBase"
                            onEnterOfTextFiled: function(text){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${text})`)
                            }
                            onColorReady: function(seletedColor){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${seletedColor})`)
                            }
                        }
                        // --- CaptionTextColor ---
                        Oran7ColorSettingGroup{
                            title:"CaptionTextColor:"
                            checkedColor: Oran7Theme.Oran7CaptionBar[colorToken+"-6"]
                            componentName: "Oran7CaptionBar"
                            colorToken:"textColorBase"
                            onEnterOfTextFiled: function(text){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${text})`)
                            }
                            onColorReady: function(seletedColor){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${seletedColor})`)
                            }
                        }
                        //<<---Caption-Background
                        Oran7SettingItem{text:"Background：";fontBold:true;showTag:false}
                        // --- CaptionBlurEnabled ---
                        Oran7SettingItem {
                            text: "BlurEnabled:"
                            Oran7SwitchToggleItem {
                                checked: Oran7Theme.Oran7CaptionBar.blurEffectEnabled
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.saveComponentToken("Oran7CaptionBar","blurEffectEnabled",checked)
                                }
                            }
                        }
                        // --- CaptionBlurEffect-saturation ---
                        Oran7NumSettingGroup{
                            value:Oran7Theme.Oran7CaptionBar.saturation
                            title: "BlurEffect-Saturation:"
                            property string componentName: "Oran7CaptionBar"
                            property string tokenName: "saturation"
                            sliderValueFrom: -1.0
                            sliderValueTo: 2.0
                            stepSize: 0.1
                            valueDecimals: 1
                            onCommitted: (value, thresholdPosition, ratio) =>{
                                Oran7Theme.saveComponentToken(componentName,tokenName,value)
                            }
                        }
                        // --- CaptionBlurEffect-brightness ---
                        Oran7NumSettingGroup{
                            value:Oran7Theme.Oran7CaptionBar.brightness
                            title: "BlurEffect-Brightness:"
                            property string componentName: "Oran7CaptionBar"
                            property string tokenName: "brightness"
                            sliderValueFrom: -1.0
                            sliderValueTo: 1.0
                            stepSize: 0.1
                            valueDecimals: 1
                            onCommitted: (value, thresholdPosition, ratio) =>{
                                Oran7Theme.saveComponentToken(componentName,tokenName,value)
                            }
                        }
                        // --- CaptionBlurEffect-contrast ---
                        Oran7NumSettingGroup{
                            value:Oran7Theme.Oran7CaptionBar.contrast
                            title: "BlurEffect-Contrast:"
                            property string componentName: "Oran7CaptionBar"
                            property string tokenName: "contrast"
                            sliderValueFrom: -1.0
                            sliderValueTo: 1.0
                            stepSize: 0.1
                            valueDecimals: 1
                            onCommitted: (value, thresholdPosition, ratio) =>{
                                Oran7Theme.saveComponentToken(componentName,tokenName,value)
                            }
                        }
                    }
                }

                // ~~~~~ Component END ~~~~~
                Oran7SettingItem{text:"";showTag: false}

                // ~~~ Binding ~~~
                // 动态绑定窗口高度到内容高度
                Binding {
                    target: root
                    property: "savedNormalHeight"
                    value: Math.max(contene_column.implicitHeight + topDragRect.height + 50, 200)
                    when: root.visible // 只在窗口可见时更新
                }
            }
            //<===ui content ends here ===>
        }
    }

    MouseArea {
        id: dragArea

        anchors.left: parent.left
        anchors.right: parent.right
        y: 16
        height: topDragRect.height

        property point pressPos: Qt.point(0, 0)

        onPressed: function(mouse) {
            root.mouseIsPressed = true
            pressPos = Qt.point(mouse.x, mouse.y)
        }

        onReleased: function(mouse) {
            root.mouseIsPressed = false
            root.savedNormalX = root.x
            root.savedNormalY = root.y
        }

        onPositionChanged: function(mouse) {
            if (!root.mouseIsPressed)
                return

            let delta = Qt.point(mouse.x - pressPos.x, mouse.y - pressPos.y)
            root.x += delta.x
            root.y += delta.y
        }
    }
}
