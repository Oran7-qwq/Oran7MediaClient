import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Item {
    id:root
    width: parent.width
    height: parent.height
    anchors.top: parent.top
    anchors.left: parent.left
    visible: true

    property bool start_gradientLayerColorAnimation: false
    onStart_gradientLayerColorAnimationChanged: {
        if(root.start_gradientLayerColorAnimation===false)
        {
            gradientLayerColorAnimationTImer.stop()
        }
        else
        {
            gradientLayerColorAnimationTImer.start()
        }
    }

    property var dataItems: [
        {value:"Every free Music 可以导入.(支持mp3，flac无损格式等)"},
        {value:"Created author by Oran柒 ヾ(≧ ▽ ≦)ゝ"},
        {value:"Oran7CloudMusic of Version is 0.7 ,仍在努力创作中~ o((>ω< ))o"},
        {value:"好喜欢好喜欢好喜欢好喜欢好喜欢好喜欢好喜欢C++！！！！！！！"}
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

    Rectangle {
        id:linearGradientTextRectangle
        anchors.top: parent.top
        anchors.topMargin: 200
        anchors.left: parent.left
        anchors.leftMargin: 180
        width: column.implicitWidth
        height: column.implicitHeight
        color: "transparent"

        // 创建渐变层
        LinearGradient {
            id: gradientLayer
            anchors.fill: parent
            start: Qt.point(0, 0)
            end: Qt.point(width, height)
            property color begainColor: "#96fbc4"
            property color middleColor: "#7ed321"
            property color endColor: "#f9f586"
            property int currentColorIndex: 0
            onCurrentColorIndexChanged: {
                gradientLayerColorAnimationTImer.start()
            }
            property var colorItems: [
                {begainColorValue: "#96fbc4",middleColorValue: "#7ed321",endColorValue: "#f9f586"},//自然绿
                {begainColorValue: "#fa709a",middleColorValue: "#fee140",endColorValue: "#ff9a8b"},//焦糖奶茶
                {begainColorValue: "#fd63a3",middleColorValue: "#fe9800",endColorValue: "#ffb74d"},//夕阳橙
                {begainColorValue: "#ff6b6b",middleColorValue: "#ff4757",endColorValue: "#ee5a52"},//热情红
                {begainColorValue: "#f093fb",middleColorValue: "#f5576c",endColorValue: "#4facfe"},//霓虹粉
                {begainColorValue: "#0093e9",middleColorValue: "#00f2fe",endColorValue: "#4facfe"},//清新蓝
                {begainColorValue: "#ffcc02",middleColorValue: "#f7971e",endColorValue: "#ffd200"},//金秋黄
                {begainColorValue: "#2d5016",middleColorValue: "#a4de6c",endColorValue: "#40e0d0"}//森林松绿色
            ]
            //动态渐变
            Timer{
                id:gradientLayerColorAnimationTImer
                interval: gradientLayerColorParallelAnimation.transDuration + 100
                repeat: false
                onTriggered: {
                    //console.log("gradientLayerColorAnimationTImer be triggered.")
                    gradientLayerColorParallelAnimation.start()
                }
            }
            ParallelAnimation{
                id:gradientLayerColorParallelAnimation
                property real transDuration: 1000 //2000ms
                PropertyAnimation{property:"begainColor"; to:gradientLayer.colorItems[gradientLayer.currentColorIndex].begainColorValue;
                    target:gradientLayer; duration:gradientLayerColorParallelAnimation.transDuration}
                PropertyAnimation{property:"middleColor"; to:gradientLayer.colorItems[gradientLayer.currentColorIndex].middleColorValue;
                    target:gradientLayer; duration:gradientLayerColorParallelAnimation.transDuration}
                PropertyAnimation{property:"endColor"; to:gradientLayer.colorItems[gradientLayer.currentColorIndex].endColorValue;
                    target:gradientLayer; duration:gradientLayerColorParallelAnimation.transDuration}
                onFinished: {
                    //console.log("gradientLayerColorParallelAnimation finished,index:",gradientLayer.currentColorIndex)
                    gradientLayer.currentColorIndex = (gradientLayer.currentColorIndex + 1)%gradientLayer.colorItems.length
                }
            }

            gradient: Gradient {
                GradientStop { position: 0.0; color: gradientLayer.begainColor}
                GradientStop { position: 0.5; color: gradientLayer.middleColor }
                GradientStop { position: 1.0; color: gradientLayer.endColor}
            }
        }

        // 创建白色文字层
        Column {
            id: column
            width: 900
            anchors.left: parent.left
            anchors.top: parent.top
            spacing: 17
            Text{text:"Hello!欢迎使用"; font.pixelSize: 37; color: "white"; font.weight: Font.Bold}
            Text{text:"Oran柒MediaClient"; font.pixelSize: 70; color: "white"; font.weight: Font.Bold ;font.family: "华文彩云"}
            Text{text:"⇲author by Oran柒"; font.pixelSize: 37; color: "white"}
            Text{text:"⫸喜欢专注于C艹开发，QtQuick前端创作及ffmpeg音视频处理技术~"; font.pixelSize: 24; color: "white";font.weight: Font.Bold}
            Item {
                width: parent.width
                height: 40
                Row {
                    spacing: 12
                    anchors.verticalCenter: parent.verticalCenter
                    z:linearGradientTextRectangle.z+1
                    Text {id: typewriterDisplay; text: root.currentText; font.pixelSize: 27; color: "white"; font.weight: Font.Bold}
                    Text {id: cursor; text: "|"; font.pixelSize: 27; color:"white"; font.weight: Font.Bold; visible: root.currentText.length > 0}
                    Timer{id: blinkTimer; interval: 500; repeat: true; running: root.currentText.length > 0; onTriggered: {cursor.visible = !cursor.visible}
                    }
                }
            }
        }
        // 将渐变层和文字层组合
        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: column
            source: gradientLayer
        }
    }

    Component.onCompleted: {
        column.forceLayout();
        typeWriteTimer.start()
    }
}
