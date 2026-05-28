import QtQuick
import QtQuick.Controls

import "../"
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

    property real sliderValueFrom: 0 //bind outSize, can't inner to change
    property real sliderValueTo: 100 //bind outSize, can't inner to change

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
        }
        Oran7SettingItem{
            id:slider
            showTag:false
            Oran7ValueSlider{
                anchors.left: parent.left
                anchors.leftMargin: progressHandleWidth/2 + 1
                anchors.verticalCenter: parent.verticalCenter
                width:parent.width * 0.7
                height: parent.height
                trackHeight: Oran7MainUiSetting.itemHeight * 0.5
                sliderColor_themeIndex:0
                progressHandleWidth:trackHeight*1.1
                from: root.sliderValueFrom
                to: root.sliderValueTo
                value: root.value
                thresholdMaximum: 65535
                stepSize: 1

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
                font.family: Oran7MainUiSetting.fontFamily
                font.pixelSize: Oran7MainUiSetting.textPixelSize
                color:Oran7MainUiSetting.textColor
                text:String(root.value) //initilized, not binding
            }
        }
    }
}
