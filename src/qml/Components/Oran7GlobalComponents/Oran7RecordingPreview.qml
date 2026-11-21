import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 预览容器只负责布局/反馈，视频仍由原 Loader 内的生产渲染组件消费。
Rectangle {
    id: root
    required property var controller
    required property var pageState
    property Component previewComponent
    property Component sourceOptionsComponent
    property Item sourceDock
    property string sourceName: ""
    property string sourceDetails: ""
    property bool showInfo: false
    property bool fillPreview: false
    property size outputSize: Qt.size(0, 0)
    property size sourceSizeHint: Qt.size(0, 0)
    property bool violetSkin: false
    property real materialStrength: 0.025
    property color paperColor: "#d5dfec"
    property color paperInkColor: "#263447"
    property color titleSurfaceColor: "#161e29"
    property color panelColor: "#1e252f"
    property color canvasColor: "#0f141c"
    property color textColor: "#e7edf5"
    property color mutedTextColor: "#a3afbf"
    readonly property color standbyColor: violetSkin ? "#a6dce6f8" : canvasColor
    property color accentColor: "#80a9db"
    property color recordingColor: "#e7a399"
    property color streamingColor: "#6dcbb7"
    property color borderColor: "#334154"
    property color errorColor: "#ed8795"
    property string fontFamily: ""
    property int controlPixelSize: 15
    property int headingPixelSize: 17
    property int captionPixelSize: 13
    property int sourceOptionsHeight: 44
    property var initialMonitor: ({})
    property var layerOrder: [0]
    property var layerDetails: ({})
    property Item activeEditor: null
    readonly property int selectedLayerId: activeEditor ? activeEditor.layerId : -1
    readonly property var selectedSource: layerDetails[selectedLayerId] || null
    readonly property bool sceneSupported: !!controller && typeof controller.addSceneLayer === "function"
    readonly property bool managedScenes: sceneSupported && typeof controller.loadSceneLibrary === "function"
        && typeof controller.restoreSceneSource === "function" && controller.sceneLibraryEnabled !== false
    property var scenes: []
    property string currentSceneId: ""
    property string sceneMessage: ""
    readonly property int sceneIndex: scenes.findIndex(scene => scene.id === currentSceneId)
    readonly property string currentSceneName: sceneIndex >= 0 ? scenes[sceneIndex].name : qsTranslate("Oran7Ui", "默认场景")
    property bool libraryWritable: false
    property bool loadingScene: false
    property string savedLibrary: ""
    readonly property bool windowSceneSupported: sceneSupported && typeof controller.sceneWindows === "function"
        && typeof controller.addWindowSceneLayer === "function"
    readonly property var sceneRows: layerOrder.slice().reverse().filter(id => !managedScenes || id !== 0).map(id => id === 0
        ? {id, name: sourceName, removed: !!viewportEditor && viewportEditor.layerRemoved, error: ""} : layerDetails[id])
    signal infoToggled(bool checked)
    signal scaleToggled(bool checked)
    readonly property bool recording: controller && controller.recordingState === "Recording"
    readonly property bool streaming: !!controller && !!controller.liveSession && controller.liveSession.state === "Streaming"
    readonly property size canvasResolution: viewportEditor.encodingSize
    readonly property bool unavailable: controller && controller.previewState !== undefined && controller.previewState !== "Running"
    color: root.violetSkin ? "transparent" : root.canvasColor
    radius: root.violetSkin ? 0 : 9
    border.color: root.recording ? Qt.rgba(recordingColor.r, recordingColor.g, recordingColor.b, 0.65) : root.borderColor
    clip: true
    Behavior on border.color { ColorAnimation { duration: 160 } }
    onVisibleChanged: if (!visible) clearSelection()
    onLayerOrderChanged: Qt.callLater(root.publishScene)
    Component.onCompleted: if (managedScenes) Qt.callLater(root.initializeSceneLibrary)
    onManagedScenesChanged: if (managedScenes && !currentSceneId) Qt.callLater(root.initializeSceneLibrary)

    ListModel { id: auxiliaryLayers }
    Timer { id: sceneSave; interval: 400; onTriggered: root.flushSceneLibrary() }

    function defaultTransform() {
        return {fit: !fillPreview, geometry: [0.5, 0.5, 1, 1], crop: [0, 0, 1, 1], scale: 1,
            mirrored: false, quarterTurns: 0}
    }

    function displayRecipe(monitor) {
        return {kind: "display", name: monitor.name, deviceName: monitor.deviceName, monitorPath: monitor.monitorPath,
            width: monitor.width, height: monitor.height, transform: defaultTransform()}
    }

    function initializeSceneLibrary() {
        if (currentSceneId || !managedScenes)
            return
        const result = controller.loadSceneLibrary()
        libraryWritable = result.ok === true
        const library = result.library
        if (library && library.scenes && library.scenes.length) {
            scenes = library.scenes
            currentSceneId = library.active
            savedLibrary = JSON.stringify({active: currentSceneId, scenes})
        } else {
            currentSceneId = "default"
            const monitor = initialMonitor && initialMonitor.attached ? initialMonitor : pageState.monitors.find(item => item.attached)
            const initial = monitor ? [displayRecipe(monitor)] : []
            scenes = [{id: currentSceneId, name: qsTranslate("Oran7Ui", "默认场景"), sources: initial}]
            if (!libraryWritable)
                sceneMessage = qsTranslate("Oran7Ui", result.message || "场景文件不可写")
        }
        restoreActiveScene()
        flushSceneLibrary()
    }

    function currentSources() {
        return layerOrder.filter(id => id !== 0).map(id => sourceRecipe(id))
    }

    function sourceRecipe(id) {
        const recipe = layerDetails[id].recipe
        const editor = editorFor(id)
        const layer = editor ? editor.composition() : null
        if (!layer)
            return recipe
        const size = editor.sourceSize
        const geometry = layer.geometry
        const crop = layer.crop
        return Object.assign({}, recipe, {width: size.width || recipe.width, height: size.height || recipe.height,
            transform: {fit: layer.fit, scale: layer.scale, mirrored: layer.mirrored, quarterTurns: layer.quarterTurns,
                geometry: [geometry.x, geometry.y, geometry.width, geometry.height],
                crop: [crop.x, crop.y, crop.width, crop.height]}})
    }

    function snapshotActiveScene() {
        if (sceneIndex < 0 || loadingScene)
            return
        const sources = currentSources()
        if (JSON.stringify(sources) !== JSON.stringify(scenes[sceneIndex].sources))
            scenes = scenes.map(scene => scene.id === currentSceneId ? Object.assign({}, scene, {sources}) : scene)
    }

    function flushSceneLibrary() {
        if (!managedScenes || sceneIndex < 0 || loadingScene)
            return
        sceneSave.stop()
        snapshotActiveScene()
        const library = {active: currentSceneId, scenes}
        const bytes = JSON.stringify(library)
        if (!libraryWritable || bytes === savedLibrary)
            return
        const result = controller.saveSceneLibrary(library)
        if (result.ok) {
            savedLibrary = bytes
            sceneMessage = ""
        } else {
            sceneMessage = qsTranslate("Oran7Ui", result.message)
        }
    }

    function restoreActiveScene() {
        loadingScene = true
        clearSelection()
        const previous = layerOrder.filter(id => id !== 0)
        layerOrder = [0]
        auxiliaryLayers.clear()
        layerDetails = ({})
        viewportEditor.removeLayer()
        for (const source of scenes[sceneIndex].sources) {
            const result = controller.restoreSceneSource(source, "")
            if (!addSceneEditor(result.id, source, source, result.error, true, result)) {
                libraryWritable = false
                sceneMessage = qsTranslate("Oran7Ui", "无法恢复场景中的源")
            }
        }
        loadingScene = false
        publishScene()
        for (const id of previous)
            controller.removeSceneLayer(id)
    }

    function activateScene(id) {
        if (id === currentSceneId || !scenes.some(scene => scene.id === id))
            return
        snapshotActiveScene()
        currentSceneId = id
        restoreActiveScene()
        flushSceneLibrary()
    }

    function createScene(name) {
        const id = Date.now().toString(36) + "-" + Math.random().toString(36).slice(2)
        snapshotActiveScene()
        scenes = scenes.concat({id, name, sources: []})
        activateScene(id)
    }

    function moveScene(direction) {
        const index = sceneIndex
        const next = index + direction
        if (index < 0 || next < 0 || next >= scenes.length)
            return
        const order = scenes.slice()
        const scene = order[index]
        order[index] = order[next]
        order[next] = scene
        scenes = order
        flushSceneLibrary()
    }

    function renameScene(name) {
        if (sceneIndex >= 0 && name.trim()) {
            scenes = scenes.map(scene => scene.id === currentSceneId ? Object.assign({}, scene, {name: name.trim()}) : scene)
            flushSceneLibrary()
        }
    }

    function deleteScene() {
        if (scenes.length < 2)
            return
        scenes = scenes.filter(scene => scene.id !== currentSceneId)
        currentSceneId = scenes[0].id
        restoreActiveScene()
        flushSceneLibrary()
    }

    function editorFor(id) {
        if (id === 0)
            return viewportEditor
        for (let i = 0; i < auxiliaryEditors.count; ++i) {
            const editor = auxiliaryEditors.itemAt(i)
            if (editor && editor.layerId === id)
                return editor
        }
        return null
    }

    function addSceneSource(monitor, replacing) {
        if (!sceneSupported || !monitor || !monitor.attached)
            return false
        if (managedScenes) {
            const recipe = displayRecipe(monitor)
            const result = controller.restoreSceneSource(recipe, "")
            if (replacing > 0)
                return replaceSceneEditor(replacing, recipe, result)
            return addSceneEditor(result.id, monitor, recipe, result.error, false)
        }
        return addSceneEditor(controller.addSceneLayer(monitor.outputIndex), monitor)
    }

    function addWindowSource(windowInfo, replacing) {
        if (!windowSceneSupported || !windowInfo)
            return false
        if (managedScenes) {
            const recipe = {kind: "window", name: windowInfo.name, windowClass: windowInfo.windowClass,
                executable: windowInfo.executable, width: windowInfo.width, height: windowInfo.height, transform: defaultTransform()}
            const result = controller.restoreSceneSource(recipe, windowInfo.token)
            if (replacing > 0)
                return replaceSceneEditor(replacing, recipe, result)
            return addSceneEditor(result.id, windowInfo, recipe, result.error, false)
        }
        return addSceneEditor(controller.addWindowSceneLayer(windowInfo.token), windowInfo)
    }

    function addImageSource(file, replacing) {
        const result = controller.openSceneImage(file)
        const recipe = {kind: "image", name: result.name, file: result.file, width: result.width,
            height: result.height, transform: defaultTransform()}
        if (replacing > 0)
            return replaceSceneEditor(replacing, recipe, result)
        return addSceneEditor(result.id, recipe, recipe, result.error, false, result)
    }

    function replaceSceneEditor(previous, recipe, result) {
        if (!result.id)
            return false
        const index = layerOrder.indexOf(previous)
        if (index < 1 || result.error || layerDetails[previous].recipe.kind !== recipe.kind) {
            controller.removeSceneLayer(result.id)
            return false
        }
        const saved = sourceRecipe(previous)
        recipe = Object.assign(recipe, {transform: saved.transform, hidden: !!saved.hidden, locked: !!saved.locked})
        const order = layerOrder.slice()
        addSceneEditor(result.id, recipe, recipe, "", true, result)
        order[index] = result.id
        layerOrder = order
        discardSceneEditor(previous)
        selectSceneLayer(result.id)
        publishScene()
        controller.removeSceneLayer(previous)
        return true
    }

    function addSceneEditor(id, source, recipe, error, restore, runtime) {
        if (!id)
            return false
        const details = Object.assign({}, layerDetails)
        details[id] = {id, name: source.name, error: error || "", removed: false, recipe, imageUrl: runtime?.url || ""}
        layerDetails = details
        auxiliaryLayers.append({sceneId: id, sourceWidth: runtime?.width || source.width, sourceHeight: runtime?.height || source.height})
        layerOrder = layerOrder.concat(id)
        if (!restore) Qt.callLater(() => {
            const editor = editorFor(id)
            if (editor) {
                editor.placeNewLayer()
                editor.selectLayer()
                publishScene()
            }
        })
        return true
    }

    function publishScene() {
        if (!controller || loadingScene)
            return
        const layers = []
        for (const id of layerOrder) {
            const editor = editorFor(id)
            const layer = editor ? editor.composition() : null
            if (layer)
                layers.push(layer)
        }
        if (typeof controller.setSceneLayers === "function")
            controller.setSceneLayers(layers)
        else if (layers.length === 1 && typeof controller.setPreviewComposition === "function") {
            const layer = layers[0]
            controller.setPreviewComposition(true, layer.fit, layer.geometry, layer.scale, layer.crop,
                                             layer.quarterTurns, layer.mirrored, layer.removed)
        }
        if (managedScenes && sceneIndex >= 0)
            sceneSave.restart()
    }

    function toggleSourceFlag(id, flag) {
        const source = layerDetails[id]
        if (!source || !source.recipe)
            return
        const details = Object.assign({}, layerDetails)
        const recipe = Object.assign({}, source.recipe)
        recipe[flag] = !recipe[flag]
        details[id] = Object.assign({}, source, {recipe})
        layerDetails = details
        publishScene()
    }

    function clearSelection() {
        const previous = activeEditor
        activeEditor = null
        if (typeof previous?.clearSelection === "function")
            previous.clearSelection()
    }

    function updateSelection(editor) {
        if (editor.selected) {
            const previous = activeEditor
            activeEditor = editor
            if (previous !== editor && typeof previous?.clearSelection === "function")
                previous.clearSelection()
        } else if (activeEditor === editor) {
            activeEditor = null
        }
    }

    function selectSceneLayer(id) {
        const editor = editorFor(id)
        if (!editor)
            return
        if (editor.layerRemoved)
            editor.restoreLayer()
        else
            editor.selectLayer()
    }

    function layerAt(x, y) {
        const direction = activeEditor?.showSelectionGuides && typeof activeEditor.hitDirection === "function"
            ? activeEditor.hitDirection(x, y) : null
        if (direction && (direction.x || direction.y))
            return activeEditor
        for (let i = layerOrder.length - 1; i >= 0; --i) {
            const id = layerOrder[i]
            const editor = editorFor(id)
            const info = layerDetails[id]
            if (editor?.canEdit && !editor.sourceHidden && !editor.layerRemoved && !info?.error
                && typeof editor.hitDirection === "function" && editor.hitDirection(x, y))
                return editor
        }
        return null
    }

    function canMoveLayer(front) {
        if (!activeEditor)
            return false
        const index = layerOrder.indexOf(activeEditor.layerId)
        const step = front ? 1 : -1
        for (let i = index + step; i >= 0 && i < layerOrder.length; i += step) {
            const editor = editorFor(layerOrder[i])
            if (editor && !editor.layerRemoved)
                return true
        }
        return false
    }

    function moveLayer(front) {
        if (!canMoveLayer(front))
            return
        const id = activeEditor.layerId
        const order = layerOrder.filter(item => item !== id)
        if (managedScenes && !front)
            order.splice(1, 0, id)
        layerOrder = front ? order.concat(id) : (managedScenes ? order : [id].concat(order))
        publishScene()
    }

    function stepLayer(front) {
        if (!canMoveLayer(front))
            return
        const order = layerOrder.slice()
        const index = order.indexOf(selectedLayerId)
        const next = index + (front ? 1 : -1)
        order[index] = order[next]
        order[next] = selectedLayerId
        layerOrder = order
        publishScene()
    }

    function removeActiveLayer() {
        const id = selectedLayerId
        if (id === 0) {
            viewportEditor.removeLayer()
            return
        }
        if (id < 0)
            return
        clearSelection()
        layerOrder = layerOrder.filter(item => item !== id)
        discardSceneEditor(id)
        publishScene()
        controller.removeSceneLayer(id)
    }

    function discardSceneEditor(id) {
        for (let i = 0; i < auxiliaryLayers.count; ++i)
            if (auxiliaryLayers.get(i).sceneId === id) {
                auxiliaryLayers.remove(i)
                break
            }
        const details = Object.assign({}, layerDetails)
        delete details[id]
        layerDetails = details
    }

    Connections {
        target: root.controller
        ignoreUnknownSignals: true
        function onSceneLayerError(id, message) {
            if (!root.layerDetails[id])
                return
            const details = Object.assign({}, root.layerDetails)
            details[id] = Object.assign({}, details[id], {error: message})
            root.layerDetails = details
        }
    }

    // 在窗口层被动观察按下，即使导航或其它面板接收点击，也能结束选中。
    Item {
        id: selectionObserver
        parent: root.Window.window ? root.Window.window.contentItem : null
        anchors.fill: parent
        z: 10000
        visible: root.visible && !!root.activeEditor

        function insidePreviewClip(position) {
            for (let item = root; item && item !== parent; item = item.parent) {
                if (item.clip && !item.contains(mapToItem(item, position.x, position.y)))
                    return false
            }
            return true
        }

        PointHandler {
            enabled: selectionObserver.visible
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onActiveChanged: {
                if (!active)
                    return
                const position = point.position
                if (sourceOptions.item?.interacting)
                    return
                for (const item of [root.sourceDock, sourceOptions, selectionTools]) {
                    if (item && item.visible && item.contains(selectionObserver.mapToItem(item, position.x, position.y)))
                        return
                }
                if (!selectionObserver.insidePreviewClip(position)) {
                    root.clearSelection()
                    return
                }
                const previewPoint = selectionObserver.mapToItem(canvas, position.x, position.y)
                if (!canvas.contains(previewPoint) || !root.layerAt(previewPoint.x, previewPoint.y))
                    root.clearSelection()
            }
        }
    }

    function selectScale(fill) {
        const editor = activeEditor
        if (editor) {
            editor.setCropMode(false)
            editor.resetGeometry()
            if (editor === viewportEditor) {
                if (fill !== fillPreview)
                    scaleToggled(fill)
            } else {
                editor.fillPreview = fill
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0
        Rectangle {
            objectName: "recordingPreviewHeader"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(64, headerRow.implicitHeight + 14)
            Layout.minimumWidth: 0
            color: root.panelColor
            radius: root.violetSkin ? 0 : 11
            bottomLeftRadius: root.violetSkin ? 0 : radius
            bottomRightRadius: bottomLeftRadius
            Rectangle { visible: !root.violetSkin; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 12; color: root.panelColor }
            Oran7StudioMaterial { anchors.fill: parent; anchors.margins: 2; grainOpacity: root.materialStrength }
            RowLayout {
                id: headerRow
                anchors.fill: parent
                anchors.leftMargin: 12; anchors.rightMargin: 12
                spacing: 10
                Rectangle {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    Layout.preferredHeight: Math.max(46, previewDetails.implicitHeight + 10)
                    radius: 5; color: root.titleSurfaceColor
                    border.color: Qt.rgba(root.borderColor.r,root.borderColor.g,root.borderColor.b,.55)
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 10
                        spacing: 9
                        Oran7RecordingIcon {
                            Layout.preferredWidth: 22; Layout.preferredHeight: 22
                            glyph: "monitor"; ink: root.accentColor
                        }
                        Rectangle { width: 1; Layout.preferredHeight: 24; color: Qt.rgba(root.borderColor.r,root.borderColor.g,root.borderColor.b,.8) }
                        ColumnLayout {
                            id: previewDetails
                            Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 2
                            Label {
                                Layout.fillWidth: true
                                text: "02 / " + (root.managedScenes ? root.currentSceneName
                                    : root.sourceName || qsTranslate("Oran7Ui", "画面预览"))
                                color: root.textColor; font.family: root.fontFamily; font.pixelSize: root.headingPixelSize; font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: root.managedScenes ? qsTranslate("Oran7Ui", "画面预览")
                                    : root.sourceDetails || qsTranslate("Oran7Ui", "画面预览")
                                color: root.mutedTextColor
                                font.family: root.fontFamily; font.pixelSize: root.captionPixelSize; elide: Text.ElideRight
                            }
                        }
                    }
                }
                Oran7PreviewAction {
                    objectName: "recordingPreviewHeaderInfo"
                    compact: true
                    glyph: "info"; text: qsTranslate("Oran7Ui", "画面信息")
                    checked: root.showInfo
                    onToggled: root.infoToggled(checked)
                }
                Rectangle {
                    objectName: "recordingPreviewBadge"
                    Layout.preferredWidth: badgeRow.implicitWidth + 16
                    Layout.minimumWidth: badgeRow.implicitWidth + 16
                    Layout.maximumWidth: badgeRow.implicitWidth + 16
                    Layout.preferredHeight: Math.max(28, badgeRow.implicitHeight + 8)
                    radius: 12
                    color: root.recording ? Qt.rgba(root.recordingColor.r, root.recordingColor.g, root.recordingColor.b, 0.13) : "#0cffffff"
                    Row {
                        id: badgeRow
                        anchors.centerIn: parent; spacing: 6
                        Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 6; height: 6; radius: 3; color: root.recording ? root.recordingColor : root.streaming ? root.streamingColor : root.unavailable ? root.mutedTextColor : root.accentColor }
                        Label {
                            visible: root.width >= 420 || root.recording || root.streaming
                            text: root.recording && root.streaming ? "REC + LIVE" : root.recording ? qsTranslate("Oran7Ui", "正在录制") : root.streaming ? qsTranslate("Oran7Ui", "推流中") : qsTranslate("Oran7Ui", "画面预览")
                            color: root.recording ? root.recordingColor : root.streaming ? root.streamingColor : root.mutedTextColor
                            font.family: root.fontFamily; font.pixelSize: root.captionPixelSize
                        }
                    }
                }
            }
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: root.borderColor; opacity: 0.5 }
        Rectangle {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredHeight: root.sourceOptionsHeight
            visible: root.managedScenes && !!root.sourceOptionsComponent
            color: root.titleSurfaceColor
            Label {
                anchors.fill: parent
                leftPadding: 12; rightPadding: 12
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                visible: !sourceOptions.visible
                text: root.sourceDetails
                color: root.mutedTextColor
                font.family: root.fontFamily; font.pixelSize: root.captionPixelSize
            }
            Loader {
                id: sourceOptions
                objectName: "recordingSourceOptions"
                anchors.fill: parent
                visible: active
                active: root.managedScenes && !!root.selectedSource && !!root.sourceOptionsComponent
                sourceComponent: root.sourceOptionsComponent
            }
        }
        Item {
            id: canvas
            objectName: "recordingPreviewCanvas"
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0
            Rectangle {
                id: sceneBackground
                anchors.fill: parent
                visible: !root.unavailable
                color: root.canvasColor
            }
            Oran7SceneLayer {
                id: viewportEditor
                fillPreview: root.fillPreview
                outputSize: root.outputSize
                sourceSizeHint: root.sourceSizeHint
                sourceComponent: root.previewComponent
            }
            Repeater {
                id: auxiliaryEditors
                model: auxiliaryLayers
                delegate: Oran7SceneLayer {
                    id: editor
                    required property int sceneId
                    required property int sourceWidth
                    required property int sourceHeight
                    readonly property var sourceInfo: root.layerDetails[sceneId]
                    objectName: "recordingSceneLayer_" + sceneId
                    layerId: sceneId
                    sourceSizeHint: Qt.size(sourceWidth, sourceHeight)
                    outputSize: viewportEditor.encodingSize
                    sceneRect: viewportEditor.outputRect
                    sourceSelectionAllowed: root.managedScenes
                    sourceHidden: sourceInfo?.recipe?.hidden === true
                    sourceLocked: sourceInfo?.recipe?.locked === true
                    Component.onCompleted: {
                        const recipe = root.layerDetails[sceneId].recipe
                        if (recipe)
                            editor.restoreComposition(recipe.transform)
                    }
                    sourceComponent: Item {
                        id: layerHost
                        property var video: null
                        property size previewSourceSize: editor.sourceSizeHint
                        readonly property bool imageSource: editor.sourceInfo?.recipe?.kind === "image"
                        opacity: editor.sourceInfo?.error ? 0 : 1
                        readonly property real previewSourceAspect: previewSourceSize.height > 0
                            ? previewSourceSize.width / previewSourceSize.height : 0
                        Component.onCompleted: if (!imageSource) video = root.controller.attachSceneLayer(editor.layerId, layerHost)
                        Image {
                            objectName: "recordingSceneImage_" + editor.layerId
                            anchors.fill: parent
                            visible: layerHost.imageSource && status === Image.Ready
                            source: layerHost.imageSource ? editor.sourceInfo?.imageUrl || "" : ""
                            cache: false
                            autoTransform: true
                        }
                        Connections {
                            target: layerHost.video
                            ignoreUnknownSignals: true
                            function onSourceSizeChanged(w, h) { layerHost.previewSourceSize = Qt.size(w, h) }
                        }
                    }
                }
            }
            Item { id: sceneOverlay; anchors.fill: parent; clip: true; z: 100 }
            // 所有图层共享一个输入坐标系。选中框可在上方绘制，视频仍按场景层级合成。
            MouseArea {
                id: scenePointer
                anchors.fill: parent
                z: 101
                enabled: !root.unavailable
                hoverEnabled: enabled
                preventStealing: true
                property Item draggingLayer: null
                readonly property Item hoveredLayer: draggingLayer || root.layerAt(mouseX, mouseY)
                cursorShape: hoveredLayer ? hoveredLayer.cursorFor(hoveredLayer.hitDirection(mouseX, mouseY)) : Qt.ArrowCursor
                onPressed: mouse => {
                    const editor = root.layerAt(mouse.x, mouse.y)
                    if (editor && editor.beginGesture(mouse.x, mouse.y))
                        draggingLayer = editor
                    else
                        mouse.accepted = false
                }
                onPositionChanged: mouse => {
                    if (draggingLayer)
                        draggingLayer.updateGesture(mouse.x, mouse.y)
                }
                onReleased: mouse => {
                    if (draggingLayer)
                        draggingLayer.finishGesture(mouse.x, mouse.y)
                    draggingLayer = null
                }
                onCanceled: {
                    if (draggingLayer)
                        draggingLayer.cancelGesture()
                    draggingLayer = null
                }
                onDoubleClicked: mouse => {
                    const editor = root.layerAt(mouse.x, mouse.y)
                    if (editor && !editor.sourceLocked)
                        editor.resetGeometry()
                }
            }
            Oran7PreviewAction {
                objectName: "recordingRestoreLayer"
                z: 102
                anchors.centerIn: parent
                visible: !root.managedScenes && viewportEditor.layerRemoved && root.layerOrder.length === 1 && !root.unavailable
                glyph: "reset"
                text: qsTranslate("Oran7Ui", "恢复画面")
                checkable: false
                onClicked: viewportEditor.restoreLayer()
            }
            // 状态遮罩不销毁 Loader，确保首次挂载与自动恢复可以继续推进。
            Rectangle {
                objectName: "recordingPreviewStandby"
                anchors.fill: parent
                z: 103
                color: root.standbyColor
                visible: root.unavailable
                // 紫色皮肤的待机遮罩保留磨砂背景，开启采集后由预览画布遮住。
                Item {
                    anchors.fill: parent
                    clip: true
                    visible: !root.violetSkin
                    Oran7StudioMaterial { anchors.fill: parent; grainOpacity: root.materialStrength }
                    Oran7StudioMotif {
                        width: Math.min(280, parent.width * 0.55, parent.height * 0.75)
                        height: width; anchors.centerIn: parent
                        visible: !root.violetSkin
                        ink: root.mutedTextColor; opacity: 0.055
                    }
                }
                Column {
                    anchors.centerIn: parent
                    width: Math.max(0, parent.width - 32)
                    spacing: 12
                    Oran7RecordingIcon { anchors.horizontalCenter: parent.horizontalCenter; width: 32; height: 32; ink: root.mutedTextColor }
                    Label {
                        width: parent.width
                        text: root.pageState.previewHint || (root.controller && root.controller.previewError) || qsTranslate("Oran7Ui", "预览尚未启动")
                        horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                        color: root.mutedTextColor; font.family: root.fontFamily; font.pixelSize: root.controlPixelSize
                    }
                }
            }
        }
    }

    Rectangle {
        id: selectionTools
        objectName: "recordingSelectionTools"
        z: 2
        visible: !!root.activeEditor && root.activeEditor.showSelectionGuides
        width: selectionRow.implicitWidth + 12
        height: selectionRow.implicitHeight + 10
        radius: 8
        readonly property rect selectionRect: root.activeEditor ? root.activeEditor.viewportRect : viewportEditor.viewportRect
        readonly property real stageLeft: canvas.parent.x + canvas.x
        readonly property real stageTop: canvas.parent.y + canvas.y
        readonly property real layerLeft: stageLeft + selectionRect.x
        readonly property real layerTop: stageTop + selectionRect.y
        readonly property real clearance: viewportEditor.handleMargin + 3
        readonly property real centeredX: viewportEditor.clamp(layerLeft + (selectionRect.width - width) / 2,
            4, root.width - width - 4)
        readonly property real aboveEdgeY: layerTop - height - (root.managedScenes ? clearance : 8)
        readonly property real floatingY: viewportEditor.clamp(aboveEdgeY,
            root.managedScenes ? stageTop + 4 : 4, root.height - height - 4)
        // 工具栏优先在边框上方或两侧，避让空间用尽后放在手柄下方。
        readonly property bool overlapsTop: root.managedScenes && selectionRect.y >= -clearance
            && floatingY < layerTop + clearance && floatingY > aboveEdgeY
            && centeredX + width > layerLeft - clearance && centeredX < layerLeft + selectionRect.width + clearance
        readonly property real leftLimit: layerLeft - clearance - width
        readonly property real rightLimit: layerLeft + selectionRect.width + clearance
        readonly property real sideX: {
            if (leftLimit >= 4 && (rightLimit + width > root.width - 4 || centeredX - leftLimit <= rightLimit - centeredX))
                return leftLimit
            return rightLimit + width <= root.width - 4 ? rightLimit : -1
        }
        x: overlapsTop && sideX >= 0 ? sideX : centeredX
        y: overlapsTop && sideX < 0 ? Math.max(stageTop + 4, layerTop + clearance) : floatingY
        color: Qt.rgba(root.panelColor.r, root.panelColor.g, root.panelColor.b, 0.98)
        border.color: root.borderColor

        MouseArea { anchors.fill: parent }
        Row {
            id: selectionRow
            anchors.centerIn: parent
            spacing: 3
            Repeater {
                model: [
                    { glyph: "crop", label: qsTranslate("Oran7Ui", "裁剪"), trigger: () => root.activeEditor.setCropMode(!root.activeEditor.cropMode) },
                    { glyph: "rotateLeft", label: qsTranslate("Oran7Ui", "逆时针旋转"), trigger: () => root.activeEditor.rotateLeft() },
                    { glyph: "mirror", label: qsTranslate("Oran7Ui", "镜像翻转"), trigger: () => root.activeEditor.flipHorizontally() },
                    { glyph: "fit", label: qsTranslate("Oran7Ui", "适配"), trigger: () => root.selectScale(false) },
                    { glyph: "fill", label: qsTranslate("Oran7Ui", "填充"), trigger: () => root.selectScale(true) },
                    { glyph: "toFront", label: qsTranslate("Oran7Ui", "置顶"), separator: true, trigger: () => root.moveLayer(true) },
                    { glyph: "toBack", label: qsTranslate("Oran7Ui", "置底"), trigger: () => root.moveLayer(false) },
                    { glyph: "reset", label: qsTranslate("Oran7Ui", "重置"), separator: true,
                      trigger: () => { root.selectScale(false); root.activeEditor.resetLayer() } },
                    { glyph: "remove", label: qsTranslate("Oran7Ui", "删除"), trigger: () => root.removeActiveLayer() }
                ]
                delegate: Item {
                    required property var modelData
                    width: action.width + (modelData.separator ? 7 : 0)
                    height: action.height
                    HoverHandler { id: unavailableAction; enabled: !action.enabled; blocking: false }
                    ToolTip.visible: unavailableAction.hovered
                    ToolTip.delay: 400
                    ToolTip.text: modelData.glyph === "toFront" ? qsTranslate("Oran7Ui", "已在最上层") : qsTranslate("Oran7Ui", "已在最下层")
                    Rectangle {
                        visible: parent.modelData.separator === true
                        width: 1; height: 24; x: 2
                        anchors.verticalCenter: parent.verticalCenter
                        color: root.borderColor
                    }
                    Oran7PreviewAction {
                        id: action
                        anchors.right: parent.right
                        objectName: "recordingSelection_" + parent.modelData.glyph
                        glyph: parent.modelData.glyph
                        text: parent.modelData.label
                        compact: true
                        width: Math.min(46, Math.max(20, Math.floor((root.width - (root.managedScenes ? 78 : 58)) / 9)))
                        height: 42
                        iconSize: Math.min(22, width - 6)
                        checkable: false
                        enabled: !!root.activeEditor && (glyph === "toFront" ? root.canMoveLayer(true)
                            : glyph === "toBack" ? root.canMoveLayer(false) : true)
                        highlighted: !!root.activeEditor && (glyph === "crop" && root.activeEditor.cropMode
                            || glyph === "mirror" && root.activeEditor.mirrored
                            || glyph === "fit" && !root.activeEditor.fillPreview || glyph === "fill" && root.activeEditor.fillPreview)
                        onClicked: if (root.activeEditor) parent.modelData.trigger()
                    }
                }
            }
        }
    }

    component Oran7SceneLayer: Oran7RecordingViewport {
        id: editor
        anchors.fill: parent
        z: root.layerOrder.indexOf(layerId) + 1
        canvasBackground: sceneBackground
        overlayHost: sceneOverlay
        previewAvailable: !root.unavailable
        selectionIsCurrent: root.activeEditor === editor
        fontFamily: root.fontFamily
        guidePixelSize: root.captionPixelSize
        selectionColor: root.violetSkin ? "#b8a7f7" : root.accentColor
        handleColor: root.violetSkin ? "#f4efff" : "#eef4ff"
        handleBorderColor: root.violetSkin ? "#514887" : "#3459a9"
        onSelectedChanged: root.updateSelection(editor)
        onCompositionChanged: Qt.callLater(root.publishScene)
        Component.onDestruction: if (root.activeEditor === editor) root.activeEditor = null
    }

    // 两路状态独立，切换时不改变舞台或工具区的尺寸。
    Row {
        visible: !root.violetSkin || root.recording || root.streaming
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.bottomMargin: 3
        spacing: 4; height: 3
        Rectangle {
            width: (parent.width - 4) / 2; height: 3; radius: 1
            color: root.recording ? root.recordingColor : "#233b454d"
            Behavior on color { ColorAnimation { duration: 240 } }
        }
        Rectangle {
            width: (parent.width - 4) / 2; height: 3; radius: 1
            color: root.streaming ? root.streamingColor : "#233b454d"
            Behavior on color { ColorAnimation { duration: 240 } }
        }
    }

    component Oran7PreviewAction: Button {
        id: action
        property string glyph
        property bool compact: false
        property int iconSize: 18
        checkable: true
        implicitWidth: compact ? 36 : row.implicitWidth + 16
        implicitHeight: Math.max(34, root.captionPixelSize + 16)
        hoverEnabled: true
        ToolTip.visible: compact && hovered
        ToolTip.text: text
        ToolTip.delay: 400
        contentItem: Row {
            id: row
            spacing: 5
            Item {
                anchors.verticalCenter: parent.verticalCenter
                width: action.compact ? row.width : action.iconSize
                height: action.iconSize
                Oran7RecordingIcon {
                    anchors.centerIn: parent
                    width: action.iconSize; height: width; glyph: action.glyph
                    ink: action.checked || action.highlighted ? root.accentColor
                        : action.enabled ? root.textColor : Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.40)
                }
            }
            Label { visible: !action.compact; text: action.text; anchors.verticalCenter: parent.verticalCenter; color: action.checked ? root.accentColor : root.textColor; font.family: root.fontFamily; font.pixelSize: root.captionPixelSize }
        }
        background: Oran7StudioControlSurface {
            polished: root.violetSkin; tintColor: root.accentColor; highlighted: action.checked || action.highlighted || action.hovered
            radius: 6
            color: action.checked || action.highlighted || (root.violetSkin && action.hovered)
                ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.13) : action.hovered ? "#12ffffff" : "transparent"
            border.color: action.activeFocus ? root.accentColor : "transparent"
            Behavior on color { ColorAnimation { duration: 100 } }
        }
    }
}
