//MainStackView.qml
import QtQuick 2.15
import QtQuick.Controls
import "../../Basic"
import Client 1.0

StackView{
    id:mainStackView
    background: Rectangle{
        color: "transparent"
        anchors.fill: parent
    }

    // 列表直接绑定 Client.tracks，不再逐条转发信号构建 QML ListModel。
}
