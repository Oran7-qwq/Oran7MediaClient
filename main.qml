import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Shapes 1.10
import Qt5Compat.GraphicalEffects

import "./src/qml/LeftPage"
import "./src/qml/RightPage"
import "./src/qml/BottomPage"
import "./src/qml/Basic"
import "./src/qml/Components"
import "./src/qml/Settings/SettingWindows"

import Client 1.0
import Oran7UI.Impl 1.0

ApplicationWindow {
    id: mainWindow
    objectName: "__Oran7Window__"
    width: 1180
    height: 680
    visible: true
    color: "transparent"
    title: "Oran7MediaClient"
    minimumWidth: 1180
    minimumHeight: 680

    property alias __Oran7WindowBackGround__: mainWindowBackground

    onWidthChanged: {
        Qt.callLater(function () {
            if (mainWindow.visibility === Window.Windowed)
                Oran7MainUiSetting.savedNormalWidth = mainWindow.width;
        });
    }
    onHeightChanged: {
        Qt.callLater(function () {
            if (mainWindow.visibility === Window.Windowed)
                Oran7MainUiSetting.savedNormalHeight = mainWindow.height;
        });
    }
    onXChanged: {
        Qt.callLater(function () {
            if (mainWindow.visibility === Window.Windowed)
                Oran7MainUiSetting.savedNormalX = mainWindow.x;
        });
    }
    onYChanged: {
        Qt.callLater(function () {
            if (mainWindow.visibility === Window.Windowed)
                Oran7MainUiSetting.savedNormalY = mainWindow.y;
        });
    }
    Component.onCompleted: {
        Oran7MainUiSetting.savedNormalWidth = mainWindow.width;
        Oran7MainUiSetting.savedNormalHeight = mainWindow.height;
        Oran7MainUiSetting.savedNormalX = mainWindow.x;
        Oran7MainUiSetting.savedNormalY = mainWindow.y;

        // 设置全局mainWindow引用
        BasicConfig.mainWindow = mainWindow;
    }

    // 背景图片 - 作为直接子元素，可以被alias引用
    Image {
        id: mainWindowBackground
        anchors.fill: parent
        sourceSize.width: Screen.width * Screen.devicePixelRatio
        sourceSize.height: Screen.height * Screen.devicePixelRatio
        source: filehepler.fileExists("file:///" + Oran7Theme.Oran7MainGUI.backgroundImage) ?
                   "file:///" + Oran7Theme.Oran7MainGUI.backgroundImage : "qrc:/image/defaultBg.jpg"
        //source: ""
        opacity: 0.999

        fillMode: Image.PreserveAspectCrop
        asynchronous: false
        mipmap: false
        smooth: false
        cache: false
        transformOrigin: Item.Center
    }

    // 设置background属性引用mainWindowBackground
    background: mainWindowBackground

    // --- MainWoindow functions ---
    function restoreFocus() {// 恢复主窗口焦点
        mainWindow.requestActivate();//native-api
    }

    // --- MainWoindow 性能监测 ---
    Text {
        id: fpsText
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 5
        z: 99
        color: "black"
        font.pixelSize: 12

        property int frameCount: 0
        property double lastMs: Date.now()
        property var win: null

        Component.onCompleted: {
            win = Window.window;
            console.log("fps win =", win);
        }

        Connections {
            target: fpsText.win
            function onFrameSwapped() {
                fpsText.frameCount++;
            }
        }

        Timer {
            interval: 1000
            repeat: true
            running: true
            onTriggered: {
                var now = Date.now();
                var fps = fpsText.frameCount * 1000 / (now - fpsText.lastMs);
                fpsText.text = "FPS: " + fps.toFixed(0);
                fpsText.frameCount = 0;
                fpsText.lastMs = now;
            }
        }
    }

    //// 使用无边框窗口样式，但保留原生窗口控制按钮<---已弃用，已用QWK::QuickWindowAgent代理
    // flags: Qt.FramelessWindowHint | Qt.Window | Qt.WindowSystemMenuHint | Qt.WindowMinMaxButtonsHint | Qt.WindowCloseButtonHint
    // // --- 无边框窗口处理器 --- //<---Discard 2026/5/9
    // FramelessWindow {
    //     id: framelessWindow
    //     targetWindow: mainWindow
    //     borderWidth: 6
    //     borderHeight: 6
    //     titleBarHeight: Oran7MainUiSetting.window_titleBarWidth

    //     Component.onCompleted: {
    //         setupWindow();
    //     }
    // }

    // // --- 右上角原生窗口控制按钮（覆盖在主UI上） ---
    // Row {
    //     id: nativeWindowControls
    //     width: 140  // 三个按钮的总宽度
    //     height: 36
    //     anchors.top: parent.top
    //     anchors.right: parent.right
    //     anchors.rightMargin: 12  // 增加右边距，给边框拉伸留出足够空间
    //     anchors.topMargin: 12    // 增加上边距，给边框拉伸留出足够空间
    //     spacing: 4  // 按钮之间的间距
    //     z: 999
    //     layoutDirection: Qt.RightToLeft

    //     // 关闭按钮
    //     WindowControlButton {
    //         id: nativeCloseButton
    //         width: 46
    //         height: 32
    //         buttonColor: "transparent"
    //         hoverColor: "#c42b1c"
    //         iconText: "✕"
    //         onClicked: {
    //             mainWindow.close();
    //         }
    //     }

    //     // 最大化/还原按钮
    //     WindowControlButton {
    //         id: nativeMaximizeButton
    //         width: 46
    //         height: 32
    //         buttonColor: "transparent"
    //         hoverColor: Qt.rgba(0.2, 0.2, 0.2, 0.5)
    //         // 为□符号设置更大的字体大小
    //         buttonTextPixelSize: mainWindow.visibility === Window.Maximized ? 15 : 34
    //         iconText: mainWindow.visibility === Window.Maximized ? "❐" : "▢"

    //         onClicked: {
    //             framelessWindow.nativeMaximize();
    //         }
    //     }

    //     // 最小化按钮
    //     WindowControlButton {
    //         id: nativeMinimizeButton
    //         width: 46
    //         height: 32
    //         buttonColor: "transparent"
    //         hoverColor: Qt.rgba(0.2, 0.2, 0.2, 0.5)
    //         iconText: "━"

    //         onClicked: {
    //             framelessWindow.nativeMinimize();
    //         }
    //     }
    // }

    Oran7WindowAgent{
        id:__windowAgent
        Component.onCompleted: {
              // 启用 DWM 模糊（Windows 系统级透明+模糊）
              __windowAgent.setWindowAttribute('dwm-blur', true)
              // 可选：深色模式标题栏
              //__windowAgent.setWindowAttribute('dark-mode', true)
          }
    }

    Oran7CaptionBar{
        id: __captionBar
        z: 65535
        width: parent.width
        height: Oran7Theme.Oran7MainGUI.topBarDefaultHeight
        anchors.top: parent.top
        targetWindow: mainWindow
        windowAgent:__windowAgent
        showWinTitle:false
        showWinIcon:false
    }

    //====================== <Main ui> ========================//
    // --- MainUi Rectangle Container ---
    Rectangle {
        id: mainRectangle
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        radius: 20
        color: "transparent"
        clip: false
        //窗体拖拽功能
        MouseArea {
            id: mainRectangleMouseArea
            anchors.fill: parent
            propagateComposedEvents: true

            property point clickPos: Qt.point(0, 0)
            property bool mouseIsPressed: false

            // 检查是否在右上角的原生控制按钮区域
            function isInControlButtonArea(x, y) {
                var controlButtonWidth = 140; // Row的宽度
                var controlButtonHeight = 36;  // Row的高度
                var margin = 12;             // 边距
                var windowWidth = mainRectangle.width;
                return (x >= windowWidth - controlButtonWidth - margin && y <= controlButtonHeight + margin);
            }

            onPressed: mouse => {
                // 如果在控制按钮区域，不处理拖拽
                if (isInControlButtonArea(mouse.x, mouse.y)) {
                    return;
                }

                clickPos = Qt.point(mouse.x, mouse.y);
                // 调整拖拽区域高度，为原生控制按钮留出空间
                if (clickPos.y > 50)
                    return;
                mainRectangleMouseArea.mouseIsPressed = true;
                BasicConfig.runningInfinitePropertyAnimation = false;
            // oran7IconRectangle.layer.enabled = false
            //leftRectangle.blurEnabled=false
            }
            onReleased: {
                BasicConfig.runningInfinitePropertyAnimation = true;
                mainRectangleMouseArea.mouseIsPressed = false;
                // 延迟恢复效果，避免卡顿
                delayRestoreTimer.restart();
            }

            onPositionChanged: mouse => {
                mouse.accepted = false;
                //<---Discard 使用Windows原生窗体drag
                // if(mainRectangleMouseArea.mouseIsPressed === false)return;
                // if(mouse.y >= 80)return;
                // let delta = Qt.point(mouse.x-clickPos.x,mouse.y-clickPos.y)
                // mainWindow.x+=delta.x
                // mainWindow.y+=delta.y
            }

            onClicked: {
                BasicConfig.clickedOutside();
            }
        }
        // 延迟恢复效果
        Timer {
            id: delayRestoreTimer
            interval: 300
            onTriggered: {
                BasicConfig.runningInfinitePropertyAnimation = true;
                oran7IconRectangle.layer.enabled = true;
                //leftRectangle.blurEnabled = true
            }
        }

        // -- 主体右部 --
        RightPage {
            id: rightRectangle

            anchors.left: leftRectangle.right
            anchors.leftMargin: openCaptionBtn.openIngState ? 7 : 0
            anchors.top: parent.top
            anchors.bottom: bottomRectangle.top
            //anchors.bottomMargin: openCaptionBtn.openIngState ? 7 : 0
            anchors.right: parent.right

            property bool isTransparent: true
            color: isTransparent ? "transparent" : "#f8c7c7"

            topBarHeight: openCaptionBtn.openIngState ? Oran7Theme.Oran7MainGUI.topBarDefaultHeight : 0
        }

        // -- 主体底部 --
        Oran7BlurCard{
            id:bottomRectangle
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: openCaptionBtn.openIngState ? 7 : 0

            borderRadius:17

            Behavior on height {
                enabled: true
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.OutCubic
                }
            }
            z: leftRectangle.z > rightRectangle.z ? leftRectangle.z + 1 : rightRectangle.z + 1

            //initilization
            height: 0
            visible: true
            themeColor: "#04FFFFFF"
            blurSource: mainWindowBackground
            blurEnabled: true
            borderWidth: 1
            property real visibleOpacity: 0.0

            BottomPage {
                id: bottomPage
                anchors.fill: parent
                //color: "#2a1a22"
                color: "transparent"
                //initilization
                visible: bottomRectangle.visible
                visibleOpacity: bottomRectangle.visibleOpacity
            }
        }

        // -- 主体左部 --
        Oran7BlurCard {
            id: leftRectangle
            width: !openIngState ? 0 :
                              simpleMode ? leftPage.simpleModeWidth : leftPage.defaultWidth
            property alias simpleMode: leftPage.simpleMode
            readonly property bool openIngState: openCaptionBtn.openIngState

            Behavior on width {
                NumberAnimation {
                    duration: 200
                    easing.type: Easing.OutCubic
                }
            }

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: openIngState ? 7 : 0
            anchors.bottom: bottomRectangle.top

            blurSource: mainWindowBackground
            blurEnabled: true
            borderRadius:17
            borderWidth: 1
            themeColor: "#04FFFFFF"
            LeftPage {
                id: leftPage
                simpleMode: Oran7Theme.Oran7CaptionBar.isSimpleMode
                anchors.fill: parent
                visible: leftRectangle.openIngState
                color: "transparent"
            }
        }
        // -- 左部导航栏->打开关闭button --
        Image {
            id: openCaptionBtn

            sourceSize.height: Oran7Theme.Oran7MainGUI.topBarDefaultHeight * 1
            sourceSize.width: Oran7Theme.Oran7MainGUI.topBarDefaultHeight * 1
            anchors.left: leftRectangle.right
            anchors.leftMargin: 0
            anchors.top: leftRectangle.top
            anchors.topMargin: Oran7Theme.Oran7MainGUI.topBarDefaultHeight * 0.1
            z: leftRectangle.z + 1

            source: openCaptionBtn.openIngState ? "qrc:/image/sitQiubi.png" : "qrc:/image/hideQiubi.png"

            property bool openIngState: true

            asynchronous: false
            mipmap: true
            cache: true
            antialiasing: true

            Connections {
                target: BasicConfig
                function onClearAllUi_inVIdeoRenderArea(ok) {
                    if (ok === false)
                        openCaptionBtn.visible = true;
                    else if (openCaptionBtn.openIngState !== true)
                        openCaptionBtn.visible = false;
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (openCaptionBtn.openIngState === false) {
                        openCaptionBtn.openIngState = true;
                    } else {
                        openCaptionBtn.openIngState = false;
                    }
                }
            }
        }

        // -- 最左上角关于开发者头像icon—Item --
        Item {
            id: oran7IconAreaItem
            width: 40
            height: width
            anchors.left: parent.left
            anchors.leftMargin: leftRectangle.simpleMode ? 22 :12
            anchors.top: parent.top
            anchors.topMargin: 18
            opacity: 1.0
            visible: openCaptionBtn.openIngState
            scale: 1.0

            Behavior on anchors.leftMargin { NumberAnimation { duration: 150 } }
            Rectangle {
                id: oran7IconRectangle
                anchors.fill: parent
                color: "transparent"

                // 使用layer.enabled和DropShadow组合
                layer.enabled: true
                layer.effect: DropShadow {
                    id: oran7IconRectangleDropShadow
                    anchors.fill: oran7IconRectangle
                    z: oran7IconRectangle.z
                    source: oran7IconRectangle
                    color: "transparent"
                    samples: 60
                    spread: 0.3
                    radius: oran7IconRectangle.width / 2 + (oran7IconRectangle.width * oran7IconRectangleDropShadow.spread)
                    horizontalOffset: 0
                    verticalOffset: 0
                    SequentialAnimation on color {
                        id: shadowAnimation
                        running: BasicConfig.runningInfinitePropertyAnimation
                        loops: Animation.Infinite
                        property real transDuration: 500

                        PropertyAnimation {
                            to: Oran7ColorGenerator.presetToColor("#Preset_Red")
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: Oran7ColorGenerator.presetToColor(String("#Preset_Volcano"))
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: Oran7ColorGenerator.presetToColor(String("#Preset_Orange"))
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: Oran7ColorGenerator.presetToColor(String("#Preset_Gold"))
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: Oran7ColorGenerator.presetToColor(String("#Preset_Yellow"))
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: Oran7ColorGenerator.presetToColor(String("#Preset_Lime"))
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: Oran7ColorGenerator.presetToColor(String("#Preset_Green"))
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: Oran7ColorGenerator.presetToColor(String("#Preset_Cyan"))
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: Oran7ColorGenerator.presetToColor(String("#Preset_Blue"))
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: Oran7ColorGenerator.presetToColor(String("#Preset_Geekblue"))
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: Oran7ColorGenerator.presetToColor(String("#Preset_Purple"))
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: Oran7ColorGenerator.presetToColor(String("#Preset_Magenta"))
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: Oran7ColorGenerator.presetToColor(String("#Preset_Grey"))
                            duration: shadowAnimation.transDuration
                        }
                    }
                }
                Image {
                    id: oran7IconImage
                    anchors.fill: parent
                    source: "qrc:/image/Oran7.png"
                    asynchronous: false  // 拖动时改为同步，避免异步计算导致的卡顿
                    smooth: false  // 拖动时关闭平滑，提高性能
                    mipmap: true
                    antialiasing: true
                }
            }
            MouseArea {
                anchors.fill: parent
                onPressed: parent.scale = 0.95
                onReleased: parent.scale = 1.0
                onCanceled: parent.scale = 1.0
                onClicked: {
                    animatedWindowWarper.open(oran7IconAreaItem);
                }
            }
        }

        //-- AnimatedWindow --
        Oran7AnimatedWindow {
            id: animatedWindowWarper
            buttonColor: "#f8c7c7"
            fullscreenColor: "#f8c7c7"
            maxTiltAngle: 30
            animDuration: 500
            onStateChanged: {
                if (state === "fullscreenState") {
                    oran7Brief.start_gradientLayerColorAnimation = true;
                } else {
                    oran7Brief.start_gradientLayerColorAnimation = false;
                }
            }
            //嵌入子项到内部contentArea区域
            Image {
                id: animatedWindowWarperbackgoundImage
                width: mainWindow.width
                height: mainWindow.height
                sourceSize.width: Screen.width * Screen.devicePixelRatio
                sourceSize.height: Screen.height * Screen.devicePixelRatio
                source: animatedWindowWarperbackgoundImage.focus ? "" : filehepler.fileExists("file:///" + Oran7Theme.Oran7MainGUI.backgroundImage) ?
                            "file:///" + Oran7Theme.Oran7MainGUI.backgroundImage : "qrc:/image/test_background1.jpg"

                fillMode: Image.PreserveAspectCrop
                asynchronous: false  // 拖动时改为同步，避免异步计算导致的卡顿
                mipmap: true  // 启用mipmap，提高缩放性能
                smooth: false  // 拖动时关闭平滑，提高性能
                cache: true
                transformOrigin: Item.Center
            }
            Oran7BlurCard {
                anchors.fill: parent
                blurSource: animatedWindowWarperbackgoundImage
                themeColor: "#04FFFFFF"
                Oran7AuthorBriefItem {
                    id: oran7Brief
                    start_gradientLayerColorAnimation: false
                }
            }
        }

        //<-- MianUi Rectangle Container End -->
    }

    // 原生窗口控制按钮组件
    component WindowControlButton: Rectangle {
        id: button
        color: buttonColor
        radius: 4

        property color buttonColor: "transparent"
        property color hoverColor: "#c42b1c"
        property string iconText: "✕"

        property int buttonTextPixelSize: 15

        signal clicked

        // 颜色过渡动画
        Behavior on color {
            ColorAnimation {
                duration: 150  // 增加动画持续时间，更平滑
                easing.type: Easing.InOutCubic  // 更平滑的缓动函数
            }
        }

        // 透明度过渡动画
        Behavior on opacity {
            NumberAnimation {
                duration: 150
                easing.type: Easing.InOutCubic
            }
        }

        // 按下效果
        scale: mouseArea.pressed ? 0.95 : 1.0

        Behavior on scale {
            NumberAnimation {
                duration: 100
                easing.type: Easing.OutBack
            }
        }

        Text {
            id: buttonText
            anchors.centerIn: parent
            text: iconText
            color: "#ffffff"
            font.pixelSize: button.buttonTextPixelSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            opacity: 0.8

            // 文字透明度过渡
            Behavior on opacity {
                NumberAnimation {
                    duration: 150
                    easing.type: Easing.InOutCubic
                }
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            propagateComposedEvents: true  // 传递未处理的事件给底层窗口

            onEntered: {
                button.color = hoverColor;
                buttonText.opacity = 1.0;
            }

            onExited: {
                button.color = button.buttonColor;  // 修复：重置为初始的buttonColor
                buttonText.opacity = 0.8;
            }

            onClicked: {
                button.clicked();
            }

            onPressed: {
                button.scale = 0.95;
            }

            onReleased: {
                button.scale = 1.0;
            }

            onCanceled: {
                button.scale = 1.0;
            }
        }
    }

    // --- Tools ---
    Oran7FileHelper{
        id:filehepler
    }

    // --- OutLine Border ---
    // Window{
    //     id:outLine_border
    //     width: mainWindow.width + (4+borderSpace) * 2
    //     height: mainWindow.height + (4+borderSpace) * 2
    //     visible: mainWindow.visible && mainWindow.active

    //     objectName: "__BORDER__"

    //     property real borderSpace: 7

    //     x: mainWindow.x - (4+borderSpace)
    //     y: mainWindow.y - (4+borderSpace)
    //     z:mainWindow.z -1
    //     //flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    //     color: "transparent"

    //     // 鼠标穿透代理
    //     Oran7WindowAgent {
    //         id: __borderWindowAgent__
    //     }
    //     // Item {
    //     //     id: __hitTestPassThroughArea
    //     //     anchors.fill: parent
    //     //     Component.onCompleted: {
    //     //         __borderWindowAgent__.setsetHitTestVisible(__hitTestPassThroughArea, true);
    //     //     }
    //     // }

    //     Rectangle {
    //          anchors.fill: parent
    //          border.width: 4
    //          border.color:"cyan"
    //          radius: 10
    //          color:"transparent"
    //      }
    // }

    // --- Setting windows ---
    Oran7SettingsContainerWindow {
        id: settingsContainer

        // 监听设置窗口请求恢复焦点的信号
        onRestoreMainWindowFocusRequested: {
            mainWindow.restoreFocus();
        }
    }
}
