import QtQuick
import QtQuick.Controls
import Oran7UI.Impl
import "../../Settings/GlobalSettings"

// 方形预览作为入口，复用语言/字体菜单的定位、动画和焦点管理。
Oran7PreferenceChoice {
    id: root
    property color checkedColor: "#ffffff"
    // 保留旧调用参数兼容；菜单使用固定预设，主题内部仍自行生成色阶。
    property string componentName: "Oran7MainGUI"
    property string colorTokenName: "colorPrimaryBase"
    property int displayAmount: 10
    property string title: ""
    property string pickerLabel: qsTranslate("Oran7Ui", "自定义")
    property var presetColors: ["#A78BFA", "#60A5FA", "#818CF8", "#22D3EE", "#34D399",
                                "#A3E635", "#FBBF24", "#FB923C", "#FB7185", "#F472B6"]
    property var presetNames: [qsTranslate("Oran7Ui", "鸢尾紫"), qsTranslate("Oran7Ui", "晴空蓝"),
                              qsTranslate("Oran7Ui", "靛青蓝"), qsTranslate("Oran7Ui", "冰川青"),
                              qsTranslate("Oran7Ui", "薄荷绿"), qsTranslate("Oran7Ui", "青柠绿"),
                              qsTranslate("Oran7Ui", "琥珀金"), qsTranslate("Oran7Ui", "落日橙"),
                              qsTranslate("Oran7Ui", "珊瑚红"), qsTranslate("Oran7Ui", "樱花粉")]
    property var picker: null
    signal colorReady(color selectedColor)

    implicitWidth: Oran7MainUiSetting.itemHeight * 0.7
    implicitHeight: implicitWidth
    anchors.left: parent.right
    anchors.verticalCenter: parent.verticalCenter
    popupTitle: title
    leftPadding: 0
    rightPadding: 0
    popupStartsAtTop: true
    colorSamples: [root.checkedColor].concat(root.presetColors)
    model: [root.pickerLabel].concat(root.presetNames)
    subtitles: colorSamples.map(function(color) { return String(color).toUpperCase() })
    function selectedIndex() {
        for (var i = 1; i < colorSamples.length; ++i)
            if (Qt.colorEqual(colorSamples[i], checkedColor)) return i
        return 0
    }
    currentIndex: selectedIndex()
    onActivated: function(index) {
        if (index === 0) openColorPicker()
        else if (index > 0 && index < colorSamples.length) root.colorReady(colorSamples[index])
        // 选择后恢复外部权威颜色的索引绑定。
        currentIndex = Qt.binding(function() { return root.selectedIndex() })
    }
    contentItem: Item {}
    indicator: Item {}
    background: Rectangle {
        radius: 5
        color: root.checkedColor
        border.width: root.hovered || root.popup.visible ? 2 : 1
        border.color: root.popup.visible ? root.accentColor : Oran7MainUiSetting.textColor
        scale: root.down ? 0.94 : (root.hovered ? 1.05 : 1)
        Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
    }
    ToolTip.text: String(checkedColor).toUpperCase()

    function openColorPicker() {
        if (picker) { picker.requestActivate(); return }
        var component = Qt.createComponent(Qt.resolvedUrl("../Oran7GlobalComponents/Oran7ColorPickerWindow.qml"))
        if (component.status !== Component.Ready) {
            console.warn(component.errorString())
            return
        }
        picker = component.createObject(root, {
            selectedColor: root.checkedColor,
            transientParent: root.Window.window
        })
        if (!picker) return
        var pos = root.mapToGlobal(0, root.height + 6)
        picker.x = pos.x - picker.width / 2
        picker.y = pos.y
        picker.colorSelected.connect(function(color) { root.colorReady(color) })
        // 取色窗口负责关闭动画、计数及销毁，避免提前销毁泄漏焦点计数。
        picker.showPicker()
    }
}
