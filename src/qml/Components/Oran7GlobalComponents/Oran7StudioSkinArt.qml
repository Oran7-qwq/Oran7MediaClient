import QtQuick

// 混音区的静态冰蓝与淡紫雾色。
Image {
    id: root
    property bool active: false
    property real strength: 0.65
    objectName: "studioDiffuseHaze"
    visible: active
    source: active ? Qt.resolvedUrl("../../../../image/skins/violet/diffuse-haze.svg") : ""
    sourceSize: Qt.size(768, 256)
    fillMode: Image.Stretch
    opacity: strength
    smooth: true
}
