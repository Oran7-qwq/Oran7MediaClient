import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import QtMultimedia

import "../Settings/GlobalSettings"

import Oran7UI.Impl

Window {
    id: root
    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.Dialog
    modality: Qt.ApplicationModal
    width: 350
    height: 400
    opacity: 0

    // 可配置属性
    property color selectedColor: "#ffffff"
    property int windowRadius: 10

    // 信号
    signal colorSelected(color selectedColor)
    signal colorPickerCanceled()

    // 显示窗口函数
    function showPicker() {
        root.visible = true
        root.opacity = 0
        Oran7MainUiSetting.activeOtherItemCount ++;
        window_openAnimation.start()
    }

    // 内部颜色属性 (HSV + Alpha)
    property real hue: 0.0
    property real saturation: 1.0
    property real value: 1.0
    property real alpha: 1.0

    readonly property color currentColor: Qt.hsva(hue, saturation, value, alpha)

    function updateColorFromHsv(h, s, v) {
        hue = h
        saturation = s
        value = v
    }

    function updateColorFromRGB(r, g, b) {
        var color = Qt.rgba(r, g, b, alpha)
        hue = color.hsvHue
        saturation = color.hsvSaturation
        value = color.hsvValue
    }

    function colorToHexString(color) {
        var r = Math.round(color.r * 255).toString(16).padStart(2, '0').toUpperCase()
        var g = Math.round(color.g * 255).toString(16).padStart(2, '0').toUpperCase()
        var b = Math.round(color.b * 255).toString(16).padStart(2, '0').toUpperCase()
        return r + g + b
    }

    function getContrastColor(color) {
        var brightness = (color.r * 299 + color.g * 587 + color.b * 114) / 1000
        return brightness > 0.5 ? "black" : "white"
    }

    property point clickPos: Qt.point(0, 0)
    property bool mouseIsPressed: false

    // 初始化选中的颜色
    Component.onCompleted: {
        if (String(selectedColor).toLowerCase() !== "#ffffff") {
            //console.log("selectedColor:",selectedColor)
            updateColorFromRGB(selectedColor.r, selectedColor.g, selectedColor.b)
        }
    }

    // 打开动画
    ParallelAnimation {
        id: window_openAnimation
        readonly property real aniDuration: Oran7MainUiSetting.toggleOpenAniDuration

        PropertyAnimation {
            target: root
            property: "opacity"
            from: 0
            to: 1
            duration: window_openAnimation.aniDuration
            easing.type: Easing.OutCubic
        }

        onFinished: {
            // 动画完成后，恢复height的绑定
            root.height = Qt.binding(function() { return 400; });
        }
    }

    // 关闭动画
    ParallelAnimation {
        id: window_closeAnimation
        readonly property real aniDuration: Oran7MainUiSetting.toggleOpenAniDuration

        PropertyAnimation {
            target: root
            property: "opacity"
            from: 1
            to: 0
            duration: window_closeAnimation.aniDuration
            easing.type: Easing.InExpo
        }

        PropertyAnimation {
            target: root
            property: "y"
            from: 100
            to: 0
            duration: window_closeAnimation.aniDuration
            easing.type: Easing.InExpo
        }
        onStarted: {
            root.visible = false;
            Oran7MainUiSetting.activeOtherItemCount --;
        }

        onFinished: {
            root.colorPickerCanceled();
            root.destroy()
        }
    }

    // 主容器
    Rectangle {
        id: ui_root
        anchors.fill: parent
        color: "transparent"
        radius: windowRadius

        // 阴影效果
        layer.enabled: true
        layer.effect: DropShadow {
            horizontalOffset: 0
            verticalOffset: 5
            radius: 12
            samples: 16
            spread: 0.7
            color: Oran7MainUiSetting.winShadowColor
            transparentBorder: true
        }

        Rectangle {
            id: ui_content
            anchors.fill: parent
            anchors.margins: 8
            color: Oran7MainUiSetting.backColor
            radius: windowRadius
            opacity: 1

            // 标题栏
            Rectangle {
                id: titleBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 40
                color: Oran7MainUiSetting.itemBackColor
                radius: windowRadius

                Text {
                    anchors.centerIn: parent
                    text: "Color Picker"
                    font.pixelSize: 16
                    font.family: Oran7MainUiSetting.fontFamily
                    color: Oran7MainUiSetting.textColor
                    font.bold: true
                }
            }

            // 内容区域
            Item {
                id: contentArea
                anchors.top: titleBar.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom

                // 颜色选择区域 (色相+饱和度+明度)
                Rectangle {
                    id: colorPreview
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 12
                    width: 200
                    height: 200
                    radius: 8

                    // 基础色相背景
                    Rectangle {
                        anchors.fill: parent
                        color: Qt.hsva(hue, 1, 1, 1)
                        radius: 8
                    }

                    // 水平饱和度渐变
                    Rectangle {
                        anchors.fill: parent
                        radius: 8
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#ffffffff" }
                            GradientStop { position: 1.0; color: "#00ffffff" }
                        }
                    }

                    // 垂直明度渐变
                    Rectangle {
                        anchors.fill: parent
                        radius: 8
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0.0; color: "#00000000" }
                            GradientStop { position: 1.0; color: "#ff000000" }
                        }
                    }

                    // 选择指示器
                    Rectangle {
                        x: saturation * (parent.width - 12)
                        y: (1 - value) * (parent.height - 12)
                        width: 12
                        height: 12
                        radius: 6
                        color: "transparent"
                        border.width: 2
                        border.color: Oran7MainUiSetting.isDark ? "black" : "white"
                    }

                    MouseArea {
                        anchors.fill: parent
                        onPressed: mouse => handlePreviewMouse(mouse)
                        onPositionChanged: mouse => handlePreviewMouse(mouse)

                        function handlePreviewMouse(mouse) {
                            saturation = Math.max(0, Math.min(1, mouse.x / width))
                            value = Math.max(0, Math.min(1, 1 - mouse.y / height))
                        }
                    }
                }

                // 色相滑块
                Rectangle {
                    id: hueSlider
                    anchors.left: parent.left
                    anchors.top: colorPreview.bottom
                    anchors.margins: 12
                    width: 200
                    height: 12

                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.00; color: "#ff0000" }
                        GradientStop { position: 0.17; color: "#ffff00" }
                        GradientStop { position: 0.33; color: "#00ff00" }
                        GradientStop { position: 0.50; color: "#00ffff" }
                        GradientStop { position: 0.67; color: "#0000ff" }
                        GradientStop { position: 0.83; color: "#ff00ff" }
                        GradientStop { position: 1.00; color: "#ff0000" }
                    }
                    radius: 8

                    // 色相指示器
                    Rectangle {
                        x: hue * (parent.width - 14)
                        anchors.verticalCenter: parent.verticalCenter
                        width: 14
                        height: 14
                        radius: 7
                        color: Qt.hsva(hue, 1, 1, 1)
                        border.width: 1
                        border.color: Oran7MainUiSetting.isDark ? "black" : "white"
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onPressed: mouse => hue = Math.max(0, Math.min(1, mouse.x / width))
                        onPositionChanged: mouse => hue = Math.max(0, Math.min(1, mouse.x / width))
                    }
                }

                // 右侧控制区域
                Column {
                    anchors.left: hueSlider.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    anchors.right: parent.right
                    spacing: 12

                    // 当前颜色预览
                    Rectangle {
                        width: 100
                        height: 100
                        radius: 8
                        border.width: 1
                        border.color: Oran7MainUiSetting.itemBackColor
                        color: currentColor

                        Text {
                            anchors.centerIn: parent
                            text: colorToHexString(currentColor)
                            font.pixelSize: 14
                            font.family: Oran7MainUiSetting.fontFamily
                            color: getContrastColor(currentColor)
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                var colorString = colorToHexString(currentColor)
                                Qt.clipboard.text = "#" + colorString
                            }
                        }
                    }

                    // 预设颜色
                    Grid {
                        columns: 3
                        spacing: 6
                        width: parent.width

                        Repeater {
                            model: presetColors
                            delegate: Rectangle {
                                width: (parent.width - parent.spacing * (parent.columns - 1)) / parent.columns
                                height: width
                                radius: 6
                                color: model.color

                                Text {
                                    anchors.centerIn: parent
                                    text: "✓"
                                    font.pixelSize: 16
                                    font.family: Oran7MainUiSetting.fontFamily
                                    color: getContrastColor(model.color)
                                    font.bold: true
                                    visible: currentColor === model.color
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        updateColorFromRGB(model.color.r, model.color.g, model.color.b)
                                    }
                                }
                            }
                        }
                    }

                    // 确认按钮
                    Rectangle {
                        width: parent.width
                        height: 40
                        radius: 8
                        color: "#4CAF50"

                        Text {
                            anchors.centerIn: parent
                            text: "OK"
                            font.pixelSize: 14
                            font.family: Oran7MainUiSetting.fontFamily
                            color: "white"
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onPressed: parent.color = "#43A047"
                            onReleased: parent.color = "#4CAF50"
                            onClicked: {
                                root.selectedColor = currentColor
                                root.colorSelected(currentColor)
                                window_closeAnimation.start();
                                Oran7Theme.in
                            }
                        }
                    }
                }
            }
        }
    }

    // 预设颜色列表
    // ListModel {
    //     id: presetColors
    //     ListElement { color: "#FFEBEE" }
    //     ListElement { color: "#FFCDD2" }
    //     ListElement { color: "#EF9A9A" }
    //     ListElement { color: "#EF476F" }
    //     ListElement { color: "#F48FB1" }
    //     ListElement { color: "#F06292" }
    //     ListElement { color: "#E76F51" }
    //     ListElement { color: "#D98880" }
    //     ListElement { color: "#80ED99" }
    //     ListElement { color: "#00B894" }
    //     ListElement { color: "#00A8C5" }
    //     ListElement { color: "#0077B6" }
    //     ListElement { color: "#4834D4" }
    //     ListElement { color: "#9D4EDD" }
    //     ListElement { color: "#E59866" }
    //     ListElement { color: "#606C38" }
    // }
    ListModel {
          id: presetColors

          Component.onCompleted: {
              presetColors.append({ "color": Oran7ColorGenerator.presetToColor("#Preset_Red") })
              presetColors.append({ "color": Oran7ColorGenerator.presetToColor("#Preset_Volcano") })
              presetColors.append({ "color": Oran7ColorGenerator.presetToColor("#Preset_Orange") })
              presetColors.append({ "color": Oran7ColorGenerator.presetToColor("#Preset_Gold") })
              presetColors.append({ "color": Oran7ColorGenerator.presetToColor("#Preset_Yellow") })
              presetColors.append({ "color": Oran7ColorGenerator.presetToColor("#Preset_Lime") })
              presetColors.append({ "color": Oran7ColorGenerator.presetToColor("#Preset_Green") })
              presetColors.append({ "color": Oran7ColorGenerator.presetToColor("#Preset_Cyan") })
              presetColors.append({ "color": Oran7ColorGenerator.presetToColor("#Preset_Blue") })
              presetColors.append({ "color": Oran7ColorGenerator.presetToColor("#Preset_Geekblue") })
              presetColors.append({ "color": Oran7ColorGenerator.presetToColor("#Preset_Purple") })
              presetColors.append({ "color": Oran7ColorGenerator.presetToColor("#Preset_Magenta") })
              presetColors.append({ "color": Oran7ColorGenerator.presetToColor("#Preset_Grey") })
          }
      }

    // 拖动区域
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        onPressed: function(mouse) {
            root.mouseIsPressed = true
            root.clickPos = Qt.point(mouse.x, mouse.y)

            // 检查是否在标题栏区域（用于拖动）
            let inTitleBar = mouse.y <= titleBar.height

            if (inTitleBar) {
                mouse.accepted = true // 标题栏拖动事件被接受
            } else {
                mouse.accepted = false // 其他区域事件传递给子组件
                Oran7MainUiSetting.clickedOutSide()
            }
        }
        onReleased: function(mouse) {
            root.mouseIsPressed = false
            mouse.accepted = false
        }
        onPositionChanged: function(mouse) {
            if (root.mouseIsPressed === false) return
            if (root.clickPos.y > titleBar.height || root.clickPos.y < 0) return

            let delta = Qt.point(mouse.x - root.clickPos.x, mouse.y - root.clickPos.y)
            root.x += delta.x
            root.y += delta.y
        }
        onClicked: function(mouse) {
            mouse.accepted = false // 允许事件继续传递给子组件
        }
    }

    // --- Delay Timer ---
    Oran7DelayTimer {
        id: delayTimer
    }
}
