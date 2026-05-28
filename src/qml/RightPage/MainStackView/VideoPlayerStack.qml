import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import "../../Basic"
import "../../Components"
import Client 1.0
import Oran7UI.Impl 1.0
import BilibiliRoomAddressCatch 1.0

Item {
    id: root_parent
    property string pageName: ""

    // 为每个实例分配唯一标识
    Component.onCompleted: {
        var timestamp = new Date().getTime()
        var instanceId = "VideoPlayerStack_" + timestamp + "_" + Math.floor(Math.random() * 1000)
        root_parent.objectName = instanceId
    }

    Component.onDestruction: {
        // 清理资源
    }

    Item {
        id: root
        // 使用 Binding 动态绑定到父容器尺寸，确保在父容器尺寸变化时也能正确更新
        Binding on width {
            value: root.parent ? root.parent.width : 0
            when: root.parent !== null
        }
        Binding on height {
            value: root.parent ? root.parent.height : 0
            when: root.parent !== null
        }
        layer.enabled: false

        //====================  VideoPlayerStack in MainStackView isActive ???  ==================///
        property bool isActive: false
        // 获取 StackView
        property var _myStackView: null
        // 尝试获取 StackView
        function findAndSetupStackView() {
            if (StackView.view) {
                //console.log("通过 StackView.view 获取");
                _myStackView = StackView.view;
            } else {
                //通过父级查找
                //console.log("通过父级查找 StackView");
                var parent = root.parent;
                var depth = 0;
                while (parent && depth < 20) {
                    //console.log("查找深度", depth, "父级:", parent);
                    if (typeof parent.push === "function" && typeof parent.pop === "function") {
                        //console.log("找到 StackView");
                        _myStackView = parent;
                        break;
                    }
                    parent = parent.parent;
                    depth++;
                }
            }
            if (root._myStackView) {
                //console.log("成功获取 StackView，开始监听");
                // 监听 currentItem 变化
                root._myStackView.currentItemChanged.connect(updateActiveState);
                // 立即更新一次
                root.updateActiveState();
            } else {
                //console.warn("无法获取 StackView");
            }
        }
        // 更新活动状态
        function updateActiveState() {
            if (!_myStackView || !_myStackView.currentItem) {
                root.isActive = false;
                //console.log("_myStackView 或_myStackView.currentItem 无效-->isActive=false")
                return;
            }
            const newActive = (_myStackView.currentItem === root.parent);
            if (root.isActive !== newActive) {
                // console.log("活动状态变化:", newActive);
                root.isActive = newActive;
                if (root.isActive === true) {}
            } else {
                // console.log("root.isActive !== newActive")
                // console.log(_myStackView.currentItem)
                // console.log(root.parent)
            }
        }
        // 初始化
        Component.onCompleted: {
            Qt.callLater(function () {
                findAndSetupStackView();
            });
        }
        // 清理
        Component.onDestruction: {
            if (_myStackView) {
                _myStackView.currentItemChanged.disconnect(updateActiveState);
            }
        }
        //================================================

        //=============== 自定义组件 ===============
        Oran7FileHelper {
            id: fileHelper
        }
        Oran7VideoRendererItem {
            id: videoRenderItem
            anchors.fill: parent
            renderObject: Client.VideoPlayerRender
            openVideoInfo: false

            radius: 10

            layer.effect: DropShadow{
                source: videoRenderItem
                samples: 60
                spread: 0.3
                horizontalOffset: 10
                verticalOffset: 0
                radius:10
                color:"black"
            }
        }
        Connections {
            target: Client
            function onSigStop() {
                Client.renderBlackFrame(videoRenderItem.renderObject);
            }
        }

        //openSemiCircleRect
        Rectangle {
            id: openSemiCircleRect
            width: 40
            height: 40
            color: /*"#fef2e8"*/ "transparent"
            radius: width * 0.3
            anchors.right: videoPlayerMenueRectangle.left
            anchors.rightMargin: -width / 2
            anchors.top: videoPlayerMenueRectangle.top
            anchors.topMargin: 20

            Connections {
                target: BasicConfig
                function onClearAllUi_inVIdeoRenderArea(ok) {
                    if (ok === false) {
                        openImage.visible = true;
                    } else {
                        openImage.visible = false;
                        // 只在 root 有正确尺寸且不在页面切换期间时才隐藏菜单，避免布局错误
                        if (root.width > 0) {
                            openSemiCircleRect.openIngState = false;
                        }
                    }
                }
            }

            property bool openIngState: false
            Image {
                id: openImage
                scale: 2
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 0 //4
                source: "qrc:/image/stackback.png"

                rotation: parent.openIngState ? 180 : 0
                Behavior on rotation {
                    NumberAnimation {
                        duration: 200
                        easing.type: Easing.OutCubic
                    }
                }
                property color openImageColorOverlay_defaultColor: "#c74054"
                property color openImageColorOverlay_selectedColor: "#fc3c55"
                property color openImageColorOverlay_usedColor: openImage.openImageColorOverlay_defaultColor
                layer.enabled: true
                layer.effect: ColorOverlay {
                    source: openImage
                    anchors.fill: openImage
                    color: openImage.openImageColorOverlay_usedColor
                }
                asynchronous: false
                mipmap: true
                cache: true
                antialiasing: true
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (parent.openIngState === true) {
                        videoPlayerMenueRectangle.width = 0;
                        parent.openIngState = false;
                    } else {
                        videoPlayerMenueRectangle.width = videoPlayerMenueRectangle.defaultWidth;
                        parent.openIngState = true;
                    }
                }
            }
        }
        // =============== VideoPlayer Menue Rectangle ================
        Rectangle {
            id: videoPlayerMenueRectangle

            property real defaultWidth: 300
            property bool fileDialog_isOpen: inputFilePathWay_OpenFileDialog.isOpen

            width:openSemiCircleRect.openIngState ? defaultWidth : 0

            Behavior on width {
                NumberAnimation {
                    duration: 200
                    easing.type: Easing.OutCubic
                }
            }
            height: root.height > 0 ? root.height * 0.8 : 0
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            color: "#fef2e8"
            radius: 10

            Connections {
                target: BasicConfig
                function onClickedOutside() {
                    if (bilibiliRoomTextFiled.focus === true)
                        bilibiliRoomTextFiled.focus = false;
                    if (inputFilePathTextArea.focus === true)
                        inputFilePathTextArea.focus = false;
                }
            }
            Column {
                anchors.left: parent.left
                anchors.leftMargin: 7
                anchors.top: parent.top
                anchors.topMargin: 7
                spacing: 7

                //----->First way : open in bilibili room number
                Label {
                    text: "bilibili直播房间号 :"
                    font.family: "微软雅黑"
                    font.pixelSize: 20
                    font.bold: true
                    color: "#2a1a22"
                }
                Rectangle {
                    id: inputBilibiliRoomNumRect
                    property int collapsedWidth: 140
                    property int expandedWidth: 200
                    property int currentWidth: collapsedWidth
                    width: currentWidth
                    Behavior on width {
                        NumberAnimation {
                            duration: 200
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
                    Connections {
                        target: Client
                        function onSigStop() {
                            videoNetwork_Loader.visible = false;
                            if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_LivePlayerIndex)
                                bilibiliRoomNumWayPlayBtnImage.source = bilibiliRoomNumWayPlayBtnImage.bilibiliRoomNumWayBtn_playImageSourceUrl;
                            if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_VideoPlayerIndex)
                                inputFilePath_WayPlayBtnImage.source = inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_playImageSourceUrl;
                        }
                    }
                    BilibiliRoomAddressCatch {
                        id: bilibiliRoomAddressCatch
                        property var urls: []
                        onUrlsReady: {
                            urls = []; //Clear
                            bilibiliRoomAddressCatch.avliStrAdr.forEach(function (url) {
                                urls.push(url);
                            });
                            bilibiliRoomNumWayPlayBtnImage.sureReadyPlay = true;
                            videoNetwork_Loader.visible = false; //reset
                        }
                        onUrlsError: {
                            bilibiliRoomNumWayPlayBtnImage.sureReadyPlay = false;
                            bilibiliRoomNumWayPlayBtnImage.source = bilibiliRoomNumWayPlayBtnImage.bilibiliRoomNumWayBtn_playImageSourceUrl;
                            videoNetwork_Loader.visible = false;
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
                            if (focus) {
                                BasicConfig.newTextAreaFocused(bilibiliRoomTextFiled);
                                inputBilibiliRoomNumRect.currentWidth = inputBilibiliRoomNumRect.expandedWidth;
                            } else {
                                if (bilibiliRoomTextFiled.text.length === 0)
                                    inputBilibiliRoomNumRect.currentWidth = inputBilibiliRoomNumRect.collapsedWidth;
                            }
                        }
                        onTextChanged: {
                            var isValid = /^[0-9]+$/.test(text);
                            if (isValid && text.length <= 10) {
                                videoNetwork_Loader.visible = true;
                                netStreamAddressGet_DelayTimer.restart();
                            }
                        }

                        Timer {
                            id: netStreamAddressGet_DelayTimer
                            interval: 3000
                            repeat: false
                            running: false
                            onTriggered: {
                                bilibiliRoomAddressCatch.getRoomInfo(bilibiliRoomTextFiled.text);
                            }
                        }
                    }
                    //BilibiliRoomNumWayPlayBtnRect
                    Image {
                        id: bilibiliRoomNumWayPlayBtnImage
                        height: inputBilibiliRoomNumRect.height
                        width: height
                        anchors.left: bilibiliRoomTextFiled.right
                        anchors.leftMargin: 4
                        anchors.verticalCenter: bilibiliRoomTextFiled.verticalCenter
                        scale: 0.77
                        Behavior on scale {
                            NumberAnimation {
                                duration: 300
                                easing.type: Easing.OutCubic
                            }
                        }
                        mipmap: true
                        cache: true
                        asynchronous: false
                        antialiasing: true
                        property string bilibiliRoomNumWayBtn_playImageSourceUrl: "qrc:/image/ClearPlay.png"
                        property string bilibiliRoomNumWayBtn_pauseImageSourceUrl: "qrc:/image/ClearPause.png"
                        source: bilibiliRoomNumWayBtn_playImageSourceUrl
                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            source: bilibiliRoomNumWayPlayBtnImage
                            color: bilibiliRoomNumWayPlayBtnImage.sureReadyPlay ? "#578b2c" : "#d63348"
                        }
                        function handle_bilibiliRoomNumWay_play() {
                            bilibiliRoomNumWayPlayBtnImage.source = bilibiliRoomNumWayPlayBtnImage.bilibiliRoomNumWayBtn_pauseImageSourceUrl;
                            BasicConfig.isPlaying = true;
                            videoRenderItem.tryAttachDelayed(); //sureLoad videoItemCpp
                            Client.qmlClickedReqPreparePlayVideo(bilibiliRoomAddressCatch.urls[0]);
                        }
                        function handle_bilibiliRoomNumWay_pause() {
                            bilibiliRoomNumWayPlayBtnImage.source = bilibiliRoomNumWayPlayBtnImage.bilibiliRoomNumWayBtn_playImageSourceUrl;
                            BasicConfig.isPlaying = false;
                            Client.qmlClickedReqPreparePlayVideo(bilibiliRoomAddressCatch.urls[0]);
                        }
                        property bool sureReadyPlay: false
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (bilibiliRoomNumWayPlayBtnImage.sureReadyPlay === true) {
                                    if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_LivePlayerIndex) {
                                        BasicConfig.isPlaying = false;
                                        BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_LivePlayerIndex;
                                    }

                                    if (BasicConfig.isPlaying === false)
                                        bilibiliRoomNumWayPlayBtnImage.handle_bilibiliRoomNumWay_play();
                                    else
                                        bilibiliRoomNumWayPlayBtnImage.handle_bilibiliRoomNumWay_pause();
                                }
                            }
                            onExited: cursorShape = Qt.ArrowCursor
                            onEntered: cursorShape = Qt.PointingHandCursor
                            onPressed: bilibiliRoomNumWayPlayBtnImage.scale = 0.5
                            onReleased: bilibiliRoomNumWayPlayBtnImage.scale = 0.77
                        }
                    }

                    // 网络加载指示器
                    Oran7Loader {
                        id: videoNetwork_Loader
                        anchors.centerIn: parent
                        size: 30
                        color: "#578b2c"
                        visible: false
                    }
                }
                //----->Second way : open in local folder
                Label {
                    text: "从本地文件夹 :"
                    font.family: "微软雅黑"
                    font.pixelSize: 20
                    font.bold: true
                    color: "#2a1a22"
                }
                //input file path rectangle
                Rectangle {
                    id: inputFilePathRect
                    property int collapsedWidth: 140
                    property int expandedWidth: 200
                    property int currentWidth: collapsedWidth
                    property int collapsedHeight: 40
                    property int expandHeight: 120
                    property int currentHeight: collapsedHeight
                    implicitWidth: currentWidth
                    width: currentWidth
                    Behavior on width {
                        NumberAnimation {
                            duration: 200
                            easing.type: Easing.OutCubic
                        }
                    }
                    height: currentHeight
                    Behavior on height {
                        NumberAnimation {
                            duration: 200
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
                            id: inputFilePathTextArea
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
                                if (focus) {
                                    BasicConfig.newTextAreaFocused(inputFilePathTextArea);

                                    inputFilePathRect.currentWidth = inputFilePathRect.expandedWidth;
                                    inputFilePathRect.currentHeight = inputFilePathRect.expandHeight;
                                } else {
                                    if (inputFilePathTextArea.text.length === 0) {
                                        inputFilePathRect.currentWidth = inputFilePathRect.collapsedWidth;
                                        inputFilePathRect.currentHeight = inputFilePathRect.collapsedHeight;
                                    }
                                }
                            }
                            onTextChanged: {
                                fileExistsDetect_DelayTimer.restart();
                            }
                            Timer {
                                id: fileExistsDetect_DelayTimer
                                interval: 500
                                repeat: false
                                running: false
                                onTriggered: {
                                    if (fileHelper.fileExists(inputFilePathTextArea.text) === true) {
                                        inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_curColorOverlay_Color = inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_sureReadyColorOverlay_Color;
                                        inputFilePath_WayPlayBtnImage.sureReadyPlay = true;
                                    } else {
                                        inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_curColorOverlay_Color = inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_noReadyColorOverlay_Color;
                                        inputFilePath_WayPlayBtnImage.sureReadyPlay = false;
                                    }
                                }
                            }
                        }
                        //
                    }
                    //inputFilePath_WayPlayBtnImage
                    Image {
                        id: inputFilePath_WayPlayBtnImage
                        height: inputFilePathRect.collapsedHeight
                        width: height
                        anchors.left: inputFilePathRect.right
                        anchors.leftMargin: 1
                        anchors.top: inputFilePathRect.top
                        scale: 0.77
                        Behavior on scale {
                            NumberAnimation {
                                duration: 300
                                easing.type: Easing.OutCubic
                            }
                        }
                        mipmap: true
                        cache: true
                        asynchronous: false
                        antialiasing: true
                        property string inputFilePathWayBtn_playImageSourceUrl: "qrc:/image/ClearPlay.png"
                        property string inputFilePathWayBtn_pauseImageSourceUrl: "qrc:/image/ClearPause.png"
                        source: inputFilePathWayBtn_playImageSourceUrl
                        property color inputFilePathWayBtn_curColorOverlay_Color: inputFilePathWayBtn_noReadyColorOverlay_Color
                        property color inputFilePathWayBtn_noReadyColorOverlay_Color: "#d63348"
                        property color inputFilePathWayBtn_sureReadyColorOverlay_Color: "#578b2c"
                        property bool sureReadyPlay: false
                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            source: inputFilePath_WayPlayBtnImage
                            color: inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_curColorOverlay_Color
                        }
                        function handle_InputFilePathWay_play() {
                            inputFilePath_WayPlayBtnImage.source = inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_pauseImageSourceUrl;
                            BasicConfig.isPlaying = true;
                            videoRenderItem.tryAttachDelayed();
                            Client.qmlClickedReqPreparePlayVideo(inputFilePathTextArea.text);
                        }
                        function handle_InputFilePathWay_pause() {
                            inputFilePath_WayPlayBtnImage.source = inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_playImageSourceUrl;
                            BasicConfig.isPlaying = false;
                            Client.qmlClickedReqPreparePlayVideo(inputFilePathTextArea.text);
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (inputFilePath_WayPlayBtnImage.sureReadyPlay === true) {
                                    if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_VideoPlayerIndex) {
                                        BasicConfig.isPlaying = false;
                                        BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_VideoPlayerIndex;
                                    }

                                    if (BasicConfig.isPlaying === false)
                                        inputFilePath_WayPlayBtnImage.handle_InputFilePathWay_play();
                                    else
                                        inputFilePath_WayPlayBtnImage.handle_InputFilePathWay_pause();
                                }
                            }
                            onExited: cursorShape = Qt.ArrowCursor
                            onEntered: cursorShape = Qt.PointingHandCursor
                            onPressed: inputFilePath_WayPlayBtnImage.scale = 0.5
                            onReleased: inputFilePath_WayPlayBtnImage.scale = 0.77
                        }
                    }
                    //inputFilePathWay_OpenFileBtnImage
                    Image {
                        id: inputFilePathWay_OpenFileBtnImage
                        source: "qrc:/image/formkit_file.png"
                        height: inputFilePathRect.collapsedHeight
                        width: height
                        anchors.left: inputFilePath_WayPlayBtnImage.right
                        anchors.leftMargin: 0
                        scale: 0.7
                        Behavior on scale {
                            NumberAnimation {
                                duration: 300
                                easing.type: Easing.OutCubic
                            }
                        }
                        asynchronous: false
                        cache: true
                        mipmap: false
                        antialiasing: true

                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            source: inputFilePathWay_OpenFileBtnImage
                            color: "#1eb074"
                        }
                        Oran7FileDialog {
                            id: inputFilePathWay_OpenFileDialog
                            selectReset: true
                            onReady: {
                                inputFilePathTextArea.text = filesArray[0];
                                inputFilePathTextArea.focus = true;
                                //reset
                                if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_VideoPlayerIndex) {
                                    BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_VideoPlayerIndex;
                                }
                                inputFilePath_WayPlayBtnImage.source = inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_playImageSourceUrl;
                                BasicConfig.isPlaying = false;
                                Client.sigStop();
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onExited: cursorShape = Qt.ArrowCursor
                            onEntered: cursorShape = Qt.PointingHandCursor
                            onPressed: inputFilePathWay_OpenFileBtnImage.scale = 0.5
                            onReleased: inputFilePathWay_OpenFileBtnImage.scale = 0.7
                            onClicked: inputFilePathWay_OpenFileDialog.open()
                        }
                    }
                    //in inputFilePathRect
                }

                //======== 启用开发者信息调试模式 openVideoInfo======//
                Oran7ESelectRect {
                    id: openVIdeoInfo_eSelRect
                    labelText: "OpenVideoInfo"
                    onClicked: {
                        if (checked === false) {
                            checked = true;
                            videoRenderItem.openVideoInfo = checked;
                        } else {
                            checked = false;
                            videoRenderItem.openVideoInfo = checked;
                        }
                    }
                }
                //============= 选择videoScaleMode ==================//
                property int scaleMode: Client.Fill
                Label {
                    id: scaleModeTextLabel
                    font.pixelSize: 20
                    font.family: "微软雅黑"
                    font.bold: true
                    text: "ScaleMode:"
                    color: "#24161d"
                }
                Oran7ESelectRect {
                    labelText: "Fit"
                    checked: parent.scaleMode === Client.Fit
                    onClicked: {
                        parent.scaleMode = Client.Fit;
                        Client.setScaleMode(Client.VideoPlayerRender, Client.Fit);
                    }
                }
                Oran7ESelectRect {
                    labelText: "Fill"
                    checked: parent.scaleMode === Client.Fill
                    onClicked: {
                        parent.scaleMode = Client.Fill;
                        Client.setScaleMode(Client.VideoPlayerRender, Client.Fill);
                    }
                }
                //==========================================//

                //in colume
            }
            HoverHandler {
                id: videoPlayerMenueHoverHandler
                acceptedDevices: PointerDevice.Mouse
            }
            //in videoPlayerMenueRectangle
        }

        //videoBottomControlBarRect
        Rectangle {
            id: videoBottomControlBarRect
            anchors.left: root.left
            anchors.right: root.right
            anchors.bottom: root.bottom
            height: 80
            color: /*"#262626"*/ "transparent"
            visible: true
            opacity: 0
            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.OutCubic
                }
            }

            //PauseOrPlay_Image
            Image {
                id: playImage
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 7
                anchors.left: parent.left
                anchors.leftMargin: 15
                sourceSize.width: 40
                sourceSize.height: 40
                scale: 0.9
                Behavior on scale {
                    NumberAnimation {
                        duration: 300
                        easing.type: Easing.OutCubic
                    }
                }
                property string playImageSourceUrl: "qrc:/image/ClearPlay.png"
                property string pouseImageSourceUrl: "qrc:/image/ClearPause.png"
                source: BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_VideoPlayerIndex && BasicConfig.isPlaying ? pouseImageSourceUrl : playImageSourceUrl
                layer.enabled: true
                layer.effect: ColorOverlay {
                    source: playImage
                    color: "white"
                }
                property bool isPlaying: false
                MouseArea {
                    id: playIamgeMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if (!(BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_VideoPlayerIndex || BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_LivePlayerIndex))
                            return;
                        if (BasicConfig.isPlaying === false) {
                            if (BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_LivePlayerIndex)
                                bilibiliRoomNumWayPlayBtnImage.handle_bilibiliRoomNumWay_play();
                            if (BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_VideoPlayerIndex)
                                inputFilePath_WayPlayBtnImage.handle_InputFilePathWay_play();
                        } else {
                            if (BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_LivePlayerIndex)
                                bilibiliRoomNumWayPlayBtnImage.handle_bilibiliRoomNumWay_pause();
                            if (BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_VideoPlayerIndex)
                                inputFilePath_WayPlayBtnImage.handle_InputFilePathWay_pause();
                        }
                    }
                    onExited: cursorShape = Qt.ArrowCursor
                    onEntered: cursorShape = Qt.PointingHandCursor
                    onPressed: playImage.scale = 0.7
                    onReleased: playImage.scale = 0.9
                }
                Connections {
                    target: BasicConfig
                    function onPlayerFocusChanged() {
                        if (BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_VideoPlayerIndex) {
                            inputFilePath_WayPlayBtnImage.source = inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_playImageSourceUrl;
                        }
                    }
                }
                //<---playImage
            }

            //Oran7ProgressSlider
            Oran7ProgressSlider {
                id: videoProgressSlider
                width: parent.width * 0.98
                height: 12
                progressHandleWidth: 14
                anchors.top: parent.top
                anchors.topMargin: 10
                anchors.horizontalCenter: parent.horizontalCenter
                focusedPlayer: BasicConfig.globalPlayer_VideoPlayerIndex
                property real ratio: 0
                onPositionChanged: {
                    hideControlRectTimer.restart();
                }
                Connections {
                    target: Client
                    enabled: videoBottomControlBarRect.visible
                                 && videoBottomControlBarRect.enabled
                                 && videoBottomControlBarRect.Window
                                 && videoBottomControlBarRect.Window.active
                    function onUpdataQmlPlayProgressSliderCurPos(CurPos, CurTime_Second) {
                        if (videoProgressSlider.isPressed === false && BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_VideoPlayerIndex) {
                            //console.log(CurPos,CurTime_Second)
                            videoProgressSlider.nowSecondTime = CurTime_Second;
                            videoProgressSlider.ratio = CurPos / BasicConfig.max_Slider_Value;
                            // console.log(videoProgressSlider.ratio)
                            videoProgressSlider.progressHandleX = videoProgressSlider.width * videoProgressSlider.ratio - videoProgressSlider.progressHandleWidth / 2;
                            videoProgressSlider.visibleProgressX = videoProgressSlider.width * videoProgressSlider.ratio;
                            if (videoProgressSlider.nowSecondTime >= videoProgressSlider.allSecondTime) {
                                inputFilePath_WayPlayBtnImage.source = inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_playImageSourceUrl;
                            }
                        }
                    }
                    function onUpdataQmlPlayNowFileAllTime(AllTime) {
                        if (BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_VideoPlayerIndex) {
                            videoProgressSlider.allSecondTime = AllTime;
                        }
                    }
                    function onUpdataQmlTransforStopIcon() {
                        inputFilePath_WayPlayBtnImage.source = inputFilePath_WayPlayBtnImage.inputFilePathWayBtn_playImageSourceUrl;
                        videoProgressSlider.progressHandleX = videoProgressSlider.width - videoProgressSlider.progressHandleWidth / 2;
                        videoProgressSlider.visibleProgressX = videoProgressSlider.width;
                        BasicConfig.isPlaying = false;
                    }
                }
            }
            Label {
                id: progressTimeLabel
                anchors.left: playImage.right
                anchors.leftMargin: 15
                anchors.verticalCenter: playImage.verticalCenter
                color: "white"
                font.family: "微软雅黑"
                font.pixelSize: 20
                text: videoProgressSlider.nowTimeText + " - " + videoProgressSlider.allTimeText
            }

            //Volume set
            Oran7PlayerVolumeComponent {
                id: videoPlayerStackBottomControl_playerVolumeControl
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 20
            }

            // ================ bottom Control Hidden logic=================
            HoverHandler {
                id: hover
                acceptedDevices: PointerDevice.Mouse

                property point lastPos: Qt.point(-99999, -99999)
                property bool hasLast: false
                property real moveThreshold: 2   // 像素阈值，防抖

                onPointChanged: {
                    if (!hovered) {
                        hasLast = false;
                        return;
                    }

                    const p = point.position;
                    if (!hasLast) {
                        // 第一次进入/第一次拿到点，不算“移动”
                        lastPos = p;
                        hasLast = true;
                        return;
                    }

                    const dx = p.x - lastPos.x;
                    const dy = p.y - lastPos.y;
                    const dist2 = dx * dx + dy * dy;

                    if (dist2 >= moveThreshold * moveThreshold) {
                        //只有真的移动才显示并重置计时器
                        videoBottomControlBarRect.opacity = 1;
                        hideControlRectTimer.restart();
                        lastPos = p;
                    }
                }

                onHoveredChanged: {
                    // 离开区域就立刻开始隐藏计时
                    if (!hovered) {
                        hasLast = false;
                        hideControlRectTimer.restart();
                    }
                }
            }

            Timer {
                id: hideControlRectTimer
                repeat: false
                interval: 1500
                onTriggered: mouse => {
                    videoBottomControlBarRect.opacity = 0;
                }
            }
            //<--videoBottomControlBarRect
        }

        //============= VideoPlayerStack鼠标定时隐藏逻辑 =============//
        property bool mouseHidden: false
        onMouseHiddenChanged: {
            if (root.isActive === false)
                return;

            if (root.mouseHidden === true && videoPlayerMenueHoverHandler.hovered === false && videoPlayerMenueRectangle.fileDialog_isOpen === false)
                BasicConfig.clearAllUi_inVIdeoRenderArea(true);
            else
                BasicConfig.clearAllUi_inVIdeoRenderArea(false);
        }

        Timer {
            id: mouseHideTimer
            interval: 2000
            repeat: false
            running: true
            onTriggered: root.mouseHidden = true
        }

        HoverHandler {
            id: rootHover
            acceptedDevices: PointerDevice.Mouse

            //HoverHandler/PointerHandler of cursorShape
            cursorShape: root.mouseHidden ? Qt.BlankCursor : Qt.ArrowCursor

            property point lastPos: Qt.point(-99999, -99999)
            property bool hasLast: false
            property real threshold: 2

            onPointChanged: {
                if (!hovered) {
                    hasLast = false;
                    return;
                }

                const p = point.position;
                if (!hasLast) {
                    lastPos = p;
                    hasLast = true;
                    return;
                }

                const dx = p.x - lastPos.x;
                const dy = p.y - lastPos.y;
                if (dx * dx + dy * dy >= threshold * threshold) {
                    root.mouseHidden = false;
                    mouseHideTimer.restart();
                    lastPos = p;
                }
            }

            onHoveredChanged: {
                if (hovered) {
                    root.mouseHidden = false;
                    mouseHideTimer.restart();
                } else {
                    hasLast = false;
                }
            }
        }
        //====================================================//

        //<---root
    }
    // Component.completed: {
    //     if(openSemiCircleRect.openIngState === true)
    //     {
    //         openImage.rotation=180
    //     }
    // }
}
