/*
 * Oran7CaptionBar
 *
 * Oran7MediaClient 自定义标题栏组件
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects

import Oran7UI.Impl 1.0
import "../../Settings/GlobalSettings"

Rectangle {
    id: control

    property var targetWindow: null
    property Oran7WindowAgent windowAgent: null
    readonly property bool windowExpanded: targetWindow?.visibility === Window.Maximized
                                           || targetWindow?.visibility === Window.FullScreen

    property alias layoutDirection: __row.layoutDirection

    property bool mirrored: false
    property string winIcon: ""
    property alias winIconWidth: __winIconLoader.width
    property alias winIconHeight: __winIconLoader.height
    property alias showWinIcon: __winIconLoader.visible

    property string winTitle: targetWindow?.title ?? ""
    property font winTitleFont: Qt.font({
        family: "Segoe UI",
        pixelSize: 14
    })
    property color winTitleColor: "#ffffff"
    property alias showWinTitle: __winTitleLoader.visible

    property bool showReturnButton: false
    property bool showThemeButton: false
    property bool topButtonChecked: false
    property bool showTopButton: false
    property bool showMinimizeButton: Qt.platform.os !== 'osx'
    property bool showMaximizeButton: Qt.platform.os !== 'osx'
    property bool showCloseButton: Qt.platform.os !== 'osx'
    property real windowButtonsOpacity: 1
    readonly property bool windowButtonsHovered: __windowButtonsLoader.item
        ? __windowButtonsLoader.item.hovered : false

    property var returnCallback: () => {}
    property var themeCallback: () => {}
    property var topCallback: checked => {}
    property var minimizeCallback: () => {
        if (targetWindow) {
            targetWindow.showMinimized();
        }
    }
    property var maximizeCallback: () => {
        if (!targetWindow)
            return;

        if (windowExpanded) {
            targetWindow.showNormal();
        } else {
            targetWindow.showMaximized();
        }
    }
    property var closeCallback: () => {
        if (targetWindow)
            targetWindow.close();
    }
    property string contentDescription: winTitle

    property Component winNavButtonsDelegate: Row {
        layoutDirection: control.mirrored ? Qt.RightToLeft : Qt.LeftToRight

        Button {
            id: __returnButton
            height: parent.height
            flat: true
            display: Button.IconOnly
            icon.source: ""
            visible: control.showReturnButton
            onClicked: control.returnCallback()
            ToolTip.text: qsTranslate("Oran7Ui", '返回')
            ToolTip.visible: hovered
            ToolTip.delay: 1000
        }
    }

    property Component winIconDelegate: Image {
        width: 24
        height: 24
        source: control.winIcon
        sourceSize.width: width
        sourceSize.height: height
        mipmap: true
    }

    property Component winTitleDelegate: Text {
        text: control.winTitle
        color: control.winTitleColor
        font: control.winTitleFont
        elide: Text.ElideRight
    }

    property Component winPresetButtonsDelegate: Row {
        layoutDirection: control.mirrored ? Qt.RightToLeft : Qt.LeftToRight

        Connections {
            target: control
            function onWindowAgentChanged() {
                control.addInteractionItem(__themeButton);
                control.addInteractionItem(__topButton);
            }
        }

        Button {
            id: __themeButton
            height: parent.height
            flat: true
            display: Button.IconOnly
            icon.source: ""  // 简化主题图标
            visible: control.showThemeButton
            onClicked: control.themeCallback()
            ToolTip.text: qsTranslate("Oran7Ui", '切换主题')
            ToolTip.visible: hovered
            ToolTip.delay: 1000
        }

        Button {
            id: __topButton
            height: parent.height
            flat: true
            display: Button.IconOnly
            checkable: true
            checked: control.topButtonChecked
            icon.source: ""
            visible: control.showTopButton
            onClicked: control.topCallback(checked)
            ToolTip.text: qsTranslate("Oran7Ui", '置顶')
            ToolTip.visible: hovered
            ToolTip.delay: 1000
        }
    }

    property Component winExtraButtonsDelegate: Item {}

    // 窗口控制按钮通用组件
    component WindowControlButton: Rectangle {
        id: button
        color: buttonColor
        radius: 4

        property color buttonColor: "transparent"
        property color hoverColor: "#c42b1c"
        property string iconText: "✕"
        property int buttonTextPixelSize: 15
        property Component iconDelegate: Text {
            text: button.iconText
            color: "#ffffff"
            font.pixelSize: button.buttonTextPixelSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        readonly property bool hovered: mouseArea.containsMouse

        signal clicked

        // 颜色过渡动画
        Behavior on color {
            ColorAnimation {
                duration: 150
                easing.type: Easing.InOutCubic
            }
        }

        // 透明度过渡动画
        Behavior on opacity {
            NumberAnimation {
                duration: 150
                easing.type: Easing.InOutCubic
            }
        }

        // 按下效果
        scale: mouseArea.pressed ? 0.95 : 1.0

        Behavior on scale {
            NumberAnimation {
                duration: 100
                easing.type: Easing.OutBack
            }
        }

        Loader {
            id: buttonIcon
            anchors.centerIn: parent
            sourceComponent: button.iconDelegate
            opacity: 0.8

            // 图标透明度过渡
            Behavior on opacity {
                NumberAnimation {
                    duration: 150
                    easing.type: Easing.InOutCubic
                }
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            propagateComposedEvents: true

            onEntered: {
                button.color = hoverColor;
                buttonIcon.opacity = 1.0;
            }

            onExited: {
                button.color = button.buttonColor;
                buttonIcon.opacity = 0.8;
            }

            onClicked: {
                button.clicked();
            }

            onPressed: {
                button.scale = 0.95;
            }

            onReleased: {
                button.scale = 1.0;
            }

            onCanceled: {
                button.scale = 1.0;
            }
        }
    }

    property Component winButtonsDelegate: Row {
        id: nativeWindowControls
        readonly property bool hovered: __closeButton.hovered || __maximizeButton.hovered || __minimizeButton.hovered
        width: (Oran7Theme.Oran7MainGUI.topBarDefaultHeight + spacing) * 3
        height: Oran7Theme.Oran7MainGUI.topBarDefaultHeight * 0.8
        // anchors.margins: Oran7Theme.Oran7MainGUI.topBarDefaultHeight * 0.1
        spacing: 4
        layoutDirection: Qt.RightToLeft

        Connections {
            target: control
            function onWindowAgentChanged() {
                if (windowAgent) {
                    windowAgent.setSystemButton(windowAgent.Minimize, __minimizeButton);
                    windowAgent.setSystemButton(windowAgent.Maximize, __maximizeButton);
                    windowAgent.setSystemButton(windowAgent.Close, __closeButton);
                }
            }
        }

        // 关闭按钮
        WindowControlButton {
            id: __closeButton
            width: Oran7Theme.Oran7MainGUI.topBarDefaultHeight
            height: Oran7Theme.Oran7MainGUI.topBarDefaultHeight * 0.8
            visible: control.showCloseButton
            buttonColor: "transparent"
            hoverColor: "#c42b1c"
            iconText: "✕"
            buttonTextPixelSize: 15
            onClicked: control.closeCallback()
        }

        // 最大化/还原按钮
        WindowControlButton {
            id: __maximizeButton
            width: Oran7Theme.Oran7MainGUI.topBarDefaultHeight
            height: Oran7Theme.Oran7MainGUI.topBarDefaultHeight * 0.8
            visible: control.showMaximizeButton
            buttonColor: "transparent"
            hoverColor: Qt.rgba(0.2, 0.2, 0.2, 0.5)
            iconDelegate: Shape {
                width: 11
                height: 11

                ShapePath {
                    fillColor: "transparent"
                    strokeColor: "#ffffff"
                    strokeWidth: 1

                    PathSvg {
                        path: control.windowExpanded
                              ? "M 2.5 2.5 V 0.5 H 10.5 V 8.5 H 8.5 M 0.5 2.5 H 8.5 V 10.5 H 0.5 Z"
                              : "M 0.5 0.5 H 10.5 V 10.5 H 0.5 Z"
                    }
                }
            }
            onClicked: control.maximizeCallback()
        }

        // 最小化按钮
        WindowControlButton {
            id: __minimizeButton
            width: Oran7Theme.Oran7MainGUI.topBarDefaultHeight
            height: Oran7Theme.Oran7MainGUI.topBarDefaultHeight * 0.8
            visible: control.showMinimizeButton
            buttonColor: "transparent"
            hoverColor: Qt.rgba(0.2, 0.2, 0.2, 0.5)
            iconText: "━"
            buttonTextPixelSize: 15
            onClicked: control.minimizeCallback()
        }
    }

    function addInteractionItem(item) {
        if (windowAgent)
            windowAgent.setHitTestVisible(item, true);
    }

    function removeInteractionItem(item) {
        if (windowAgent)
            windowAgent.setHitTestVisible(item, false);
    }

    objectName: '__Oran7CaptionBar__'
    color: "transparent"

    // 左侧事件穿透区域（x=0到x=260），不参与窗口拖拽
    Item {
        id: __hitTestPassThroughArea
        width: 260
        height: parent.height
        anchors.left: parent.left
        anchors.top: parent.top

        Component.onCompleted: {
            if (control.windowAgent) {
                // 标记此区域为命中测试可见，这样它就不会参与窗口拖拽
                control.windowAgent.setHitTestVisible(__hitTestPassThroughArea, true);
            }
        }

        Connections {
            target: control
            function onWindowAgentChanged() {
                if (control.windowAgent) {
                    control.windowAgent.setHitTestVisible(__hitTestPassThroughArea, true);
                }
            }
        }
    }

    RowLayout {
        id: __row
        anchors.fill: parent
        layoutDirection: control.mirrored ? Qt.RightToLeft : Qt.LeftToRight
        spacing: 0

        Loader {
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignVCenter
            sourceComponent: control.winNavButtonsDelegate
        }

        Item {
            id: __title
            Layout.fillWidth: true
            Layout.fillHeight: true
            Component.onCompleted: {
                if (control.windowAgent)
                    control.windowAgent.setTitleBar(__title);
            }

            Row {
                height: parent.height
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: Qt.platform.os === 'osx' ? parent.horizontalCenter : undefined
                layoutDirection: control.mirrored ? Qt.RightToLeft : Qt.LeftToRight
                spacing: 8

                Loader {
                    id: __winIconLoader
                    width: 24
                    height: 24
                    anchors.verticalCenter: parent.verticalCenter
                    sourceComponent: control.winIconDelegate
                }

                Loader {
                    id: __winTitleLoader
                    anchors.verticalCenter: parent.verticalCenter
                    sourceComponent: control.winTitleDelegate
                }
            }
        }

        Loader {
            Layout.fillHeight: false
            Layout.alignment: Qt.AlignVCenter
            sourceComponent: control.winPresetButtonsDelegate
        }

        Loader {
            Layout.fillHeight: false
            Layout.alignment: Qt.AlignVCenter
            sourceComponent: control.winExtraButtonsDelegate
        }

        Loader {
            id: __windowButtonsLoader
            objectName: "nativeWindowButtons"
            Layout.fillHeight: false
            Layout.alignment: Qt.AlignVCenter
            sourceComponent: control.winButtonsDelegate
            opacity: Math.max(0, Math.min(1, control.windowButtonsOpacity))
            visible: opacity > 0
            enabled: opacity > 0.1
        }
    }

    Accessible.role: Accessible.TitleBar
    Accessible.name: control.contentDescription
    Accessible.description: control.contentDescription
}
