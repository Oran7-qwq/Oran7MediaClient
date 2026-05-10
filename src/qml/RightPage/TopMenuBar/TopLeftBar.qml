import QtQuick 2.15
import Qt5Compat.GraphicalEffects
import QtQuick.Controls
import "../../Basic"

Item {
    id:leftTopMenuItem
    property real gradientPosition2: 1.0
    anchors.left: parent.left
    anchors.leftMargin: 50
    Row{
        id:mainRow
        spacing: 8
        anchors.verticalCenter: parent.verticalCenter
        //stackBackRectangle
        Rectangle{
            id:stackBackRectangle
            width: 30
            height:38
            color:"#fef2e8"
            radius: 10
            border.color: "#2a1a22"
            border.width: 1
            anchors.verticalCenter: parent.verticalCenter
            Image{
                id:stackbackImage
                scale: 0.4
                anchors.fill: parent
                source: "/image/stackback.png"

                property color stackbackImageColorOverlay_defaultColor: "#2a1a22"
                property color stackbackImageColorOverlay_selectedColor: "#616161"
                property color stackbackImageColorOverlay_usedColor: stackbackImage.stackbackImageColorOverlay_defaultColor
                layer.enabled: true
                layer.effect: ColorOverlay{
                    source: stackbackImage
                    anchors.fill: stackbackImage
                    color: stackbackImage.stackbackImageColorOverlay_usedColor
                }

                asynchronous: false
                mipmap: true
                cache: true
                antialiasing: true
            }
            MouseArea{
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    stackbackImage.stackbackImageColorOverlay_usedColor=stackbackImage.stackbackImageColorOverlay_selectedColor
                    cursorShape = Qt.PointingHandCursor
                }
                onExited: {
                    stackbackImage.stackbackImageColorOverlay_usedColor=stackbackImage.stackbackImageColorOverlay_defaultColor
                    cursorShape =Qt.ArrowCursor
                }
                onClicked: {
                    mainStackView.popPage()
                }
            }
        }
        //SearchRectangle---->false
        Rectangle{
            id:searchRectangleBorder
            visible: false
            height: stackBackRectangle.height
            width: 248
            radius: 10
            gradient:Gradient{
                orientation:Gradient.Horizontal
                GradientStop{color: "#2a1a22";position: 0.0}
                GradientStop{color: "#382635";position: 1.0}
            }
            Rectangle{
                id:searchRectangle
                // height: stackBackRectangle.height
                // width: 248
                anchors.fill: parent
                anchors.margins: 1
                color: "#191d2b"
                radius: 10
                anchors.verticalCenter: parent.verticalCenter
                clip: true
                gradient:Gradient{
                    orientation:Gradient.Horizontal
                    GradientStop{color: "#fef2e8";position: 0.0}
                    GradientStop{color: "#fef2e8";position: BasicConfig.gradientPosition2}
                }

                Image{
                    id:serchImage
                    height: parent.height
                    width: height
                    source: "/image/search.png"
                    scale :1
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    layer.enabled: false
                    layer.effect: ColorOverlay{
                        source:serchImage
                        color: "#2a1a22"
                    }
                    MouseArea{
                        anchors.fill: serchImage
                        hoverEnabled: true
                        onEntered: {
                            serchImage.layer.enabled = true
                        }
                        onExited: {
                            serchImage.layer.enabled = false
                        }
                        onClicked: {
                            //---
                        }
                    }
                }
                TextField{
                    id:searchTextField
                    anchors.left: serchImage.right
                    anchors.leftMargin: 5
                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    anchors.top: parent.top
                    anchors.topMargin: 2
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 2

                    placeholderText : "大家都在搜 一只柒橘"
                    placeholderTextColor:"#98989b"
                    color:"white"
                    maximumLength:24
                    font.pixelSize: 14
                    font.family: "微软雅黑 Light"

                    verticalAlignment: TextInput.AlignVCenter
                    background: Rectangle {
                                color: "transparent"
                                border.color: "transparent"
                            }
                    MouseArea{
                        anchors.fill: parent
                        onPressed: {
                            BasicConfig.gradientPosition2 = 0.0
                            searchPopup.open()
                        }
                    }
                }
            }
        }
        //mikeRectangle-->false
        Rectangle{
            id:mikeRectangle
            visible: false
            width: stackBackRectangle.height
            height: width
            anchors.verticalCenter: stackBackRectangle.verticalCenter
            radius: 10
            border.width:1
            border.color: "#2a1a22"
            color:"#fef2e8"
            Image {
                id: mikeImage
                source: "/image/mike.png"
                scale:0.4
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }
            BrightnessContrast{
                id:mikeRectangleBrightness
                anchors.fill:mikeRectangle
                source: mikeRectangle
                brightness: 0.0
                contrast: 0.0
            }
            MouseArea{
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    mikeRectangleBrightness.brightness = 0.1
                }
                onExited: {
                    mikeRectangleBrightness.brightness = 0.0
                }
                onClicked: {
                    //---
                }
            }
        }
    }
}
