import QtQuick
import Oran7UI.Impl

Item {
    id:root
    width: parent.width
    height: parent.height
    anchors.top: parent.top
    anchors.left: parent.left
    visible: true

    property var dataItems: [
        {value:qsTranslate("Oran7Ui", "VideoPlayer模块稳定支持播放4k60fps高帧率视频资源文件~")},
        {value:qsTranslate("Oran7Ui", "Every free Music 可以导入.(支持mp3，flac无损格式等)")},
        {value:qsTranslate("Oran7Ui", "Created author by Oran柒 ヾ(≧ ▽ ≦)ゝ")},
        {value:qsTranslate("Oran7Ui", "Oran7CloudMusic of Version is 0.7 ,仍在努力创作中~ o((>ω< ))o")},
        {value:qsTranslate("Oran7Ui", "好喜欢好喜欢好喜欢好喜欢好喜欢好喜欢好喜欢C++！！！！！！！")}
    ]

    property int currentIndex: 0
    property string currentText: ""
    property bool isTyping: true
    property int currentCharIndex: 0

    //Type machine Timer
    Timer{
        id:typeWriteTimer
        interval:80 //80ms
        repeat:true
        running: false

        onTriggered: {
            let item=root.dataItems[root.currentIndex]
            let fullText = item.value
            if(root.isTyping)
            {
                //isTyping
                root.currentCharIndex+=1
                if(root.currentCharIndex >= fullText.length)
                {
                    //typeWrite is over
                    root.isTyping=false
                    root.currentCharIndex=fullText.length
                    root.currentText=fullText
                    typeWriteTimer.stop()
                    delayDeleteTypedTextTimer.start()
                    return
                }
                root.currentText=fullText.substring(0,root.currentCharIndex)
            }
            else
            {
                //isDeleting
                root.currentCharIndex-=1
                if(root.currentCharIndex <=0)
                {
                    //chage to next item
                    root.currentIndex=(root.currentIndex+1)%root.dataItems.length

                    root.isTyping=true
                    root.currentCharIndex=0;
                    root.currentText=""

                    //reset typewrite speed
                    typeWriteTimer.interval = 80
                    typeWriteTimer.start()
                    return
                }
                root.currentText = fullText.substring(0,root.currentCharIndex)
            }
        }
    }
    //延时删除器
    Timer{
        id:delayDeleteTypedTextTimer
        interval: 1500 //1500ms
        repeat: false
        onTriggered: {
            typeWriteTimer.interval = 30//delete more faster
            typeWriteTimer.start()          //begain to delete text
        }
    }

    Oran7ShaderTextMask {
        anchors.centerIn: parent
        width: column.implicitWidth
        height: column.implicitHeight

        shaderName: "clouds"
        effectEnabled: root.visible

        Column {
            id: column
            width: 900
            anchors.left: parent.left
            anchors.top: parent.top
            spacing: 17
            Text{text:qsTranslate("Oran7Ui", "Hello!欢迎使用"); font.pixelSize: 37; color: "white"; font.weight: Font.Bold}
            Text{text:"Oran柒MediaClient"; font.pixelSize: 70; color: "white"; font.weight: Font.Bold ;font.family: Oran7Theme.Primary.fontBrandFamily || (typeof Oran7UiPreferences !== "undefined" ? Oran7UiPreferences.font : Qt.application.font).family}
            Text{text:qsTranslate("Oran7Ui", "⇲author by Oran柒"); font.pixelSize: 37; color: "white"}
            Text{text:qsTranslate("Oran7Ui", "⫸喜欢专注于C艹开发，QtQuick前端创作及ffmpeg音视频处理技术~"); font.pixelSize: 24; color: "white";font.weight: Font.Bold}
            Item {
                width: parent.width
                height: 40
                Row {
                    spacing: 12
                    anchors.verticalCenter: parent.verticalCenter
                    Text {id: typewriterDisplay; text: root.currentText; font.pixelSize: 27; color: "white"; font.weight: Font.Bold}
                    Text {id: cursor; text: "|"; font.pixelSize: 27; color:"white"; font.weight: Font.Bold; visible: root.currentText.length > 0}
                    Timer{id: blinkTimer; interval: 500; repeat: true; running: root.currentText.length > 0; onTriggered: {cursor.visible = !cursor.visible}
                    }
                }
            }
        }
    }
    Component.onCompleted: {
        column.forceLayout();
        typeWriteTimer.start()
    }
}
