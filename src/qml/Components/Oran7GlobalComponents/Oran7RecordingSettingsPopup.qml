import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    property var controller
    property var draft: ({})
    property string message: ""
    width: Math.min(660, parent ? parent.width - 24 : 660)
    height: Math.min(650, parent ? parent.height - 24 : 650)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    modal: true
    padding: 18
    onOpened: {
        draft = controller.recordingSettings()
        message = ""
        options.model = []
        options.model = [
            {key:"outputDirectory", label:qsTranslate("Oran7Ui", "输出目录（留空使用视频目录）"), secret:false},
            {key:"filePrefix", label:qsTranslate("Oran7Ui", "文件名前缀（自动追加时间和随机后缀）"), secret:false},
            {key:"width", label:qsTranslate("Oran7Ui", "输出宽度（宽高同时为 0 使用原始尺寸）"), secret:false},
            {key:"height", label:qsTranslate("Oran7Ui", "输出高度"), secret:false},
            {key:"fps", label:qsTranslate("Oran7Ui", "录制帧率上限（1～240）"), secret:false},
            {key:"bitrateKbps", label:qsTranslate("Oran7Ui", "视频码率（kbps）"), secret:false},
            {key:"whipUrl", label:qsTranslate("Oran7Ui", "WHIP 完整接口地址"), secret:false},
            {key:"bearerToken", label:qsTranslate("Oran7Ui", "WHIP Bearer 凭据"), secret:true},
            {key:"stunUrl", label:qsTranslate("Oran7Ui", "STUN 地址（可选）"), secret:false},
            {key:"turnUrl", label:qsTranslate("Oran7Ui", "TURN 地址（可选）"), secret:false},
            {key:"turnUser", label:qsTranslate("Oran7Ui", "TURN 用户名"), secret:false},
            {key:"turnPassword", label:qsTranslate("Oran7Ui", "TURN 密码"), secret:true},
            {key:"signalingUrl", label:qsTranslate("Oran7Ui", "房间信令 WS/WSS 地址（可选）"), secret:false},
            {key:"signalingUser", label:qsTranslate("Oran7Ui", "信令用户名"), secret:false},
            {key:"signalingPassword", label:qsTranslate("Oran7Ui", "信令密码"), secret:true},
            {key:"roomId", label:qsTranslate("Oran7Ui", "房间名称"), secret:false}
        ]
    }
    ColumnLayout {
        anchors.fill: parent
        Label { text: qsTranslate("Oran7Ui", "录制与推流设置"); font.pixelSize: 20 }
        Label { text: qsTranslate("Oran7Ui", "文件录制会生成独立 MP4；推流需提供有效 WHIP 服务。"); wrapMode: Text.Wrap; Layout.fillWidth: true }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                ComboBox {
                    Layout.fillWidth: true
                    model: [qsTranslate("Oran7Ui", "保存 MP4"), qsTranslate("Oran7Ui", "WHIP 推流")]
                    currentIndex: root.draft.mode === undefined ? 0 : root.draft.mode
                    onActivated: root.draft.mode = currentIndex
                }
                CheckBox {
                    text: qsTranslate("Oran7Ui", "采集系统音频")
                    checked: root.draft.audioEnabled === true
                    onToggled: root.draft.audioEnabled = checked
                }
                Repeater {
                    id: options
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Label { text: modelData.label }
                        TextField {
                            objectName: modelData.key
                            Layout.fillWidth: true
                            text: root.draft[modelData.key] === undefined ? "" : String(root.draft[modelData.key])
                            echoMode: modelData.secret ? TextInput.Password : TextInput.Normal
                            selectByMouse: true
                            onTextEdited: {
                                var key = modelData.key
                                root.draft[key] = ["width","height","fps","bitrateKbps"].indexOf(key) >= 0 ? Number(text) : text
                            }
                        }
                    }
                }
            }
        }
        Label { text: root.message; wrapMode: Text.Wrap; Layout.fillWidth: true }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button { text: qsTranslate("Oran7Ui", "取消"); onClicked: root.close() }
            Button {
                objectName: "saveRecordingSettings"
                text: qsTranslate("Oran7Ui", "保存")
                enabled: root.controller && !root.controller.recordingBusy
                onClicked: {
                    var result = root.controller.applyRecordingSettings(root.draft)
                    root.message = result.msg
                    if (result.ok) root.close()
                }
            }
        }
    }
}
