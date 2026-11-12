import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects

import "../GlobalSettings"
import "../../Components/Oran7GlobalComponents"
import "../../Components/Oran7SettingUiWindowItems"

import Oran7UI.Impl
import Client 1.0

Oran7SettingsPanel {
    id: root
    property var equalizerEditor: null
    function openEqualizer() {
        if (equalizerEditor) { equalizerEditor.requestActivate(); return }
        const component = Qt.createComponent(Qt.resolvedUrl("../../Components/Oran7GlobalComponents/Oran7EqualizerWindow.qml"))
        if (component.status !== Component.Ready) { console.warn(component.errorString()); return }
        equalizerEditor = component.createObject(root, {effects: Client.audioEffects, transientParent: root.Window.window})
        if (!equalizerEditor) return
        const host = root.Window.window
        equalizerEditor.x = host ? host.x + (host.width - equalizerEditor.width) / 2 : 80
        equalizerEditor.y = host ? host.y + (host.height - equalizerEditor.height) / 2 : 80
        equalizerEditor.commitRequested.connect(function() { Client.saveAudioEffects() })
        equalizerEditor.showEditor()
    }
    width: Oran7Theme.Oran7MainGUI.settingWinItemDefalutWidth
    x: root.savedNormalX

    property int  winIndex : 3

    property real savedNormalX: 80 + Oran7Theme.Oran7MainGUI.settingWinItemDefalutWidth * root.winIndex
    savedNormalY: 40
    savedNormalHeight: 600

    property point clickPos: Qt.point(0, 0)
    property bool mouseIsPressed: false

    animationDuration: Oran7MainUiSetting.toggleOpenAniDuration

    Rectangle {
        id: ui_root
        anchors.fill: parent
        color: "transparent"
        radius: 10

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
            anchors.margins: 16
            color: Oran7MainUiSetting.backColor
            Behavior on color{PropertyAnimation{duration: Oran7Theme.Primary.durationMid}}
            radius: 10
            clip: true
            opacity: 1

            Oran7SetingTitleItem{
                id:topDragRect
                title:qsTranslate("Oran7Ui", "音乐播放器设置")
                anchors.top: parent.top
                anchors.margins: 2
            }

            //<--- ui content goes here --->
            Column {
                id:contene_column
                anchors.top: topDragRect.bottom
                anchors.topMargin: 8
                anchors.left: parent.left
                anchors.leftMargin: 1
                anchors.right: parent.right
                spacing: 2

                // ~~~~~ MusicListView Settings ~~~~~
                Oran7SettingItem{
                    id:musicListViewSettings
                    objectName: "musicListViewSettings"
                    index: 0
                    text:qsTranslate("Oran7Ui", "音乐列表设置")
                    fontBold:true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand:false
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: musicListViewSettings.expand
                    Column{
                        Oran7ColorSettingGroup {
                            objectName: "playlistBackgroundSettings"
                            title: qsTranslate("Oran7Ui", "列表背景颜色")
                            checkedColor: Oran7Theme.Oran7MusicPlaylistView.rowBackgroundColor
                            componentName: "Oran7MusicPlaylistView"
                            colorToken: "rowBackgroundColor"
                            pickerLabel: qsTranslate("Oran7Ui", "取色板")
                            presetColors: ["#ede7db"]
                            presetNames: [qsTranslate("Oran7Ui", "米白色")]
                            onEnterOfTextFiled: function(text) {
                                Oran7Theme.saveComponentToken(componentName, colorToken, text)
                            }
                            onColorReady: function(color) {
                                Oran7Theme.saveComponentToken(componentName, colorToken, String(color))
                            }
                        }
                        // --- textColor ---
                        Oran7ColorSettingGroup{
                            id: playlistTextSettings
                            objectName: "playlistTextSettings"
                            title:qsTranslate("Oran7Ui", "列表文字颜色")
                            readonly property color listBackground: Oran7Theme.Oran7MusicPlaylistView.rowBackgroundColor
                            readonly property string colorMode: (listBackground.r * 0.2126
                                + listBackground.g * 0.7152 + listBackground.b * 0.0722) > 0.58 ? "Light" : "Dark"
                            readonly property color originalTextColor: Oran7Theme.Oran7MusicPlaylistView["rowText" + colorMode]
                            checkedColor: Oran7Theme.Oran7MusicPlaylistView.rowTextColor || originalTextColor
                            componentName: "Oran7MusicPlaylistView"
                            colorToken: "rowTextColor"
                            presetColors: [originalTextColor, "#A78BFA", "#60A5FA", "#818CF8", "#22D3EE", "#34D399",
                                           "#A3E635", "#FBBF24", "#FB923C", "#FB7185", "#F472B6"]
                            presetNames: [qsTranslate("Oran7Ui", "原版列表配色"),
                                qsTranslate("Oran7Ui", "鸢尾紫"), qsTranslate("Oran7Ui", "晴空蓝"),
                                qsTranslate("Oran7Ui", "靛青蓝"), qsTranslate("Oran7Ui", "冰川青"),
                                qsTranslate("Oran7Ui", "薄荷绿"), qsTranslate("Oran7Ui", "青柠绿"),
                                qsTranslate("Oran7Ui", "琥珀金"), qsTranslate("Oran7Ui", "落日橙"),
                                qsTranslate("Oran7Ui", "珊瑚红"), qsTranslate("Oran7Ui", "樱花粉")]
                            function saveTextColor(color) {
                                // 原版选项恢复整套主次颜色，而不是旧设置中的生成色色阶。
                                Oran7Theme.saveComponentToken(componentName, colorToken,
                                    Qt.colorEqual(color, originalTextColor) ? "" : String(color))
                            }
                            onEnterOfTextFiled: function(text){
                                saveTextColor(text)
                            }
                            onColorReady: function(seletedColor){
                                saveTextColor(seletedColor)
                            }
                        }

                        Oran7ColorSettingGroup {
                            objectName: "playlistAccentSettings"
                            title: qsTranslate("Oran7Ui", "列表强调色")
                            checkedColor: Oran7Theme.Oran7MusicPlaylistView.rowAccentColor
                            componentName: "Oran7MusicPlaylistView"
                            colorToken: "rowAccentColor"
                            onEnterOfTextFiled: function(text) {
                                Oran7Theme.saveComponentToken(componentName, colorToken, text)
                            }
                            onColorReady: function(color) {
                                Oran7Theme.saveComponentToken(componentName, colorToken, String(color))
                            }
                        }

                        // --- textFontPixelSize ---
                        Oran7NumSettingGroup{
                            value:Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize
                            title: qsTranslate("Oran7Ui", "列表文字大小")
                            property string componentName: "Oran7MusicPlaylistView"
                            property string tokenName: "listViewFontPixelSize"
                            sliderValueFrom: 7
                            sliderValueTo: 20
                            onCommitted: (value, thresholdPosition, ratio) =>{
                                Oran7Theme.saveComponentToken(componentName,tokenName,value)
                            }
                        }
                    }
                }
                // ~~~~~ MusicPlayControlsBar ~~~~~
                Oran7SettingItem{
                    id:musicPlayControlsBar
                    index: 1
                    text:qsTranslate("Oran7Ui", "播放控制栏")
                    fontBold:true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand:false
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: musicPlayControlsBar.expand
                    Column{
                        // --- textColor ---
                        Oran7ColorSettingGroup{
                            title:qsTranslate("Oran7Ui", "文字颜色")
                            checkedColor: Oran7Theme.Oran7MusicPlayControls[colorToken +"-6"]
                            componentName: "Oran7MusicPlayControls"
                            colorToken:"textColorBase"
                            onEnterOfTextFiled: function(text){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${text})`)
                            }
                            onColorReady: function(seletedColor){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${seletedColor})`)
                            }
                        }

                        // --- iconColor ---
                        Oran7ColorSettingGroup{
                            title:qsTranslate("Oran7Ui", "图标颜色")
                            checkedColor: Oran7Theme.Oran7MusicPlayControls[colorToken +"-6"]
                            componentName: "Oran7MusicPlayControls"
                            colorToken:"iconColorBase"
                            onEnterOfTextFiled: function(text){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${text})`)
                            }
                            onColorReady: function(seletedColor){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${seletedColor})`)
                            }
                        }

                        //---progressVisibleBar_ColorIndex ---
                        Oran7NumSettingGroup{
                            value:Oran7Theme.Oran7MusicPlayControls.progressVisibleBar_ColorIndex
                            title: qsTranslate("Oran7Ui", "进度条颜色序号")
                            property string componentName: "Oran7MusicPlayControls"
                            property string tokenName: "progressVisibleBar_ColorIndex"
                            sliderValueFrom: 0
                            sliderValueTo: 19
                            stepSize: 1
                            valueDecimals: 0
                            onCommitted: (value, thresholdPosition, ratio) =>{
                                Oran7Theme.saveComponentToken(componentName,tokenName,value)
                            }
                            //<-preview
                            Oran7GradientMask{
                                width: Oran7MainUiSetting.itemHeight * 0.7
                                height: width
                                anchors.right: parent.right
                                anchors.rightMargin: 2
                                anchors.top: parent.top
                                anchors.topMargin: Oran7MainUiSetting.itemHeight * 0.4
                                gradientMaskEnabled: true
                                gradientColorIndex: Oran7Theme.Oran7MusicPlayControls.progressVisibleBar_ColorIndex
                                dynamicGradient: Oran7Theme.Oran7MusicPlayControls.progressVisibleBar_dynamicGradientEnabled
                                transitionDuration: Oran7Theme.Primary.durationMid  * 4
                                Rectangle{
                                    anchors.fill: parent
                                    color:"white"
                                    radius: 4
                                }
                            }
                        }
                        // --- progressVisibleBar_dynamicGradientEnabled ---
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "动态渐变")
                            // 开关按钮
                            Oran7SwitchToggleItem {
                                checked: Oran7Theme.Oran7MusicPlayControls.progressVisibleBar_dynamicGradientEnabled
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.saveComponentToken("Oran7MusicPlayControls","progressVisibleBar_dynamicGradientEnabled",checked)
                                    if(checked === true)
                                    Oran7Theme.saveComponentToken("Oran7MusicPlayControls","progressVisibleBar_ColorIndex",0)//retset to 0
                                }
                            }
                        }
                        //<<---Oran7MusicPlayControls-Background
                        Oran7SettingItem{text:qsTranslate("Oran7Ui", "背景");fontBold:true;showTag:false}
                        // --- Oran7MusicPlayControls-BlurEnabled ---
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "启用模糊")
                            Oran7SwitchToggleItem {
                                checked: Oran7Theme.Oran7MusicPlayControls.blurEffectEnabled
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.saveComponentToken("Oran7MusicPlayControls","blurEffectEnabled",checked)
                                }
                            }
                        }
                        // --- Oran7MusicPlayControls-BlurEffect-saturation ---
                        Oran7NumSettingGroup{
                            value:Oran7Theme.Oran7MusicPlayControls.saturation
                            title: qsTranslate("Oran7Ui", "模糊饱和度")
                            property string componentName: "Oran7MusicPlayControls"
                            property string tokenName: "saturation"
                            sliderValueFrom: -1.0
                            sliderValueTo: 2.0
                            stepSize: 0.1
                            valueDecimals: 1
                            onCommitted: (value, thresholdPosition, ratio) =>{
                                Oran7Theme.saveComponentToken(componentName,tokenName,value)
                            }
                        }
                        // --- Oran7MusicPlayControls-BlurEffect-brightness ---
                        Oran7NumSettingGroup{
                            value:Oran7Theme.Oran7MusicPlayControls.brightness
                            title: qsTranslate("Oran7Ui", "模糊亮度")
                            property string componentName: "Oran7MusicPlayControls"
                            property string tokenName: "brightness"
                            sliderValueFrom: -1.0
                            sliderValueTo: 1.0
                            stepSize: 0.1
                            valueDecimals: 1
                            onCommitted: (value, thresholdPosition, ratio) =>{
                                Oran7Theme.saveComponentToken(componentName,tokenName,value)
                            }
                        }
                        // --- Oran7MusicPlayControls-Effect-contrast ---
                        Oran7NumSettingGroup{
                            value:Oran7Theme.Oran7MusicPlayControls.contrast
                            title: qsTranslate("Oran7Ui", "模糊对比度")
                            property string componentName: "Oran7MusicPlayControls"
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
                // ~~~~~ MusicLyricsWindow ~~~~~
                Oran7SettingItem{
                    id:musicLyricsWindow
                    index: 2
                    text:qsTranslate("Oran7Ui", "歌词窗口")
                    fontBold:true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand:false
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: musicLyricsWindow.expand
                    Column{
                        //<<---Lyrics
                        Oran7SettingItem{text:qsTranslate("Oran7Ui", "歌词");fontBold:true;showTag:false}
                        // --- LyricsWin_Lyrics-TextColor ---
                        Oran7ColorSettingGroup{
                            title:qsTranslate("Oran7Ui", "歌词颜色")
                            checkedColor: Oran7Theme.Oran7MusicLyricsWindow[colorToken+"-6"]
                            componentName: "Oran7MusicLyricsWindow"
                            colorToken: "textColorBase"
                            onEnterOfTextFiled: function(text){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${text})`)
                            }
                            onColorReady: function(seletedColor){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${seletedColor})`)
                            }
                        }
                        // --- LyricsWin_Lyrics-TextFontSize ---
                        Oran7NumSettingGroup{
                            value:Oran7Theme.Oran7MusicLyricsWindow[tokenName+"-2"]
                            title: qsTranslate("Oran7Ui", "歌词字号")
                            property string componentName: "Oran7MusicLyricsWindow"
                            property string tokenName: "textSizeBase"
                            sliderValueFrom: 12
                            sliderValueTo: 40
                            stepSize: 1
                            valueDecimals: 0
                            onCommitted: (value, thresholdPosition, ratio) =>{
                                Oran7Theme.saveComponentToken(componentName,tokenName,`$genFontSize(${value})`)
                            }
                        }

                        //<<---Background
                        Oran7SettingItem{text:qsTranslate("Oran7Ui", "背景");fontBold: true;showTag: false}
                        // --- LyricsWin_Background-BlurEnabled ---
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "启用模糊")
                            showTag: false
                            Oran7SwitchToggleItem {
                                checked: Oran7Theme.Oran7MusicLyricsWindow.blurEffectEnabled
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.saveComponentToken("Oran7MusicLyricsWindow","blurEffectEnabled",checked)
                                }
                            }
                        }
                        // --- LyricsWin_Background-BlurEffect-saturation ---
                        Oran7NumSettingGroup{
                            value:Oran7Theme.Oran7MusicLyricsWindow.saturation
                            title: qsTranslate("Oran7Ui", "模糊饱和度")
                            property string componentName: "Oran7MusicLyricsWindow"
                            property string tokenName: "saturation"
                            sliderValueFrom: -1.0
                            sliderValueTo: 2.0
                            stepSize: 0.1
                            valueDecimals: 1
                            onCommitted: (value, thresholdPosition, ratio) =>{
                                Oran7Theme.saveComponentToken(componentName,tokenName,value)
                            }
                        }
                        // --- LyricsWin_Background-BlurEffect-brightness ---
                        Oran7NumSettingGroup{
                            value:Oran7Theme.Oran7MusicLyricsWindow.brightness
                            title: qsTranslate("Oran7Ui", "模糊亮度")
                            property string componentName: "Oran7MusicLyricsWindow"
                            property string tokenName: "brightness"
                            sliderValueFrom: -1.0
                            sliderValueTo: 1.0
                            stepSize: 0.1
                            valueDecimals: 1
                            onCommitted: (value, thresholdPosition, ratio) =>{
                                Oran7Theme.saveComponentToken(componentName,tokenName,value)
                            }
                        }
                        // --- LyricsWin_Background-BlurEffect-contrast ---
                        Oran7NumSettingGroup{
                            value:Oran7Theme.Oran7MusicLyricsWindow.contrast
                            title: qsTranslate("Oran7Ui", "模糊对比度")
                            property string componentName: "Oran7MusicLyricsWindow"
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

                // ~~~~~ MusicLyricsIsland ~~~~~
                Oran7SettingItem{
                    id:musicLyricsIsland
                    index: 3
                    text:qsTranslate("Oran7Ui", "歌词灵动岛")
                    fontBold:true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand:false
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: musicLyricsIsland.expand
                    Column {
                        // --- LyricsIsland_Lyrics-TextFontSize ---
                        Oran7NumSettingGroup{
                            value:Oran7Theme.Oran7MusicLyricsIsland[tokenName]
                            title: qsTranslate("Oran7Ui", "歌词字号")
                            property string componentName: "Oran7MusicLyricsIsland"
                            property string tokenName: "lyricsFontSize"
                            sliderValueFrom: 12
                            sliderValueTo: 40
                            stepSize: 1
                            valueDecimals: 0
                            onCommitted: (value, thresholdPosition, ratio) =>{
                                Oran7Theme.saveComponentToken(componentName,tokenName,value)
                            }
                        }
                        // --- LyricsIsland_BackdropColor（背景图关闭时的胶囊底色） ---
                        Oran7ColorSettingGroup{
                            title:qsTranslate("Oran7Ui", "背景颜色")
                            checkedColor: Oran7Theme.Oran7MusicLyricsIsland[colorToken+"-6"]
                            componentName: "Oran7MusicLyricsIsland"
                            colorToken: "floatingBackdropColor"
                            onEnterOfTextFiled: function(text){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${text})`)
                            }
                            onColorReady: function(seletedColor){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${seletedColor})`)
                            }
                        }
                        // --- LyricsIsland_BackgroundImageEnabled ---
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "背景图片")
                            showTag: false
                            Oran7SwitchToggleItem {
                                checked: Oran7Theme.Oran7MusicLyricsIsland.backgroundImageEnabled
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland","backgroundImageEnabled",checked)
                                }
                            }
                        }
                        // --- item: backgroundImage ---
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "背景图片路径")
                            showTag: false
                        }
                        Oran7TextFieldItem {
                            id: background_image_textField
                            tempText: Oran7Theme.Oran7MusicLyricsIsland[tokenName]
                            placeholderText: qsTranslate("Oran7Ui", "请输入图片路径")
                            property string componentName: "Oran7MusicLyricsIsland"
                            property string tokenName: "backgroundImage_url"
                            detectEnable: true
                            detectType: Oran7MainUiSetting.DetectionType.FileDetection
                            onEnterPressed: {
                                Oran7Theme.saveComponentToken(componentName,tokenName,tempText)
                            }
                            anchors.rightMargin: background_imageOpen.height + 2
                            Oran7OpenFolderItem {
                                id: background_imageOpen
                                fileDialog_selectReset: true
                                isMultiSelect: false
                                anchors.left: parent.right
                                anchors.leftMargin: 2
                                onReady: {
                                    Oran7Theme.saveComponentToken(parent.componentName,parent.tokenName,background_imageOpen.filesArray[0]);
                                    background_image_textField.textField.text = Oran7Theme.Oran7MusicLyricsIsland[parent.tokenName]
                                    background_image_textField.tempText = Oran7Theme.Oran7MusicLyricsIsland[parent.tokenName]
                                }
                            }
                        }
                        //<<--- 悬浮歌词窗（桌面歌词）
                        Oran7SettingItem{text:qsTranslate("Oran7Ui", "悬浮");fontBold:true;showTag:false}
                        Oran7NumSettingGroup {
                            title: qsTranslate("Oran7Ui", "悬浮窗歌词高度")
                            value: Oran7Theme.Oran7MusicLyricsIsland.floatingLyricsHeight ?? 52
                            sliderValueFrom: 40
                            sliderValueTo: 96
                            stepSize: 1
                            onCommitted: (value) => {
                                Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland", "floatingLyricsHeight", value)
                            }
                        }
                        // --- Floating-Enabled ---
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "启用")
                            showTag: false
                            Oran7SwitchToggleItem {
                                checked: Oran7Theme.Oran7MusicLyricsIsland.floatingEnabled
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland","floatingEnabled",checked)
                                }
                            }
                        }
                        // --- Floating-Locked ---
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "锁定")
                            showTag: false
                            Oran7SwitchToggleItem {
                                checked: Oran7Theme.Oran7MusicLyricsIsland.floatingLocked
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland","floatingLocked",checked)
                                }
                            }
                        }
                        // --- Floating-ClickThrough ---
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "鼠标穿透")
                            showTag: false
                            Oran7SwitchToggleItem {
                                checked: Oran7Theme.Oran7MusicLyricsIsland.floatingClickThrough
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland","floatingClickThrough",checked)
                                }
                            }
                        }
                        // --- Floating-AlwaysOnTop ---
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "窗口置顶")
                            showTag: false
                            Oran7SwitchToggleItem {
                                checked: Oran7Theme.Oran7MusicLyricsIsland.floatingAlwaysOnTop
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.saveComponentToken("Oran7MusicLyricsIsland","floatingAlwaysOnTop",checked)
                                }
                            }
                        }
                        // --- Floating-Opacity ---
                        Oran7NumSettingGroup{
                            value:Oran7Theme.Oran7MusicLyricsIsland.floatingOpacity
                            title: qsTranslate("Oran7Ui", "不透明度")
                            property string componentName: "Oran7MusicLyricsIsland"
                            property string tokenName: "floatingOpacity"
                            sliderValueFrom: 0.0
                            sliderValueTo: 1.0
                            stepSize: 0.05
                            valueDecimals: 2
                            onCommitted: (value, thresholdPosition, ratio) =>{
                                Oran7Theme.saveComponentToken(componentName,tokenName,value)
                            }
                        }
                    }
                }
                //<--- title item end hear --->
                // ~~~~~ NeteasePlayerSetting ~~~~~
                Oran7SettingItem{
                    id:neteasePlayerSetting
                    objectName: "neteaseRoamSettingsGroup"
                    index: 3
                    text:qsTranslate("Oran7Ui", "网易云播放设置")
                    fontBold:true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand:false
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: neteasePlayerSetting.expand
                    Column{
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "漫游过滤已喜欢歌曲")
                            showTag: false
                            Oran7SwitchToggleItem {
                                id: roamFilterLikedToggle
                                objectName: "roamFilterLikedSwitch"
                                checked: Client.musicQueue.roamFilterLiked
                                onSwitchToggleChanged: function (checked) {
                                    Client.musicQueue.roamFilterLiked = checked
                                }
                                Connections {
                                    target: Client.musicQueue
                                    function onRoamFilterLikedChanged() {
                                        roamFilterLikedToggle.checked = Client.musicQueue.roamFilterLiked
                                    }
                                }
                            }
                        }
                    }
                }
                // ~~~~~ AudioEffects ~~~~~
                Oran7SettingItem{
                    id: audioEffectsSetting
                    objectName: "audioEffectsSettingsGroup"
                    index: 4
                    text: qsTranslate("Oran7Ui", "音效均衡器")
                    fontBold: true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand: false
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: audioEffectsSetting.expand
                    Column{
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "启用音效")
                            showTag: false
                            Oran7SwitchToggleItem {
                                id: audioEffectsToggle
                                objectName: "audioEffectsEnabledSwitch"
                                checked: Client.audioEffects.enabled
                                onSwitchToggleChanged: function (checked) {
                                    Client.audioEffects.enabled = checked
                                }
                                Connections {
                                    target: Client.audioEffects
                                    function onEnabledChanged() {
                                        audioEffectsToggle.checked = Client.audioEffects.enabled
                                    }
                                }
                            }
                        }

                        Oran7OptionSettingGroup {
                            objectName: "audioEffectsPresetGroup"
                            title: qsTranslate("Oran7Ui", "预设")
                            options: {
                                const list = Client.audioEffects.presets.map(function (preset) {
                                    return { label: preset.label, enabled: true }
                                })
                                if (Client.audioEffects.presetId === "custom")
                                    list.push({ label: qsTranslate("Oran7Ui", "自定义"), enabled: false })
                                return list
                            }
                            currentIndex: {
                                if (Client.audioEffects.presetId === "custom")
                                    return Client.audioEffects.presets.length
                                const list = Client.audioEffects.presets
                                for (let i = 0; i < list.length; ++i)
                                    if (list[i].id === Client.audioEffects.presetId) return i
                                return -1
                            }
                            onOptionSelected: function (index) {
                                Client.audioEffects.applyPreset(Client.audioEffects.presets[index].id)
                            }
                        }

                        Column {
                            width: parent.width; spacing: 5; topPadding: 7; bottomPadding: 8
                            Label { text: qsTranslate("Oran7Ui", "10 段均衡器"); color: Oran7MainUiSetting.textColor; font.pixelSize: 12 }
                            Oran7PreferenceAction {
                                width: parent.width
                                text: qsTranslate("Oran7Ui", "打开面板")
                                onClicked: root.openEqualizer()
                            }
                        }

                        Oran7NumSettingGroup {
                            objectName: "audioEffectsBassGroup"
                            title: qsTranslate("Oran7Ui", "低音增强 (dB)")
                            value: Client.audioEffects.bassBoostDb
                            sliderValueFrom: 0
                            sliderValueTo: 12
                            stepSize: 0.5
                            valueDecimals: 1
                            onMoved: function (value) { Client.audioEffects.bassBoostDb = value }
                            onCommitted: function (value) { Client.audioEffects.bassBoostDb = value }
                        }
                        Oran7NumSettingGroup {
                            objectName: "audioEffectsHighPassGroup"
                            title: qsTranslate("Oran7Ui", "高通滤波 (Hz)")
                            value: Client.audioEffects.highPassHz
                            sliderValueFrom: 20
                            sliderValueTo: 200
                            stepSize: 5
                            valueDecimals: 0
                            onMoved: function (value) { Client.audioEffects.highPassHz = value }
                            onCommitted: function (value) { Client.audioEffects.highPassHz = value }
                        }
                        Oran7NumSettingGroup {
                            objectName: "audioEffectsLowPassGroup"
                            title: qsTranslate("Oran7Ui", "低通滤波 (Hz)")
                            value: Client.audioEffects.lowPassHz
                            sliderValueFrom: 8000
                            sliderValueTo: 20000
                            stepSize: 100
                            valueDecimals: 0
                            onMoved: function (value) { Client.audioEffects.lowPassHz = value }
                            onCommitted: function (value) { Client.audioEffects.lowPassHz = value }
                        }
                        Oran7NumSettingGroup {
                            objectName: "audioEffectsWidthGroup"
                            title: qsTranslate("Oran7Ui", "立体声宽度")
                            value: Client.audioEffects.stereoWidth
                            sliderValueFrom: 0
                            sliderValueTo: 2
                            stepSize: 0.05
                            valueDecimals: 2
                            onMoved: function (value) { Client.audioEffects.stereoWidth = value }
                            onCommitted: function (value) { Client.audioEffects.stereoWidth = value }
                        }
                        Oran7OptionSettingGroup {
                            objectName: "audioEffectsCrossfeedGroup"
                            title: qsTranslate("Oran7Ui", "交叉馈送")
                            options: [
                                { label: qsTranslate("Oran7Ui", "关闭"), enabled: true },
                                { label: qsTranslate("Oran7Ui", "轻"), enabled: true },
                                { label: qsTranslate("Oran7Ui", "标准"), enabled: true },
                                { label: qsTranslate("Oran7Ui", "强"), enabled: true }
                            ]
                            currentIndex: Client.audioEffects.crossfeed
                            onOptionSelected: function (index) {
                                Client.audioEffects.crossfeed = index
                            }
                        }
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "压缩器")
                            showTag: false
                            Oran7SwitchToggleItem {
                                id: audioEffectsCompressorToggle
                                objectName: "audioEffectsCompressorSwitch"
                                checked: Client.audioEffects.compressorEnabled
                                onSwitchToggleChanged: function (checked) {
                                    Client.audioEffects.compressorEnabled = checked
                                }
                                Connections {
                                    target: Client.audioEffects
                                    function onCompressorEnabledChanged() {
                                        audioEffectsCompressorToggle.checked = Client.audioEffects.compressorEnabled
                                    }
                                }
                            }
                        }
                        Oran7NumSettingGroup {
                            objectName: "audioEffectsThresholdGroup"
                            title: qsTranslate("Oran7Ui", "压缩阈值 (dB)")
                            value: Client.audioEffects.compressorThresholdDb
                            sliderValueFrom: -60
                            sliderValueTo: 0
                            stepSize: 1
                            valueDecimals: 0
                            enabled: Client.audioEffects.compressorEnabled
                            onMoved: function (value) { Client.audioEffects.compressorThresholdDb = value }
                            onCommitted: function (value) { Client.audioEffects.compressorThresholdDb = value }
                        }
                        Oran7NumSettingGroup {
                            objectName: "audioEffectsRatioGroup"
                            title: qsTranslate("Oran7Ui", "压缩比率")
                            value: Client.audioEffects.compressorRatio
                            sliderValueFrom: 1
                            sliderValueTo: 20
                            stepSize: 0.5
                            valueDecimals: 1
                            enabled: Client.audioEffects.compressorEnabled
                            onMoved: function (value) { Client.audioEffects.compressorRatio = value }
                            onCommitted: function (value) { Client.audioEffects.compressorRatio = value }
                        }
                        Oran7NumSettingGroup {
                            objectName: "audioEffectsMakeupGroup"
                            title: qsTranslate("Oran7Ui", "压缩补偿 (dB)")
                            value: Client.audioEffects.compressorMakeupDb
                            sliderValueFrom: -12
                            sliderValueTo: 12
                            stepSize: 0.5
                            valueDecimals: 1
                            enabled: Client.audioEffects.compressorEnabled
                            onMoved: function (value) { Client.audioEffects.compressorMakeupDb = value }
                            onCommitted: function (value) { Client.audioEffects.compressorMakeupDb = value }
                        }
                        Oran7NumSettingGroup {
                            objectName: "audioEffectsPreampGroup"
                            title: qsTranslate("Oran7Ui", "前级增益 (dB)")
                            value: Client.audioEffects.preampDb
                            sliderValueFrom: -24
                            sliderValueTo: 24
                            stepSize: 0.5
                            valueDecimals: 1
                            onMoved: function (value) { Client.audioEffects.preampDb = value }
                            onCommitted: function (value) { Client.audioEffects.preampDb = value }
                        }
                        Oran7NumSettingGroup {
                            objectName: "audioEffectsOutputGroup"
                            title: qsTranslate("Oran7Ui", "输出增益 (dB)")
                            value: Client.audioEffects.outputGainDb
                            sliderValueFrom: -24
                            sliderValueTo: 24
                            stepSize: 0.5
                            valueDecimals: 1
                            onMoved: function (value) { Client.audioEffects.outputGainDb = value }
                            onCommitted: function (value) { Client.audioEffects.outputGainDb = value }
                        }
                        Oran7NumSettingGroup {
                            objectName: "audioEffectsCeilingGroup"
                            title: qsTranslate("Oran7Ui", "限幅上限 (dB)")
                            value: Client.audioEffects.limiterCeilingDb
                            sliderValueFrom: -24
                            sliderValueTo: 0
                            stepSize: 0.5
                            valueDecimals: 1
                            onMoved: function (value) { Client.audioEffects.limiterCeilingDb = value }
                            onCommitted: function (value) { Client.audioEffects.limiterCeilingDb = value }
                        }
                        Oran7SettingItem {
                            objectName: "audioEffectsReset"
                            text: qsTranslate("Oran7Ui", "重置为平直")
                            showTag: false
                            enableMouseArea: true
                            enableHoverHandler: true
                            onLeftClicked: Client.audioEffects.reset()
                        }
                    }
                }
            }

            Binding {
                target: root
                property: "savedNormalHeight"
                value: Math.max(contene_column.implicitHeight + topDragRect.height + 50, 200)
                when: root.visible // 只在窗口可见时更新
            }
            //<--- ui content ends here --->
        }
    }

    // 拖动区域
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
