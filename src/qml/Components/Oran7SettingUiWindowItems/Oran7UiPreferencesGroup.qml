import QtQuick
import QtQuick.Controls
import Oran7UI.Impl
import "../../Settings/GlobalSettings"

Column {
    id: root
    anchors.left: parent.left
    anchors.right: parent.right
    spacing: 2
    property var preferences: null
    property bool headingGradientEnabled: true
    property alias expanded: heading.expand
    property var activeChoice: null
    readonly property bool popupOpen: activeChoice !== null
    function closePopup() { if (activeChoice) activeChoice.popup.close() }
    onExpandedChanged: if (!expanded) closePopup()

    Oran7SettingItem {
        id: heading
        objectName: "uiPreferencesHeading"
        index: 3
        text: qsTranslate("Oran7Ui", "通用基础设置")
        fontBold: true
        gradientMaskEnabled: root.headingGradientEnabled
        enableMouseArea: true
        enableHoverHandler: true
        property bool expand: false
        onLeftClicked: expand = !expand
        onRightClicked: expand = !expand
    }
    Oran7ExpandItem {
        objectName: "uiPreferencesExpansion"
        expand: heading.expand
        Column {
            spacing: 11
            topPadding: 7
            bottomPadding: 8
            Column {
                width: parent.width; spacing: 5
                Label { text: qsTranslate("Oran7Ui", "界面语言"); color: Oran7MainUiSetting.textColor; font.pixelSize: 12 }
                Oran7PreferenceChoice {
                    id: languageChoice
                    objectName: "uiLanguageChoice"
                    onPopupTrackedChanged: {
                        if (popupTracked) root.activeChoice = languageChoice
                        else if (root.activeChoice === languageChoice) root.activeChoice = null
                    }
                    width: parent.width
                    badge: "文"
                    popupTitle: qsTranslate("Oran7Ui", "界面语言")
                    model: ["中文", "English"]
                    subtitles: [qsTranslate("Oran7Ui", "简体中文"), "English"]
                    enabled: !!root.preferences
                    currentIndex: root.preferences && root.preferences.language === "en_US" ? 1 : 0
                    onActivated: function(index) {
                        root.preferences.selectLanguage(index === 1 ? "en_US" : "zh_CN")
                        currentIndex = Qt.binding(function() { return root.preferences.language === "en_US" ? 1 : 0 })
                    }
                }
            }
            Column {
                width: parent.width; spacing: 5
                Label { text: qsTranslate("Oran7Ui", "字体"); color: Oran7MainUiSetting.textColor; font.pixelSize: 12 }
                Oran7PreferenceChoice {
                    id: fontChoice
                    objectName: "uiFontChoice"
                    onPopupTrackedChanged: {
                        if (popupTracked) root.activeChoice = fontChoice
                        else if (root.activeChoice === fontChoice) root.activeChoice = null
                    }
                    width: parent.width
                    popupTitle: qsTranslate("Oran7Ui", "字体")
                    previewFonts: true
                    model: root.preferences ? root.preferences.fontFamilies : []
                    enabled: !!root.preferences
                    currentIndex: root.preferences ? model.indexOf(root.preferences.fontFamily) : -1
                    onActivated: function(index) {
                        root.preferences.selectFont(textAt(index), root.preferences.fontPixelSize)
                        currentIndex = Qt.binding(function() { return model.indexOf(root.preferences.fontFamily) })
                    }
                }
            }
            Oran7NumSettingGroup {
                id: fontSizeSetting
                objectName: "uiFontSizeChoice"
                title: qsTranslate("Oran7Ui", "基础字号")
                fontPixelSize: 12
                rowHeight: 27
                sliderValueFrom: 10
                sliderValueTo: 24
                stepSize: 1
                valueDecimals: 0
                accentColor: Oran7Theme.Oran7MainGUI.themeColor
                accentHoverColor: Qt.lighter(accentColor, 1.15)
                enabled: !!root.preferences
                value: root.preferences ? root.preferences.fontPixelSize : 16
                onCommitted: function(value, thresholdPosition, ratio) {
                    if (!root.preferences.selectFont(root.preferences.fontFamily, Math.round(value)))
                        Qt.callLater(function() { fontSizeSetting.resetValueDisplay() })
                }
            }
            Oran7SettingItem {
                objectName: "developerModeSetting"
                text: qsTranslate("Oran7Ui", "开发者模式")
                fontPixelSize: 12
                rowHeight: 27
                enabled: !!root.preferences
                Oran7SwitchToggleItem {
                    id: developerSwitch
                    objectName: "developerModeSwitch"
                    checked: root.preferences ? root.preferences.developerMode : false
                    onSwitchToggleChanged: function(checked) {
                        if (root.preferences) root.preferences.selectDeveloperMode(checked)
                        developerSwitch.checked = Qt.binding(function() {
                            return root.preferences ? root.preferences.developerMode : false
                        })
                    }
                }
            }
            Label {
                width: parent.width
                visible: !!root.preferences && root.preferences.lastError.length > 0
                text: root.preferences ? qsTranslate("Oran7Ui", root.preferences.lastError) : ""
                wrapMode: Text.Wrap; color: "#e36a75"; font.pixelSize: 11
            }
        }
    }
}
