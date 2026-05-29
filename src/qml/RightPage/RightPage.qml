import QtQuick
import Qt5Compat.GraphicalEffects
import QtQuick.Controls
import "./TopMenuBar"
import "../Basic"
import "./MainStackView"

import Oran7UI.Impl

Rectangle {
    id: root
    clip: false

    property int topBarHeight: Oran7Theme.Oran7MainGUI.topBarDefaultHeight

    //===================background 动态渐变效果 - 在启用============
    // 创建渐变层
    LinearGradient {
        id: gradientLayer

        visible: false

        anchors.fill: parent
        start: Qt.point(0, 0)
        end: Qt.point(width, height)
        property color begainColor: gradientLayer.colorItems[gradientLayer.currentColorIndex].begainColorValue
        property color middleColor: gradientLayer.colorItems[gradientLayer.currentColorIndex].middleColorValue
        property color endColor: gradientLayer.colorItems[gradientLayer.currentColorIndex].endColorValue
        property int currentColorIndex: 7
        onCurrentColorIndexChanged: {
            gradientLayerColorAnimationTImer.start();
        }
        property var colorItems: [
            {
                begainColorValue: "#96fbc4",
                middleColorValue: "#7ed321",
                endColorValue: "#f9f586"
            }//自然绿
            ,
            {
                begainColorValue: "#fa709a",
                middleColorValue: "#fee140",
                endColorValue: "#ff9a8b"
            }//焦糖奶茶
            ,
            {
                begainColorValue: "#fd63a3",
                middleColorValue: "#fe9800",
                endColorValue: "#ffb74d"
            }//夕阳橙
            ,
            {
                begainColorValue: "#ff6b6b",
                middleColorValue: "#ff4757",
                endColorValue: "#ee5a52"
            }//热情红
            ,
            {
                begainColorValue: "#f093fb",
                middleColorValue: "#f5576c",
                endColorValue: "#4facfe"
            }//霓虹粉
            ,
            {
                begainColorValue: "#0093e9",
                middleColorValue: "#00f2fe",
                endColorValue: "#4facfe"
            }//清新蓝
            ,
            {
                begainColorValue: "#ffcc02",
                middleColorValue: "#f7971e",
                endColorValue: "#ffd200"
            }//金秋黄
            ,
            {
                begainColorValue: "#2d5016",
                middleColorValue: "#a4de6c",
                endColorValue: "#40e0d0"
            }//森林松绿色
        ]
        //动态渐变
        Timer {
            id: gradientLayerColorAnimationTImer
            interval: gradientLayerColorParallelAnimation.transDuration + 1
            repeat: false
            onTriggered: {
                //console.log("gradientLayerColorAnimationTImer be triggered.")
                gradientLayerColorParallelAnimation.start();
            }
        }
        ParallelAnimation {
            id: gradientLayerColorParallelAnimation
            property real transDuration: 1000 //2000ms
            PropertyAnimation {
                property: "begainColor"
                to: gradientLayer.colorItems[gradientLayer.currentColorIndex].begainColorValue
                target: gradientLayer
                duration: gradientLayerColorParallelAnimation.transDuration
            }
            PropertyAnimation {
                property: "middleColor"
                to: gradientLayer.colorItems[gradientLayer.currentColorIndex].middleColorValue
                target: gradientLayer
                duration: gradientLayerColorParallelAnimation.transDuration
            }
            PropertyAnimation {
                property: "endColor"
                to: gradientLayer.colorItems[gradientLayer.currentColorIndex].endColorValue
                target: gradientLayer
                duration: gradientLayerColorParallelAnimation.transDuration
            }
            onFinished: {
                //console.log("gradientLayerColorParallelAnimation finished,index:",gradientLayer.currentColorIndex)
                gradientLayer.currentColorIndex = (gradientLayer.currentColorIndex + 1) % gradientLayer.colorItems.length;
            }
        }

        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: gradientLayer.begainColor
            }
            GradientStop {
                position: 0.5
                color: gradientLayer.middleColor
            }
            GradientStop {
                position: 1.0
                color: gradientLayer.endColor
            }
        }
    }

    property bool start_gradientLayerColorAnimation: false
    onStart_gradientLayerColorAnimationChanged: {
        if (root.start_gradientLayerColorAnimation === false) {
            gradientLayerColorAnimationTImer.stop();
        } else {
            gradientLayerColorAnimationTImer.start();
        }
    }

    //========================Ui ---->顶部=====================//
    Rectangle {
        id: topBarRect
        height: root.topBarHeight
        property bool haideUI: false
        // height:0
        Behavior on height {
            NumberAnimation {
                duration: 300
                easing.type: Easing.OutCubic
            }
        }
        clip: true

        anchors.left: parent.left
        anchors.right: parent.right
        color: "transparent"


        // ---- 暂停维护 2026/5/4 ---
        // TopRightBar {
        //     id: rightTopMenuItem
        //     visible: !topBarRect.haideUI
        //     anchors.top: parent.top
        //     anchors.right: parent.right
        //     height: 70
        //     width: 390
        // }

        // TopMiddleMenuBar {
        //     id: middleTopMenuItem
        //     visible: !topBarRect.haideUI
        //     anchors.top: parent.top
        //     anchors.right: rightTopMenuItem.left
        //     height: 70
        // }

        // TopLeftBar {
        //     id: leftTopMenuItem
        //     visible: !topBarRect.haideUI
        //     anchors.top: parent.top
        //     anchors.left: parent.left
        //     height: 70
        // }
        // -----------------------------
    }

    // ---- 暂停维护 2026/5/4 ---
    // SearchPopup {
    //     id: searchPopup
    //     width: 776
    //     height: 596
    //     x: leftTopMenuItem.x
    //     y: leftTopMenuItem.y + 65
    //     onClosed: {
    //         BasicConfig.gradientPosition2 = 1.0;
    //     }
    // }
    // LoginPopup {
    //     id: loginPopup
    //     width: 380
    //     height: 520
    //     anchors.centerIn: parent
    // }
    // MainLoginPopup {
    //     id: mainLoginPopup
    //     width: loginPopup.width
    //     height: loginPopup.height
    //     anchors.centerIn: parent
    // }
    // -----------------------------

    //=============  MainStackView  ===============
    MainStackView {
        id: mainStackView
        anchors.top: topBarRect.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: openCaptionBtn.openIngState ? 7 : 0
        clip: true

        initialItem: Qt.createComponent(BasicConfig.getUrl(BasicConfig.videoPlayerPage)).createObject(mainStackView, {
            "pageName": "VideoPlayerPage"
        })

        property bool isSettingStack: false
        signal upDateStack

        function popPage() {
            if (mainStackView.depth <= 1)
                return;
            mainStackView.pop(StackView.Immediate);

            mainStackView.upDateStack();
        }

        //========<Page Manager>=========//
        property var pageManager: ({
                //cache
                cache: {},
                // get Or create page
                getPage: function (url, pageName) {
                    if (this.cache[pageName]) {
                        return this.cache[pageName];
                    }

                    //Create
                    var component = Qt.createComponent(url);
                    if (component.status === Component.Ready) {
                        var page = component.createObject(mainStackView, {
                            "pageName": pageName
                        });
                        //cache this page
                        this.cache[pageName] = page;
                        return page;
                    }

                    console.warn("Failed load the Component.", url, pageName, component.errorString());
                    return null;
                },

                //navigate
                navigateTo: function (url, pageName) {
                    // 先检查栈中是否已有该页面
                    var existingStackPage = null;
                    for (var i = 0; i < mainStackView.depth; i++) {
                        var stackPage = mainStackView.get(i, StackView.DontLoad);
                        if (stackPage && stackPage.pageName === pageName) {
                            existingStackPage = stackPage;
                            break;
                        }
                    }

                    if (existingStackPage) {
                        // 如果栈中已有该页面，弹出到它
                        mainStackView.pop(existingStackPage, StackView.Immediate);
                        // 确保使用的是栈中的实例更新缓存
                        this.cache[pageName] = existingStackPage;
                    } else {
                        // 如果栈中没有该页面，从缓存获取或创建新实例
                        var page = this.getPage(url, pageName);
                        if (!page)
                            return;

                        mainStackView.push(page, {
                            "pageName": pageName
                        }, StackView.Immediate);
                    }
                }
            })

        Connections {
            target: BasicConfig
            function onPushVideoPlayerStackInto_RightPageMainStackView() {
                mainStackView.pageManager.navigateTo(BasicConfig.getUrl(BasicConfig.videoPlayerPage), "VideoPlayerPage");
                mainStackView.upDateStack();
            }
            function onPushMyFavoriteMusicStackInto_RightPageMainStackView() {
                mainStackView.pageManager.navigateTo(BasicConfig.getUrl(BasicConfig.myFavoritePage), "MyFavoriteMusicPage");
                BasicConfig.triggerLoad_localMusicList_Aniamtion(0);
                mainStackView.upDateStack();
            }
            function onPushLocalMusicStackInto_RightPageMainStackView() {
                mainStackView.pageManager.navigateTo(BasicConfig.getUrl(BasicConfig.localMusicPage), "LocalMusicPage");
                BasicConfig.triggerLoad_localMusicList_Aniamtion(0);
                mainStackView.upDateStack();
            }
            function onPushScreenCaptureStackInto_RightPageMainStackView() {
                mainStackView.pageManager.navigateTo(BasicConfig.getUrl(BasicConfig.screenCapturePage), "ScreenCapturePage");
                mainStackView.upDateStack();
            }
        }

        Connections {
            target: mainStackView
            function onUpDateStack() {
                if (!mainStackView.currentItem) {
                    console.warn("No current item in StackView");
                    return;
                }

                BasicConfig.clearElementBackgroundColorInLeftPage();

                var pageName = mainStackView.currentItem.pageName;
                if (!pageName) {
                    console.warn("Current item has no pageName property");
                    return;
                }

                var pageMap = {
                    "VideoPlayerPage": function () {
                        BasicConfig.focusCurrent_SelectedMenuModel("VideoPlayerPage");
                    },
                    "MyFavoriteMusicPage": function () {
                        BasicConfig.focusCurrent_SelectedMenuModel("MyFavoriteMusicPage");
                    },
                    "LocalMusicPage": function () {
                        BasicConfig.focusCurrent_SelectedMenuModel("LocalMusicPage");
                    },
                    "ScreenCapturePage": function () {
                        BasicConfig.focusCurrent_SelectedMenuModel("ScreenCapturePage");
                    }
                };
                //Control leftPageMenuElement SelectedStatue
                if (pageMap[pageName])
                    pageMap[pageName]();

                //Control BottomPage isVisible
                if (pageMap[pageName] && (mainStackView.currentItem.pageName === "LocalMusicPage" || mainStackView.currentItem.pageName === "MyFavoriteMusicPage")) {
                    if (bottomRectangle.visibleOpacity === 0) {
                        bottomRectangle.height = 80;
                        bottomRectangle.visibleOpacity = 1.0;
                    }
                } else {
                    if (bottomRectangle.visibleOpacity === 1) {
                        bottomRectangle.height = 0;
                        bottomRectangle.visibleOpacity = 0.0;
                    }
                }
            }
        }

        /*=====传递Global ListModel 实例=====*/
        property ListModel myFavoriteMusicListModel: ListModel {
            id: myFavoriteMusicListModel
        }
        property ListModel localMusicListModel: ListModel {
            id: localMusicListModel
        }

        Component.onCompleted: {
            BasicConfig.myFavoriteMusicListModel = myFavoriteMusicListModel;
            BasicConfig.localMusicListModel = localMusicListModel;
            start_gradientLayerColorAnimation = false;
        }
    }
}
