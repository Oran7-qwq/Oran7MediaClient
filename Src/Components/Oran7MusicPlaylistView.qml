import QtQuick 2.15
import QtQuick.Controls
import QtQuick.Dialogs
import Qt5Compat.GraphicalEffects
import "../Basic"
import Client 1.0

Item {
    id: root

    // ===== 外部属性配置 =====
    property string pageName: ""
    property int leftMargin: 40
    property int rightMargin: 0
    property bool showAddButton: true
    property bool showSearch: true
    property bool showMultiSelect: true
    property bool allowDragReorder: true
    property bool showDuration: true

    property bool isPlaying: false //bind  property
    // ===== 模型数据 =====
    property var titleModel: null
    property ListModel listModel: null
    property alias curPlayingIndex: playlistColumn.playingIndex
    property alias curSelectingIndex:playlistColumn.itemSeleted_index
    property alias curHoveredIndex: playlistColumn.itemHovered_index
    property bool isMultiSelected: false

    // ----------  展示列表的元素间隔和元素高度 ----------
    property int listColumSpacing: 4
    property int elementRectangleHeight: 50

    // ----------  定义Colum的各个element统一换算比例的宽度 ----------
    property int indexTopLabelWidth: 30
    property int titleTopLabelWidth: (parent.width ) * (0.5 - 0.02)  //微调
    property int albumTopLabelWidth: (parent.width) * 0.3
    property int timeSizeTopLabelWidth: (parent.width ) * (0.1 -0.02) //微调
    // ----------  定义Colum的各个element统一换算比例的Margin ----------
    property int indexTopLabelLeftMargin: 10
    property int titleTopLabelLeftMargin: 45
    property int albumTopLabelLeftMargin: root.titleTopLabelLeftMargin + root.titleTopLabelWidth
    property int timeSizeTopLabelLeftMargin: root.albumTopLabelLeftMargin + root.albumTopLabelWidth + 20

    //-----   Drag define  properties  ------
    property bool show_dragElement_floatTag: false
    property int dragSourceIndex: -1
    property int dragTargetIndex: -1
    property int hoveredGapIndex: -1

    property bool mouseInPlaylist: false

    // ===== 事件信号 =====
    signal itemClicked(int index)
    signal itemDoubleClicked(int index)
    signal itemAdded(var files)
    signal reorderCompleted(var newOrder)

    signal itemSelectAll()
    signal itemClearSelectAll()

    signal ask_dir_of_item_index(string file_path)
    signal reponse_dir_of_item_index(int index)

    signal focus_current_playlistItem()

    signal addNewItemOfFiles(var filesArray)

    // ===== 功能函数 =====

    //从指定索引开始触发所有 playlistItem 的 animateIn 动画
    function animateItemsFromIndex(startIndex) {
        playlistFlickable.animateRecentItems(startIndex)
    }

    // ===== 根容器 =====
    Item {
        id: container
        anchors.fill: parent
        anchors.leftMargin: root.leftMargin
        anchors.rightMargin: root.rightMargin
        clip: true

        // ===== 标题栏 =====
        Flow {
            id: titleFlow
            anchors.top: parent.top
            anchors.left: parent.left
            spacing: 16

            Repeater {
                id:titleRepeater
                model: root.titleModel ? root.titleModel : ["列表"]
                anchors.fill: parent
                property int focIndex: 0
                delegate: Label {
                    text: modelData
                    font.family: "微软雅黑"
                    font.bold: true
                    font.pixelSize: 24
                    color: index === titleRepeater.focIndex ? "#FF7381" : "#2a1a22"

                    Rectangle {
                        id: underline
                        width: parent.implicitWidth - 24
                        height: 3
                        radius: 1
                        anchors.top: parent.bottom
                        anchors.topMargin: 3
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: index === titleRepeater.focIndex ? "#ff3a3a" : "transparent"
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            titleRepeater.focIndex = index
                        }
                    }
                }
            }
        }

        // ===== 搜索框 =====
        Rectangle {
            id: searchRect
            anchors.right: multiSelectRect.left
            anchors.rightMargin: 10
            anchors.verticalCenter: titleFlow.verticalCenter
            anchors.top: parent.top

            property int initWidth: showSearch ? 60 : 0
            width: searchRect.initWidth
            height: 30
            color: "#fef2e8"
            radius: height / 2
            border.width: 1
            border.color: "#4d4d56"
            visible: showSearch

            TextField {
                id: searchField
                anchors.top: parent.top
                anchors.topMargin: 2
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 2
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.right: parent.right
                anchors.rightMargin: 8
                color: "cyan"
                background: Rectangle { color: "transparent" }

                onFocusChanged: {
                    if (focus) {
                        BasicConfig.newTextAreaFocused(searchField);
                    }
                }
            }
            Connections{
                target: BasicConfig
                function onClickedOutside()
                {
                    if(searchField.focus === true)
                        searchField.focus = false
                }
            }

            ParallelAnimation{
                id:searchRectParallelAnimation_maxsize
                PropertyAnimation{
                    target:searchRect
                    property: "width"
                    from: searchRect.initWidth
                    to:searchRect.initWidth*2
                    easing.type: Easing.OutCubic
                }
            }
            ParallelAnimation{
                id:searchRectParallelAnimation_minisize
                PropertyAnimation{
                    target: searchRect
                    property: "width"
                    from:searchRect.width
                    to:searchRect.initWidth
                    easing.type: Easing.OutCubic
                }
            }
            MouseArea{
                hoverEnabled: true
                anchors.fill: parent
                onEntered: {
                    searchRectParallelAnimation_maxsize.start()
                }
                onExited: {
                    searchRectParallelAnimation_minisize.start()
                    // searchRectangle.forceActiveFocus()
                }
                onClicked: {
                    searchField.forceActiveFocus()
                }
            }
        }

        // ===== 多选按钮 =====
        Rectangle {
            id: multiSelectRect
            width: 40
            height: width
            anchors.right: parent.right
            anchors.rightMargin: 30
            anchors.verticalCenter: searchRect.verticalCenter

            color: "transparent"
            visible: showMultiSelect

            Image {
                id: multiSelectIcon
                anchors.fill: parent
                source: "qrc:/image/selectmore.png"
                sourceSize.width: parent.width
                sourceSize.height: parent.height

                asynchronous:true
                cache: false
                mipmap: true

                property color colorOverlay: "#2a1a22"
                layer.enabled: true
                layer.effect: ColorOverlay {
                    source: multiSelectIcon
                    color: multiSelectIcon.colorOverlay
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        multiSelectIcon.colorOverlay = "#616161";
                        cursorShape = Qt.PointingHandCursor;
                    }
                    onExited: {
                        multiSelectIcon.colorOverlay = "#2a1a22";
                        cursorShape = Qt.ArrowCursor;
                    }
                    onClicked: {
                        root.isMultiSelected = !root.isMultiSelected;
                    }
                }
            }
        }

        // ===== 多选菜单 =====
        Rectangle {
            id: multiSelectMenu
            height: 28
            anchors.left: playlistFlickable.left
            anchors.right: playlistFlickable.right
            anchors.bottom: dive1.top
            anchors.bottomMargin: 7
            color: "transparent"
            visible: root.isMultiSelected

            property bool checked: false
            onCheckedChanged: {
                if(checked === true)
                    root.itemSelectAll()
                else
                    root.itemClearSelectAll()
            }

            Image {
                id: selectAllIcon
                sourceSize.width: 30
                sourceSize.height: 30
                anchors.left: parent.left
                anchors.leftMargin: 4
                anchors.top: parent.top
                anchors.topMargin: 2

                asynchronous: false
                smooth: true
                mipmap: true
                antialiasing: true

                property color colorOverlay: "#2a1a22"
                source: "qrc:/image/selectAll.png"
                layer.enabled: true
                layer.effect: ColorOverlay {
                    source: selectAllIcon
                    color: multiSelectMenu.checked ? "#ff3a3a" : "#2a1a22"
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        cursorShape = Qt.PointingHandCursor
                    }
                    onExited: {
                        cursorShape = Qt.ArrowCursor
                    }
                    onClicked: {
                        multiSelectMenu.checked = !multiSelectMenu.checked
                    }
                }
            }

            Text {
                id: selectAllText
                anchors.left: selectAllIcon.right
                anchors.leftMargin: 4
                anchors.verticalCenter: selectAllIcon.verticalCenter
                color: "#2a1a22"
                text: "全选"
                font.pixelSize: 18
                font.family: "微软雅黑"
                font.bold: false
            }
        }

        //-----------------------  分割线1 -----------------------//
        Rectangle{
            id:dive1
            width: parent.width-24
            height: 1
            color: "#2a1a22"
            anchors.left: parent.left
            anchors.top: listTopRow.bottom
            anchors.topMargin: 6
        }

        //-----------------------  Rectangle"#  , 标题 , 专辑 , '     ' ,  时长"  -----------------------//
        Rectangle{
            id:listTopRow
            anchors.top: titleFlow.bottom
            anchors.topMargin: 16
            anchors.left: parent.left
            anchors.right: parent.right
            height: 28

            color: "transparent"

            visible: root.isMultiSelected ? false : true

            //  #
            Label{
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
            // 标题
            Label{
                height: parent.height
                anchors.verticalCenter: parent.verticalCenter
                width: root.titleTopLabelWidth
                anchors.left: parent.left
                anchors.leftMargin: root.titleTopLabelLeftMargin
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
            // 专辑
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
            // "    "
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
            // 时长
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

        // ===== 拖拽提示线 =====  // 插入位置提示线组件 , 以every item 的Top为标准-->
        Rectangle {
            id: insertLine
            visible: false
            z: playlistColumn.z + 1
            y:dive1.y
            height: 4
            width: playlistColumn.width - 40
            radius: 2

            // 渐变效果：两侧淡色，中间原色
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#ffb3b3" }
                GradientStop { position: 0.45; color: "#ff7384" }
                GradientStop { position: 0.5; color: "#ff7384" }
                GradientStop { position: 0.8; color: "#ff9999" }
                GradientStop { position: 1.0; color: "#ffb3b3" }
            }
            opacity: 1
            Behavior on opacity{
                NumberAnimation{
                    duration: 100
                    easing.type:Easing.OutQuad
                }
            }

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
        //<----The dragElementFloatTag Ui
        Rectangle{
            id:dragElementFloatTag
            x:root.x + 100
            y: 0
            z: playlistFlickable.z + 1
            clip: true
            width: 300
            height: 48
            radius: 4
            color: "#fef2e8"
            opacity: 1
            visible: root.show_dragElement_floatTag
            layer.enabled: root.show_dragElement_floatTag
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

        // ---- 拖动选中位置时，未在当前Flickable页码，自定义拖动至边缘触发--->代码控制滑动效果的Timer
        Timer{
            id:flickTimer
            repeat: true
            running: false
            interval: 16 //60fps
            property real flickYValue: 0
            onTriggered: {
                playlistFlickable.flick(0,flickTimer.flickYValue)
            }
        }
        //  ----------  drag of algorithm function ----------

        //计算每个项的位置和高度
        function calculateItemRect(index)
        {
            var itemY=index * (root.listColumSpacing + root.elementRectangleHeight) + playlistFlickable.contentY
            return {
                y: itemY,
                top: itemY,
                center: itemY + root.elementRectangleHeight/2,
                bottom: itemY + root.elementRectangleHeight
            }
        }

        // 查找鼠标所在的位置是哪个项之间
        function findHoveredGap(mouseY)
        {
            var count = root.listModel.count
            if (count === 0) return 0

            var relativeY = mouseY - playlistFlickable.y + playlistFlickable.contentY

            // 计算每个"槽位"（项+间距）的高度
            var slotHeight = root.elementRectangleHeight + root.listColumSpacing

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
                    return root.listModel.count  // 在最后一项之后
                else
                    return count
            }
            else
            {
                // 在槽位内部，根据位置决定插入点
                if (offsetInSlot <= root.elementRectangleHeight / 2)// 在上半部分
                    return slotIndex
                else // 在下半部分
                    return slotIndex + 1
            }
        }

        //填充渲染insertLine <<----主要接口
        function renderInsertLine(mouseY)
        {
            //判断是否越界 , 越上界--->向下滑动 ; 越下界--->向上滑动 ；；；深度边界检测在Flickable中的onContentYChanged:
            //console.log("contentY:",musicListFlickables.contentY)
            //console.log("maxContentY:",musicListFlickables.maxContentY - 1.0)
            if(mouseY - playlistFlickable.y<= 0 && !playlistFlickable.contentY_atTop)
            {
                flickTimer.flickYValue = (root.elementRectangleHeight + root.listColumSpacing) * 10
                flickTimer.restart()
            }
            else if(mouseY - playlistFlickable.y > playlistFlickable.height && !playlistFlickable.contentY_atBottom)
            {
                flickTimer.flickYValue = -(root.elementRectangleHeight + root.listColumSpacing )* 10
                flickTimer.restart()
            }
            else flickTimer.stop()

            root.dragTargetIndex = findHoveredGap(mouseY)
            //console.log("dragTargetIndex:", root.dragTargetIndex)

            var lineY
            if (root.dragTargetIndex === 0) {
                // 在第一项之前
                lineY = playlistFlickable.y
            } else if (root.dragTargetIndex === root.count) {
                // 在最后一项之后
                var lastItemRect = calculateItemRect(root.dragTargetIndex - 1)
                lineY = playlistFlickable.y + (lastItemRect.bottom - playlistFlickable.contentY * 2)
            } else {
                // 在两项之间
                var prevItemRect = calculateItemRect(root.dragTargetIndex - 1)
                lineY = playlistFlickable.y + (prevItemRect.bottom - playlistFlickable.contentY * 2) //注意：*2
            }
            insertLine.visible = false//reset
            insertLine.opacity = 0

            if(lineY <= dive1.y)return;

            insertLine.insertLineYChangeAnimation_fromY = insertLine.y
            insertLine.insertLineYChangeAnimation_toY =lineY
            insertLineYChangeAnimation.restart()

            insertLine.x = playlistFlickable.x + 10

            insertLine.visible = true
            insertLine.opacity = 1

            //console.log("insertLine.y:", lineY, "contentY:", musicListFlickables.contentY)
        }

        //重排序reOrder <<---主要接口
        function reOrder()
        {
            var flickable_orignContentY = playlistFlickable.contentY
            var listCount=root.listModel.count
            // 使用 JSON 创建深拷贝
            var items = []
            for (var i = 0; i < listCount; i++) {
                var original = root.listModel.get(i)
                var copy = JSON.parse(JSON.stringify(original))
                items.push(copy)
            }

            // for(i=0 ;i<listCount;i++)
            // {
            //     items.push(root.listModel.get(i ))
            // }

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

            root.listModel.clear()
            for(i =0 ;i<listCount ;i ++)
            {
                root.listModel.clear()
                for(i = 0; i < listCount; i++) {
                root.listModel.append({
                      icon: items[i].icon,
                      music_name: items[i].music_name,
                      music_artist: items[i].music_artist,
                      music_album: items[i].music_album,
                      timesize: items[i].timesize,
                      filepath: items[i].filepath
                    })
                }
            }
            playlistFlickable.animateRecentItems(0)//started by index of "0"
            //reset back
            playlistFlickable.contentY=flickable_orignContentY
        }
        // function--->用 splice 实现元素移动
        function moveItem(array, fromIndex, toIndex) {
            if (fromIndex === toIndex) return array;
            // 删除原位置的元素
            var element = array[fromIndex];
            array.splice(fromIndex, 1);
            // 插入到新位置
            array.splice(toIndex, 0, element);
            return array;
        }



        //-----------------------滑动列表右下的手动添加导入本地文件"+"-----------------------//
        Rectangle{
            id:addNewFileRectangle
            property int initWidth: 50
            width: addNewFileRectangle.initWidth
            height: addNewFileRectangle.width
            z:1
            anchors.right: parent.right
            anchors.rightMargin: addNewFileRectangle.initWidth
            anchors.bottom: parent.bottom
            anchors.bottomMargin: addNewFileRectangle.initWidth
            color: "white"
            radius: addNewFileRectangle.initWidth/2

            visible: root.isMultiSelected ? false : true

            layer.enabled: true
            layer.effect: DropShadow{
                id:addNewFileRectangleDropShadow
                anchors.fill: addNewFileRectangle
                z:addNewFileRectangle.z
                source: addNewFileRectangle
                color: "#80000000"
                radius: 15
                samples: 30
                horizontalOffset: 5
                verticalOffset: 5
            }

            Rectangle{
                width: addNewFileRectangle.initWidth*0.5
                height: 4
                radius: addNewFileRectangle.initWidth/2
                color: "gray"
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Rectangle{
                width: 4
                height: addNewFileRectangle.initWidth*0.5
                radius: addNewFileRectangle.initWidth/2
                color: "gray"
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }
            ParallelAnimation{
                id:addLocalMusicRectangleParallelAnimation_minisize
                PropertyAnimation{
                    target: addNewFileRectangle
                    property: "width"
                    from: addNewFileRectangle.initWidth
                    to:addNewFileRectangle.initWidth*0.9
                    duration: 80
                    easing.type: Easing.InQuad
                }
            }
            ParallelAnimation{
                id:addLocalMusicRectangleParallelAnimation_maxsize
                PropertyAnimation{
                    target: addNewFileRectangle
                    property: "width"
                    from: addNewFileRectangle.width
                    to:addNewFileRectangle.initWidth
                    duration: 80
                    easing.type: Easing.InQuad
                }
            }

            // <<<<<<功能组件，打开文件对话框>>>>>>//
            Oran7FileDialog{
                id:addNewFileRectangle_FileDialog
                selectReset: true
                fileMode:FileDialog.OpenFiles
                onReady: {
                    root.addNewItemOfFiles(filesArray)
                }
            }

            MouseArea {
                id: addMusicMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onPressed: {
                    addLocalMusicRectangleParallelAnimation_minisize.start()
                }
                onReleased: {
                    addLocalMusicRectangleParallelAnimation_maxsize.start()
                }
                onClicked: {
                    addNewFileRectangle_FileDialog.open()
                }
            }
        }

        // ======== main of playlistFlickable ========
        Flickable {
            id: playlistFlickable
            contentHeight: root.listModel ? root.listModel.count * (root.elementRectangleHeight + root.listColumSpacing) +
                                            (root.elementRectangleHeight + root.listColumSpacing)  : 0
            anchors.top: dive1.bottom
            anchors.topMargin: 2
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true

            ScrollBar.vertical: ScrollBar {
                anchors.right: parent.right
                anchors.rightMargin: 0
                width: 10
            }

            // 对外接口--->触发指定元素的动画
            function animateItem(index) {
                var item = playListRepeater.itemAt(index)
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
                for (var i = startIndex; i < root.listModel.count; i++) {
                    playlistFlickable.animateItem(i )
                }
            }

            // 监听 contentY 变化
            property bool contentY_atTop: true
            property bool contentY_atBottom: false
            onContentYChanged: {
                var maxContentY = contentHeight - height
                var tolerance = 1.0
                playlistFlickable.contentY_atBottom = contentY >= (maxContentY - tolerance)
                playlistFlickable.contentY_atTop = contentY <= 0

                if(playlistFlickable.contentY_atBottom || playlistFlickable.contentY_atTop)
                {
                    flickTimer.stop()
                }
            }

            HoverHandler {
                id: playlistHoverHandler
                acceptedDevices: PointerDevice.Mouse
                // HoverHandler 不会拦截事件，也不会被子元素影响
                onHoveredChanged:{
                    root.mouseInPlaylist = playlistHoverHandler.hovered
                    if (!playlistHoverHandler.hovered) {
                        playlistColumn.itemHovered_index = -1
                    }
                }
            }

            Column {
                id: playlistColumn
                anchors.fill: parent
                anchors.topMargin: root.listColumSpacing
                spacing: root.listColumSpacing

                property int playingIndex: -1 // '-1' of no item isPlaying ,what is paused statue
                property int itemSeleted_index: -1
                property int itemHovered_index: -1

                // 定义统一的列宽
                property int indexWidth: 30
                property int titleWidth: root.titleTopLabelWidth
                property int albumWidth: root.albumTopLabelWidth
                property int timeWidth: root.timeSizeTopLabelWidth

                // 定义统一的列位置
                property int indexLeftMargin: 10
                property int titleLeftMargin: 45
                property int albumLeftMargin: titleLeftMargin + titleWidth + 10
                property int timeLeftMargin: albumLeftMargin + albumWidth + 20

                function opacityAnimate_fromIndex(index){

                }

                Repeater {
                    id:playListRepeater
                    model: root.listModel
                    delegate: Rectangle {
                        id: playlistItem

                        height: root.elementRectangleHeight
                        width: playlistColumn.width - 20
                        radius: 10
                        color: root.isMultiSelected ? "transparent" :
                                                      playlistItem.itemIsSelected ? "#fef2e8" :
                                                             playlistItem.itemIsHovered && root.mouseInPlaylist ? "#BEBEBE" : "transparent"

                        Behavior on color{
                            PropertyAnimation{
                                duration: 1000
                                easing.type:Easing.OutCubic
                            }
                        }

                        property int itemIndex: model.index
                        property string itemIcon: model.icon
                        property string itemTitle: model.music_name
                        property string itemArtist: model.music_artist
                        property string itemAlbum: model.music_album
                        property string itemDuration: model.timesize
                        property string itemFilepath: model.filepath

                        property bool itemIsSelected: model.index  === playlistColumn.itemSeleted_index //bind
                        property bool itemIsHovered: model.index === playlistColumn.itemHovered_index //bind
                        property bool itemIsPlaying: model.index === playlistColumn.playingIndex //bind

                        // --- item opacity Animation ---
                        property bool animationEnabled: true// 控制动画的状态
                        property bool isAnimating: false
                        opacity: /*musicElementRectangle.animationEnabled === true ? 0 : */ 1// 初始透明度
                        function animateIn()// --->触发opacity Animation动画对外接口
                        {
                            if (playlistItem.animationEnabled && !playlistItem.isAnimating)
                            {
                                playlistItem.isAnimating = true
                                fadeInAnimation.start()
                            }
                        }
                        SequentialAnimation {// 动画行为
                            id: fadeInAnimation
                            NumberAnimation
                            {
                                target: playlistItem
                                property: "opacity"
                                from: 0
                                to: 1
                                duration: 300
                                easing.type: Easing.OutCubic
                            }
                            ScriptAction {
                                script: playlistItem.isAnimating = false
                            }
                        }
                        //-----------------------------  Connections -----------------------------//
                        Connections{
                            target: root
                            function onAsk_dir_of_item_index(file_path){
                                if(playlistItem.itemFilepath === file_path){
                                    root.reponse_dir_of_item_index(playlistItem.itemIndex)
                                }
                            }
                        }

                        //----------------------------------  Ui  ----------------------------------//

                        // --- 多选复选框 ---
                        Oran7ESelectRect{
                            id:checkBox
                            width: 20
                            anchors.left: parent.left
                            anchors.leftMargin: playlistColumn.indexLeftMargin
                            anchors.verticalCenter: parent.verticalCenter
                            z: 2
                            labelTextEnable: false
                            checkedColor: "cyan"
                            border.color: "cyan"
                            visible: root.isMultiSelected

                            checked: false
                            onClicked: {
                                checked = !checked
                                //console.log("clicked")
                            }
                            Connections{
                                target: root
                                function onItemSelectAll(){
                                    checkBox.checked = true
                                }
                                function onItemClearSelectAll(){
                                    checkBox.checked = false
                                }
                            }
                        }

                        // 序号/播放图标
                        Rectangle {
                            id: indexRect
                            width: playlistColumn.indexWidth
                            height: width
                            anchors.left: parent.left
                            anchors.leftMargin: playlistColumn.indexLeftMargin
                            anchors.verticalCenter: parent.verticalCenter
                            color: "transparent"

                            Label {
                                id: indexLabel
                                visible: !root.isMultiSelected && (!playlistItem.itemIsHovered || !root.mouseInPlaylist) && !playlistItem.itemIsSelected
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 5
                                text: model.index < 9 ? "0" + (model.index + 1) : (model.index + 1)
                                font.family: "微软雅黑"
                                font.pixelSize: 14
                                color: "#2a1a22"
                            }

                            Image {
                                id: playIcon
                                visible: !root.isMultiSelected && playlistItem.itemIsHovered && root.mouseInPlaylist
                                source: playlistItem.itemIsPlaying ? "qrc:/image/ClearPause.png" : "qrc:/image/ClearPlay.png"
                                scale: 0.7
                                anchors.fill: parent
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 5
                                z:playlistItem.z + 1

                                layer.enabled: true
                                layer.effect: ColorOverlay {
                                    source: playIcon
                                    color: "#FF8F6E"
                                }
                            }
                            Oran7WaveBarChart{
                                id:waveBarChart
                                anchors.top: playIcon.top
                                anchors.topMargin: 0
                                anchors.horizontalCenter: playIcon.horizontalCenter

                                visible: !playlistItem.itemIsHovered && playlistItem.itemIsSelected

                                animationEnabled:playlistItem.itemIsPlaying
                            }

                            MouseArea{
                                anchors.fill: parent
                                onClicked: {
                                    root.itemClicked(model.index)
                                    //console.log("clicked")
                                }
                            }
                        }
                        // 专辑封面
                        Rectangle {
                            id: coverRect
                            width: 36
                            height:width
                            anchors.left: parent.left
                            anchors.leftMargin: 50
                            anchors.verticalCenter: parent.verticalCenter
                            radius: 7
                            visible: true

                            OpacityMask {
                                anchors.fill: coverRect
                                source: Image {
                                    source: playlistItem.itemIcon
                                    asynchronous: true
                                    cache: true
                                    mipmap: true
                                    antialiasing: true
                                }
                                maskSource: coverRect
                            }
                        }

                        // 标题和艺术家
                        Column {
                            anchors.left: coverRect.right
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter

                            Label {
                                id: titleLabel
                                text: playlistItem.itemTitle
                                color: "#2a1a22"
                                font.pixelSize: 15
                                font.family: "微软雅黑"
                                width: parent.width - 100
                            }

                            Rectangle {
                                id: typeRect
                                width: 44
                                height: 16
                                color: "transparent"
                                border.color: "#d3a03b"
                                border.width: 0.8
                                radius: 2

                                Label {
                                    text: "超清母带"
                                    color: "#d3a03b"
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    font.family: "微软雅黑"
                                    font.pixelSize: 10
                                    Label {
                                        id: artistLabel
                                        text: playlistItem.itemArtist
                                        anchors.left: parent.right
                                        anchors.leftMargin: 4
                                        anchors.bottom: parent.bottom
                                        width:playlistColumn.titleWidth - typeRect.width- coverRect.width - 7
                                        clip:true
                                        font.family: "微软雅黑"
                                        font.pixelSize: 12
                                        color: "#2a1a22"
                                    }
                                }
                            }
                        }

                        // 专辑
                        Label {
                            id: albumLabel
                            anchors.left: parent.left
                            anchors.leftMargin: playlistColumn.albumLeftMargin
                            anchors.verticalCenter: parent.verticalCenter
                            width: 200
                            text: itemAlbum
                            font.family: "微软雅黑"
                            font.pixelSize: 14
                            color: "#2a1a22"
                        }

                        // 时长
                        Label {
                            id: durationLabel
                            anchors.left: parent.left
                            anchors.leftMargin: playlistColumn.timeLeftMargin + 4
                            anchors.verticalCenter: parent.verticalCenter
                            width: 50
                            text: formatDuration(itemDuration)
                            font.family: "微软雅黑"
                            font.pixelSize: 13
                            color: "#2a1a22"
                            visible: !root.isMultiSelected
                        }

                        // 拖拽图标
                        Image {
                            id: dragIcon
                            anchors.left: parent.left
                            anchors.leftMargin: playlistColumn.timeLeftMargin + 4
                            anchors.verticalCenter: parent.verticalCenter
                            sourceSize.width: 40
                            sourceSize.height: 40
                            source: "qrc:/image/drag.png"
                            visible: root.isMultiSelected && root.allowDragReorder

                            property bool item_isDraging: false

                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                source: dragIcon
                                color: dragIcon.item_isDraging ? "#ff7384" : "#2a1a22"
                            }

                            MouseArea {
                                anchors.fill: parent
                                property bool hasMoved: false
                                onPositionChanged: (mouse) => {
                                    var posInParent = mapToItem(root, mouse.x, mouse.y)

                                    // console.log("父控件坐标:", posInParent.x, posInParent.y)
                                    // console.log("全局坐标:", mapToGlobal(mouse.x, mouse.y))
                                    // //相对坐标
                                    // var relativeX = mouse.x/* + dragImage.x*/
                                    // var relativeY = mouse.y/* + dragImage.y*/
                                    // console.log("相对坐标:", relativeX, relativeY)

                                    //initilization dragElementFloatTag_ParallelAnimation-->Value
                                    dragElementFloatTag.dragElementFloatTag_YChangeAmination_fromY=dragElementFloatTag.y
                                    dragElementFloatTag.dragElementFloatTag_YChangeAmination_toY = posInParent.y - dragElementFloatTag.height/2
                                    //dragElementFloatTag_ParallelAnimation --->restart
                                    dragElementFloatTag_ParallelAnimation.restart()

                                    //渲染填充GopLine
                                    container.renderInsertLine(dragElementFloatTag.y)
                                    hasMoved=true
                                }
                                onPressed: (mouse)=>{
                                    dragElementFloatTag.musicTagIconImageSource = icon
                                    dragElementFloatTag.musicTagName = music_name
                                    dragElementFloatTag.musicTagArtist = music_artist

                                    playlistFlickable.interactive = false
                                    root.show_dragElement_floatTag = true

                                    var posInParent = mapToItem(root, mouse.x, mouse.y)
                                    dragElementFloatTag.y = posInParent.y - dragElementFloatTag.height/2
                                    insertLine.y = dragElementFloatTag.y
                                    //initilization dragElementFloatTag_OpacityChangeAnimation---> Value
                                    dragElementFloatTag.dragElementFloatTag_OpacityChangeAnimation_fromValue = 0
                                    dragElementFloatTag.dragElementFloatTag_OpacityChangeAnimation_toValue = 1
                                    //dragElementFloatTag_OpacityChangeAnimation--->restart
                                    dragElementFloatTag_OpacityChangeAnimation.restart()

                                    dragIcon.item_isDraging=true
                                    root.dragSourceIndex=playlistItem.itemIndex
                                }
                                onReleased: {
                                    playlistFlickable.interactive = true
                                    root.show_dragElement_floatTag = false
                                    dragIcon.item_isDraging  = false

                                    insertLine.visible = false
                                    //insertLine.y=dive1.y

                                    //makesure flickTimer stop
                                    flickTimer.stop()

                                    //reOrder
                                    if( !(root.dragTargetIndex === root.dragSourceIndex || root.dragTargetIndex === root.dragSourceIndex + 1) && hasMoved)
                                    {
                                        container.reOrder()
                                        hasMoved=false
                                    }
                                }
                            }
                        }
                        // 鼠标交互
                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: !root.isMultiSelected
                            propagateComposedEvents: true

                            // onClicked: {
                            //     if (!root.isMultiSelected) {
                            //         root.itemClicked(model.index);
                            //     }
                            // }

                            // onPositionChanged: (mouse) =>{
                            //     var posInParent = mapToItem(root, mouse.x, mouse.y)

                            //     // console.log("父控件坐标:", posInParent.x, posInParent.y)
                            //     // console.log("全局坐标:", mapToGlobal(mouse.x, mouse.y))
                            //     // //相对坐标
                            //     // var relativeX = mouse.x/* + dragImage.x*/
                            //     // var relativeY = mouse.y/* + dragImage.y*/
                            //     // console.log("相对坐标:", relativeX, relativeY)
                            // }

                            onDoubleClicked: {
                                if (!root.isMultiSelected) {
                                    root.itemDoubleClicked(model.index);
                                }
                            }

                            onEntered: {
                                if (!root.isMultiSelected) {
                                    playlistColumn.itemHovered_index = playlistItem.itemIndex
                                }
                            }
                        }

                        function formatDuration(seconds) {
                            var total = parseInt(seconds);
                            var minutes = Math.floor(total / 60);
                            var secs = total % 60;
                            return Math.floor(minutes / 10) + (minutes % 10) + ":" +
                                   Math.floor(secs / 10) + (secs % 10);
                        }

                    }
                }
            }
        }
    }
}
