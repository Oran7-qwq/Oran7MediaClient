import QtQuick
// URL 拦截器替换内容，但相对导入仍以生产面板的逻辑 URL 为基准。
import "../../../../src/qml/Settings/SettingWindows" as Settings
import "../../../../src/qml/Settings/GlobalSettings"
Settings.Oran7SettingsPanel {
    property int winIndex: 0
    property bool preferencesPopupOpen: false
    function closePreferencesPopup() { preferencesPopupOpen = false }
    property real savedNormalX: 30 + winIndex * 210
    x: savedNormalX
    width: 200
    savedNormalHeight: 260 + winIndex * 30
    animationDuration: Oran7MainUiSetting.toggleOpenAniDuration * (1 + winIndex)
    Rectangle { anchors.fill: parent; color: "#334455" }
}
