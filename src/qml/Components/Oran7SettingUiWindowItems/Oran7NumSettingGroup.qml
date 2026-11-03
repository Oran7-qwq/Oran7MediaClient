import QtQuick
import QtQuick.Controls

import "../Oran7GlobalComponents"
import "../../Settings/GlobalSettings"

import Oran7UI.Impl

Item {
    id:root
    anchors.left: parent.left
    anchors.right: parent.right
    height:item1.height +slider.height

    // --- api ---

    property real value: 0 //bind outSize, can't inner to change
    onValueChanged: {valueLabel.text = String(value)}
    property string title: "title"
    property real rowHeight: Oran7MainUiSetting.itemHeight
    property string fontFamily: Oran7MainUiSetting.fontFamily
    property real fontPixelSize: Oran7MainUiSetting.textPixelSize
    property color textColor: Oran7MainUiSetting.textColor
    property color accentColor: "#00DDDD"
    property color accentHoverColor: "cyan"
    property color hoverColor: Oran7MainUiSetting.itemHoverdColor

    property real sliderValueFrom: 0
    property real sliderValueTo: 100
    property real stepSize: 0
    property real valueDecimals: 0

    // 外部提交失败时，恢复滑动预览标签到已绑定的实际值。
    function resetValueDisplay() { valueLabel.text = String(root.value) }

    // Slider拖动过程中触发
    signal moved(real value, int thresholdPosition, real ratio)

    // Slider鼠标松开时触发
    signal committed(real value, int thresholdPosition, real ratio)

    // --- Ui ---
    Column{
        anchors.top: root.top
        anchors.left: parent.left
        anchors.right: parent.right
        Oran7SettingItem{
            id:item1
            text:root.title
            showTag:false
            rowHeight: root.rowHeight
            fontFamily: root.fontFamily
            fontPixelSize: root.fontPixelSize
            textColor: root.textColor
            tagColor: root.textColor
            hoverColor: root.hoverColor
        }
        Oran7SettingItem{
            id:slider
            showTag:false
            rowHeight: root.rowHeight
            fontFamily: root.fontFamily
            fontPixelSize: root.fontPixelSize
            textColor: root.textColor
            tagColor: root.textColor
            hoverColor: root.hoverColor
            Oran7ValueSlider{
                objectName: root.objectName + "Slider"
                anchors.left: parent.left
                anchors.leftMargin: progressHandleWidth/2 + 1
                anchors.verticalCenter: parent.verticalCenter
                width:parent.width * 0.8
                height: parent.height
                trackHeight: root.rowHeight * 0.35
                sliderColor_themeIndex:0
                sliderColor_themeItems: [{default_Color: root.accentColor, sel_Color: root.accentHoverColor}]
                progressHandleWidth:trackHeight*1.3
                from: root.sliderValueFrom
                to: root.sliderValueTo
                value: root.value
                thresholdMaximum: 65535
                stepSize: root.stepSize
                valueDecimals:root.valueDecimals

                onCommitted: (value, thresholdPosition, ratio) => {
                    root.committed(value, thresholdPosition, ratio)
                    valueLabel.text = String(value)
                }
                onMoved: (value, thresholdPosition, ratio) =>{
                    root.moved(value, thresholdPosition, ratio)
                    valueLabel.text = String(value)
                }
            }
            anchors.right: parent.right
            Label{
                id:valueLabel
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                font.family: root.fontFamily
                font.pixelSize: root.fontPixelSize
                color: root.textColor
                text:String(root.value) //initilized, not binding
            }
        }
    }
}
