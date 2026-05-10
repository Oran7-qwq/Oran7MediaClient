pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects
import QtQuick.Effects
import QtMultimedia

import "../GlobalSettings"
import "../PagesSettings"
import "../../Components"

import "../../Components/Oran7SettingUiWindowItems"

ApplicationWindow {
    id: root
    visible: Oran7MainUiSetting.settingWin_isOpen
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    width: Screen.width
    height: Screen.height - root.taskbarHeight // 动态减去任务栏高度
    color: "transparent" // 全透明背景

    // 当设置窗口需要恢复主窗口焦点时发出
    signal restoreMainWindowFocusRequested

    property real taskbarHeight: 50

    Component.onCompleted: {
        detectTaskbarHeight();
        groupChangeTimer.start();
        textureAnimationTimer.start();
    }

    onScreenChanged: {
        detectTaskbarHeight();
    }

    function detectTaskbarHeight() {
        root.taskbarHeight = 50;
    }

    onVisibleChanged: {
        if (!visible)
            restoreMainWindowFocus()
    }

    Item {
        id: content
        anchors.fill: parent

        // 动态渐变背景 - 全屏对角线流动渐变<---待完善//2026/5/8
        Rectangle {
            visible: false

            id: gradientBackground
            anchors.fill: parent
            z: -1
            opacity: Oran7MainUiSetting.settingContent_visiable ?  0.3 : 0

            // 7组对角线渐变配置
            property var gradientGroups: [
                {
                    name: "渐变组1 - 紫色系",
                    color1: "#E6E6FA", color2: "#D8BFD8", color3: "#DDA0DD",
                    nextColor1: "#FFE4E1", nextColor2: "#FFC0CB", nextColor3: "#FFB6C1"
                },
                {
                    name: "渐变组2 - 粉色系",
                    color1: "#FFE4E1", color2: "#FFC0CB", color3: "#FFB6C1",
                    nextColor1: "#E0FFFF", nextColor2: "#AFEEEE", nextColor3: "#40E0D0"
                },
                {
                    name: "渐变组3 - 青色系",
                    color1: "#E0FFFF", color2: "#AFEEEE", color3: "#40E0D0",
                    nextColor1: "#FFFACD", nextColor2: "#F0E68C", nextColor3: "#FFD700"
                },
                {
                    name: "渐变组4 - 黄色系",
                    color1: "#FFFACD", color2: "#F0E68C", color3: "#FFD700",
                    nextColor1: "#F0FFF0", nextColor2: "#98FB98", nextColor3: "#90EE90"
                },
                {
                    name: "渐变组5 - 绿色系",
                    color1: "#F0FFF0", color2: "#98FB98", color3: "#90EE90",
                    nextColor1: "#FFA07A", nextColor2: "#FF7F50", nextColor3: "#FF6347"
                },
                {
                    name: "渐变组6 - 橙色系",
                    color1: "#FFA07A", color2: "#FF7F50", color3: "#FF6347",
                    nextColor1: "#ADD8E6", nextColor2: "#87CEEB", nextColor3: "#00BFFF"
                },
                {
                    name: "渐变组7 - 蓝色系",
                    color1: "#ADD8E6", color2: "#87CEEB", color3: "#00BFFF",
                    nextColor1: "#E6E6FA", nextColor2: "#D8BFD8", nextColor3: "#DDA0DD"
                }
            ]

            // 渐变组轮换控制
            property int currentGroupIndex: 0

            // 流动位置（从右上到左下）- 由动画控制
            property real flowPosition

            // 当前颜色值
            property color color1: {
                var group = gradientBackground.gradientGroups[gradientBackground.currentGroupIndex];
                var isNextGroup = Math.floor(gradientBackground.currentGroupIndex / 2) % 2 === 0;
                return isNextGroup ? group.nextColor1 : group.color1;
            }
            property color color2: {
                var group = gradientBackground.gradientGroups[gradientBackground.currentGroupIndex];
                var isNextGroup = Math.floor(gradientBackground.currentGroupIndex / 2) % 2 === 0;
                return isNextGroup ? group.nextColor2 : group.color2;
            }
            property color color3: {
                var group = gradientBackground.gradientGroups[gradientBackground.currentGroupIndex];
                var isNextGroup = Math.floor(gradientBackground.currentGroupIndex / 2) % 2 === 0;
                return isNextGroup ? group.nextColor3 : group.color3;
            }

            // 全屏渐变 - 从右上角到左下角
            gradient: Gradient {
                GradientStop {
                    position: Math.max(0, gradientBackground.flowPosition - 0.3)
                    color: gradientBackground.color1
                }
                GradientStop {
                    position: Math.max(0.1, Math.min(0.9, gradientBackground.flowPosition))
                    color: gradientBackground.color2
                }
                GradientStop {
                    position: Math.min(1, gradientBackground.flowPosition + 0.3)
                    color: gradientBackground.color3
                }
            }

            // 颜色变化动画
            Behavior on color1 {
                ColorAnimation {
                    duration: 1000
                    easing.type: Easing.InOutSine
                }
            }
            Behavior on color2 {
                ColorAnimation {
                    duration: 1000
                    easing.type: Easing.InOutSine
                }
            }
            Behavior on color3 {
                ColorAnimation {
                    duration: 1000
                    easing.type: Easing.InOutSine
                }
            }

            // 流动位置动画 - 从右上往左下
            NumberAnimation on flowPosition {
                from: 0.0
                to: 1.0
                duration: 6000  // 6秒完成一次流动
                loops: Animation.Infinite
                easing.type: Easing.Linear
            }

            // 渐变组轮换动画
            Timer {
                id: groupChangeTimer
                interval: 4500
                repeat: true
                onTriggered: {
                    gradientBackground.currentGroupIndex = (gradientBackground.currentGroupIndex + 1) % gradientBackground.gradientGroups.length;
                }
            }

            // 添加纹理效果
            Canvas {
                id: textureCanvas
                anchors.fill: parent
                visible: true

                onPaint: {
                    const ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    // 添加轻微的噪点纹理
                    for (let x = 0; x < width; x += 4) {
                        for (let y = 0; y < height; y += 4) {
                            if (Math.random() > 0.97) {
                                ctx.fillStyle = "rgba(255, 255, 255, 0.02)";
                                ctx.fillRect(x, y, 2, 2);
                            }
                        }
                    }
                }
            }

            // 纹理动画
            Timer {
                id: textureAnimationTimer
                interval: 100
                repeat: true
                onTriggered: {
                    textureCanvas.requestPaint();
                }
            }

            // 透明度动画
            Behavior on opacity {
                PropertyAnimation {
                    duration: Oran7MainUiSetting.toggleOpenAniDuration
                    easing.type: Easing.OutCubic
                }
            }
        }

        Oran7MainUiSettingWindow {
            id: mainUiSetting
            x:  40
            y: 20
            visible: false

            Behavior on x{NumberAnimation{duration: 50}}
            Behavior on y{NumberAnimation{duration: 50}}
        }

        Oran7MediaPlayerSettingWindow {
            id: mediaPlayerSetting
            x: 292
            y: 20
            visible: false

            Behavior on x{NumberAnimation{duration: 50}}
            Behavior on y{NumberAnimation{duration: 50}}
        }

        Column {
            anchors.bottom: content.bottom
            anchors.left: content.left
            anchors.margins: 20
            spacing: 4
            UseExplainLabel{
                text:"Click->↑→↓←: Move the Setting GUI Position."
            }
            UseExplainLabel {
                text: "ESC: Open or close the setting windows."
            }
            UseExplainLabel{
                text:"LeftClick: Click at blank area of setting window will close all setting windows."
            }
            UseExplainLabel {
                text: "Enter: Ensure the foucsed TextField value is saved and the setting is applied."
            }
            UseExplainLabel{
                text:"LShift+Delete: Clear the content of foucsed TextField."
            }
        }
    }
    component UseExplainLabel: Label {
        font.pixelSize: Oran7MainUiSetting.textPixelSize + 5
        font.family: Oran7MainUiSetting.fontFamily
        font.bold: true
        font.italic: true
        color:Oran7MainUiSetting.themeColor

        opacity: Oran7MainUiSetting.settingContent_visiable  ? 1 : 0
        Behavior on opacity {
            PropertyAnimation{
                duration: Oran7MainUiSetting.toggleOpenAniDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        onClicked: mouse => {
            Oran7MainUiSetting.clickedOutSide();
            Oran7MainUiSetting.callOpenSettingWindow()
            mouse.accepted = false;
        }
    }


    Connections{
        target: Oran7MainUiSetting
        function onCallOpenSettingWindow()
        {
            handle_OpenSettingWin_delayTimer.restart()
        }
    }

    // --- functions ---
    function restoreMainWindowFocus() {
        // 延迟一小段时间确保设置窗口完全关闭后再发出信号
        Qt.callLater(function () {
            root.restoreMainWindowFocusRequested();
        });
    }

    function handle_OpenSettingWin_signal(){//处理打开设置窗口的信号
        Oran7MainUiSetting.triggleOpen_Oran7MainUiSetting_window();
        if (Oran7MainUiSetting.settingWin_isOpen === false) {
            Oran7MainUiSetting.settingWin_isOpen = true;
            delayTimer.delay(100).then(function () {
                openSoundEffect.play();
            });
        }
        else
        {
            delayTimer.delay(Oran7MainUiSetting.toggleOpenAniDuration).then(function () {
                Oran7MainUiSetting.settingWin_isOpen = false;
                Oran7MainUiSetting.clickedOutSide()
                closeSoundEffect.play();
            });
        }
    }

    // 连接到全局键盘Keys事件过滤器
    Connections {
        target: globalEventFilter
        function onEscapeKeyPressed() {handle_OpenSettingWin_delayTimer.restart()}

        // Up key - move GUI upward
        function onUpKeyPressed(){
            handle_upKeyTimer.running = true
        }

        function onUpKeyReleased(){
            handle_upKeyTimer.running = false
        }

        // Down key - move GUI downward
        function onDownKeyPressed(){
            handle_downKeyTimer.running = true
        }

        function onDownKeyReleased(){
            handle_downKeyTimer.running = false
        }

        // Left key - move GUI leftward
        function onLeftKeyPressed(){
            handle_leftKeyTimer.running = true
        }

        function onLeftKeyReleased(){
            handle_leftKeyTimer.running = false
        }

        // Right key - move GUI rightward
        function onRightKeyPressed(){
            handle_rightKeyTimer.running = true
        }

        function onRightKeyReleased(){
            handle_rightKeyTimer.running = false
        }
    }

    // --- tools Component ---
    Oran7DelayTimer {
        id: delayTimer
    }

    Timer{
        id:handle_OpenSettingWin_delayTimer
        interval: 200
        onTriggered: {
            root.handle_OpenSettingWin_signal()
        }
    }

    // Up key timer - moves GUI upward
    Timer{
        id:handle_upKeyTimer
        interval: 50
        repeat: true
        running: false
        onTriggered: {
            mainUiSetting.y -= 10
            mediaPlayerSetting.y -= 10
        }
    }

    // Down key timer - moves GUI downward
    Timer{
        id:handle_downKeyTimer
        interval: 50
        repeat: true
        running: false
        onTriggered: {
            mainUiSetting.y += 10
            mediaPlayerSetting.y += 10
        }
    }

    // Left key timer - moves GUI leftward
    Timer{
        id:handle_leftKeyTimer
        interval: 50
        repeat: true
        running: false
        onTriggered: {
            mainUiSetting.x -= 10
            mediaPlayerSetting.x -= 10
        }
    }

    // Right key timer - moves GUI rightward
    Timer{
        id:handle_rightKeyTimer
        interval: 50
        repeat: true
        running: false
        onTriggered: {
            mainUiSetting.x += 10
            mediaPlayerSetting.x += 10
        }
    }

    // --- open and close sound effect ---
    SoundEffect {
        id: openSoundEffect
        source: "qrc:/sound/OpenSound.wav"
        volume: 0.6
    }
    SoundEffect {
        id: closeSoundEffect
        source: "qrc:/sound/CloseSound.wav"
        volume: 0.6
    }
}
