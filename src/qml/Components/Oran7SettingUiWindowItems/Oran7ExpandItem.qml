import QtQuick
import QtQuick.Controls

import "../../Settings/GlobalSettings"
import "../"

import Oran7UI.Impl

Item {
    id: root
    anchors.right: parent.right
    anchors.left: parent.left
    height: root.expand && contentLoader.item ? _viewHeight : 0
    clip: true
    Behavior on height { NumberAnimation { duration: Oran7Theme.Primary.durationMid; easing.type: Easing.OutCubic } }
    opacity: root.expand ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: Oran7Theme.Primary.durationMid; easing.type: Easing.OutCubic } }
    visible: opacity !== 0

    property bool expand: true

    // 子组件以 Component 形式传入，Loader 延迟实例化
    default property alias contentComponent: contentLoader.sourceComponent
    //default property alias contentData: contentArea.data

    // --- 缓存控制 ---
    property bool cache: true           // true: 展开过一次后内容常驻不销毁
    property bool forceLoaded: false    // preload() 时强制激活
    property bool loadedOnce: false     // 是否曾经加载过

    // 外部调用：预加载内容（创建但不显示）
    function preload() {
        forceLoaded = true;
        loadedOnce = true;
        contentLoader.active = true;
    }

    // 外部调用：释放强制标记，交给 cache/expand 控制
    function releasePreloadFlag() {
        forceLoaded = false;
    }

    onExpandChanged: {
        if (expand) loadedOnce = true;
    }

    readonly property real _contentHeight: contentLoader.item
            ? contentLoader.item.implicitHeight : 0

    readonly property real _maxViewHeight: Oran7MainUiSetting.itemHeight * 12

    readonly property real _viewHeight: Math.min(_contentHeight,_maxViewHeight)

    //--- LeftColumeLine ---
    Rectangle {
        id: line
        x: 4
        width: 2
        height: _contentHeight
        color: Oran7MainUiSetting.textColor
    }


    ScrollView{
        id:scrollView
        anchors.top: root.top
        anchors.left: line.right
        anchors.leftMargin: 6
        anchors.right: root.right
        height: root.height

        readonly property bool atTop: contentItem.atYBeginning
        readonly property bool atBottom: contentItem.atYEnd

        contentHeight:contentLoader.height
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        Loader {
            id: contentLoader
            //不随 expand 反复销毁，展开过一次后常驻
            active: root.expand || root.forceLoaded || (root.cache && root.loadedOnce)
            asynchronous: true
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.rightMargin: 7
            width: scrollView.availableWidth
        }
    }

    //down sign — 浮动指示动画
    Oran7GradientMask{
        id:downSign

        visible: opacity > 0
        opacity: scrollView.atBottom ? 0 : 1
        Behavior on opacity { NumberAnimation { duration: Oran7Theme.Primary.durationVerySlow; easing.type: Easing.Linear } }

        anchors.bottom: root.bottom
        anchors.right: root.right
        gradientMaskEnabled:true
        dynamicGradient:true
        transitionDuration:Oran7Theme.Primary.durationVerySlow
        dynamicInterval: Oran7Theme.Primary.durationVerySlow
        height: Oran7MainUiSetting.itemHeight
        width: height
        z:scrollView.z + 1

        // --- 浮动动画 ---
        Behavior on anchors.bottomMargin {
            NumberAnimation {
                duration: Oran7Theme.Primary.durationVerySlow * 1.5
                easing.type: Easing.Linear
            }
        }

        Label{
            text:"⩔";
            font.pixelSize: 20
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }
    }
    Timer {
        id: floatDownTimer
        interval: Oran7Theme.Primary.durationVerySlow * 1.5
        repeat: true
        running: !scrollView.atBottom
        property bool toggled: false
        onTriggered: {
            downSign.anchors.bottomMargin = toggled ? 0 : 7
            toggled = !toggled
        }
    }

    //up sign — 浮动指示动画
    Oran7GradientMask{
        id:upSign

        visible: opacity > 0
        opacity: scrollView.atTop ? 0 : 1
        Behavior on opacity { NumberAnimation { duration: Oran7Theme.Primary.durationVerySlow; easing.type: Easing.Linear } }

        anchors.top: root.top
        anchors.right: root.right
        gradientMaskEnabled:true
        dynamicGradient:true
        transitionDuration:Oran7Theme.Primary.durationVerySlow
        dynamicInterval: Oran7Theme.Primary.durationVerySlow
        height: Oran7MainUiSetting.itemHeight
        width: height
        z:scrollView.z + 1

        // --- 浮动动画 ---
        Behavior on anchors.topMargin {
            NumberAnimation {
                duration: Oran7Theme.Primary.durationVerySlow * 1.5
                easing.type: Easing.Linear
            }
        }

        Label{
            text:"⩓";
            font.pixelSize: 20
            anchors.right: parent.right
            anchors.top: parent.top
        }
    }
    Timer {
        id: floatUpTimer
        interval: Oran7Theme.Primary.durationVerySlow * 1.5
        repeat: true
        running: !scrollView.atTop
        property bool toggled: false
        onTriggered: {
            upSign.anchors.topMargin = toggled ? 0 : 7
            toggled = !toggled
        }
    }
}
