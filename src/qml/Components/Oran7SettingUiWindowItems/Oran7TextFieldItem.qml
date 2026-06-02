import QtQuick
import QtQuick.Controls
import Oran7UI.Impl 1.0

import "../../Settings/GlobalSettings"

import Oran7UI.Impl

Item {
    id: root

    property string tempText: ""
    property alias textField: textField //当外部从引用主动操作改变文本时，一定要同时赋值tempText，否则会发生异常

    property string placeholderText: ""
    property color placeholderTextColor: Oran7Theme.Oran7MainGUI.themeColor
    Behavior on placeholderTextColor{PropertyAnimation{duration:Oran7Theme.Primary.durationMid}}

    property bool detectEnable: false
    // 0: NoDetection, 1: FileDetection, 2: ColorDetection
    property int detectType: Oran7MainUiSetting.DetectionType.NoDetection

    anchors.left: parent.left
    anchors.right: parent.right

    height: Oran7MainUiSetting.itemHeight

    signal enterPressed

    TextField {
        id: textField
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        width: root.width - 4 * 2
        background: Rectangle {
            anchors.fill: parent
            color: "transparent"
        }
        text: root.tempText
        color: Oran7Theme.Oran7MainGUI.themeColor
        Behavior on color{PropertyAnimation{duration:Oran7Theme.Primary.durationMid}}
        font.pixelSize: Oran7MainUiSetting.textPixelSize
        font.family: Oran7MainUiSetting.fontFamily
        verticalAlignment: Text.AlignVCenter
        Label {
            anchors.fill: parent
            text: root.placeholderText
            color: root.placeholderTextColor
            Behavior on color{PropertyAnimation{duration:Oran7Theme.Primary.durationMid}}
            font.pixelSize: Oran7MainUiSetting.textPixelSize
            font.family: Oran7MainUiSetting.fontFamily
            font.italic: true  // 只设置斜体
            verticalAlignment: Text.AlignVCenter
            visible: !textField.text && !textField.focus
        }
        onFocusChanged: {
            //console.log("focus:",focus)
            if(textField.focus)
                Oran7MainUiSetting.activeOtherItemCount++
            else
                Oran7MainUiSetting.activeOtherItemCount--
        }
    }
    //index_ line
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        color: Oran7Theme.Oran7MainGUI.themeColor
        Behavior on color{PropertyAnimation{duration:Oran7Theme.Primary.durationMid}}
        height: Oran7MainUiSetting.itemHeight * 0.067
    }
    //header_ line
    // Rectangle{
    //     color: Oran7MainUiSetting.tagColor
    //     width:2
    //     height: Oran7MainUiSetting.itemHeight
    //     anchors.left: parent.left
    // }

    Connections {
        target: globalEventFilter
        function onEnterKeyPressed() {
            if (textField.focus === false)
                return;
            textField.focus = false;
            if (!root.detectEnable || textField.length <= 0) {
                root.tempText = textField.text;
                root.enterPressed();
                return;
            }

            //detect based on type
            var isValid = false;
            switch (root.detectType) {
            case Oran7MainUiSetting.DetectionType.FileDetection:
                isValid = filehelper.fileExists(textField.text);
                if(isValid) textField.text = filehelper.lastProcessedPath()
                break;
            case Oran7MainUiSetting.DetectionType.ColorDetection:
                isValid = isValidColor(textField.text);
                break;
            case Oran7MainUiSetting.DetectionType.NoDetection:
            default:
                isValid = true;
                break;
            }

            if (isValid)
            {
                root.tempText = textField.text;
                root.enterPressed();
            }
            else
            {
                //backText
                textField.text = root.tempText;
            }
        }

        function isValidColor(colorStr) {
            // Check if the string matches RGB format #RRGGBB
            var hexRegex = /^#([0-9A-Fa-f]{6})$/;
            return hexRegex.test(colorStr);
        }
        function onKeyCombinationTriggered(CombinationS) {
            if (textField.focus === false)
                return;
            if (CombinationS === "LShift+Delete") {
                textField.text = "";
            }
        }
    }
    Connections {
        target: Oran7MainUiSetting
        function onClickedOutSide() {
            // 只有当 textField 在 focus 状态且点击的不是 TextField 本身时才处理
            if (textField.focus === false)
                return;
            textField.focus = false;
            //backText
            textField.text = root.tempText;
        }
    }

    // MouseArea {
    //     anchors.fill: parent
    //     hoverEnabled: true
    //     acceptedButtons: Qt.LeftButton
    //     onEntered: {
    //         cursorShape = Qt.IBeamCursor;
    //     }
    //     onExited: {
    //         cursorShape = Qt.ArrowCursor;
    //     }
    //     onClicked: {
    //         if(!textField.focus)
    //             textField.focus = true;
    //     }
    // }

    // --- tools ---
    Oran7FileHelper {
        id: filehelper
    }
}
