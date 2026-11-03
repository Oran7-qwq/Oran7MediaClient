import QtQuick 2.15
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import Client 1.0
import "../Basic"
import "../Components/Oran7GlobalComponents"
import LyricsModel 1.0

import Oran7UI.Impl

Rectangle{
    id:root

    property alias visibleOpacity: musicPlayControls.visibleOpacity
    color: "transparent"

    // 灵动岛开关信号（转发自内部播放控制组件的 islandClicked）
    signal islandClicked
    // 桌面歌词悬浮窗开关信号（转发自内部播放控制组件的 floatingLyricsClicked）
    signal floatingLyricsClicked

    //播放控制内容区域
    Oran7MusicPlayControls{
        id:musicPlayControls
        anchors.fill: parent
        onAlbumCoverClicked: animatedWindow_LyricsWin.open(musicIconItem)
        onLyricsButtonClicked: animatedWindow_LyricsWin.open(musicIconItem)
        onIslandClicked: root.islandClicked()
        onFloatingLyricsClicked: root.floatingLyricsClicked()
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
        buttonColor: "#20FFFFFF"
        fullscreenColor: "#20FFFFFF"
        maxTiltAngle: 30
        animDuration: 400
        showCloseButton: false  // 使用自定义关闭按钮，不显示内置的

        // 挂到 mainWindow 的 contentItem 上，使全屏展开覆盖整个窗口
        Component.onCompleted: {
            var win = Window.window;
            if (win && win.contentItem)
                parent = win.contentItem;
        }

        // 复用主窗口的共享背景源（视频/静态图一体），保证动态背景全局只解码一份；
        // 自身仍是独立副本，避免模糊源引用 mainWindow 本身导致关闭时闪烁
        ShaderEffectSource {
            id: lyricsWinBackground
            width: root.Window.window ? root.Window.window.width : 0
            height: root.Window.window ? root.Window.window.height : 0
            sourceItem: BasicConfig.backgroundVideoSource
            live: true
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
            Oran7LyricsView {
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
