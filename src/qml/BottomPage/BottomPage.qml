import QtQuick 2.15
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import Client 1.0
import "../Basic"
import "../Components"
import LyricsModel 1.0

import Oran7UI.Impl

Rectangle{
    id:root

    property alias visibleOpacity: musicPlayControls.visibleOpacity
    color: "transparent"

    //播放控制内容区域
    Oran7MusicPlayControls{
        id:musicPlayControls
        anchors.fill: parent
        onAlbumCoverClicked: animatedWindow_LyricsWin.open(musicIconItem)
        onLyricsButtonClicked: animatedWindow_LyricsWin.open(musicIconItem)
    }

    // 提供给 animatedWindow_LyricsWin.open() 的定位参考项
    Item {
        id: musicIconItem
        anchors.left: parent.left
        anchors.leftMargin: 30
        anchors.verticalCenter: parent.verticalCenter
        width: 50
        height: 50
    }

    //全屏歌词窗口
    Oran7AnimatedWindow{
        id: animatedWindow_LyricsWin
        buttonColor: "#f8c7c7"
        fullscreenColor: "#f8c7c7"
        maxTiltAngle: 10
        animDuration: 400
        showCloseButton: false  // 使用自定义关闭按钮，不显示内置的

        // 挂到 mainWindow 的 contentItem 上，使全屏展开覆盖整个窗口
        Component.onCompleted: {
            var win = Window.window;
            if (win && win.contentItem)
                parent = win.contentItem;
        }

        // 独立的背景图副本，避免模糊源引用 mainWindow 本身导致关闭时闪烁
        Image {
            id: lyricsWinBackground
            width: mainWindow.width
            height: mainWindow.height
            sourceSize.width: Screen.width * Screen.devicePixelRatio
            sourceSize.height: Screen.height * Screen.devicePixelRatio
            source: filehepler.fileExists("file:///" + Oran7Theme.Oran7MainGUI.backgroundImage) ?
                        "file:///" + Oran7Theme.Oran7MainGUI.backgroundImage : "qrc:/image/defaultBg.jpg"
            fillMode: Image.PreserveAspectCrop
            asynchronous: false
            mipmap: true
            smooth: false
            cache: true
            transformOrigin: Item.Center
        }
        Oran7BlurCard {
            anchors.fill: parent
            blurSource: lyricsWinBackground
            themeColor: "#04FFFFFF"
            blurEnabled:Oran7Theme.Oran7MusicLyricsWindow.blurEffectEnabled
            saturation:Oran7Theme.Oran7MusicLyricsWindow.saturation
            brightness:Oran7Theme.Oran7MusicLyricsWindow.brightness
            contrast:Oran7Theme.Oran7MusicLyricsWindow.contrast
            Oran7BlurCard{
                width: 40
                height: width
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 20
                themeColor: "#04FFFFFF"
                blurSource: lyricsWinBackground
                borderWidth: 2
                borderRadius: 10
                Image{
                    id:downImage
                    anchors.fill: parent
                    scale:0.8
                    source: "qrc:/image/mingcute_arrows-down-fill.png"
                    layer.enabled: true
                    layer.effect: ColorOverlay{
                        source:downImage
                        color:Oran7Theme.Oran7MainGUI.themeColor
                    }
                }
                MouseArea{
                    anchors.fill: parent
                    onClicked: {
                        if (animatedWindow_LyricsWin.isAnimating) return;
                        animatedWindow_LyricsWin.isAnimating = true;
                        animatedWindow_LyricsWin.state = "iconState";
                    }
                }
            }
            // 歌词显示组件
            LyricsView {
                id: lyricsView
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: inner_musicPlayControls.top
                anchors.topMargin: 70
                anchors.bottomMargin: 20

                hightLight_LyricsColor: Oran7Theme.Oran7MusicLyricsWindow["textColorBase-4"]
                deepLight_LyricsColor: Oran7Theme.Oran7MusicLyricsWindow["textColorBase-7"]
                hightLight_LyricsFontSize: Oran7Theme.Oran7MusicLyricsWindow["textSizeBase-3"]
                deepLight_LyricsFontSize: Oran7Theme.Oran7MusicLyricsWindow["textSizeBase-1"]
            }
            Oran7MusicPlayControls{
                id:inner_musicPlayControls
                width:root.width
                height: root.height
                anchors.bottom: parent.bottom
                //anchors.bottomMargin: 7
                anchors.left: parent.left
                anchors.leftMargin: 7
                onAlbumCoverClicked: {
                    if (animatedWindow_LyricsWin.isAnimating) return;
                    animatedWindow_LyricsWin.isAnimating = true;
                    animatedWindow_LyricsWin.state = "iconState";
                }
            }
        }
    }
}
