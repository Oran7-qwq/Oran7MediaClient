import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Shapes 1.10
import Qt5Compat.GraphicalEffects
import QtMultimedia

import "./src/qml/LeftPage"
import "./src/qml/RightPage"
import "./src/qml/BottomPage"
import "./src/qml/Basic"
import "./src/qml/Components"
import "./src/qml/Settings/SettingWindows"

import Client 1.0
import Oran7NetEaseAuth 1.0
import Oran7Network 1.0
import Oran7UI.Impl 1.0
import Oran7Async 1.0

ApplicationWindow {
    id: mainWindow
    objectName: "__Oran7Window__"
    width: 990 //miniSize default
    height: 564 //miniSize default
    visible: true
    color: "transparent"
    title: "Oran7MediaClient"
    minimumWidth: 990
    minimumHeight: 564

    property alias __Oran7WindowBackGround__: mainWindowBackground

    // 窗口缩放中标记：背景视频据此暂停，避免 sink 重建表面闪帧
    property bool _windowResizing: false

    // 主窗口关闭 = 整体退出：桌面歌词悬浮窗是独立的顶层窗口（transientParent=null），
    // 主窗口关了它仍可见，会让 quitOnLastWindowClosed 失效 → 进程/控制台卡住不退出。
    onClosing: Qt.quit()
    // 防抖保存窗口尺寸/位置：停止缩放 200ms 后才落盘。
    // 之前用 Qt.callLater 在 onWidthChanged/onHeightChanged 里排队，拉伸时会刷
    // "QQmlExpression: Attempted to evaluate an expression in an invalid context"。
    Timer {
        id: windowResizeSaveTimer
        interval: 200
        onTriggered: {
            _windowResizing = false
            backgroundPlayer.play()   // 缩放结束恢复播放
            if (mainWindow.visibility === Window.Windowed) {
                Oran7MainUiSetting.savedNormalWidth = mainWindow.width
                Oran7MainUiSetting.savedNormalHeight = mainWindow.height
                Oran7MainUiSetting.savedNormalX = mainWindow.x
                Oran7MainUiSetting.savedNormalY = mainWindow.y
            }
        }
    }
    // 缩放/最大化期间暂停背景视频（画面定格上一帧）
    onWidthChanged: { _windowResizing = true; backgroundPlayer.pause(); windowResizeSaveTimer.restart() }
    onHeightChanged: { _windowResizing = true; backgroundPlayer.pause(); windowResizeSaveTimer.restart() }
    onXChanged: { windowResizeSaveTimer.restart() }
    onYChanged: { windowResizeSaveTimer.restart() }
    Component.onCompleted: {
        Oran7MainUiSetting.savedNormalWidth = mainWindow.width;
        Oran7MainUiSetting.savedNormalHeight = mainWindow.height;
        Oran7MainUiSetting.savedNormalX = mainWindow.x;
        Oran7MainUiSetting.savedNormalY = mainWindow.y;

        // 设置全局mainWindow引用
        BasicConfig.mainWindow = mainWindow;

        // 共享主窗口背景渲染源（视频/静态图一体），供开发者头像窗口、歌词窗口复用同一份背景
        BasicConfig.backgroundVideoSource = mainWindowBackground;

        // 启动后静默预拉取云端"我喜欢的音乐"歌单（登录态），让 BottomPage 小心心能正确显示喜欢状态。
        // Qt.callLater 延迟到 NcmSession.load()（Config 加载）之后执行；统一走 Oran7AsyncManager。
        Qt.callLater(function() {
            if (NcmSession.isLoggedIn)
                BasicConfig.fetchNeteaseLikedSongs();
        });
    }

    // 背景元素 - 作为直接子元素，可以被alias引用；循环播放 mp4 动图，视频不可用时自动回退到静态背景图
    Item {
        id: mainWindowBackground
        anchors.fill: parent

        // 循环播放的 mp4 背景
        // 循环播放的 mp4 背景
        // Video 是 QtMultimedia 的便捷类型（内部 = MediaPlayer + VideoOutput + AudioOutput 一体），
        // 完整暴露 muted/loops/hasVideo 等，且带 play()/pause() 方法。
        // 窗口缩放/最大化时用 pause() 暂停背景视频、定格上一帧，避免 sink 重建表面闪成透明露出 DWM
        Video {
            id: backgroundPlayer
            anchors.fill: parent
            // backgroundMode: "Video" 时播放循环视频；"Image" 或 DWM 模糊时 source 置空，不做无谓解码
            source: (Oran7Theme.Oran7MainGUI.DWMBlurEffectEnabled || Oran7Theme.Oran7MainGUI.backgroundMode !== "Video") ? "" :
                    filehepler.fileExists("file:///" + Oran7Theme.Oran7MainGUI.backgroundVideo) ?
                        "file:///" + Oran7Theme.Oran7MainGUI.backgroundVideo : "qrc:/image/background.mp4"
            loops: MediaPlayer.Infinite
            autoPlay: true
            muted: true // 背景动图静音
            fillMode: VideoOutput.PreserveAspectCrop
            opacity: 1.0   // 不透明，避免 alpha 合成闪烁
            visible: backgroundPlayer.hasVideo && !Oran7Theme.Oran7MainGUI.DWMBlurEffectEnabled
                     && Oran7Theme.Oran7MainGUI.backgroundMode === "Video"
        }

        // 视频不可用时的静态背景图回退
        Image {
            anchors.fill: parent
            sourceSize.width: Math.min(Screen.width * Screen.devicePixelRatio, 1920)
            sourceSize.height: Math.min(Screen.height * Screen.devicePixelRatio, 1080)
            source: Oran7Theme.Oran7MainGUI.DWMBlurEffectEnabled ? "" : filehepler.fileExists("file:///" + Oran7Theme.Oran7MainGUI.backgroundImage) ?
                       "file:///" + Oran7Theme.Oran7MainGUI.backgroundImage : "qrc:/image/default_background.jpeg"
            opacity: 1.0   // 与视频一致：完全不透明，避免 alpha 合成闪烁
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            mipmap: false
            smooth: false
            cache: false
            transformOrigin: Item.Center
            visible: !backgroundPlayer.hasVideo || Oran7Theme.Oran7MainGUI.DWMBlurEffectEnabled
                     || Oran7Theme.Oran7MainGUI.backgroundMode !== "Video"
        }
    }

    // 设置background属性引用mainWindowBackground
    background: mainWindowBackground

    // --- MainWoindow functions ---
    function restoreFocus() {// 恢复主窗口焦点
        mainWindow.requestActivate();//native-api
    }

    // --- MainWindow 性能监测：最右上角网络延迟，FPS 放在其左侧 ---
    Text {
        id: netLatencyText
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 5
        z: 99
        color: !Oran7NetworkMonitor.online ? "#c42b1c"
              : (Oran7NetworkMonitor.latencyMs < 180 ? "#2ea44f" : "#e6a23c")
        font.pixelSize: 12
        font.bold: false
        // 由 Oran7NetworkMonitor.latencyMsChanged / onlineChanged 驱动绑定刷新
        text: Oran7NetworkMonitor.online
              ? ("NET: " + (Oran7NetworkMonitor.latencyMs < 0 ? "--" : Oran7NetworkMonitor.latencyMs) + "ms")
              : "无网络"
    }

    Text {
        id: fpsText
        anchors.top: parent.top
        anchors.right: netLatencyText.left
        anchors.margins: 5
        z: 99
        color: "#74ccd6"
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

    // --- 网易云 Cookie 失效自动重登（网络监测器触发） ---
    Connections {
        target: Oran7NetworkMonitor
        function onCookieExpired() {
            if (!NcmSession.isLoggedIn)
                return;   // 已处于未登录态，无需重复处理
            console.log("Oran7NetworkMonitor: 网易云 Cookie 已失效，自动退出登录并引导重新登录");
            NcmSession.logout();
            // 挂起标记 + 跳转搜索页 + 打开登录弹窗（弹窗在搜索页内，未实例化时由 onCompleted 消费）
            BasicConfig.pendingNeteaseLoginPopup = true;
            BasicConfig.pushSearchMusicStackInto_RightPageMainStackView();
            BasicConfig.openNeteaseLoginPopup();
        }
    }

    Oran7WindowAgent{
        id:__windowAgent
        Component.onCompleted: {
              // 启用 DWM 模糊（Windows 系统级透明+模糊）。
              // 注意：窗口缩放/最大化期间会在 resize 处理器里临时关闭、结束后恢复，
              // 避免 acrylic 重新应用导致窗口层高频闪烁
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

        // ====== 灵动岛======= //
        Oran7LyricsIsland {
            id: lyricsIsland
            anchors.top: parent.top
            anchors.topMargin: 14
            anchors.horizontalCenter: rightRectangle.horizontalCenter
            islandVisible: true
            lyricGradientColorIndex: 12

            // 点击底部播放栏的“灵动岛”按钮切换显示/隐藏
            Connections
            {
                target: bottomPage
                function onIslandClicked() {
                    lyricsIsland.toggle()
                }
            }
        }

        // ====== 桌面歌词悬浮窗（独立于主窗口） ======
        // 无边框置顶的顶层 Window，复用 Oran7LyricsIsland 歌词渲染核心；
        // 状态/位置由 Oran7MusicLyricsWindow 主题持久化，启动时自动回读。
        Oran7LyricsFloatingWindow {
            id: lyricsFloatingWindow
            Connections {
                target: bottomPage
                function onFloatingLyricsClicked() {
                    lyricsFloatingWindow.toggleEnabled()
                }
            }
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
            blurEnabled: Oran7Theme.Oran7MusicPlayControls.blurEffectEnabled
            saturation:Oran7Theme.Oran7MusicPlayControls.saturation
            brightness:Oran7Theme.Oran7MusicPlayControls.brightness
            contrast:Oran7Theme.Oran7MusicPlayControls.contrast

            property real visibleOpacity: 0.0

            BottomPage {
                id: bottomPage
                anchors.fill: parent
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
            blurEnabled: Oran7Theme.Oran7CaptionBar.blurEffectEnabled
            borderRadius:17
            borderWidth: 1
            themeColor: "#04FFFFFF"

            saturation: Oran7Theme.Oran7CaptionBar.saturation
            brightness: Oran7Theme.Oran7CaptionBar.brightness
            contrast: Oran7Theme.Oran7CaptionBar.contrast
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
            buttonColor: "#f8c7c7"/*"transparent"*/
            fullscreenColor: "#f8c7c7"/*"transparent"*/
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
            //复用主窗口的共享背景源（视频/静态图一体），保证动态背景全局只解码一份
            ShaderEffectSource {
                id: animatedWindowWarperbackgoundImage
                width: mainWindow.width
                height: mainWindow.height
                sourceItem: BasicConfig.backgroundVideoSource
                live: true
            }
            Oran7BlurCard {
                anchors.fill: parent
                blurSource: animatedWindowWarperbackgoundImage
                themeColor: "#04FFFFFF"
                Oran7AuthorBriefItem {
                    id: oran7Brief
                }
                Rectangle {
                    id:windowContentCloseBtnRectangle
                    anchors.top: parent.top;
                    anchors.left: parent.left
                    anchors.margins: 20
                    width: 40
                    height: width
                    radius:windowContentCloseBtnRectangle.width/2
                    visible: parent.visible;
                    color:"#f8c7c7"
                    layer.enabled: true
                    layer.effect:DropShadow{
                        id:windowContentCloseBtnDropShadow
                        anchors.fill: windowContentCloseBtnRectangle
                        source: windowContentCloseBtnRectangle
                        z:windowContentCloseBtnRectangle.z
                        spread: 0.05
                        samples: 60
                        radius:windowContentCloseBtnRectangle.width / 2 + windowContentCloseBtnRectangle.width*spread
                        horizontalOffset: 0
                        verticalOffset: 0
                        SequentialAnimation on color{
                            id: shadowAnimation_inner
                            running: BasicConfig.runningInfinitePropertyAnimation
                            loops: Animation.Infinite
                            property real transDuration: 500

                            PropertyAnimation { to: "#FFA500"; duration: shadowAnimation_inner.transDuration}
                            PropertyAnimation { to: "#FFFF00"; duration: shadowAnimation_inner.transDuration }
                            PropertyAnimation { to: "#00FF00"; duration: shadowAnimation_inner.transDuration }
                            PropertyAnimation { to: "#00FFFF"; duration: shadowAnimation_inner.transDuration }
                            PropertyAnimation { to: "#0000FF"; duration: shadowAnimation_inner.transDuration }
                            PropertyAnimation { to: "#800080"; duration: shadowAnimation_inner.transDuration }
                            PropertyAnimation { to: "#FF0000"; duration: shadowAnimation_inner.transDuration }
                        }
                    }
                    Image {
                        anchors.fill: parent
                        source: "qrc:/image/Oran7.png"
                        mipmap: true
                        antialiasing: true
                    }
                    MouseArea{
                        anchors.fill: parent
                        onClicked: {
                            if (animatedWindowWarper.isAnimating) return;
                            animatedWindowWarper.isAnimating = true;
                            animatedWindowWarper.state = "iconState";
                        }
                    }
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

    // --- Setting windows ---
    Oran7SettingsContainerWindow {
        id: settingsContainer

        // 监听设置窗口请求恢复焦点的信号
        onRestoreMainWindowFocusRequested: {
            mainWindow.restoreFocus();
        }
    }

    // 云端"我喜欢的音乐"歌单填充统一收口在 BasicConfig.applyNeteaseLikedFetchResult：
    // 启动预拉取 / 页签进入 / 登录 / 排序重试的结果都经它落盘（取代此处的 onLikedSongReady 连接）
}
