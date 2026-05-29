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
        root.height = 0;
        root.visible = true;
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
            radius: 10
            clip: true
            opacity: 1

            Oran7SetingTitleItem{
                id:topDragRect
                title:"MusicPlayListSettings"
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
                // --- textColor ---
                Oran7ColorSettingGroup{
                    title:"ListViewTextColor:"
                    checkedColor: Oran7Theme.Oran7MusicPlaylistView[colorToken +"-6"]
                    componentName: "Oran7MusicPlaylistView"
                    colorToken:"textColorBase"
                    onEnterOfTextFiled: function(text){
                        Oran7Theme.saveComponentToken(componentName,colorToken,`$genColor(${seletedColor})`)
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

            //<--- ui content ends here --->
        }
    }

    // 拖动区域
    MouseArea {
        anchors.fill: parent
        onPressed: function(mouse) {
            root.mouseIsPressed = true;
            root.clickPos = Qt.point(mouse.x, mouse.y);

            let inTitleBar = mouse.y <= topDragRect.height + 16 && mouse.y >= 16;

            if (inTitleBar) {
                mouse.accepted = true;
            } else {
                mouse.accepted = false;
                let inMargin = mouse.x < 16 || mouse.x > root.width - 16
                            || mouse.y < 16 || mouse.y > root.height - 16;
                if (inMargin) {
                    Oran7MainUiSetting.clickedOutSide();
                }
            }
        }
        onReleased: function(mouse) {
            root.mouseIsPressed = false;
            root.savedNormalX = root.x
            root.savedNormalY = root.y
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
            mouse.accepted = false;
        }
    }
}
