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
import Client 1.0

ApplicationWindow {
    id:mainWindow
    objectName: "mainWindow"
    width: 1056
    height: 750
    visible: true
    color: "#c46d7c"
    title: "Oran7MediaClient"
    minimumWidth: 1056
    minimumHeight:750
    //保存的原始尺寸
    property real savedNormalWidth: 0
    property real savedNormalHeight: 0
    property real savedNormalX: 0
    property real savedNormalY: 0
    onWidthChanged: {
        Qt.callLater(function() {
            if(mainWindow.visibility === Window.Windowed)
                mainWindow.savedNormalWidth = mainWindow.width
        })
    }
    onHeightChanged: {
        Qt.callLater(function() {
            if(mainWindow.visibility === Window.Windowed)
                mainWindow.savedNormalHeight=mainWindow.height
        })
    }
    onXChanged: {
        Qt.callLater(function(){
            if(mainWindow.visibility === Window.Windowed)
                mainWindow.savedNormalX = mainWindow.x
        })
    }
    onYChanged: {
        Qt.callLater(function(){
            if(mainWindow.visibility === Window.Windowed)
                mainWindow.savedNormalY = mainWindow.y
        })
    }
    Component.onCompleted: {
        mainWindow.savedNormalWidth = mainWindow.width
        mainWindow.savedNormalHeight = mainWindow.height
        mainWindow.savedNormalX = mainWindow.x
        mainWindow.savedNormalY = mainWindow.y
    }

    background: Image{
        id:mainWindowBackground
        anchors.fill: parent
        sourceSize.width: mainWindow.minimumWidth
        sourceSize.height: mainWindow.minimumHeight
        source: "qrc:/image/themBackground.jpg"
        opacity: 0.999

        fillMode: Image.PreserveAspectCrop
        asynchronous: false  // 拖动时改为同步，避免异步计算导致的卡顿
        mipmap: true            // 启用mipmap，提高缩放性能
        smooth: false            // 拖动时关闭平滑，提高性能
        cache: true
        transformOrigin: Item.Center
    }

    // 性能监测
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
            win = Window.window
            console.log("fps win =", win)
        }

        Connections {
            target: fpsText.win
            function onFrameSwapped() {
                fpsText.frameCount++
            }
        }

        Timer {
            interval: 1000
            repeat: true
            running: true
            onTriggered: {
                var now = Date.now()
                var fps = fpsText.frameCount * 1000 / (now - fpsText.lastMs)
                fpsText.text = "FPS: " + fps.toFixed(0)
                fpsText.frameCount = 0
                fpsText.lastMs = now
            }
        }
    }

    //ETheme  qml-Config
    ETheme {
        id: theme
    }

    // flags:Qt.FramelessWindowHint | Qt.Window
    //         | Qt.WindowSystemMenuHint |
    //        Qt.WindowMaximizeButtonHint | Qt.WindowMinimizeButtonHint

    //无边框但保留原生窗口特性
    //flags: Qt.FramelessWindowHint

    //====================== <Main ui> ========================//
    Rectangle{
        id:mainRectangle
        anchors.fill: parent
        radius: 20
        color: "transparent"
        clip: false
        //窗体拖拽功能
        MouseArea{
            id:mainRectangleMouseArea
            anchors.fill:parent
            propagateComposedEvents: true

            property point clickPos: "0,0"
            property bool mouseIsPressed: false
            onPressed:(mouse)=>{
                clickPos = Qt.point(mouse.x,mouse.y)
                if(clickPos.y > 88)return

                mainRectangleMouseArea.mouseIsPressed=true
                BasicConfig.runningInfinitePropertyAnimation=false
                // oran7IconRectangle.layer.enabled = false
                //leftRectangle.blurEnabled=false
            }
            onReleased: {
                BasicConfig.runningInfinitePropertyAnimation=true
                mainRectangleMouseArea.mouseIsPressed=false
                // 延迟恢复效果，避免卡顿
                delayRestoreTimer.restart()
            }

            onPositionChanged:(mouse)=> {
                if(mainRectangleMouseArea.mouseIsPressed === false)return;
                // if(mouse.y >= 80)return;

                let delta = Qt.point(mouse.x-clickPos.x,mouse.y-clickPos.y)
                mainWindow.x+=delta.x
                mainWindow.y+=delta.y
                mouse.accepted = false
            }

            onClicked: {
                BasicConfig.clickedOutside()
            }
        }
        // 延迟恢复效果
        Timer {
            id: delayRestoreTimer
            interval: 300
            onTriggered: {
                BasicConfig.runningInfinitePropertyAnimation = true
                oran7IconRectangle.layer.enabled = true
                //leftRectangle.blurEnabled = true
            }
        }

        //主体底部
        BottomPage{
            id:bottomRectangle
            anchors.left:parent.left
            anchors.right:parent.right
            anchors.bottom: parent.bottom
            color:"#2a1a22"
            //initilization
            visible: true
            visibleOpacity: 0.0
            height: 0
            z:leftRectangle.z > rightRectangle.z ? leftRectangle.z + 1 : rightRectangle.z +1

            Behavior on height {
                enabled: true
                NumberAnimation{
                    duration: 300
                    easing.type: Easing.OutCubic
                }
            }
        }

        //主体左部
        EBlurCard{
            id:leftRectangle
            property real defaultWidth: 204

            width:defaultWidth
            Behavior on width {
                NumberAnimation{
                    duration: 200
                    easing.type: Easing.OutCubic
                }
            }
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.bottom: bottomRectangle.top

            blurSource:mainWindowBackground
            blurEnabled: true
            borderRadius: 0
            borderWidth: 0
            borderColor:"pink"
            dragable: false
            themeColor: "#0EFFFFFF"
            LeftPage{
                id:leftPage
                anchors.fill: parent
                //color:"#c36c7c"
                color:"transparent"
            }
        }
        //左部打开关闭button
        Shape{
            id:openSemiCircle
            width: height/2
            height: 40
            anchors.left: leftRectangle.right
            anchors.leftMargin: 4
            anchors.top: leftRectangle.top
            anchors.topMargin: 30
            z:leftRectangle.z + 1
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
            Connections{
                target:BasicConfig
                function onClearAllUi_inVIdeoRenderArea(ok)
                {
                    if(ok === false)
                        openImage.visible = true
                    else if(openSemiCircle.openIngState !== true)
                        openImage.visible = false
                }
            }
            Image{
                id:openImage
                scale: 2
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 0
                source: "/image/stackback.png"

                rotation: 0
                Behavior on rotation {
                    NumberAnimation{
                        duration: 200
                        easing.type: Easing.OutCubic
                    }
                }
                property color openImageColorOverlay_defaultColor: "#fef2e8"
                property color openImageColorOverlay_selectedColor: "#616161"
                property color openImageColorOverlay_usedColor: openImage.openImageColorOverlay_defaultColor
                layer.enabled: true
                layer.effect: ColorOverlay{
                    source: openImage
                    anchors.fill: openImage
                    color: openImage.openImageColorOverlay_usedColor
                }

                asynchronous: false
                mipmap: true
                cache: true
                antialiasing: true
            }
            MouseArea{
                anchors.fill: parent
                onClicked: {
                    if(openSemiCircle.openIngState === false)
                    {
                        openImage.rotation = 0
                        leftRectangle.width = leftRectangle.defaultWidth

                        openSemiCircle.openIngState = true
                        rightRectangle.topBarHeight = rightRectangle.topBarDefultHeight
                    }
                    else
                    {
                        openImage.rotation = 180
                        leftRectangle.width = 0

                        openSemiCircle.openIngState = false
                        rightRectangle.topBarHeight = 0
                    }
                }
            }
        }

        //最左上角关于开发者头像icon—Item
        Item{
            id:oran7IconAreaItem
            width: 40
            height: width
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.top: parent.top
            anchors.topMargin: 18
            opacity: 1.0
            scale:1.0
            Rectangle {
                id: oran7IconRectangle
                anchors.fill: parent
                color: "transparent"

                // 使用layer.enabled和DropShadow组合
                layer.enabled: true
                layer.effect:DropShadow{
                    id:oran7IconRectangleDropShadow
                    anchors.fill: oran7IconRectangle
                    z:oran7IconRectangle.z
                    source: oran7IconRectangle
                    color: "transparent"
                    samples: 60
                    spread: 0.3
                    radius: oran7IconRectangle.width/2 + (oran7IconRectangle.width*oran7IconRectangleDropShadow.spread)
                    horizontalOffset: 0
                    verticalOffset: 0
                    SequentialAnimation on color{
                        id: shadowAnimation
                        running: BasicConfig.runningInfinitePropertyAnimation
                        loops: Animation.Infinite
                        property real transDuration: 500

                        PropertyAnimation { to: "#FFA500"; duration: shadowAnimation.transDuration}
                        PropertyAnimation { to: "#FFFF00"; duration: shadowAnimation.transDuration }
                        PropertyAnimation { to: "#00FF00"; duration: shadowAnimation.transDuration }
                        PropertyAnimation { to: "#00FFFF"; duration: shadowAnimation.transDuration }
                        PropertyAnimation { to: "#0000FF"; duration: shadowAnimation.transDuration }
                        PropertyAnimation { to: "#800080"; duration: shadowAnimation.transDuration }
                        PropertyAnimation { to: "#FF0000"; duration: shadowAnimation.transDuration }
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
            MouseArea{
                anchors.fill:parent
                onPressed: parent.scale = 0.95
                onReleased: parent.scale = 1.0
                onCanceled: parent.scale = 1.0
                onClicked: {
                    animatedWindowWarper.open(oran7IconAreaItem)
                }
            }
        }

        //AnimatedWindow
        EAnimatedWindow{
            id:animatedWindowWarper
            buttonColor:"#f8c7c7"
            fullscreenColor:"#c46d7c"
            maxTiltAngle:30
            animDuration: 500
            onStateChanged: {
                if(state === "fullscreenState")
                {
                    oran7Brief.start_gradientLayerColorAnimation=true
                }
                else
                {
                    oran7Brief.start_gradientLayerColorAnimation=false
                }
            }
            //嵌入子项到内部contentArea区域
            Image{
                id:animatedWindowWarperbackgoundImage
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
            EBlurCard{
                anchors.fill: parent
                blurSource: animatedWindowWarperbackgoundImage
                dragable: false
                themeColor: "#0EFFFFFF"
                AuthorBriefItem{
                    id:oran7Brief
                    start_gradientLayerColorAnimation:false
                }
            }
        }

        //主体右部
        RightPage{
            id:rightRectangle
            anchors.left: leftRectangle.right
            anchors.top: parent.top
            anchors.bottom: bottomRectangle.top
            //anchors.bottom: parent.bottom
            anchors.right: parent.right
            // color:"#13131a"
            color:"#f8c7c7"
            //clip: true
        }
    }
}
