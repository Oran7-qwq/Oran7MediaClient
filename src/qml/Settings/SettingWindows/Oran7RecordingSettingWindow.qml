import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

import "../GlobalSettings"
import "../../Components/Oran7SettingUiWindowItems"
import Oran7UI.Impl

// 直播工作台的独立设置面板；后续皮肤控件在此扩展。
// 开关与高度进度仍统一交给设置容器，不能自行隐藏原生窗口。
Oran7SettingsPanel {
    id: root
    objectName: "recordingSettingsPanel"
    property int winIndex: 3
    property real savedNormalX: 80 + Oran7Theme.Oran7MainGUI.settingWinItemDefalutWidth * winIndex
    width: Oran7Theme.Oran7MainGUI.settingWinItemDefalutWidth
    x: savedNormalX
    savedNormalY: 40
    savedNormalHeight: 200
    animationDuration: Oran7MainUiSetting.toggleOpenAniDuration
    property var activeSkinChoice: null
    function closeSkinPopup() { if (activeSkinChoice) activeSkinChoice.popup.close() }
    onVisibleChanged: if (!visible) closeSkinPopup()

    DropShadow {
        anchors.fill: settingsRoot
        source: settingsRoot
        horizontalOffset: 0
        verticalOffset: 5
        radius: 12
        samples: 16
        spread: 0.7
        color: Oran7MainUiSetting.winShadowColor
        transparentBorder: true
        z: -1
    }

    Rectangle {
        id: settingsRoot
        anchors.fill: parent
        color: "transparent"
        radius: 10
        Rectangle {
            anchors.fill: parent; anchors.margins: 16
            radius: 10; clip: true
            color: Oran7MainUiSetting.backColor
            Behavior on color { ColorAnimation { duration: Oran7Theme.Primary.durationMid } }
            Oran7SetingTitleItem {
                id: heading
                objectName: "recordingSettingsTitle"
                anchors.top: parent.top; anchors.margins: 2
                title: qsTranslate("Oran7Ui", "直播工作台")
            }
            Column {
                id: settingsColumn
                anchors.top: heading.bottom; anchors.topMargin: 8
                anchors.left: parent.left; anchors.right: parent.right
                anchors.leftMargin: 1
                spacing: 2
                Oran7SettingItem {
                    id: themeHeading
                    objectName: "recordingThemeHeading"
                    index: 0
                    text: qsTranslate("Oran7Ui", "直播工作台主题")
                    fontBold: true
                    gradientMaskEnabled: true
                    enableMouseArea: true
                    enableHoverHandler: true
                    property bool expand: false
                    onExpandChanged: if (!expand) root.closeSkinPopup()
                    onLeftClicked: expand = !expand
                    onRightClicked: expand = !expand
                }
                Oran7ExpandItem {
                    objectName: "recordingThemeExpansion"
                    expand: themeHeading.expand
                    Column {
                        spacing: 7; topPadding: 7; bottomPadding: 10
                        Label {
                            text: qsTranslate("Oran7Ui", "皮肤")
                            color: Oran7MainUiSetting.textColor
                            font.family: Oran7MainUiSetting.fontFamily
                            font.pixelSize: Oran7MainUiSetting.textPixelSize
                        }
                        Oran7PreferenceChoice {
                            id: skinChoice
                            objectName: "recordingSkinChoice"
                            width: parent.width
                            badge: "01"
                            popupTitle: qsTranslate("Oran7Ui", "皮肤")
                            readonly property var skinIds: ["default", "violet"]
                            model: [qsTranslate("Oran7Ui", "默认"), qsTranslate("Oran7Ui", "雪松幽境")]
                            subtitles: [qsTranslate("Oran7Ui", "保留当前工作台外观"), qsTranslate("Oran7Ui", "雪林背景 · 冰蓝磨砂玻璃")]
                            currentIndex: Math.max(0, skinIds.indexOf(Oran7Theme.Oran7ScreenCapture.workspaceSkin))
                            onActivated: function(index) {
                                if (index < 0 || index >= skinIds.length) return
                                Oran7Theme.saveComponentToken("Oran7ScreenCapture", "workspaceSkin", skinIds[index])
                                currentIndex = Qt.binding(function() {
                                    return Math.max(0, skinIds.indexOf(Oran7Theme.Oran7ScreenCapture.workspaceSkin))
                                })
                            }
                            onPopupTrackedChanged: {
                                if (popupTracked) root.activeSkinChoice = skinChoice
                                else if (root.activeSkinChoice === skinChoice) root.activeSkinChoice = null
                            }
                        }
                    }
                }
            }
            Binding {
                target: root; property: "savedNormalHeight"
                value: Math.max(settingsColumn.implicitHeight + heading.height + 50, 200)
                when: root.visible
            }
        }
    }
    MouseArea {
        anchors.left: parent.left; anchors.right: parent.right
        y: 16; height: heading.height
        property point pressPosition: Qt.point(0, 0)
        onPressed: function(mouse) { pressPosition = Qt.point(mouse.x, mouse.y) }
        onPositionChanged: function(mouse) {
            if (!pressed) return
            root.x += mouse.x - pressPosition.x
            root.y += mouse.y - pressPosition.y
        }
        onReleased: {
            root.savedNormalX = root.x
            root.savedNormalY = root.y
        }
        onCanceled: {
            root.savedNormalX = root.x
            root.savedNormalY = root.y
        }
    }
}
