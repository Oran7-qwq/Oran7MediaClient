import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects

import "../GlobalSettings"
import "../../Components/Oran7GlobalComponents"
import "../../Components/Oran7SettingUiWindowItems"

import Oran7UI.Impl

Oran7SettingsPanel {
    id: root
    width: Oran7Theme.Oran7MainGUI.settingWinItemDefalutWidth
    x: root.savedNormalX

    readonly property var preferences: typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences : null

    readonly property bool preferencesPopupOpen: preferencesGroup.popupOpen
    function closePreferencesPopup() { preferencesGroup.closePopup() }

    property int  winIndex : 0

    property int savedNormalX: 80 + Oran7Theme.Oran7MainGUI.settingWinItemDefalutWidth * root.winIndex
    savedNormalY: 40
    savedNormalHeight: 700

    property point clickPos: Qt.point(0, 0)
    property bool mouseIsPressed: false

    animationDuration: Oran7MainUiSetting.toggleOpenAniDuration

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
                title:qsTranslate("Oran7Ui", "主界面设置")
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

                Oran7UiPreferencesGroup {
                    id: preferencesGroup
                    objectName: "uiPreferencesGroup"
                    preferences: root.preferences
                }
                // ~~~~~ Oran7SettingWindow ~~~~~
                Oran7SettingItem{
                    id:settingWindowSettings
                    index: 0
                    text:qsTranslate("Oran7Ui", "设置窗口")
                    fontBold:true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand:false
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: settingWindowSettings.expand
                    Column{
                        // --- item: isDarkMode ---
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "深色模式")
                            // 开关按钮
                            Oran7SwitchToggleItem {
                                id: darkModeSwitch
                                checked: Oran7Theme.isDark
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.darkMode = checked ? Oran7Theme.Dark : Oran7Theme.Light
                                    // 开关内部点击会赋值 checked，恢复与唯一主题状态的绑定。
                                    darkModeSwitch.checked = Qt.binding(function() { return Oran7Theme.isDark })
                                }
                            }
                        }
                        // --- theme color set ---
                        Oran7ColorSettingGroup{
                            title:qsTranslate("Oran7Ui", "设置窗口主题色")
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
                            title:qsTranslate("Oran7Ui", "提示音音量")
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
                    text:qsTranslate("Oran7Ui", "主界面背景")
                    index: 1
                    fontBold: true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand:false
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: mainWindowBackGroundSettings.expand
                    Column{
                        // --- DWMBlurEffect ---
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "窗口毛玻璃")
                            // 开关按钮
                            Oran7SwitchToggleItem {
                                checked: Oran7Theme.Oran7MainGUI.DWMBlurEffectEnabled
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.saveComponentToken("Oran7MainGUI","DWMBlurEffectEnabled",checked)
                                }
                            }
                        }
                        // --- BackgroundMode: Image / Video ---
                        // 控制主窗口背景是静态图片还是循环视频，默认 Video
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "背景模式：") + (Oran7Theme.Oran7MainGUI.backgroundMode === "Video" ? qsTranslate("Oran7Ui", "视频") : qsTranslate("Oran7Ui", "图片"))
                            Oran7SwitchToggleItem {
                                checked: Oran7Theme.Oran7MainGUI.backgroundMode === "Video"
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.saveComponentToken("Oran7MainGUI","backgroundMode", checked ? "Video" : "Image")
                                }
                            }
                        }
                        // --- item: backgroundImagePath ---
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "背景图片")
                            showTag: false
                        }
                        Oran7TextFieldItem {
                            id: background_image_textField
                            tempText: Oran7Theme.Oran7MainGUI.backgroundImage
                            placeholderText: qsTranslate("Oran7Ui", "请输入图片路径")
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
                        // --- item: backgroundVideoPath ---
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "背景视频")
                            showTag: false
                        }
                        Oran7TextFieldItem {
                            id: background_video_textField
                            tempText: Oran7Theme.Oran7MainGUI.backgroundVideo
                            placeholderText: qsTranslate("Oran7Ui", "请输入视频路径")
                            detectEnable: true
                            detectType: Oran7MainUiSetting.DetectionType.FileDetection
                            onEnterPressed: {
                                Oran7Theme.saveComponentToken("Oran7MainGUI","backgroundVideo",tempText)
                            }
                            anchors.rightMargin: background_videoOpen.height + 2
                            Oran7OpenFolderItem {
                                id: background_videoOpen
                                fileDialog_selectReset: true
                                isMultiSelect: false
                                anchors.left: parent.right
                                anchors.leftMargin: 2
                                onReady: {
                                    Oran7Theme.saveComponentToken("Oran7MainGUI","backgroundVideo",background_videoOpen.filesArray[0]);
                                    background_video_textField.textField.text = Oran7Theme.Oran7MainGUI.backgroundVideo
                                    background_video_textField.tempText = Oran7Theme.Oran7MainGUI.backgroundVideo
                                }
                            }
                        }
                    }
                }

                // ~~~~~ Oran7CaptionBar Settings ~~~~~
                Oran7SettingItem{
                    id:captionBarSettings
                    index: 2
                    text:qsTranslate("Oran7Ui", "标题栏设置")
                    fontBold:true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand:false
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: captionBarSettings.expand
                    Column{
                        // --- SimpleCaptionBar ---
                        Oran7SettingItem{
                            text:qsTranslate("Oran7Ui", "简洁标题栏")
                            Oran7SwitchToggleItem{
                                checked: Oran7Theme.Oran7CaptionBar.isSimpleMode
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.saveComponentToken("Oran7CaptionBar","isSimpleMode",checked);
                                }
                            }
                        }

                        // --- CaptionSelectedColor ---
                        Oran7ColorSettingGroup{
                            title:qsTranslate("Oran7Ui", "标题栏选中颜色")
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
                            title:qsTranslate("Oran7Ui", "标题栏悬停颜色")
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
                            title:qsTranslate("Oran7Ui", "标题栏图标颜色")
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
                            title:qsTranslate("Oran7Ui", "标题栏文字颜色")
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
                        Oran7SettingItem{text:qsTranslate("Oran7Ui", "背景");fontBold:true;showTag:false}
                        // --- CaptionBlurEnabled ---
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "启用模糊")
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
                            title: qsTranslate("Oran7Ui", "模糊饱和度")
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
                            title: qsTranslate("Oran7Ui", "模糊亮度")
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
                            title: qsTranslate("Oran7Ui", "模糊对比度")
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
