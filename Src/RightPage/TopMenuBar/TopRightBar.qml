import QtQuick 2.15
import Qt5Compat.GraphicalEffects

//rightTopMenu,迷你窗口，最小化，最大化，关闭程序
Item{
    id:rightTopMenuItem
    anchors.rightMargin: 44
    Row{
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        //迷你窗口
        Image{
            id:miniImage
            anchors.verticalCenter: parent.verticalCenter
            source: "/image/miniwindow.png"
            property string miniImageColorOverlay_Color: "#2a1a22"
            layer.enabled: true
            layer.effect: ColorOverlay{
                source: miniImage
                color: miniImage.miniImageColorOverlay_Color
            }
            MouseArea{
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    miniImage.miniImageColorOverlay_Color = "#616161"
                }
                onExited: {
                    miniImage.miniImageColorOverlay_Color = "#2a1a22"
                }
                onClicked:{
                    //----
                }
            }
        }
        //最小化
        Image{
            id:hideImage
            anchors.verticalCenter: parent.verticalCenter
            source: "/image/hide.png"
            property string hideImageColorOverlay_Color: "#2a1a22"
            layer.enabled: true
            layer.effect: ColorOverlay{
                source: hideImage
                color: hideImage.hideImageColorOverlay_Color
            }
            MouseArea{
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    hideImage.hideImageColorOverlay_Color = "#616161"
                }
                onExited: {
                    hideImage.hideImageColorOverlay_Color = "#2a1a22"
                }
                onClicked: {
                    mainWindow.showMinimized()
                }
            }
        }
        // Image{
        //     id:maxsizeImage
        //     anchors.verticalCenter: parent.verticalCenter
        //     source: "./image/maxsize.png"
        // }
        //最大化
        Rectangle{
            id:maxsizeRectangle
            width: 15
            height: width
            radius: 2
            border.width: 2
            border.color: "#2a1a22"
            color:"transparent"
            anchors.verticalCenter: parent.verticalCenter
            property bool isMaxSize: false
            //向下还原image
            Image {
                id: backMaxSizeImage
                anchors.centerIn: parent
                visible: false
                source: "qrc:/image/backmaxsize.png"
                property string backMaxSizeImageColorOverLay_Color: "#2a1a22"
                layer.enabled: true
                layer.effect: ColorOverlay{
                    source:backMaxSizeImage
                    color: backMaxSizeImage.backMaxSizeImageColorOverLay_Color
                }
            }

            // 最大化动画效果
            ParallelAnimation {
                id: parallelAnimationGroupScreenMaxSize
                running: false // 显式控制动画状态

                // 使用 NumberAnimation 替代 PropertyAnimation（更高效）
                NumberAnimation {
                    target: mainWindow
                    property: "width"
                    from: mainWindow.width
                    to: Screen.desktopAvailableWidth
                    duration: 10
                    easing.type: Easing.InOutQuad // 改用更平滑的缓动曲线
                }
                NumberAnimation {
                    target: mainWindow
                    property: "x"
                    from: mainWindow.x
                    to: 0
                    duration: 10
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    target: mainWindow
                    property: "height"
                    from: mainWindow.height
                    to: Screen.desktopAvailableHeight
                    duration: 10
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    target: mainWindow
                    property: "y"
                    from: mainWindow.y
                    to: 0
                    duration: 10
                    easing.type: Easing.InOutQuad
                }

                // 动画完成后强制重绘
                onStarted: {
                    mainWindow.update()// 强制更新
                    mainWindow.requestActivate()// 确保窗口激活
                }
            }

            // 还原动画效果
            ParallelAnimation {
                id: parallelAnimationGroupScreenBackMaxSize
                running: false

                // 使用绑定确保动画从当前值开始（避免硬编码）
                NumberAnimation {
                    target: mainWindow
                    property: "width"
                    from: mainWindow.width
                    to: mainWindow.savedNormalWidth // 改用保存的原始尺寸
                    duration: 1
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    target: mainWindow
                    property: "x"
                    from: mainWindow.x
                    to: mainWindow.savedNormalX // 改用保存的原始位置
                    duration: 1
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    target: mainWindow
                    property: "height"
                    from: mainWindow.height
                    to: mainWindow.savedNormalHeight
                    duration: 1
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    target: mainWindow
                    property: "y"
                    from: mainWindow.y
                    to: mainWindow.savedNormalY
                    duration: 1
                    easing.type: Easing.InOutQuad
                }

                // 动画完成后触发布局更新
                onStopped: {
                    mainWindow.update()// 强制更新
                    mainWindow.requestActivate()// 确保窗口激活
                }
            }

            MouseArea{
                anchors.fill:parent
                hoverEnabled: true
                onEntered: {
                    if(maxsizeRectangle.isMaxSize===false)
                    {
                        maxsizeRectangle.border.color = "#616161"
                    }
                    else
                    {
                        backMaxSizeImage.backMaxSizeImageColorOverLay_Color = "#616161"
                    }
                }
                onExited: {
                    if(maxsizeRectangle.isMaxSize===false)
                        maxsizeRectangle.border.color = "#2a1a22"
                    else
                        backMaxSizeImage.backMaxSizeImageColorOverLay_Color = "#2a1a22"
                }
                onClicked: {
                    if(maxsizeRectangle.isMaxSize===false)
                    {
                        mainWindow.savedNormalX=mainWindow.x
                        mainWindow.savedNormalY=mainWindow.y
                        maxsizeRectangle.border.width=0
                        backMaxSizeImage.visible=true
                        maxsizeRectangle.isMaxSize = true
                        parallelAnimationGroupScreenMaxSize.start()
                    }
                    else
                    {
                        backMaxSizeImage.visible=false
                        maxsizeRectangle.border.width=2
                        maxsizeRectangle.isMaxSize = false
                        parallelAnimationGroupScreenBackMaxSize.start()
                    }
                }
            }
        }
        //关闭窗口
        Image{
            id:closeImage
            anchors.verticalCenter: parent.verticalCenter
            source: "/image/close.png"
            property string closeImageColorOverLay: "#2a1a22"
            layer.enabled: true
            layer.effect: ColorOverlay{
                source: closeImage
                color:closeImage.closeImageColorOverLay
            }
            MouseArea{
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    closeImage.closeImageColorOverLay = "#616161"
                }
                onExited: {
                    closeImage.closeImageColorOverLay = "#2a1a22"
                }
                onClicked: {
                    Qt.quit()
                }
            }
        }
    }
}
