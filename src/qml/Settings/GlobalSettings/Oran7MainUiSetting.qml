pragma Singleton
import QtQml 2.15
import QtQuick 2.15

import Oran7UI.Impl

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

    // ================= SettingUi_Window_Property ================
    property real toggleOpenAniDuration: 300
    property bool settingWindow_isOpening_Or_isClosing: false

    property real itemHeight: 30
    property real textPixelSize: 16
    property string fontFamily : "微软雅黑"
    property bool isDarkMode: true

    readonly property color backColor: isDarkMode ? "#030303" : "#ffffff"
    readonly property color itemBackColor: isDarkMode ? "#242020" : "#d1d1d1"
    readonly property color tagColor: textColor
    readonly property color textColor: isDarkMode ? "#dadada" : "#5c5c5c"
    readonly property color winShadowColor: isDarkMode ? "#80000000" : "#20000000"
    property color themeColor: /*"#8cffff"*//*"#fc3c55"*/"#beb7ff"

    //--- statue manager ---
    //过滤内层控件聚焦状态
    readonly property bool __SurelySetingWinIsFocus__: !isOtherItemActive && settingWinActive
    readonly property bool settingWinActive : Oran7Theme.Oran7MainGUI.OpenSettingWin
    readonly property bool isOtherItemActive:activeOtherItemCount > 0
    /*引用计数机制
    * Contain of : Oran7TextField ; Oran7ColorPickWindow
    */
    property int activeOtherItemCount: 0

    // --- Oran7TextField InputText Auto DetectionType ENUM ---
    enum DetectionType {
        NoDetection = 0,
        FileDetection = 1,
        ColorDetection = 2
    }

    signal callOpenSettingWindow()//out-api
    signal triggleOpen_Oran7MainUiSetting_window //inner-api
    signal clickedOutSide()
}
