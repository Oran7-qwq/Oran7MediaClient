import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects

import "../GlobalSettings"
import "../../Components/Oran7GlobalComponents"
import "../../Components/Oran7SettingUiWindowItems"

import Client 1.0
import Oran7UI.Impl

Oran7SettingsPanel {
    id: root
    width: Oran7Theme.Oran7MainGUI.settingWinItemDefalutWidth
    x: root.savedNormalX

    property int  winIndex : 1

    property real savedNormalX: 80 + Oran7Theme.Oran7MainGUI.settingWinItemDefalutWidth * root.winIndex
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
            Behavior on color{PropertyAnimation{duration: Oran7Theme.Primary.durationMid}}
            radius: 10
            clip: true
            opacity: 1

            Oran7SetingTitleItem{
                id:topDragRect
                title:qsTranslate("Oran7Ui", "视频播放器设置")
                anchors.top: parent.top
                anchors.margins: 2
            }

            //<--- ui content goes here --->
            Column{
                id:contene_column
                anchors.top: topDragRect.bottom
                anchors.topMargin: 8
                anchors.left: parent.left
                anchors.leftMargin: 1
                anchors.right: parent.right
                spacing: 2

                // ~~~ VideoPlayerNormal ~~~
                Oran7SettingItem{
                    id: videoPlayerSetting
                    index: 0
                    text:qsTranslate("Oran7Ui", "视频播放")
                    fontBold:true
                    gradientMaskEnabled: true

                    enableMouseArea: true
                    property bool expand:false
                    onRightClicked: expand = !expand
                    onLeftClicked: expand = !expand

                    enableHoverHandler: true
                }
                Oran7ExpandItem{
                    expand: videoPlayerSetting.expand
                    Column{
                        // --- OpenVideoInfo ---
                        Oran7SettingItem {
                            text: qsTranslate("Oran7Ui", "显示视频信息")
                            Oran7SwitchToggleItem {
                                checked: Oran7MainUiSetting.openVideoInfo
                                onSwitchToggleChanged: function (checked) {
                                    Oran7MainUiSetting.openVideoInfo = checked
                                }
                            }
                        }
                        // --- ScaleMode ---
                        Oran7SettingItem {
                            text: String("ScaleMode：" + String(checked  ? "Fill" : "Fit"))
                            readonly property bool checked: Oran7Theme.Oran7VideoPlayer.ScaleMode === "Fill"
                            Oran7SwitchToggleItem {
                                checked: parent.checked
                                onSwitchToggleChanged: function (checked) {
                                    Oran7Theme.saveComponentToken("Oran7VideoPlayer","ScaleMode",checked  ? "Fill" : "Fit")
                                }
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
