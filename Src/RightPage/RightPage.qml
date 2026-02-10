import QtQuick
import Qt5Compat.GraphicalEffects
import QtQuick.Controls
import "./TopMenuBar"
import "../Basic"
import "./MainStackView"
Rectangle{
    id:root
    clip: false

    property int topBarHeight : topBarDefultHeight
    property int topBarDefultHeight: 70

    //================background Color动态效果-未启用================//
    property real waveOffset: 0.0

    // 使用Canvas绘制波纹 - 作为背景
    Canvas {
        id: canvas
        anchors.fill: parent
        z: root.z

        visible: false

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            // 创建从右下到左上的渐变
            var gradient = ctx.createLinearGradient(
                width, height, 0, 0
            );

            // 定义颜色
            var baseColor = "#f8c7c7";
            var waveColor = "#f9f586";

            // 波纹宽度
            var waveWidth = 0.9;  // 30%的宽度

            // 计算波纹的开始和结束位置
            var startPos = Math.max(0, waveOffset - waveWidth/2);
            var endPos = Math.min(1, waveOffset + waveWidth/2);

            // 基础颜色
            gradient.addColorStop(0, baseColor);
            gradient.addColorStop(1, baseColor);

            // 只在波纹范围内添加渐变
            if (endPos > startPos) {
                // 添加渐变点
                for (var i = 0; i <= 10; i++) {
                    var pos = startPos + (endPos - startPos) * (i / 10);

                    // 计算到波纹中心的相对距离（-0.5到0.5）
                    var distance = (pos - waveOffset) / (waveWidth/2);

                    // 使用余弦函数创建平滑的波纹
                    // 当distance=0（中心）时，cos(0)=1
                    // 当distance=±1（边缘）时，cos(π)=0
                    var blendFactor = 0.5 + 0.5 * Math.cos(distance * Math.PI);

                    // 混合颜色
                    var r1 = parseInt(baseColor.substring(1, 3), 16);
                    var g1 = parseInt(baseColor.substring(3, 5), 16);
                    var b1 = parseInt(baseColor.substring(5, 7), 16);

                    var r2 = parseInt(waveColor.substring(1, 3), 16);
                    var g2 = parseInt(waveColor.substring(3, 5), 16);
                    var b2 = parseInt(waveColor.substring(5, 7), 16);

                    // 线性混合
                    var r = Math.round(r1 + (r2 - r1) * blendFactor);
                    var g = Math.round(g1 + (g2 - g1) * blendFactor);
                    var b = Math.round(b1 + (b2 - b1) * blendFactor);

                    // 转换为CSS颜色
                    var mixedColor = "#" +
                        r.toString(16).padStart(2, '0') +
                        g.toString(16).padStart(2, '0') +
                        b.toString(16).padStart(2, '0');

                    gradient.addColorStop(pos, mixedColor);
                }
            }

            // 填充背景
            ctx.fillStyle = gradient;
            ctx.fillRect(0, 0, width, height);
        }
    }

    // 动画
    NumberAnimation on waveOffset {
        from: -0.5
        to: 1.5
        duration: 3000
        loops: Animation.Infinite
        easing.type: Easing.InOutSine
        running: canvas.visible
    }

    onWaveOffsetChanged: canvas.requestPaint()

    //===================background 动态渐变效果============
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
            interval: gradientLayerColorParallelAnimation.transDuration + 1
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

    //========================Ui ---->顶部三件框=====================//
    Rectangle{
        id:topBarRect
        height:root.topBarHeight
        Behavior on height{
            NumberAnimation{
                duration: 300
                easing.type:Easing.OutCubic
            }
        }
        clip: true

        anchors.left: parent.left
        anchors.right: parent.right
        color: "transparent"
        TopRightBar{
            id:rightTopMenuItem
            anchors.top: parent.top
            anchors.right: parent.right
            height:70
            width: 390
        }

        //--------------------------------
        TopMiddleMenuBar{
            id:middleTopMenuItem
            anchors.top: parent.top
            anchors.right: rightTopMenuItem.left
            height:70
        }
        //--------------------------------
        TopLeftBar{
            id:leftTopMenuItem
            anchors.top: parent.top
            anchors.left: parent.left
            height:70
        }
    }
    SearchPopup{
        id:searchPopup
        width: 776
        height:596
        x:leftTopMenuItem.x
        y:leftTopMenuItem.y+65
        onClosed: {
            BasicConfig.gradientPosition2 = 1.0
        }
    }
    LoginPopup{
        id:loginPopup
        width: 380
        height:520
        anchors.centerIn: parent
    }
    MainLoginPopup{
        id:mainLoginPopup
        width: loginPopup.width
        height:loginPopup.height
        anchors.centerIn: parent
    }

    //============================
    //中间的主要大型栈界面
    MainStackView {
        id: mainStackView
        anchors.top: topBarRect.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true

        initialItem: Qt.createComponent("qrc:/Src/RightPage/MainStackView/VideoPlayerStack.qml")
                     .createObject(mainStackView, {"pageName": "VideoPlayerPage"})

        function popPage()
        {
            if(mainStackView.depth <= 1)return;
            mainStackView.pop(StackView.Immediate)

            mainStackView.upDateStack()
        }

        //========<Page Manager>=========//
        property var pageManager: ({
            //cache
            cache: {},
            // get Or create page
            getPage: function (url,pageName){
                if(this.cache[pageName])
                {
                    return this.cache[pageName]
                }

                //Create
                var component = Qt.createComponent(url)
                if(component.status === Component.Ready)
                {
                    var page = component.createObject(mainStackView, {"pageName": pageName})
                    //cache this page
                    this.cache[pageName]=page
                    return page
                }

                console.warn("Failed load the Component.",url,pageName,component.errorString())
                return null
            },

            //navigate
            navigateTo :function(url,pageName){
                var page = this.getPage(url,pageName);
                if(!page)return;

                //detect page isAlready in stack
                var isAlreadyInStack = false
                for(var i =0;i<mainStackView.depth;i++)
                {
                    var stackPage = mainStackView.get(i,StackView.DontLoad);
                    if(stackPage && stackPage.pageName === pageName)
                    {
                        mainStackView.pop(stackPage,StackView.Immediate)
                        isAlreadyInStack =true
                        break;
                    }
                }

                if(!isAlreadyInStack)
                {
                    //not in stack -->push new Page item
                    mainStackView.push(page,{"pageName":pageName},StackView.Immediate)
                    //maybe stack is too depthly ---> cleanup old stackPage
                }
            }
        });



        property bool isSettingStack: false
        signal upDateStack()

        Connections{
            target: BasicConfig
            function onPushVideoPlayerStackInto_RightPageMainStackView()
            {
                mainStackView.pageManager.navigateTo(BasicConfig.getUrl(BasicConfig.videoPlayerPage), "VideoPlayerPage")
                mainStackView.upDateStack()
            }
            function onPushMyFavoriteMusicStackInto_RightPageMainStackView()
            {
                mainStackView.pageManager.navigateTo(BasicConfig.getUrl(BasicConfig.myFavoritePage),"MyFavoriteMusicPage")
                BasicConfig.triggerLoad_localMusicList_Aniamtion(0)
                mainStackView.upDateStack()
            }
            function onPushLocalMusicStackInto_RightPageMainStackView()
            {
                mainStackView.pageManager.navigateTo(BasicConfig.getUrl(BasicConfig.localMusicPage),"LocalMusicPage");
                BasicConfig.triggerLoad_localMusicList_Aniamtion(0)
                mainStackView.upDateStack();
            }
            function onPushScreenCaptureStackInto_RightPageMainStackView()
            {
                mainStackView.pageManager.navigateTo(BasicConfig.getUrl(BasicConfig.screenCapturePage), "ScreenCapturePage")
                mainStackView.upDateStack();
            }
        }

        Connections {
            target: mainStackView
            function onUpDateStack()
            {
                if (!mainStackView.currentItem) {
                    console.warn("No current item in StackView")
                    return
                }

                BasicConfig.clearElementBackgroundColorInLeftPage()

                var pageName = mainStackView.currentItem.pageName
                if (!pageName) {
                    console.warn("Current item has no pageName property")
                    return
                }

                var pageMap = {
                    "VideoPlayerPage": function() {
                        BasicConfig.focusCurrent_SelectedMenuModel("VideoPlayerPage")
                    },
                    "MyFavoriteMusicPage": function() {
                        BasicConfig.focusCurrent_SelectedMenuModel("MyFavoriteMusicPage")
                    },
                    "LocalMusicPage": function() {
                        BasicConfig.focusCurrent_SelectedMenuModel("LocalMusicPage")
                    },
                    "ScreenCapturePage": function() {
                        BasicConfig.focusCurrent_SelectedMenuModel("ScreenCapturePage")
                    }
                }
                //Control leftPageMenuElement SelectedStatue
                if (pageMap[pageName])
                    pageMap[pageName]()

                //Control BottomPage isVisible
                if(pageMap[pageName] &&
                        (mainStackView.currentItem.pageName === "LocalMusicPage" ||
                         mainStackView.currentItem.pageName === "MyFavoriteMusicPage"))
                {
                    if(bottomRectangle.visibleOpacity === 0){
                        bottomRectangle.height = 80
                        bottomRectangle.visibleOpacity = 1.0
                    }
                }
                else
                {
                    if(bottomRectangle.visibleOpacity === 1){
                        bottomRectangle.height = 0
                        bottomRectangle.visibleOpacity = 0.0
                    }
                }
            }
        }

        /*=====传递Global ListModel 实例=====*/
        property ListModel myFavoriteMusicListModel: ListModel { id: myFavoriteMusicListModel }
        property ListModel localMusicListModel: ListModel { id: localMusicListModel }

        Component.onCompleted: {
            BasicConfig.myFavoriteMusicListModel = myFavoriteMusicListModel
            BasicConfig.localMusicListModel = localMusicListModel
            start_gradientLayerColorAnimation = true
        }
    }


}
