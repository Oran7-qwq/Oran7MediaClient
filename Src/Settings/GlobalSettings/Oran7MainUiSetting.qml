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

    signal triggleOpen_Oran7MainUiSetting_window()
}
