import QtQuick 2.15
import QtQuick.Controls
import QtCore
import QtQuick.Dialogs
import Qt5Compat.GraphicalEffects
import "../../Basic"
import Client 1.0

Item {
    id:root_parent
    // anchors.fill: parent
    property string pageName: ""
    Item {
        id:root
        anchors.fill: parent
        anchors.left: parent.left
        anchors.leftMargin: 40
        anchors.top: parent.top
        anchors.topMargin: 14
        clip: true

        //====================  LocalMusicStack in MainStackView isActive ???  ==================///
        property bool isActive: false
        // 获取 StackView
        property var _myStackView: null
        // 尝试获取 StackView
        function findAndSetupStackView()
        {
            if (StackView.view)
            {
                //console.log("通过 StackView.view 获取");
                _myStackView = StackView.view;
            }
            else
            {
                //通过父级查找
                //console.log("通过父级查找 StackView");
                var parent = root.parent;
                var depth = 0;
                while (parent && depth < 20)
                {
                    //console.log("查找深度", depth, "父级:", parent);
                    if (typeof parent.push === "function" &&
                        typeof parent.pop === "function")
                    {
                        //console.log("找到 StackView");
                        _myStackView = parent;
                        break;
                    }
                    parent = parent.parent;
                    depth++;
                }
            }
            if (root._myStackView)
            {
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
        function updateActiveState()
        {
            if (!_myStackView || !_myStackView.currentItem)
            {
                root.isActive = false;
                //console.log("_myStackView 或_myStackView.currentItem 无效-->isActive=false")
                return;
            }
            const newActive = (_myStackView.currentItem === root.parent);
            if (root.isActive !== newActive) {
                //console.log("活动状态变化:", newActive);
                root.isActive = newActive;
                if(root.isActive===true)
                {
                    musicListColum.clearAllMusicElementRectangleColorToTransparent(true)
                    BasicConfig.focusCurrentMusicInDisplayList()
                }
            }
            // else console.log("比较不通过")
        }
        // 初始化
        Component.onCompleted: {
            // console.log("=== LocalMusicStack 加载完成 ===");
            // console.log("root:", root);
            // console.log("StackView.view:", StackView.view);
            // console.log("root.parent:", root.parent);
            // 延迟执行，确保 StackView 已附加
            Qt.callLater(function() {
                findAndSetupStackView();
            });
        }
        // 清理
        Component.onDestruction: {
            if (_myStackView) {
                _myStackView.currentItemChanged.disconnect(updateActiveState);
            }
        }
        //====================   define Properties    =====================//

        //展示列表的元素间隔和元素高度
        property int musicListColumSpacing: 4
        property int musicElementRectangleHeight: 50

        property bool isMultiSelected: false
        onIsMultiSelectedChanged: {
            if(root.isMultiSelected === true)
                musicListColum.clearAllMusicElementRectangleColorToTransparent(true)
            else
                //重新聚焦播放中音乐，触发加载focused music
                BasicConfig.focusCurrentMusicInDisplayList()
        }

        //定义Colum的各个element统一换算比例的宽度
        property int indexTopLabelWidth: 30
        property int nameTopLabelWidth: (parent.width ) * (0.5 - 0.02)  //微调
        property int albumTopLabelWidth: (parent.width) * 0.3
        property int timeSizeTopLabelWidth: (parent.width ) * (0.1 -0.02) //微调
        //定义Colum的各个element统一换算比例的Margin
        property int indexTopLabelLeftMargin: 10
        property int nameTopLabelLeftMargin: 45
        property int albumTopLabelLeftMargin: root.nameTopLabelLeftMargin + root.nameTopLabelWidth
        property int timeSizeTopLabelLeftMargin: root.albumTopLabelLeftMargin + root.albumTopLabelWidth + 20

        //=======================  Ui  ====================//

        //-----------------------FvTopTitleFlow---------------------//
        Flow{
            id:localMusicTopTitleFlow
            anchors.top: parent.top
            anchors.left: parent.left
            spacing: 16
            Repeater{
                id:localMusicTopTitleFlowRepeater
                anchors.fill: parent
                model: ["本地音乐","下载中歌曲"]
                delegate: Label{
                    id:localMusicTopTitleFlowRepeaterElementLabel
                    text:modelData
                    font.family: "微软雅黑"
                    font.bold: true
                    font.pixelSize: 24
                    color: index===0?"#FF7381":"#2a1a22"
                    Connections{
                        target: BasicConfig
                        function onClearElementSubTitleMaxiHightLight_InLocalMusicStack()
                        {
                            if(localMusicTopTitleFlowRepeaterElementLabel.isFocus=== true)
                            {
                                localMusicTopTitleFlowRepeaterElementLabel.color="#2a1a22"
                                localMusicTopTitleFlowRepeaterElementLabel.isFocus= false
                                underlineElementRectangle.color="transparent"
                            }
                        }
                    }
                    property bool isFocus: index===0? true :false
                    Rectangle{
                        id:underlineElementRectangle
                        width: parent.implicitWidth-24
                        height: 3
                        radius: 1
                        anchors.top: parent.bottom
                        anchors.topMargin: 3
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: index===0?"#ff3a3a":"transparent"
                    }
                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                                cursorShape = Qt.PointingHandCursor
                        }
                        onExited: {
                                cursorShape = Qt.ArrowCursor
                        }
                        onClicked: {
                            // console.log("clicked")
                            if(localMusicTopTitleFlowRepeaterElementLabel.isFocus===false)
                            {
                                BasicConfig.clearElementSubTitleMaxiHightLight_InLocalMusicStack()
                                localMusicTopTitleFlowRepeaterElementLabel.isFocus=true
                                underlineElementRectangle.color="#ff3a3a"
                                localMusicTopTitleFlowRepeaterElementLabel.color="#FF7381"
                            }
                        }
                    }
                }
            }
        }

        //-----------------------  多选 MultiSelect  -----------------------//
        Rectangle{
            id:multiSelectRectangle
            width: 40
            height: width
            anchors.right: parent.right
            anchors.rightMargin: 30
            anchors.verticalCenter: searchRectangle.verticalCenter

            color: "transparent"

            Image {
                id: multiSelectImage
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
                source: "qrc:/image/selectmore.png"
                sourceSize.width: parent.width
                sourceSize.height: parent.height

                property color multiSelectImageColorOverlay_Color: "#2a1a22"
                layer.enabled: true
                layer.effect: ColorOverlay{
                    source: multiSelectImage
                    color: multiSelectImage.multiSelectImageColorOverlay_Color
                }

                asynchronous:true
                cache: false
                mipmap: true

                MouseArea{
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        multiSelectImage.multiSelectImageColorOverlay_Color = "#616161"
                        cursorShape = Qt.PointingHandCursor
                    }
                    onExited: {
                        multiSelectImage.multiSelectImageColorOverlay_Color = "#2a1a22"
                        cursorShape = Qt.ArrowCursor
                    }
                    onClicked: {
                        root.isMultiSelected = root.isMultiSelected ? false : true
                    }
                }
            }
        }
        //-----------------------  搜索框  -----------------------//
        Rectangle{
            id:searchRectangle
            // anchors.right: parent.right
            // anchors.rightMargin: 30
            anchors.right: multiSelectRectangle.left
            anchors.rightMargin: 10
            anchors.verticalCenter: localMusicTopTitleFlow.verticalCenter
            anchors.top: parent.top
            property int initWidth: 60
            width: searchRectangle.initWidth
            height: 30
            color: "#fef2e8"
            radius: height/2
            border.width: 1
            border.color: "#4d4d56"
            Connections{
                target: BasicConfig
                function onClickedOutside()
                {
                    if(localSearchRectangleTextField.focus === true)
                        localSearchRectangleTextField.focus = false
                }
            }

            TextField{
                id:localSearchRectangleTextField
                anchors.top: parent.top
                anchors.topMargin: 2
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 2
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.right: parent.right
                anchors.rightMargin: 8
                color: "cyan"
                background: Rectangle{
                    anchors.fill: parent
                    color: "transparent"
                }
                onFocusChanged: {
                    if(focus)
                    {
                        BasicConfig.newTextAreaFocused(localSearchRectangleTextField)
                    }
                }
            }
            ParallelAnimation{
                id:searchRectangleParallelAnimation_maxsize
                PropertyAnimation{
                    target:searchRectangle
                    property: "width"
                    from: searchRectangle.initWidth
                    to:searchRectangle.initWidth*2
                    easing.type: Easing.OutCubic
                }
            }
            ParallelAnimation{
                id:searchRectangleParallelAnimation_minisize
                PropertyAnimation{
                    target: searchRectangle
                    property: "width"
                    from:searchRectangle.width
                    to:searchRectangle.initWidth
                    easing.type: Easing.OutCubic
                }
            }
            MouseArea{
                hoverEnabled: true
                anchors.fill: parent
                onEntered: {
                    searchRectangleParallelAnimation_maxsize.start()
                }
                onExited: {
                    searchRectangleParallelAnimation_minisize.start()
                    // searchRectangle.forceActiveFocus()
                }
                onClicked: {
                    localSearchRectangleTextField.forceActiveFocus()
                }
            }
        }

        //-----------------------  Rectangle"#  , 标题 , 专辑 , '     ' ,  时长"  -----------------------//
        Rectangle{
            id:musicListTopRow
            anchors.top: localMusicTopTitleFlow.bottom
            anchors.topMargin: 16
            anchors.left: parent.left
            anchors.right: parent.right
            height: 28

            color: "transparent"

            visible: root.isMultiSelected ? false : true

            Label{
                id:localListIndexTopLabel
                height: parent.height
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin:root.indexTopLabelLeftMargin
                width: root.indexTopLabelWidth

                text:" # "
                font.family: "微软雅黑"
                font.pixelSize: 16
                color: "#2a1a22"
            }
            Label{
                id:localNameTopLabel
                height: parent.height
                anchors.verticalCenter: parent.verticalCenter
                width: root.nameTopLabelWidth
                anchors.left: parent.left
                anchors.leftMargin: root.nameTopLabelLeftMargin

                text:"   标题"
                font.family: "微软雅黑"
                font.pixelSize: 16
                color: "#2a1a22"
                background: Rectangle{
                    anchors.fill: parent
                    anchors.top: parent.top
                    anchors.topMargin:-4
                    radius: 8
                    opacity: 0.15
                    color: "transparent"
                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            parent.color="#7d7d84"
                        }
                        onExited: {
                            parent.color="transparent"
                        }
                    }
                }
            }
            Label{
                id:localAlbumTopLabel
                height: parent.height
                anchors.verticalCenter: parent.verticalCenter
                text:"  专辑"
                width: root.albumTopLabelWidth
                anchors.left: parent.left
                anchors.leftMargin: root.albumTopLabelLeftMargin

                font.family: "微软雅黑"
                font.pixelSize: 16
                color: "#2a1a22"
                background: Rectangle{
                    anchors.fill: parent
                    anchors.top: parent.top
                    anchors.topMargin:-4
                    radius: 8
                    opacity: 0.15
                    color: "transparent"
                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            parent.color="#7d7d84"
                        }
                        onExited: {
                            parent.color="transparent"
                        }
                    }
                }
            }
            Label{
                id:localTopLabel
                height: parent.height
                anchors.verticalCenter: parent.verticalCenter
                text:"    "
                width: 50
                anchors.left: parent.left
                anchors.leftMargin:4

                font.family: "微软雅黑"
                font.pixelSize: 16
                color: "#2a1a22"
            }
            Label{
                id:localTimeSizeTopLabel
                height: parent.height
                anchors.verticalCenter: parent.verticalCenter
                text:"   时长"
                width: root.timeSizeTopLabelWidth

                anchors.left: parent.left
                anchors.leftMargin:root.timeSizeTopLabelLeftMargin

                font.family: "微软雅黑"
                font.pixelSize: 16
                color: "#2a1a22"
                background: Rectangle{
                    anchors.fill: parent
                    anchors.top: parent.top
                    anchors.topMargin:-4
                    radius: 8
                    opacity: 0.15
                    color: "transparent"
                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            parent.color="#7d7d84"
                        }
                        onExited: {
                            parent.color="transparent"
                        }
                    }
                }
            }
        }
        //-----------------------  mutilSelectMenuRectangle  -----------------------//
        Rectangle{
            id:mutilSelectMenuRectangle
            height: 28
            anchors.left: localMusicDive1.left
            anchors.right: localMusicDive1.right
            anchors.bottom: localMusicDive1.top
            anchors.bottomMargin: 6
            color: "transparent"

            visible: root.isMultiSelected ? true : false

            Image{
                id:selectAllCheckButtonImage
                sourceSize.width: 30
                sourceSize.height:30
                anchors.left: parent.left
                anchors.leftMargin: 4
                anchors.top: parent.top
                anchors.topMargin:2

                property color defaultColorOverlay_Color: "#2a1a22"
                property color selectedColorOverlay_Color: "#ff3a3a"
                property color usedColorOverlay_Color: selectAllCheckButtonImage.defaultColorOverlay_Color

                source: "qrc:/image/selectAll.png"
                layer.enabled: true
                layer.effect: ColorOverlay{
                    anchors.fill: selectAllCheckButtonImage
                    source: selectAllCheckButtonImage
                    color: selectAllCheckButtonImage.usedColorOverlay_Color
                }
                asynchronous: false
                smooth: true
                mipmap: true
                antialiasing: true
                MouseArea{
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        cursorShape = Qt.PointingHandCursor
                    }
                    onExited: {
                        cursorShape = Qt.ArrowCursor
                    }
                    onClicked: {
                        if(selectAllCheckButtonImage.usedColorOverlay_Color === selectAllCheckButtonImage.defaultColorOverlay_Color)
                        {
                            selectAllCheckButtonImage.usedColorOverlay_Color = selectAllCheckButtonImage.selectedColorOverlay_Color
                            musicListFlickables.signal_SelectedAllElement()
                        }
                        else
                        {
                            selectAllCheckButtonImage.usedColorOverlay_Color = selectAllCheckButtonImage.defaultColorOverlay_Color
                            musicListFlickables.signal_ClearAllSelectedElement()
                        }
                    }
                }
            }
            Text{
                id:allSelectText
                anchors.left: selectAllCheckButtonImage.right
                anchors.leftMargin: 4
                anchors.verticalCenter: selectAllCheckButtonImage.verticalCenter
                color: "#2a1a22"
                text: "全选"
                font.pixelSize: 18
                font.family: "微软雅黑"
                font.bold: false
            }
        }
        //-----------------------  drag of float Tag  -----------------------//
        //拖拽相关的属性
        property bool showDragElementFloatTag: false
        property int dragSourceIndex: -1
        property int dragTargetIndex: -1
        property int hoveredGapIndex: -1
        property bool isDragging: false
        // 插入位置提示线组件 , 以every item 的Top为标准-->
        Rectangle {
            id: insertLine
            visible: false
            z: localMusicDive1.z-5
            y:localMusicDive1.y
            height: 4
            width: localMusicDive1.width-40
            color: "#ff7384"
            radius: 2

            property int insertLineYChangeAnimation_fromY: 0
            property int insertLineYChangeAnimation_toY: 0
            PropertyAnimation{
                id:insertLineYChangeAnimation
                target: insertLine
                property: "y"
                from:insertLine.insertLineYChangeAnimation_fromY
                to:insertLine.insertLineYChangeAnimation_toY
                duration: 100
                easing.type:Easing.OutQuad
            }
        }
        //拖动选中位置时，未在当前Flickable页码，自定义拖动至边缘触发--->代码控制滑动效果的Timer
        Timer{
            id:flickTimer
            repeat: true
            running: false
            interval: 16 //60fps
            property real flickYValue: 0
            onTriggered: {
                musicListFlickables.flick(0,flickTimer.flickYValue)
            }
        }

        //计算每个项的位置和高度
        function calculateItemRect(index)
        {
            var itemY=index * (root.musicListColumSpacing + root.musicElementRectangleHeight) + musicListFlickables.contentY
            return {
                y: itemY,
                top: itemY,
                center: itemY + 25,
                bottom: itemY + 50
            }
        }
        // 查找鼠标所在的位置是哪个项之间
        function findHoveredGap(mouseY)
        {
            var count = BasicConfig.localMusicListModel.count
            if (count === 0) return 0

            var relativeY = mouseY - musicListFlickables.y + musicListFlickables.contentY

            // 计算每个"槽位"（项+间距）的高度
            var slotHeight = root.musicElementRectangleHeight + root.musicListColumSpacing

            // 计算应该在哪一个槽位
            var slotIndex = Math.floor(relativeY / slotHeight)
            //过半偏移检测标准
            var offsetInSlot = relativeY  % slotHeight

            // 检查边界
            if (slotIndex < 0)
                return 0
            else if (slotIndex >= count) {
                // 检查是否在最后一项之后
                if (relativeY > calculateItemRect(count - 1).bottom)
                    return BasicConfig.localMusicListModel.count  // 在最后一项之后
                else
                    return count
            }
            else
            {
                // 在槽位内部，根据位置决定插入点
                if (offsetInSlot <= root.musicElementRectangleHeight / 2)// 在上半部分
                    return slotIndex
                else // 在下半部分
                    return slotIndex + 1
            }
        }
        //填充渲染insertLine <<----mian接口
        function renderInsertLine(mouseY)
        {
            //判断是否越界 , 越上界--->向下滑动 ; 越下界--->向上滑动 ；；；深度边界检测在Flickable中的onContentYChanged:
            //console.log("contentY:",musicListFlickables.contentY)
            //console.log("maxContentY:",musicListFlickables.maxContentY - 1.0)
            if(mouseY - musicListFlickables.y<= 0 && !musicListFlickables.contentY_atTop)
            {
                flickTimer.flickYValue = (root.musicElementRectangleHeight + root.musicListColumSpacing) * 10
                flickTimer.restart()
            }
            else if(mouseY - musicListFlickables.y > musicListFlickables.height && !musicListFlickables.contentY_atBottom)
            {
                flickTimer.flickYValue = -(root.musicElementRectangleHeight + root.musicListColumSpacing )* 10
                flickTimer.restart()
            }
            else flickTimer.stop()

            root.dragTargetIndex = findHoveredGap(mouseY)
            //console.log("dragTargetIndex:", root.dragTargetIndex)

            var lineY
            if (root.dragTargetIndex === 0) {
                // 在第一项之前
                lineY = musicListFlickables.y
            } else if (root.dragTargetIndex === BasicConfig.localMusicListModel.count) {
                // 在最后一项之后
                var lastItemRect = calculateItemRect(root.dragTargetIndex - 1)
                lineY = musicListFlickables.y + (lastItemRect.bottom - musicListFlickables.contentY * 2)
            } else {
                // 在两项之间
                var prevItemRect = calculateItemRect(root.dragTargetIndex - 1)
                lineY = musicListFlickables.y + (prevItemRect.bottom - musicListFlickables.contentY * 2) //注意：*2
            }
            insertLine.visible = false//reset

            if(lineY <= localMusicDive1.y)return;

            insertLine.insertLineYChangeAnimation_fromY = insertLine.y
            insertLine.insertLineYChangeAnimation_toY =lineY
            insertLineYChangeAnimation.restart()

            insertLine.x = musicListFlickables.x + 10

            insertLine.visible = true

            //console.log("insertLine.y:", lineY, "contentY:", musicListFlickables.contentY)
        }
        //reOrder <<---mian接口
        function reOrder()
        {
            var flickable_orignContentY = musicListFlickables.contentY
            var listCount=BasicConfig.localMusicListModel.count
            // 使用 JSON 创建深拷贝
            var items = []
            for (var i = 0; i < listCount; i++) {
                var original = BasicConfig.localMusicListModel.get(i)
                var copy = JSON.parse(JSON.stringify(original))
                items.push(copy)
            }

            for(i=0 ;i<listCount;i++)
            {
                items.push(BasicConfig.localMusicListModel.get(i ))
            }

            if(root.dragSourceIndex < root.dragTargetIndex-1)
                moveItem(items,root.dragSourceIndex,root.dragTargetIndex-1 < 0 ? 0 : root.dragTargetIndex -1)
            else if(root.dragSourceIndex > root.dragTargetIndex)
                moveItem(items,root.dragSourceIndex,root.dragTargetIndex)

            var itemsPath = []
            for(i =0 ;i<listCount ;i ++)
            {
                itemsPath.push(items[i ].filepath)
            }
            Client.reOrder_localMusicList(itemsPath);//emit signal

            BasicConfig.localMusicListModel.clear()
            for(i =0 ;i<listCount ;i ++)
            {
                BasicConfig.addLocalMusicItem(items[i].icon,items[i].music_name,items[i].music_artist,items[i].music_album,items[i].timesize,"-1",items[i].filepath)
            }
            musicListFlickables.animateRecentItems(0)//started by index of "0"
            //reset back
            musicListFlickables.contentY=flickable_orignContentY
        }
        // 用 splice 实现元素移动
        function moveItem(array, fromIndex, toIndex) {
            if (fromIndex === toIndex) return array;
            // 删除原位置的元素
            var element = array[fromIndex];
            array.splice(fromIndex, 1);
            // 插入到新位置
            array.splice(toIndex, 0, element);
            return array;
        }
        //<----The dragElementFloatTag Ui
        Rectangle{
            id:dragElementFloatTag
            x:root.x + 100
            y: 0
            z: musicListFlickables.z + 1
            clip: true
            width: 300
            height: 48
            radius: 4
            color: "#fef2e8"
            opacity: 1
            visible: root.showDragElementFloatTag
            layer.enabled: root.showDragElementFloatTag
            layer.effect: DropShadow{
                anchors.fill: dragElementFloatTag
                z:dragElementFloatTag.z
                source: dragElementFloatTag
                color:"#40000000"
                radius: 15
                samples: 30
                horizontalOffset: 5
                verticalOffset: 5
            }
            //这些都是默认甚至是无效的初值，需要外部预先初始化设定
            property int dragElementFloatTag_ParallelAniamtion_Duration: 200
            property int dragElementFloatTag_YChangeAmination_fromY: 0
            property int dragElementFloatTag_YChangeAmination_toY: 0
            property real dragElementFloatTag_OpacityChangeAnimation_fromValue: 0
            property real dragElementFloatTag_OpacityChangeAnimation_toValue: 1
            ParallelAnimation{
                id:dragElementFloatTag_ParallelAnimation
                PropertyAnimation{
                    target: dragElementFloatTag
                    property: "y"
                    from: dragElementFloatTag.dragElementFloatTag_YChangeAmination_fromY
                    to:dragElementFloatTag.dragElementFloatTag_YChangeAmination_toY
                    duration: dragElementFloatTag.dragElementFloatTag_ParallelAniamtion_Duration
                    easing.type:Easing.OutQuad
                }
            }
            PropertyAnimation{
                id:dragElementFloatTag_OpacityChangeAnimation
                target: dragElementFloatTag
                properties: "opacity"
                from: dragElementFloatTag.dragElementFloatTag_OpacityChangeAnimation_fromValue
                to:dragElementFloatTag.dragElementFloatTag_OpacityChangeAnimation_toValue
                duration: dragElementFloatTag.dragElementFloatTag_ParallelAniamtion_Duration
                easing.type:Easing.OutQuad
            }

            property string musicTagIconImageSource: "qrc:/image/Oran7.jpg"
            property string musicTagName:"Funny"
            property string musicTagArtist: "zzz"

            Rectangle {
                id: musicTagIconImageRectangle
                width: 36
                height: width
                anchors.left: parent.left
                anchors.leftMargin: 7
                anchors.verticalCenter: parent.verticalCenter
                radius: 7
                visible: false
            }
            OpacityMask{
                anchors.fill: musicTagIconImageRectangle
                anchors.centerIn:musicTagIconImageRectangle
                source: Image{
                    source:dragElementFloatTag.musicTagIconImageSource
                    asynchronous: false
                    cache: true
                    mipmap:true
                    antialiasing:true
                }
                maskSource: musicTagIconImageRectangle
            }

            Label{
                text:dragElementFloatTag.musicTagName
                color:"#2a1a22"
                clip: true
                font.pixelSize: 15
                font.family: "微软雅黑"
                width: dragElementFloatTag.width - 40
                anchors.left: musicTagIconImageRectangle.right
                anchors.leftMargin: 4
                anchors.top:dragElementFloatTag.top
                anchors.topMargin: 2
            }

            //仿：超清母带
            Rectangle{
                id:audioTagTypeRectangle
                color: "transparent"
                width: 44
                height: 16
                anchors.left: musicTagIconImageRectangle.right
                anchors.leftMargin: 4
                anchors.bottom: dragElementFloatTag.bottom
                anchors.bottomMargin: 6
                border.color: "#d3a03b"
                border.width: 0.8
                radius: 2
                Label{
                    text: "超清母带"
                    color: "#d3a03b"
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.family: "微软雅黑"
                    font.pixelSize: 10
                }
            }
            Label{
                text:dragElementFloatTag.musicTagArtist
                color:"#2a1a22"
                clip: true
                font.pixelSize: 15
                font.family: "微软雅黑"
                width: dragElementFloatTag.width - 100
                anchors.left: audioTagTypeRectangle.right
                anchors.leftMargin: 2
                anchors.bottom: audioTagTypeRectangle.bottom
            }
        }

        //-----------------------  分割线1 -----------------------//
        Rectangle{
            id:localMusicDive1
            width: parent.width-24
            height: 1
            color: "#2a1a22"
            anchors.left: parent.left
            anchors.top: musicListTopRow.bottom
            anchors.topMargin: 6
        }
        //-----------------------滑动列表右下的手动添加导入本地文件"+"-----------------------//
        Rectangle{
            id:addLocalMusicFileRectangle
            property int initWidth: 50
            width: addLocalMusicFileRectangle.initWidth
            height: addLocalMusicFileRectangle.width
            z:1
            anchors.right: parent.right
            anchors.rightMargin: addLocalMusicFileRectangle.initWidth
            anchors.bottom: parent.bottom
            anchors.bottomMargin: addLocalMusicFileRectangle.initWidth
            color: "white"
            radius: addLocalMusicFileRectangle.initWidth/2

            visible: root.isMultiSelected ? false : true

            layer.enabled: true
            layer.effect: DropShadow{
                id:addLocalMusicFileRectangleDropShadow
                anchors.fill: addLocalMusicFileRectangle
                z:addLocalMusicFileRectangle.z
                source: addLocalMusicFileRectangle
                color: "#80000000"
                radius: 15
                samples: 30
                horizontalOffset: 5
                verticalOffset: 5
            }

            Rectangle{
                width: addLocalMusicFileRectangle.initWidth*0.5
                height: 4
                radius: addLocalMusicFileRectangle.initWidth/2
                color: "gray"
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Rectangle{
                width: 4
                height: addLocalMusicFileRectangle.initWidth*0.5
                radius: addLocalMusicFileRectangle.initWidth/2
                color: "gray"
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }
            ParallelAnimation{
                id:addLocalMusicRectangleParallelAnimation_minisize
                PropertyAnimation{
                    target: addLocalMusicFileRectangle
                    property: "width"
                    from: addLocalMusicFileRectangle.initWidth
                    to:addLocalMusicFileRectangle.initWidth*0.9
                    duration: 80
                    easing.type: Easing.InQuad
                }
            }
            ParallelAnimation{
                id:addLocalMusicRectangleParallelAnimation_maxsize
                PropertyAnimation{
                    target: addLocalMusicFileRectangle
                    property: "width"
                    from: addLocalMusicFileRectangle.width
                    to:addLocalMusicFileRectangle.initWidth
                    duration: 80
                    easing.type: Easing.InQuad
                }
            }

            // <<<<<<功能组件，打开文件对话框>>>>>>//
            FileDialog{
                id:addLocalMusicFileRectangleFileDialog
                title:"选择本地音乐文件"
                nameFilters:["所有文件 (*)","mp3(*.mp3)","flac(*.flac)"]
                fileMode:FileDialog.OpenFiles
                currentFolder:{
                    if(BasicConfig.lastAddLocalMusicFolderPath==="")
                    {
                        var path = StandardPaths.writableLocation(StandardPaths.DesktopLocation)
                        if (!path || path === "")
                        {
                                path = StandardPaths.writableLocation(StandardPaths.HomeLocation)
                        }
                    }
                    else
                    {
                        path=BasicConfig.lastAddLocalMusicFolderPath
                    }
                    return path
                }
                onAccepted: {
                    const currentFolder = this.currentFolder.toString()
                    if(BasicConfig.lastAddLocalMusicFolderPath===""  || currentFolder !== BasicConfig.lastAddLocalMusicFolderPath)
                    {
                        BasicConfig.lastAddLocalMusicFolderPath=currentFolder
                    }
                    const files=selectedFiles.map(url => url.toString())
                    files.forEach(fileUrl => {
                            const localPath = fileUrl.replace("file:///", "")
                            console.log("==>[QML::addLocalMusicFileRectangleFileDialog:]Current LocalPath:", localPath)
                    })
                    Client.addNewLocalMusic(files)
                    //音乐列表结构变化，重新聚焦播放中音乐，触发加载focused music
                    BasicConfig.focusCurrentMusicInDisplayList()
                }
                onRejected: console.log("==>[QML::addLocalMusicFileRectangleFileDialog:]Cancel choose files...")
            }

            property bool isHovered: false
            MouseArea {
                id: addMusicMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    addLocalMusicFileRectangle.isHovered = true
                }
                onExited: {
                    addLocalMusicFileRectangle.isHovered = false
                }
                onPressed: {
                    addLocalMusicRectangleParallelAnimation_minisize.start()
                }
                onReleased: {
                    addLocalMusicRectangleParallelAnimation_maxsize.start()
                }
                onClicked: {
                    addLocalMusicFileRectangleFileDialog.open()
                }
            }
        }

        //===========================  The Main Flickable  =========================//
        //-------------------------< 大型可滑动的音乐列表 >------------------//
        Flickable{
            id:musicListFlickables
            contentHeight: BasicConfig.localMusicListModel.count * 54
            anchors.top:localMusicDive1.bottom
            anchors.topMargin: 2
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true
            ScrollBar.vertical: ScrollBar{
                anchors.right: parent.right
                anchors.rightMargin: 0
                width: 10
            }
            // 对外接口--->触发指定元素的动画
            function animateItem(index) {
                var item = musicListRepeator.itemAt(index)
                if (item) {
                    //console.log(index)
                    item.animateIn()
                    return true
                } else {
                    console.log("Item at index", index, "is not created yet")
                    return false
                }
            }
            // 对外接口--->触发所有最近添加的元素的动画
            function animateRecentItems(startIndex) {
                //console.log(startIndex)
                for (var i = startIndex; i < BasicConfig.localMusicListModel.count; i++) {
                    musicListFlickables.animateItem(i )
                }
            }
            //由Client触发的加入动画
            Connections{
                target:Client
                function onTriggerAddNewMusic_OpcityAniamtion(index){
                    if(root.isActive)
                    {
                        musicListFlickables.animateRecentItems(index)
                    }
                }
            }
            Connections{
                target: BasicConfig
                function onTriggerLoad_localMusicList_Aniamtion(index)
                {
                    musicListFlickables.animateRecentItems(index)
                }
            }

            // 监听 contentY 变化
            property bool contentY_atTop: true
            property bool contentY_atBottom: false
            onContentYChanged: {
                var maxContentY = contentHeight - height
                var tolerance = 1.0
                musicListFlickables.contentY_atBottom = contentY >= (maxContentY - tolerance)
                musicListFlickables.contentY_atTop = contentY <= 0

                if(musicListFlickables.contentY_atBottom || musicListFlickables.contentY_atTop)
                {
                    flickTimer.stop()
                }
            }
            // 定义统一的列宽计算函数
            property int indexWidth: 30
            property int nameWidth: root.nameTopLabelWidth
            property int albumWidth: root.albumTopLabelWidth
            property int timeWidth: root.timeSizeTopLabelWidth

            // 定义统一的列位置
            property int indexLeftMargin: 10
            property int nameLeftMargin: 45
            property int albumLeftMargin: nameLeftMargin + nameWidth + 10
            property int timeLeftMargin: albumLeftMargin + albumWidth + 20

            //selected all element signal
            signal signal_SelectedAllElement()
            //clear all selected element signal
            signal signal_ClearAllSelectedElement()
            Column{
                id:musicListColum
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.top: parent.top
                anchors.topMargin: root.musicListColumSpacing
                spacing: root.musicListColumSpacing
                property int prePlayIndex: -1
                property int playingIndex: -1
                signal clearAllMusicElementRectangleColorToTransparent(var isForce)

                //Connections use trigger to play next where signal from Client
                signal fromTriggerPlayNext_seletCorrect_index(var next_index)
                signal fromTriggerPlayLast_seletCorrect_index(var last_index)

                Connections{
                    target: Client
                    function onTriggerPlayNext(){
                        musicListColum.fromTriggerPlayNext_seletCorrect_index((BasicConfig.playingIndex + 1)%BasicConfig.localMusicListModel.count)
                    }
                    function onTriggerPlayLast(){
                        musicListColum.fromTriggerPlayLast_seletCorrect_index((BasicConfig.playingIndex - 1) < 0 ? BasicConfig.localMusicListModel.count-1 : BasicConfig.playingIndex - 1)
                    }
                }
                //<<<<<<-------------  Repeater
                Repeater{
                    id:musicListRepeator
                    anchors.fill: parent
                    model:BasicConfig.localMusicListModel
                    delegate: Rectangle{
                        id:musicElementRectangle
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.rightMargin: 20
                        height: root.musicElementRectangleHeight
                        radius: 10
                        color:"transparent"
                        // Behavior on color {
                        //     NumberAnimation{
                        //         duration: 100
                        //         easing.type: Easing.OutCubic
                        //     }
                        // }
                        // 控制动画的状态
                        property bool animationEnabled: true
                        property bool isAnimating: false
                        // 初始透明度
                        opacity: /*musicElementRectangle.animationEnabled === true ? 0 : */ 1
                        // 动画行为
                        Behavior on opacity {
                            enabled: musicElementRectangle.animationEnabled
                            NumberAnimation
                            {
                                duration: 300
                                easing.type: Easing.OutCubic
                            }
                        }
                        // 对外动画接口
                        function animateIn()
                        {
                            //console.log(index)
                            if (musicElementRectangle.animationEnabled && !musicElementRectangle.isAnimating)
                            {
                                musicElementRectangle.isAnimating = true
                                fadeInAnimation.start()
                            }
                        }
                        SequentialAnimation {
                            id: fadeInAnimation
                            NumberAnimation
                            {
                                target: musicElementRectangle
                                property: "opacity"
                                from: 0
                                to: 1
                                duration: 300
                                easing.type: Easing.OutCubic
                            }
                            ScriptAction {
                                script: musicElementRectangle.isAnimating = false
                            }
                        }
                        // 布局列表间隔宽度使用统一的计算值
                        property int myIndexWidth: musicListFlickables.indexWidth
                        property int myNameWidth: musicListFlickables.nameWidth
                        property int myAlbumWidth: musicListFlickables.albumWidth
                        property int myTimeWidth: musicListFlickables.timeWidth
                        //与index同级并位,Used by multilaztionSelect
                        Rectangle{
                            id:selectCheckButtonRectangle
                            width: 18
                            height:width
                            radius: 4
                            anchors.left: parent.left
                            anchors.leftMargin: musicListFlickables.indexLeftMargin
                            anchors.verticalCenter: parent.verticalCenter
                            visible: root.isMultiSelected ? true : false

                            property bool isSelected: false
                            color: selectCheckButtonRectangle.isSelected ? "#ff7381" : "transparent"
                            border.width: 1
                            border.color: /*selectCheckButtonRectangle.isSelected ? "transparent" : */"#2a1a22"
                            function change_SelectedState()
                            {
                                if(selectCheckButtonRectangle.isSelected === false)
                                    selectCheckButtonRectangle.isSelected = true
                                else
                                    selectCheckButtonRectangle.isSelected = false
                            }
                            Connections{
                                target: musicListFlickables
                                function onSignal_SelectedAllElement()
                                {
                                    if(selectCheckButtonRectangle.isSelected === false)
                                        selectCheckButtonRectangle.isSelected = true
                                }
                                function onSignal_ClearAllSelectedElement()
                                {
                                    if(selectCheckButtonRectangle.isSelected === true)
                                        selectCheckButtonRectangle.isSelected = false
                                }
                            }
                            MouseArea{
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    selectCheckButtonRectangle.change_SelectedState()
                                }
                            }
                        }
                        //index
                        Rectangle{
                            id:localListIndexRectangle
                            width: musicElementRectangle.myIndexWidth
                            height: width
                            anchors.left:parent.left
                            anchors.leftMargin: musicListFlickables.indexLeftMargin
                            anchors.verticalCenter: parent.verticalCenter
                            color:"transparent"
                            visible: root.isMultiSelected ? false : true
                            property bool indexLabelVisible: true
                            Label{
                                id:localListIndexRectangleLabel
                                visible:localListIndexRectangle.indexLabelVisible
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left:parent.left
                                anchors.leftMargin: 5
                                text: index<9 ? "0"+String(index+1):String(index+1)
                                font.family: "微软雅黑"
                                font.pixelSize: 14
                                color: "#2a1a22"
                            }
                            //每一行实例对全局BasicConfig.isPlaying的观测响应
                            Connections{
                                target: BasicConfig
                                function onIsPlayingChanged(){
                                    if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)return;
                                    if(BasicConfig.isPlaying===true)
                                    {
                                        if(index ===musicListColum.playingIndex)
                                            localListIndexRectangPlayBtnImage.source = musicElementRectangle.clearPauseIconURL
                                    }
                                    else
                                    {
                                        if(index ===musicListColum.playingIndex)
                                            localListIndexRectangPlayBtnImage.source = musicElementRectangle.clearPlayIconURL
                                    }
                                }
                            }
                            //用于触发播放上一首or下一首
                            Connections{
                                target: musicListColum
                                function onFromTriggerPlayNext_seletCorrect_index(next_index){
                                    if(next_index===index && root.isActive===true)
                                    {
                                        //console.log("next_index:",next_index)
                                        headerPlaybuttonMouseArea.handleClicked()
                                    }
                                }
                                function onFromTriggerPlayLast_seletCorrect_index(last_index){
                                    if(last_index === index && root.isActive===true)
                                    {
                                        headerPlaybuttonMouseArea.handleClicked()
                                    }
                                }
                            }
                            //headerPlaybuttonMouseArea
                            MouseArea{
                                id:headerPlaybuttonMouseArea
                                anchors.fill: parent
                                enabled: root.isMultiSelected ? false : true
                                //封装的点击事件处理逻辑
                                function handleClicked()
                                {
                                    //由于当handleClicked()的调用来源是onFromTriggerPlayNext/Last_seletCorrect_index时，这里只处理了清空选中状态，包括背景和头部index和playimage的转换
                                    //但仍未处理musicElementRectangle.Entered()选中下一元素的事件，这里额外处理
                                    //console.log("handledClided")
                                    if(musicElementRectangle.color !=="#fef2e8" && root.isMultiSelected === false)
                                    {
                                       //console.log("handledClided=========")
                                       musicElementRectangle.color="#fef2e8"
                                       localListIndexRectangle.indexLabelVisible = false
                                       localListIndexRectangPlayBtnImage.visible = true
                                    }
                                    //更新ui层播放状态
                                    if(BasicConfig.isPlaying==false || index !==musicListColum.playingIndex)
                                    {
                                        BasicConfig.isPlaying=true
                                    }
                                    else
                                    {
                                        BasicConfig.isPlaying=false
                                    }
                                    //响应后台
                                    if(musicListColum.playingIndex===-1)
                                    {
                                        //初选文件,单独设置交互和响应
                                        BasicConfig.resetAllPlayListHeadicon()
                                        localListIndexRectangPlayBtnImage.source = musicElementRectangle.clearPauseIconURL
                                        //update playingIndex
                                        musicListColum.playingIndex=index
                                        BasicConfig.playingIndex = musicListColum.playingIndex
                                        BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_MusicPlayerIndex
                                        BasicConfig.currentMediaFilePath=filepath
                                        BasicConfig.currentMediaName=music_name
                                        BasicConfig.currentMediaArtistAuthor=music_artist
                                        BasicConfig.currentMediaCoverFilePath=musicElementRectangle.innerCurrentMediaoCover
                                    }
                                    else if(index !==musicListColum.playingIndex)
                                    {
                                        //console.log("handledClided=========")
                                        //切换文件 , 切换之间未改变BasicConfig.isPlaying，一直为true无法触发全局响应，此处单独设置
                                        BasicConfig.resetAllPlayListHeadicon()
                                        localListIndexRectangPlayBtnImage.source = musicElementRectangle.clearPauseIconURL
                                        //检测到上一次选择非空，先触发清空上一则背景选择效果，再改变playingIndex
                                        musicListColum.clearAllMusicElementRectangleColorToTransparent(false)
                                        musicListColum.playingIndex=index
                                        BasicConfig.playingIndex = musicListColum.playingIndex
                                        BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_MusicPlayerIndex
                                        BasicConfig.currentMediaFilePath=filepath
                                        BasicConfig.currentMediaName=music_name
                                        BasicConfig.currentMediaArtistAuthor=music_artist
                                        BasicConfig.currentMediaCoverFilePath=musicElementRectangle.innerCurrentMediaoCover
                                    }
                                    Client.qmlClickedReqPreparePlayMusic(filepath)//发送到cpp
                                }
                                onClicked: {
                                    //改为MusicPlayerFocus
                                    if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                                    {
                                        BasicConfig.isPlaying =false
                                        BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_MusicPlayerIndex
                                    }
                                    headerPlaybuttonMouseArea.handleClicked()//调用封装成函数的点击事件处理逻辑
                                }
                            }
                        }
                        //---->播放中时，替换index显示的序号转为播放图标
                        property string listPlayBtnImageColorOverlay_Color: "#FF8F6E"
                        property string clearPlayIconURL: "qrc:/image/ClearPlay.png"
                        property string clearPauseIconURL: "qrc:/image/ClearPause.png"
                        Image {
                            id:localListIndexRectangPlayBtnImage
                            source:musicElementRectangle.clearPlayIconURL
                            scale: 0.7
                            visible: false

                            anchors.fill: localListIndexRectangle
                            anchors.verticalCenter: localListIndexRectangle.verticalCenter
                            anchors.left:parent.left
                            anchors.leftMargin: 5
                            layer.enabled: true
                            layer.effect: ColorOverlay{
                                source: localListIndexRectangPlayBtnImage
                                color: musicElementRectangle.listPlayBtnImageColorOverlay_Color
                            }
                            asynchronous: false  // 拖动时改为同步，避免异步计算导致的卡顿
                            // mipmap: true  // 启用mipmap，提高缩放性能
                            // smooth: false  // 拖动时关闭平滑，提高性能
                            antialiasing: true
                        }
                        //musicIcon  采用异步加载
                        Rectangle {
                            id: localIconImageRectangle
                            width: 36
                            height: width
                            anchors.left: parent.left
                            anchors.leftMargin: 50
                            anchors.verticalCenter: parent.verticalCenter
                            radius: 7
                            visible: false
                        }
                        property string innerCurrentMediaoCover:icon
                        OpacityMask {
                            anchors.fill: localIconImageRectangle
                            anchors.centerIn: localIconImageRectangle
                            source: Image {
                                source: musicElementRectangle.innerCurrentMediaoCover
                                asynchronous: true // 异步加载
                                cache: true             // 启用缓存
                                mipmap: true
                                antialiasing: true
                            }
                            maskSource: localIconImageRectangle
                        }
                        //music_Name
                        Label{
                            id:localNameLabel
                            text:music_name
                            color:"#2a1a22"
                            font.pixelSize: 15
                            font.family: "微软雅黑"
                            anchors.left: localIconImageRectangle.right
                            anchors.leftMargin: 8
                            anchors.top: localIconImageRectangle.top
                            width: musicElementRectangle.myNameWidth - 44  // 减去图标宽度和边距
                        }
                        //仿：超清母带
                        Rectangle{
                            id:audioTypeRectangle
                            color: "transparent"
                            width: 44
                            height: 16
                            anchors.top: localNameLabel.bottom
                            anchors.topMargin: 2
                            anchors.left: localNameLabel.left
                            border.color: "#d3a03b"
                            border.width: 0.8
                            radius: 2
                            Label{
                                text: "超清母带"
                                color: "#d3a03b"
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.horizontalCenter: parent.horizontalCenter
                                font.family: "微软雅黑"
                                font.pixelSize: 10
                            }
                        }
                        //artist
                        Label{
                            id:localArtistLabel
                            text: music_artist
                            width: musicElementRectangle.myNameWidth - audioTypeRectangle.width - 100
                            clip: true
                            anchors.left:audioTypeRectangle.right
                            anchors.leftMargin: 4
                            anchors.bottom: audioTypeRectangle.bottom
                            font.family: "微软雅黑"
                            font.pixelSize: 14
                            color: "#2a1a22"
                        }
                        //album
                        Label{
                            id:localAlbumLabel
                            anchors.left: parent.left
                            anchors.leftMargin: musicListFlickables.albumLeftMargin
                            anchors.verticalCenter: parent.verticalCenter
                            width: musicElementRectangle.myAlbumWidth
                            text:music_album
                            font.family: "微软雅黑"
                            font.pixelSize: 14
                            color:"#2a1a22"
                        }
                        //loved
                        // Image {
                        //     id: isLovedImage
                        //     source: "qrc:/image/loved.png"
                        //     anchors.verticalCenter: parent.verticalCenter
                        //     anchors.left: parent.left
                        //     anchors.leftMargin: 654
                        //     layer.enabled: true
                        //     layer.effect: ColorOverlay{
                        //         source:isLovedImage
                        //         color: "#2a1a22"
                        //     }
                        // }
                        //timesize
                        Label{
                            id:localTimeSizeLabel
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: musicListFlickables.timeLeftMargin
                            width: musicElementRectangle.myTimeWidth

                            property int allTime: parseInt(timesize)
                            text:String(Math.floor(Math.floor(allTime/60)/10))+String(Math.floor(allTime/60)%10)+":"+String(Math.floor((allTime%60/10)))+String((allTime%60%10))
                            color: "#2a1a22"
                            visible: root.isMultiSelected ? false : true
                            font.family: "微软雅黑"
                            font.pixelSize: 13
                        }
                        //drag icon in multilization selected mode
                        Image{
                            id:dragImage
                            anchors.right: parent.right
                            anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            sourceSize.width: 40
                            sourceSize.height: 40
                            source: "qrc:/image/drag.png"
                            visible: root.isMultiSelected ? true : false

                            property color defaultColorOverlay_Color: "#2a1a22"
                            property color dragedColorOverlay_Color: "#ff7384"
                            property color usedColorOverlay_Color: dragImage.defaultColorOverlay_Color

                            layer.enabled: true
                            layer.effect: ColorOverlay{
                                anchors.fill: dragImage
                                source:dragImage
                                color:dragImage.usedColorOverlay_Color
                            }

                            asynchronous: false
                            smooth: true
                            mipmap: true
                            antialiasing: true

                            //拖拽窗口示例，自定义计算方法
                            MouseArea{
                                anchors.fill: parent
                                //启用拖拽
                                // drag.target: dragTarget
                                // drag.axis: Drag.XAndYAxis
                                // drag.threshold: 0
                                // drag.smoothed: false
                                // 拖拽代理
                                // Item {
                                //     id: dragTarget
                                //     visible: false
                                //     width: dragElementFloatTag.width
                                //     height: dragElementFloatTag.height
                                // }
                                property bool hasMoved: false
                                onPositionChanged: (mouse) => {
                                    // 将鼠标坐标映射到父控件
                                    var posInParent = mapToItem(root, mouse.x, mouse.y)
                                    //console.log("父控件坐标:", posInParent.x, posInParent.y)
                                    //console.log("全局坐标:", mapToGlobal(mouse.x, mouse.y))
                                    //相对坐标
                                    // var relativeX = mouse.x/* + dragImage.x*/
                                    // var relativeY = mouse.y/* + dragImage.y*/
                                    // console.log("相对坐标:", relativeX, relativeY)

                                    //initilization dragElementFloatTag_ParallelAnimation-->Value
                                    dragElementFloatTag.dragElementFloatTag_YChangeAmination_fromY=dragElementFloatTag.y
                                    dragElementFloatTag.dragElementFloatTag_YChangeAmination_toY = posInParent.y - dragElementFloatTag.height/2
                                    //dragElementFloatTag_ParallelAnimation --->restart
                                    dragElementFloatTag_ParallelAnimation.restart()

                                    //渲染填充GopLine
                                    root.renderInsertLine(dragElementFloatTag.y)
                                    hasMoved=true
                                }
                                onPressed: (mouse)=>{
                                    dragElementFloatTag.musicTagIconImageSource = icon
                                    dragElementFloatTag.musicTagName = music_name
                                    dragElementFloatTag.musicTagArtist = music_artist

                                    musicListFlickables.interactive = false
                                    root.showDragElementFloatTag = true
                                    //initilization dragElementFloatTag_OpacityChangeAnimation---> Value
                                    dragElementFloatTag.dragElementFloatTag_OpacityChangeAnimation_fromValue = 0
                                    dragElementFloatTag.dragElementFloatTag_OpacityChangeAnimation_toValue = 1
                                    //dragElementFloatTag_OpacityChangeAnimation--->restart
                                    dragElementFloatTag_OpacityChangeAnimation.restart()


                                    root.isDragging=true
                                    root.dragSourceIndex=index
                                    dragImage.usedColorOverlay_Color = dragImage.dragedColorOverlay_Color

                                    // 设置拖拽元素的初始位置为鼠标位置
                                    var posInParent = mapToItem(root, mouse.x, mouse.y)
                                    dragElementFloatTag.y=posInParent.y - dragElementFloatTag.height/2
                                }
                                onReleased: {
                                    musicListFlickables.interactive = true

                                    root.showDragElementFloatTag = false
                                    root.isDragging=false
                                    insertLine.visible = false
                                    insertLine.y=localMusicDive1.y
                                    dragImage.usedColorOverlay_Color = dragImage.defaultColorOverlay_Color

                                    //makesure flickTimer stop
                                    flickTimer.stop()

                                    //reOrder
                                    if( !(root.dragTargetIndex === root.dragSourceIndex || root.dragTargetIndex === root.dragSourceIndex + 1) && hasMoved)
                                    {
                                        root.reOrder()
                                        hasMoved=false
                                    }

                                    // dragElementFloatTag.musicTagIconImageSource = null
                                    // dragElementFloatTag.musicTagName = ""
                                    // dragElementFloatTag.musicTagArtist = ""
                                }
                            }
                        }

                        property string musicFilePath: filepath//传出ListModel中存储的文件路径
                        Connections {
                            target: musicListColum
                            function onClearAllMusicElementRectangleColorToTransparent(isForce) {
                                if(musicListColum.playingIndex === index || isForce === true)
                                {
                                    musicElementRectangle.color = "transparent"
                                    localListIndexRectangPlayBtnImage.visible=false
                                    localListIndexRectangle.indexLabelVisible=true
                                }
                            }
                        }
                        Connections{
                            target: BasicConfig
                            function onResetAllPlayListHeadicon(){
                                if(musicListColum.playingIndex === index)
                                    localListIndexRectangPlayBtnImage.source = clearPlayIconURL
                            }
                            function onFocusCurrentMusicInDisplayList(){//这个一般在初始化加载当前Stack窗口时触发，在addLocalMusic时触发
                                if(filepath === BasicConfig.currentMediaFilePath)
                                {
                                    //musicListColum.clearAllMusicElementRectangleColorToTransparent(true)//确保其它的已经取消选中
                                    //额外触发musicElementRectangle.Entered()选中元素的事件
                                    if(musicElementRectangle.color !=="#fef2e8")
                                    {
                                       musicElementRectangle.color="#fef2e8"
                                       localListIndexRectangle.indexLabelVisible = false
                                       localListIndexRectangPlayBtnImage.visible = true
                                    }
                                    //更新全局变量
                                    musicListColum.playingIndex=index
                                    BasicConfig.playingIndex = musicListColum.playingIndex
                                    //如果是播放中的状态要设置head play icon 为 true
                                    if(BasicConfig.isPlaying === true && BasicConfig.globalPlayingFocus === BasicConfig.globalPlayer_MusicPlayerIndex)
                                    {
                                        localListIndexRectangPlayBtnImage.source = clearPauseIconURL
                                    }
                                }
                            }
                        }
                        MouseArea{
                            id:musicElementRectangleMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            propagateComposedEvents: true
                            enabled: root.isMultiSelected ? false : true

                            function handleExitedEvent()
                            {
                                musicElementRectangle.color="transparent"
                                localListIndexRectangle.indexLabelVisible=true
                                localListIndexRectangPlayBtnImage.visible=false
                            }
                            function handleEnteredEvent()
                            {
                                //确保进入下一项时，其它项都已取消选中
                                //musicListColum.clearAllMusicElementRectangleColorToTransparent()

                                musicElementRectangle.color="#fef2e8"
                                localListIndexRectangle.indexLabelVisible=false
                                localListIndexRectangPlayBtnImage.visible = true
                            }
                            onEntered:{
                                if(musicListColum.playingIndex !== index)
                                {
                                    musicElementRectangleMouseArea.handleEnteredEvent()
                                }
                            }
                            onPositionChanged: (mouse)=>{
                                // 检测鼠标是否在内层playBtnImage上方
                                var point = mapToItem(localListIndexRectangle, mouse.x, mouse.y);
                                var isOverIndexRect = (point.x >= 0 && point.x <= localListIndexRectangle.width &&
                                                      point.y >= 0 && point.y <= localListIndexRectangle.height);
                                if (isOverIndexRect)
                                {
                                    // 鼠标在playBtnImage内层区域
                                    if(musicElementRectangle.listPlayBtnImageColorOverlay_Color !== "#FF6233")
                                    {
                                        musicElementRectangle.listPlayBtnImageColorOverlay_Color = "#FF6233"
                                        cursorShape = Qt.PointingHandCursor
                                    }
                                }
                                else
                                {
                                    // 鼠标在playBtnImage外层区域
                                    if(musicElementRectangle.listPlayBtnImageColorOverlay_Color !== "#FF8F6E")
                                    {
                                        musicElementRectangle.listPlayBtnImageColorOverlay_Color = "#FF8F6E"
                                        cursorShape = Qt.ArrowCursor
                                    }
                                }
                                // 外层hover效果
                                if(musicListColum.playingIndex !== index)
                                {
                                    if(musicElementRectangle.color !=="#fef2e8")
                                    {
                                       musicElementRectangle.color="#fef2e8"
                                       localListIndexRectangle.indexLabelVisible = false
                                       localListIndexRectangPlayBtnImage.visible = true
                                    }
                                }
                                //检测鼠标是否在addMusicFileRectangle上方
                                if(addLocalMusicFileRectangle.isHovered===true)
                                {
                                    musicElementRectangleMouseArea.handleExitedEvent()
                                }
                                else
                                {
                                    musicElementRectangleMouseArea.handleEnteredEvent()
                                }
                            }
                            onExited: {
                                if(musicListColum.playingIndex !== index)//#FF8F6E
                                {
                                    musicElementRectangleMouseArea.handleExitedEvent()
                                }
                                //console.log("Existed index:",index)
                                //console.log("PlayingIndex:",musicListColum.playingIndex)
                            }
                            onDoubleClicked: {
                                //改为MusicPlayerFocus
                                if(BasicConfig.globalPlayingFocus !== BasicConfig.globalPlayer_MusicPlayerIndex)
                                {
                                    BasicConfig.isPlaying =false
                                    BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_MusicPlayerIndex
                                }
                                //更新ui层播放状态
                                if(BasicConfig.isPlaying==false || index !==musicListColum.playingIndex)
                                {
                                    BasicConfig.isPlaying=true
                                }
                                else
                                {
                                    BasicConfig.isPlaying=false
                                }
                                //响应后台
                                if(musicListColum.playingIndex===-1)
                                {
                                    //初选文件,单独设置交互和响应
                                    BasicConfig.resetAllPlayListHeadicon()
                                    localListIndexRectangPlayBtnImage.source = musicElementRectangle.clearPauseIconURL
                                    //update playingIndex
                                    musicListColum.playingIndex=index
                                    BasicConfig.playingIndex = musicListColum.playingIndex
                                    BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_MusicPlayerIndex
                                    BasicConfig.currentMediaFilePath=filepath
                                    BasicConfig.currentMediaName=music_name
                                    BasicConfig.currentMediaArtistAuthor=music_artist
                                    BasicConfig.currentMediaCoverFilePath=musicElementRectangle.innerCurrentMediaoCover
                                }
                                else if(index !==musicListColum.playingIndex)
                                {
                                    //切换文件 , 切换之间未改变BasicConfig.isPlaying，一直为true无法触发全局响应，此处单独设置
                                    BasicConfig.resetAllPlayListHeadicon()
                                    localListIndexRectangPlayBtnImage.source = musicElementRectangle.clearPauseIconURL
                                    //检测到上一次选择非空，先触发清空上一则背景选择效果，再改变playingIndex
                                    musicListColum.clearAllMusicElementRectangleColorToTransparent(false)
                                    musicListColum.playingIndex=index
                                    BasicConfig.playingIndex = musicListColum.playingIndex
                                    BasicConfig.globalPlayingFocus = BasicConfig.globalPlayer_MusicPlayerIndex
                                    BasicConfig.currentMediaFilePath=filepath
                                    BasicConfig.currentMediaName=music_name
                                    BasicConfig.currentMediaArtistAuthor=music_artist
                                    BasicConfig.currentMediaCoverFilePath=musicElementRectangle.innerCurrentMediaoCover
                                }
                                Client.qmlClickedReqPreparePlayMusic(filepath)//发送到cpp
                            }
                            //修复无法正常取消Hover选中的问题
                            onReleased: {
                                    musicListColum.clearAllMusicElementRectangleColorToTransparent(true)
                                    BasicConfig.focusCurrentMusicInDisplayList()
                            }
                        }
                        //<---musicElementRectangle
                    }
                }
            }
        }
    }
}
