import QtQuick
 import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import "../../Basic"
import "../../Components"
import Client 1.0
import FileHelper 1.0
import BilibiliRoomAddressCatch 1.0

Item {
    id:root_parent
    property string pageName: ""
    StackView.onActivated: {
        // 切回到该页面时，确保重新挂载（配合 VideoRendererItem 的 Timer 更稳）
        if (videoRenderItem) {
            Client.attachVideoItem(videoRenderItem.videoHost)
        }
    }
    Item {
        id:root
        anchors.fill: parent
        layer.enabled: false
        FileHelper{
            id:fileHelper
        }
        VideoRendererItem{
            id:videoRenderItem
            anchors.fill: parent
            //fillMode: 1
            openVideoInfo: false
        }
        //openSemiCircleRect
        Rectangle{
            id:openSemiCircleRect
            width: 40
            height: 40
            color: /*"#fef2e8"*/"transparent"
            radius: width * 0.3
            anchors.right: videoPlayerMenueRectangle.left
            anchors.rightMargin: -width/2
            // anchors.verticalCenter: videoPlayerMenueRectangle.verticalCenter
            anchors.top: videoPlayerMenueRectangle.top
            anchors.topMargin: 20

            property bool openIngState: true
            Image{
                id:openImage
                scale: 2
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 0 //4
                source: "/image/stackback.png"

                rotation: 0
                Behavior on rotation {
                    NumberAnimation{
                        duration: 200
                        easing.type: Easing.OutCubic
                    }
                }
                property color openImageColorOverlay_defaultColor: "#c74054"
                property color openImageColorOverlay_selectedColor: "#fc3c55"
                property color openImageColorOverlay_usedColor: openImage.openImageColorOverlay_defaultColor
                layer.enabled: true
                layer.effect: ColorOverlay{
                    source: openImage
                    anchors.fill: openImage
                    color: openImage.openImageColorOverlay_usedColor
                }
                asynchronous: false
                mipmap: true
                cache: true
                antialiasing: true
            }
            MouseArea{
                anchors.fill: parent
                onClicked: {
                    if(parent.openIngState === true)
                    {
                        openImage.rotation = 0
                        videoPlayerMenueRectangle.width = 0

                        parent.openIngState = false
                    }
                    else
                    {
                        openImage.rotation = 180
                        videoPlayerMenueRectangle.width = videoPlayerMenueRectangle.defaultWidth

                        parent.openIngState = true
                    }
                }
            }
        }
        //videoPlayerMenueRectangle
        Rectangle{
            id:videoPlayerMenueRectangle
            property real defaultWidth: 300
            width: openSemiCircleRect.openIngState===true ?  defaultWidth : 0
            Behavior on width {
                NumberAnimation{
                    duration: 200
                    easing.type: Easing.OutCubic
                }
            }
            height: root.height * 0.8
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            color: "#fef2e8"
            radius: 10

            Connections{
                target:BasicConfig
                function onClickedOutside()
                {
                    if(bilibiliRoomTextFiled.focus === true)
                        bilibiliRoomTextFiled.focus = false
                    if(inputFilePathTextArea.focus === true)
                        inputFilePathTextArea.focus = false
                }
            }
            Column{
                anchors.left: parent.left
                anchors.leftMargin:7
                anchors.top: parent.top
                anchors.topMargin: 7
                spacing: 7

                //----->First way : open in bilibili room number
                Label{
                    text:"bilibili直播房间号 :"
                    font.family: "微软雅黑"
                    font.pixelSize: 20
                    font.bold: true
                    color:"#2a1a22"
                }
                Rectangle {
                    id: inputBilibiliRoomNumRect
                    property int collapsedWidth: 140
                    property int expandedWidth: 200
                    property int currentWidth: collapsedWidth
                    width: currentWidth
                    Behavior on width {
                        NumberAnimation{
                            duration:200
                            easing.type: Easing.OutCubic
                        }
                    }
                    height: 40
                    color: "#f8c7c7"
                    radius: 10
                    border.width: 1
                    border.color: "#4d4d56"

                    //Behavior on inputBilibiliRoomNumRect width
                    Behavior on height {
                        NumberAnimation {
                            duration: 200
                            easing.type: Easing.OutCubic
                        }
                    }
                    BilibiliRoomAddressCatch{
                        id:bilibiliRoomAddressCatch
                        property bool isAlready: false
                        onIsAlreadyChanged: {
                            if(bilibiliRoomAddressCatch.isAlready === true)
                            {
                                //console.log("Now---->play:",urls[0])
                                videoRenderItem.tryAttachDelayed()//这个很关键,获取渲染item父对象实例
                                Client.qmlClickedReqPreparePlayMusic(urls[0])
                            }
                        }

                        property var urls: []
                        onUrlsReady:{
                            urls = []
                            bilibiliRoomAddressCatch.avliStrAdr.forEach(function(url) {
                                urls.push(url)
                            });
                            bilibiliRoomAddressCatch.isAlready = true
                        }
                    }
                    TextField {
                        id: bilibiliRoomTextFiled
                        anchors.fill: parent
                        anchors.margins: 5
                        color: "black"
                        font.family: "微软雅黑"
                        font.pixelSize: 15
                        background: Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                        }
                        //placeholderText
                        placeholderText: "bilibili"
                        placeholderTextColor: "#888888"
                        onFocusChanged: {
                            if(focus)
                            {
                                BasicConfig.newTextAreaFocused(bilibiliRoomTextFiled)

                                inputBilibiliRoomNumRect.currentWidth = inputBilibiliRoomNumRect.expandedWidth
                            }
                            else
                            {
                                if(bilibiliRoomTextFiled.text.length === 0)
                                    inputBilibiliRoomNumRect.currentWidth = inputBilibiliRoomNumRect.collapsedWidth
                            }
                        }
                        onTextChanged: {
                            var isValid = /^[0-9]+$/.test(text);
                            if (isValid && text.length <= 10) {
                                netStreamAddressGet_DelayTimer.restart()
                            }
                        }

                        Timer{
                            id:netStreamAddressGet_DelayTimer
                            interval: 3000
                            repeat: false
                            running: false
                            onTriggered: {
                                bilibiliRoomAddressCatch.getRoomInfo(bilibiliRoomTextFiled.text)
                            }
                        }
                    }
                    //BilibiliRoomNumWayPlayBtnRect
                    Image{
                        id:bilibiliRoomNumWayPlayBtnImage
                        height:inputBilibiliRoomNumRect.height
                        width: height
                        anchors.left: bilibiliRoomTextFiled.right
                        anchors.leftMargin: 4
                        anchors.verticalCenter: bilibiliRoomTextFiled.verticalCenter
                        scale:0.77
                        mipmap: true
                        cache: true
                        asynchronous: false
                        antialiasing: true
                        property string bilibiliRoomNumWayBtn_playImageSourceUrl: "qrc:/image/ClearPlay.png"
                        property string bilibiliRoomNumWayBtn_pauseImageSourceUrl: "qrc:/image/ClearPause.png"
                        source: bilibiliRoomNumWayBtn_playImageSourceUrl
                        property color bilibiliRoomNumWayPlayBtn_noReadyColorOverlay_Color: "#d63348"
                        property color bilibiliRoomNumWayPlayBtn_sureReadyColorOverlay_Color: "#578b2c"
                        layer.enabled: true
                        layer.effect: ColorOverlay{
                            source: bilibiliRoomNumWayPlayBtnImage
                            color:bilibiliRoomNumWayPlayBtnImage.bilibiliRoomNumWayPlayBtn_noReadyColorOverlay_Color
                        }
                        function handle_bilibiliRoomNumWay_play()
                        {
                            bilibiliRoomNumWayPlayBtnImage.source = bilibiliRoomNumWayPlayBtnImage.bilibiliRoomNumWayBtn_pauseImageSourceUrl
                            playImage.source = playImage.pouseImageSourceUrl
                            BasicConfig.isPlaying = true
                            videoRenderItem.tryAttachDelayed()

                        }
                        function handle_bilibiliRoomNumWay_pause()
                        {
                            bilibiliRoomNumWayPlayBtnImage.source = bilibiliRoomNumWayPlayBtnImage.bilibiliRoomNumWayBtn_playImageSourceUrl
                            playImage.source = playImage.playImageSourceUrl
                            BasicConfig.isPlaying = false

                        }
                        property bool sureReadyPlay : false
                        MouseArea{
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if(inputFilePath_WayPlayBtnImage.sureReadyPlay === true)
                                {
                                    if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_VideoPlayerIndex)
                                    {
                                        BasicConfig.isPlaying =false
                                        BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_VideoPlayerIndex
                                    }

                                    if(BasicConfig.isPlaying === false)
                                        inputFilePath_WayPlayBtnImage.handle_InputFilePathWay_play()
                                    else
                                        inputFilePath_WayPlayBtnImage.handle_InputFilePathWay_pause()
                                }
                            }
                        }
                    }
                }
                //----->Second way open in local folder
                Label{
                    text:"从本地文件夹 :"
                    font.family: "微软雅黑"
                    font.pixelSize: 20
                    font.bold: true
                    color:"#2a1a22"
                }
                //input file path rectangle
                Rectangle{
                    id:inputFilePathRect
                    property int collapsedWidth: 140
                    property int expandedWidth: 200
                    property int currentWidth: collapsedWidth
                    property int collapsedHeight: 40
                    property int expandHeight: 120
                    property int currentHeight: collapsedHeight
                    implicitWidth: currentWidth
                    width: currentWidth
                    Behavior on width {
                        NumberAnimation{
                            duration:200
                            easing.type: Easing.OutCubic
                        }
                    }
                    height: currentHeight
                    Behavior on height {
                        NumberAnimation{
                            duration:200
                            easing.type: Easing.OutCubic
                        }
                    }
                    color: "#f8c7c7"
                    radius: 10
                    border.width: 1
                    border.color: "#4d4d56"
                    ScrollView {
                        id: inputFilePathView
                        anchors.fill: parent
                        TextArea {
                            id:inputFilePathTextArea
                            width: inputFilePathRect.currentWidth
                            height: inputFilePathRect.currentHeight
                            //implicitBackgroundWidth: inputFilePathRect.currentWidth
                            text: ""
                            font.pixelSize: 15
                            font.family: "微软雅黑"
                            wrapMode: TextArea.Wrap
                            placeholderText: "C:/.../video.mp4"
                            placeholderTextColor: "#888888"
                            onFocusChanged: {
                                if(focus)
                                {
                                    BasicConfig.newTextAreaFocused(inputFilePathTextArea)

                                    inputFilePathRect.currentWidth = inputFilePathRect.expandedWidth
                                    inputFilePathRect.currentHeight = inputFilePathRect.expandHeight
                                }
                                else
                                {
                                    if(inputFilePathTextArea.text.length === 0)
                                    {
                                        inputFilePathRect.currentWidth = inputFilePathRect.collapsedWidth
                                        inputFilePathRect.currentHeight = inputFilePathRect.collapsedHeight
                                    }
                                }
                            }
                            onTextChanged: {
                                fileExistsDetect_DelayTimer.restart()
                            }
                            Timer{
                                id:fileExistsDetect_DelayTimer
                                interval: 500
                                repeat: false
                                running: false
                                onTriggered: {
                                    if(fileHelper.fileExists(inputFilePathTextArea.text)===true)
                                    {
                                        inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_curColorOverlay_Color
                                                = inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_sureReadyColorOverlay_Color ;
                                        inputFilePath_WayPlayBtnImage.sureReadyPlay = true
                                    }
                                    else
                                    {
                                        inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_curColorOverlay_Color
                                                = inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_noReadyColorOverlay_Color ;
                                        inputFilePath_WayPlayBtnImage.sureReadyPlay = false
                                    }
                                }
                            }
                        }
                        //
                    }
                    //inputFilePath_WayPlayBtnImage
                    Image{
                        id:inputFilePath_WayPlayBtnImage
                        height:inputFilePathRect.collapsedHeight
                        width: height
                        anchors.left: inputFilePathRect.right
                        anchors.leftMargin: 1
                        anchors.top: inputFilePathRect.top
                        scale:0.77
                        mipmap: true
                        cache: true
                        asynchronous: false
                        antialiasing: true
                        property string inputFilePathWayBtn_playImageSourceUrl: "qrc:/image/ClearPlay.png"
                        property string inputFilePathWayBtn_pauseImageSourceUrl: "qrc:/image/ClearPause.png"
                        source:inputFilePathWayBtn_playImageSourceUrl
                        property color inputFilePathWayBtn_curColorOverlay_Color: inputFilePathWayBtn_noReadyColorOverlay_Color
                        property color inputFilePathWayBtn_noReadyColorOverlay_Color: "#d63348"
                        property color inputFilePathWayBtn_sureReadyColorOverlay_Color: "#578b2c"
                        property bool sureReadyPlay: false
                        layer.enabled: true
                        layer.effect: ColorOverlay{
                            source: inputFilePath_WayPlayBtnImage
                            color:inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_curColorOverlay_Color
                        }
                        function handle_InputFilePathWay_play()
                        {
                            inputFilePath_WayPlayBtnImage.source = inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_pauseImageSourceUrl
                            playImage.source = playImage.pouseImageSourceUrl
                            BasicConfig.isPlaying = true
                            videoRenderItem.tryAttachDelayed()
                            Client.qmlClickedReqPreparePlayMusic(inputFilePathTextArea.text)
                        }
                        function handle_InputFilePathWay_pause()
                        {
                            inputFilePath_WayPlayBtnImage.source = inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_playImageSourceUrl
                            playImage.source = playImage.playImageSourceUrl
                            BasicConfig.isPlaying = false
                            Client.qmlClickedReqPreparePlayMusic(inputFilePathTextArea.text)
                        }

                        MouseArea{
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if(inputFilePath_WayPlayBtnImage.sureReadyPlay === true)
                                {
                                    if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_VideoPlayerIndex)
                                    {
                                        BasicConfig.isPlaying =false
                                        BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_VideoPlayerIndex
                                    }

                                    if(BasicConfig.isPlaying === false)
                                        inputFilePath_WayPlayBtnImage.handle_InputFilePathWay_play()
                                    else
                                        inputFilePath_WayPlayBtnImage.handle_InputFilePathWay_pause()
                                }
                            }
                        }
                    }
                    //in inputFilePathRect
                }

                //========启用开发者信息调试模式openVideoInfo======//
                Oran7ESelectRect{
                    id:openVIdeoInfo_eSelRect
                    labelText: "OpenVideoInfo"
                    onClicked: {
                        if(checked === false)
                        {
                            checked = true
                            videoRenderItem.openVideoInfo = checked
                        }
                        else
                        {
                            checked = false
                            videoRenderItem.openVideoInfo = checked
                        }
                    }
                }
                //============= 选择videoScaleMode ==================//
                property int scaleMode: Client.Fit
                Label{
                    id:scaleModeTextLabel
                    font.pixelSize: 20
                    font.family: "微软雅黑"
                    font.bold: true
                    text: "ScaleMode:"
                    color:"#24161d"
                }
                Oran7ESelectRect {
                    labelText: "Fit"
                    //radius: width/2
                    checked: parent.scaleMode === Client.Fit
                    onClicked: {
                        parent.scaleMode = Client.Fit
                        Client.setScaleMode(Client.Fit)
                    }
                }
                Oran7ESelectRect {
                    labelText: "Fill"
                    //radius: width/2
                    checked: parent.scaleMode === Client.Fill
                    onClicked: {
                        parent.scaleMode = Client.Fill
                        Client.setScaleMode(Client.Fill)
                    }
                }
                //==========================================//

                //in colume
            }
            //in videoPlayerMenueRectangle
        }

        //videoBottomControlBarRect
        Rectangle{
            id:videoBottomControlBarRect
            anchors.left: root.left
            anchors.right: root.right
            anchors.bottom: root.bottom
            height:80
            color: /*"#262626"*/"transparent"
            visible: true
            opacity: 0
            Behavior on opacity{
                NumberAnimation{
                    duration: 300
                    easing.type:Easing.OutCubic
                }
            }

            //PauseOrPlay_Image
            Image{
                id:playImage
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 7
                anchors.left: parent.left
                anchors.leftMargin: 15
                sourceSize.width:40
                sourceSize.height: 40
                scale: 0.9
                property string playImageSourceUrl: "qrc:/image/ClearPlay.png"
                property string pouseImageSourceUrl: "qrc:/image/ClearPause.png"
                source: playImageSourceUrl
                layer.enabled: true
                layer.effect: ColorOverlay{
                    source: playImage
                    color:"white"
                }
                property bool isPlaying: false
                MouseArea{
                    id:playIamgeMouseArea
                    anchors.fill: parent
                    onClicked: {
                        if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_VideoPlayerIndex)return;
                        if(BasicConfig.isPlaying === false)
                        {
                            inputFilePath_WayPlayBtnImage.handle_InputFilePathWay_play()
                        }
                        else
                        {
                            inputFilePath_WayPlayBtnImage.handle_InputFilePathWay_pause()
                        }
                    }
                }
                Connections{
                    target: BasicConfig
                    function onPlayerFocusChanged()
                    {
                        if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_VideoPlayerIndex)
                        {
                            inputFilePath_WayPlayBtnImage.source = inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_playImageSourceUrl
                            playImage.source = playImage.playImageSourceUrl
                        }
                    }
                }
                //<---playImage
            }

            //Oran7ProgressSlider
            Oran7ProgressSlider{
                id:videoProgressSlider
                width:parent.width * 0.98
                height: 12
                progressHandleWidth: 14
                anchors.top:parent.top
                anchors.topMargin: 10
                anchors.horizontalCenter: parent.horizontalCenter
                focusedPlayer: BasicConfig.globalPlayer_VideoPlayerIndex
                property real ratio :0
                Connections{
                    target: Client
                    // enabled: videoBottomControlBarRect.visible
                    //              && videoBottomControlBarRect.enabled
                    //              && videoBottomControlBarRect.Window
                    //              && videoBottomControlBarRect.Window.active
                    function onUpdataQmlPlayProgressSliderCurPos(CurPos,CurTime_Second){
                        if(videoProgressSlider.isPressed === false && BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_VideoPlayerIndex)
                        {
                            //console.log(CurPos,CurTime_Second)
                            videoProgressSlider.nowSecondTime=CurTime_Second
                            videoProgressSlider.nowTimeText = String(Math.floor(Math.floor(CurTime_Second/60)/10))+String(Math.floor(Math.floor(CurTime_Second/60)%10))+":"
                                    +String(Math.floor(Math.floor(CurTime_Second%60)/10))+String(Math.floor(Math.floor(CurTime_Second%60)%10))
                            videoProgressSlider.ratio=CurPos/BasicConfig.max_Slider_Value
                            // console.log(videoProgressSlider.ratio)
                            videoProgressSlider.progressHandleX=videoProgressSlider.width * videoProgressSlider.ratio - videoProgressSlider.progressHandleWidth/2
                            videoProgressSlider.visibleProgressX=videoProgressSlider.width * videoProgressSlider.ratio
                            if(videoProgressSlider.nowSecondTime>=videoProgressSlider.allSecondTime)
                            {
                                inputFilePath_WayPlayBtnImage.source = inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_playImageSourceUrl
                                playImage.source = playImage.playImageSourceUrl
                            }
                        }
                    }
                    function onUpdataQmlPlayNowFileAllTime(AllTime){
                        if(BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_VideoPlayerIndex)
                        {
                            videoProgressSlider.allSecondTime = AllTime
                            videoProgressSlider.allTimeText = String(Math.floor(Math.floor(AllTime/60)/10)) + String(Math.floor(Math.floor(AllTime/60)%10)) + ":"
                                    +String(Math.floor(Math.floor(AllTime%60)/10)) + String(Math.floor(Math.floor(AllTime%60)%10))
                        }
                    }
                }
            }
            Label{
                id:progressTimeLabel
                anchors.left:playImage.right
                anchors.leftMargin: 15
                anchors.verticalCenter: playImage.verticalCenter
                color: "white"
                font.family: "微软雅黑"
                font.pixelSize: 20
                text: videoProgressSlider.nowTimeText+" - "+videoProgressSlider.allTimeText
            }

            HoverHandler {
                id: hover
                acceptedDevices: PointerDevice.Mouse

                property point lastPos: Qt.point(-99999, -99999)
                property bool hasLast: false
                property real moveThreshold: 2   // 像素阈值，防抖

                onPointChanged: {
                    if (!hovered) {
                        hasLast = false
                        return
                    }

                    const p = point.position
                    if (!hasLast) {
                        // 第一次进入/第一次拿到点，不算“移动”
                        lastPos = p
                        hasLast = true
                        return
                    }

                    const dx = p.x - lastPos.x
                    const dy = p.y - lastPos.y
                    const dist2 = dx*dx + dy*dy

                    if (dist2 >= moveThreshold *moveThreshold) {
                        //只有真的移动才显示并重置计时器
                        videoBottomControlBarRect.opacity = 1
                        hideControlRectTimer.restart()
                        lastPos = p
                    }
                }

                onHoveredChanged: {
                    // 离开区域就立刻开始隐藏计时
                    if (!hovered) {
                        hasLast = false
                        hideControlRectTimer.restart()
                    }
                }
            }

            Timer {
                id: hideControlRectTimer
                repeat: false
                interval: 5000
                onTriggered:(mouse)=> {
                    videoBottomControlBarRect.opacity = 0
                }
            }
            //<--videoBottomControlBarRect
        }

        //============= VideoPlayerStack鼠标定时隐藏逻辑 =============//
        property bool mouseHidden: false

        Timer {
            id: mouseHideTimer
            interval: 8000
            repeat: false
            running: true
            onTriggered: root.mouseHidden = true
        }

        HoverHandler {
            id: rootHover
            acceptedDevices: PointerDevice.Mouse

            //HoverHandler/PointerHandler 在 Qt6 里有 cursorShape
            cursorShape: root.mouseHidden ? Qt.BlankCursor : Qt.ArrowCursor

            property point lastPos: Qt.point(-99999, -99999)
            property bool hasLast: false
            property real threshold: 2

            onPointChanged: {
                if (!hovered) { hasLast = false; return }

                const p = point.position
                if (!hasLast) { lastPos = p; hasLast = true; return }

                const dx = p.x - lastPos.x
                const dy = p.y - lastPos.y
                if (dx*dx + dy*dy >= threshold*threshold) {
                    root.mouseHidden = false
                    mouseHideTimer.restart()
                    lastPos = p
                }
            }

            onHoveredChanged: {
                if (hovered) {
                    root.mouseHidden = false
                    mouseHideTimer.restart()
                } else {
                    hasLast = false
                }
            }
        }
        //====================================================//
        // ELoader{
        //     size: 100
        //     color: "#ff7384"
        //     anchors.horizontalCenter: parent.horizontalCenter
        //     anchors.verticalCenter: parent.verticalCenter
        //     speed: 0.8
        // }

        //<---root
    }
    // Component.completed: {
    //     if(openSemiCircleRect.openIngState === true)
    //     {
    //         openImage.rotation=180
    //     }
    // }
}
