//Oran7OptionSettingGroup.qml
// 通用「多选项」设置组：标题行显示当前选中项，展开后列出所有可选项点选
// 用法：
//   Oran7OptionSettingGroup {
//       title: "抓屏源"
//       options: [{label:"显示器1", sub:"1920x1080 主", enabled:true}, ...]
//       currentIndex: 0
//       onOptionSelected: function(index) { ... }
//   }
import QtQuick
import QtQuick.Controls

import "../../Settings/GlobalSettings"
import "../Oran7GlobalComponents"

import Oran7UI.Impl

Item {
    id: root
    anchors.left: parent.left
    anchors.right: parent.right
    height: column.height

    // --- api ---
    property string title: "title"
    /*! @note 选项数组，元素形如 {label, sub, enabled} */
    property var options: []
    /*! @note 当前选中项下标，-1 表示无选中 */
    property int currentIndex: -1
    /*! @note 展开状态，外部可控制 */
    property bool expand: false

    signal optionSelected(int index)

    /*! @note 把选中项拼成一行可读文本 */
    function labelAt(index) {
        if (index < 0 || index >= options.length)
            return "--"
        var opt = options[index]
        if (!opt)
            return "--"
        return String(opt.label || "") + (opt.sub ? "  " + opt.sub : "")
    }

    Column {
        id: column
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        Oran7SettingItem {
            id: header
            text: root.title + "：" + root.labelAt(root.currentIndex)
            enableMouseArea: true
            enableHoverHandler: true
            onLeftClicked: root.expand = !root.expand
            onRightClicked: root.expand = !root.expand
        }

        Oran7ExpandItem {
            expand: root.expand

            Column {
                // Oran7SettingItem 自带 anchors.left/right 绑定，无需在此指定
                Repeater {
                    model: root.options

                    delegate: Oran7SettingItem {
                        required property int index
                        required property var modelData

                        readonly property bool optionEnabled: modelData && modelData.enabled !== undefined
                                                             ? modelData.enabled : true

                        showTag: false
                        radius: 4
                        fontBold: true
                        text: root.labelAt(index)
                        textColor: index === root.currentIndex
                                   ? Oran7MainUiSetting.themeColor
                                   : Oran7MainUiSetting.textColor
                        opacity: optionEnabled ? 1 : 0.4

                        MouseArea {
                            anchors.fill: parent
                            enabled: parent.optionEnabled
                            hoverEnabled: true
                            onClicked: root.optionSelected(index)
                        }
                    }
                }
            }
        }
    }
}