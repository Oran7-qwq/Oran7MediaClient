import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Shapes 1.10
import Qt5Compat.GraphicalEffects

import "./Src/LeftPage"
import "./Src/RightPage"
import "./Src/BottomPage"
import "./Src/Basic"
import "./Src/Components"
import "./Src/Settings/SettingWindows"

import Client 1.0
import FramelessWindow 1.0

ApplicationWindow {
    id: mainWindow
    objectName: "mainWindow"
    width: 1180
    height: 680
    visible: true
    color: "transparent"
    title: "Oran7MediaClient"
    minimumWidth: 1180
    minimumHeight: 680

    // 使用无边框窗口样式，但保留原生窗口控制按钮
    flags: Qt.FramelessWindowHint | Qt.Window | Qt.WindowSystemMenuHint | Qt.WindowMinMaxButtonsHint | Qt.WindowCloseButtonHint

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
    }

    background: Image {
        id: mainWindowBackground
        anchors.fill: parent
        sourceSize.width: mainWindow.minimumWidth
        sourceSize.height: mainWindow.minimumHeight
        //source: "qrc:/image/themBackground.jpg"
        source: Oran7MainUiSetting.backgroundImagePath
        opacity: 0.999

        fillMode: Image.PreserveAspectCrop
        asynchronous: false  // 拖动时改为同步，避免异步计算导致的卡顿
        mipmap: true            // 启用mipmap，提高缩放性能
        smooth: false            // 拖动时关闭平滑，提高性能
        cache: true
        transformOrigin: Item.Center
    }

    // --- MainWoindow functions ---
    function restoreFocus() {// 恢复主窗口焦点
        console.log("恢复主窗口焦点");
        mainWindow.requestActivate();
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

    //  --- 无边框窗口处理器 ---
    FramelessWindow {
        id: framelessWindow
        targetWindow: mainWindow
        borderWidth: 6
        borderHeight: 6
        titleBarHeight: Oran7MainUiSetting.window_titleBarWidth

        Component.onCompleted: {
            setupWindow();
        }
    }

    //====================== <Main ui> ========================//
    // --- 右上角原生窗口控制按钮（覆盖在主UI上） ---
    Row {
        id: nativeWindowControls
        width: 140  // 三个按钮的总宽度
        height: 36
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.rightMargin: 12  // 增加右边距，给边框拉伸留出足够空间
        anchors.topMargin: 12    // 增加上边距，给边框拉伸留出足够空间
        spacing: 4  // 按钮之间的间距
        z: 999
        layoutDirection: Qt.RightToLeft

        // 关闭按钮
        WindowControlButton {
            id: nativeCloseButton
            width: 46
            height: 32
            buttonColor: "transparent"
            hoverColor: "#c42b1c"
            iconText: "✕"
            onClicked: {
                mainWindow.close();
            }
        }

        // 最大化/还原按钮
        WindowControlButton {
            id: nativeMaximizeButton
            width: 46
            height: 32
            buttonColor: "transparent"
            hoverColor: Qt.rgba(0.2, 0.2, 0.2, 0.5)
            // 为□符号设置更大的字体大小
            buttonTextPixelSize: mainWindow.visibility === Window.Maximized ? 15 : 34
            iconText: mainWindow.visibility === Window.Maximized ? "❐" : "▢"

            onClicked: {
                // if (mainWindow.visibility === Window.Maximized) {
                //     framelessWindow.nativeMaximize();
                // } else {
                //     framelessWindow.nativeMaximize();
                // }
                framelessWindow.nativeMaximize();
            }
        }

        // 最小化按钮
        WindowControlButton {
            id: nativeMinimizeButton
            width: 46
            height: 32
            buttonColor: "transparent"
            hoverColor: Qt.rgba(0.2, 0.2, 0.2, 0.5)
            iconText: "━"

            onClicked: {
                framelessWindow.nativeMinimize();
            }
        }
    }

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
            //使用Windows原生窗体drag
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
            anchors.top: parent.top
            anchors.bottom: bottomRectangle.top
            //anchors.bottom: parent.bottom
            anchors.right: parent.right
            // color:"#13131a"
            property bool isTransparent: true
            color: isTransparent ? "transparent" : "#f8c7c7"
            //clip: true
        }

        // -- 主体底部 --
        BottomPage {
            id: bottomRectangle
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            color: "#2a1a22"
            //initilization
            visible: true
            visibleOpacity: 0.0
            height: 0
            z: leftRectangle.z > rightRectangle.z ? leftRectangle.z + 1 : rightRectangle.z + 1

            Behavior on height {
                enabled: true
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.OutCubic
                }
            }
        }

        // -- 主体左部 --
        Oran7BlurCard {
            id: leftRectangle
            property real defaultWidth: 204

            width: defaultWidth
            Behavior on width {
                NumberAnimation {
                    duration: 200
                    easing.type: Easing.OutCubic
                }
            }
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.bottom: bottomRectangle.top

            blurSource: mainWindowBackground
            blurEnabled: true
            borderRadius: 0
            borderWidth: 0
            borderColor: "pink"
            dragable: false
            themeColor: "#0EFFFFFF"
            LeftPage {
                id: leftPage
                anchors.fill: parent
                //color:"#c36c7c"
                visible: openSemiCircle.openIngState
                color: "transparent"
            }
        }
        // -- 左部导航栏->打开关闭button --
        Shape {
            id: openSemiCircle
            width: height / 2
            height: Oran7MainUiSetting.topBarDefaultHeight * 0.8
            anchors.left: leftRectangle.right
            anchors.leftMargin: 12
            anchors.top: leftRectangle.top
            anchors.topMargin: Oran7MainUiSetting.topBarDefaultHeight * 0.1
            z: leftRectangle.z + 1
            layer.enabled: true
            layer.smooth: true
            layer.samples: 12
            antialiasing: true

            property bool openIngState: true

            // ShapePath{
            //     strokeWidth: 1
            //     strokeColor: "black"
            //     fillColor: "#fef2e8"
            //     startX: 0
            //     startY: 0
            //     PathArc { x: 0; y: 40; radiusX: 20; radiusY: 20 }
            //     PathLine { x: 0; y: 40 }
            // }
            Connections {
                target: BasicConfig
                function onClearAllUi_inVIdeoRenderArea(ok) {
                    if (ok === false)
                        openImage.visible = true;
                    else if (openSemiCircle.openIngState !== true)
                        openImage.visible = false;
                }
            }
            Image {
                id: openImage
                scale: 2
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 0
                source: "/image/stackback.png"

                rotation: 0
                Behavior on rotation {
                    NumberAnimation {
                        duration: 200
                        easing.type: Easing.OutCubic
                    }
                }
                property color openImageColorOverlay_defaultColor: "#fef2e8"
                property color openImageColorOverlay_selectedColor: "#616161"
                property color openImageColorOverlay_usedColor: openImage.openImageColorOverlay_defaultColor
                layer.enabled: true
                layer.effect: ColorOverlay {
                    source: openImage
                    anchors.fill: openImage
                    color: openImage.openImageColorOverlay_usedColor
                }

                asynchronous: false
                mipmap: true
                cache: true
                antialiasing: true
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (openSemiCircle.openIngState === false) {
                        openImage.rotation = 0;
                        leftRectangle.width = leftRectangle.defaultWidth;

                        openSemiCircle.openIngState = true;
                        rightRectangle.topBarHeight = Oran7MainUiSetting.topBarDefaultHeight;
                    } else {
                        openImage.rotation = 180;
                        leftRectangle.width = 0;

                        openSemiCircle.openIngState = false;
                        rightRectangle.topBarHeight = 0;
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
            anchors.leftMargin: 12
            anchors.top: parent.top
            anchors.topMargin: 18
            opacity: 1.0
            visible: openSemiCircle.openIngState
            scale: 1.0
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
                            to: "#FFA500"
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: "#FFFF00"
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: "#00FF00"
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: "#00FFFF"
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: "#0000FF"
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: "#800080"
                            duration: shadowAnimation.transDuration
                        }
                        PropertyAnimation {
                            to: "#FF0000"
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
            fullscreenColor: "#c46d7c"
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
                // anchors.fill: animatedWindowWarper
                sourceSize.width: animatedWindowWarper.width
                sourceSize.height: animatedWindowWarper.height
                source: "qrc:/image/themBackground.jpg"

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
                dragable: false
                themeColor: "#0EFFFFFF"
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

    // --- Setting windows ---
    // Oran7MainUiSettingWindow {id: oran7MainUiSettingWindow}
    // Oran7MediaPlayerSettingWindow{id: oran7MediaPlayerSettingWindow}

    // 全屏设置容器窗口
    Oran7SettingsContainerWindow {
        id: settingsContainer

        // 监听设置窗口请求恢复焦点的信号
        onRestoreMainWindowFocusRequested: {
            mainWindow.restoreFocus();
        }
    }
}
