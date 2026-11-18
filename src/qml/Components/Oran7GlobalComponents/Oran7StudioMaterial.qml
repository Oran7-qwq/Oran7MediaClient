import QtQuick

// 共用 128px 平铺颗粒，不生成大尺寸噪声纹理，不依赖模糊或持续动画。
Item {
    id: root
    property real grainOpacity: 0.035
    property color highlightColor: "#ffffff"
    Image {
        anchors.fill: parent
        source: Qt.resolvedUrl("../../../../image/oran7_studio_grain.png")
        fillMode: Image.Tile
        smooth: false
        opacity: root.grainOpacity
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.rgba(root.highlightColor.r, root.highlightColor.g, root.highlightColor.b, 0.025) }
            GradientStop { position: 0.48; color: "transparent" }
            GradientStop { position: 1; color: "#08000000" }
        }
    }
}
