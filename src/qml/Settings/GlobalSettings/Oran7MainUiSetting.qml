pragma Singleton
import QtQml 2.15
import QtQuick 2.15

Item {
    id: root

    // =========== MainUi_Window_Property ==========

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

    //TitleBarWidth
    property real topBarDefaultHeight: 50
    readonly property real window_titleBarWidth: 50

    //MainUi背景图片路径
    property string backgroundImagePath: "C:/Users/funny/QtProject.doc/Oran7MediaClient/image/test_background1.jpg"

    //-------------- CaptionBar_Property --------------
    property bool captionBar_is_simpleMode: true

    // ================= SettingUi_Window_Property ================
    property bool settingWin_isOpen: false
    property bool settingContent_visiable: false
    property real toggleOpenAniDuration: 400

    property real itemHeight: 30

    property real textPixelSize: 16
    property string fontFamily : "微软雅黑"

    property bool isDarkMode: true
    readonly property color backColor: isDarkMode ? "#030303" : "#ffffff"
    readonly property color itemBackColor: isDarkMode ? "#101110" : "#d1d1d1"
    readonly property color tagColor: "#FF818E"
    readonly property color textColor: isDarkMode ? "#dadada" : "#5c5c5c"
    readonly property color winShadowColor: isDarkMode ? "#80000000" : "#20000000"
    property color themeColor: /*"#8cffff"*//*"#fc3c55"*/"#beb7ff"

    enum DetectionType {
        NoDetection = 0,
        FileDetection = 1,
        ColorDetection = 2
    }

    signal callOpenSettingWindow()//out-api
    signal triggleOpen_Oran7MainUiSetting_window //inner-api
    signal clickedOutSide()
}
