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

    property int  winIndex : 3

    property real savedNormalX: 80 + Oran7Theme.Oran7MainGUI.settingWinItemDefalutWidth * root.winIndex
    property real savedNormalY: 40
    property real savedNormalHeight: 600

    property point clickPos: Qt.point(0, 0)
    property bool mouseIsPressed: false
    property bool isAnimating: false

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

    ParallelAnimation {
        id: window_openAnimation
        property real aniDuration: Oran7MainUiSetting.toggleOpenAniDuration

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

        onFinished: {
            root.isAnimating = false;
            root.height = Qt.binding(function() { return root.savedNormalHeight; });
            Oran7MainUiSetting.settingWindow_isOpening_Or_isClosing = false
        }
    }

    ParallelAnimation {
        id: window_closeAnimation
        property real aniDuration: Oran7MainUiSetting.toggleOpenAniDuration

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

        onFinished: {
            root.visible = false;
            root.opacity = 0
            root.y = 0;
            root.height = 0;
            root.isAnimating = false;

            //the last index of settingWindow
            Oran7MainUiSetting.settingWindow_isOpening_Or_isClosing = false
        }
    }

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
                title:"MusicPlayerSettings"
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
                    index: 0
                    text:"MusicListViewSettings"
                    fontBold:true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand:true
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: musicListViewSettings.expand
                    Column{
                        // --- textColor ---
                        Oran7ColorSettingGroup{
                            title:"ListViewTextColor:"
                            checkedColor: Oran7Theme.Oran7MusicPlaylistView[colorToken +"-6"]
                            componentName: "Oran7MusicPlaylistView"
                            colorToken:"textColorBase"
                            onEnterOfTextFiled: function(text){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${text})`)
                            }
                            onColorReady: function(seletedColor){
                                Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${seletedColor})`)
                            }
                        }

                        // --- textFontPixelSize ---
                        Oran7NumSettingGroup{
                            value:Oran7Theme.Oran7MusicPlaylistView.listViewFontPixelSize
                            title: "ListViewTextPixelSize"
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
                    text:"MusicPlayControlsBar"
                    fontBold:true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand:true
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: musicPlayControlsBar.expand
                    Column{
                        // --- textColor ---
                        Oran7ColorSettingGroup{
                            title:"textColor:"
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
                            title:"iconColor:"
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
                            title: "ProgressColor-index:"
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
                            text: "DynamicGradient:"
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
                        Oran7SettingItem{text:"Background：";fontBold:true;showTag:false}
                        // --- Oran7MusicPlayControls-BlurEnabled ---
                        Oran7SettingItem {
                            text: "BlurEnabled:"
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
                            title: "BlurEffect-Saturation:"
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
                            title: "BlurEffect-Brightness:"
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
                            title: "BlurEffect-Contrast:"
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
                    text:"MusicLyricsWindow"
                    fontBold:true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand:true
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: musicLyricsWindow.expand
                    Column{
                        //<<---Lyrics
                        Oran7SettingItem{text:"Lyrics：";fontBold:true;showTag:false}
                        // --- LyricsWin_Lyrics-TextColor ---
                        Oran7ColorSettingGroup{
                            title:"Lyrics-TextColor:"
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
                            title: "Lyrics-TextSize:"
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
                        Oran7SettingItem{text:"Background：";fontBold: true;showTag: false}
                        // --- LyricsWin_Background-BlurEnabled ---
                        Oran7SettingItem {
                            text: "BlurEnabled:"
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
                            title: "BlurEffect-Saturation:"
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
                            title: "BlurEffect-Brightness:"
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
                            title: "BlurEffect-Contrast:"
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
