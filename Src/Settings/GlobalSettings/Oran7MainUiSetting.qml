pragma Singleton
import QtQml 2.15
import QtQuick 2.15

Item {
    id: root

    // === 全局动画窗口状态聚合 ===
    // 当前打开的动画窗口数量（由各 Oran7AnimatedWindow 自动维护）
    property int openAnimatedWindowCount: 0
    // 是否有任意动画窗口处于全屏打开状态
    readonly property bool anyAnimatedWindowOpen: openAnimatedWindowCount > 0

    //实时保存的窗口尺寸size和position
    property real savedNormalWidth: 0
    property real savedNormalHeight: 0
    property real savedNormalX: 0
    property real savedNormalY: 0

    //TitleBarWith
    property real topBarDefaultHeight: 50
    readonly property real window_titleBarWidth: 50

    //MainUi背景图片路径
    property string backgroundImagePath: "file:///C:/Users/funny/QtProject.doc/Oran7MediaClient/image/themBackground1.png"

    signal triggleOpen_Oran7MainUiSetting_window

    //--- SettingUi_WindowProperty ---
    property bool settingWin_isOpen: false
    property real toggleOpenAniDuration: 400

    property real textPixelSize: 16
    property string fontFamily : "微软雅黑"

    property bool isDarkMode: true
    readonly property color backColor: isDarkMode ? "#030303" : "#ffffff"
    readonly property color itemColor: isDarkMode ? "#101110" : "#d1d1d1"
    readonly property color textColor: isDarkMode ? "#dadada" : "#5c5c5c"
    readonly property color winShadowColor: isDarkMode ? "#80000000" : "#20000000"
    property color themeColor: "#8cffff"
}
