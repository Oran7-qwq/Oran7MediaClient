import QtQuick

Item {
    id: root
    objectName: "recordingViewportEditor"
    property Component sourceComponent
    property int layerId: 0
    property Item canvasBackground
    property Item overlayHost
    property rect sceneRect: Qt.rect(0, 0, 0, 0)
    property bool previewAvailable: true
    property bool sourceSelectionAllowed: false
    property bool selectionIsCurrent: true
    property bool sourceHidden: false
    property bool sourceLocked: false
    property bool fillPreview: false
    property size outputSize: Qt.size(0, 0)
    property size sourceSizeHint: Qt.size(0, 0)
    property string fontFamily: ""
    property int guidePixelSize: 13
    property color selectionColor: "#b8a7f7"
    property color handleColor: "#f4efff"
    property color handleBorderColor: "#514887"
    readonly property size sourceSize: preview.item ? (preview.item.previewSourceSize ?? sourceSizeHint) : sourceSizeHint
    readonly property size encodingSize: outputSize.width > 0 && outputSize.height > 0 ? outputSize
        : Qt.size(2 * Math.floor(sourceSize.width / 2), 2 * Math.floor(sourceSize.height / 2))
    readonly property rect outputRect: {
        if (sceneRect.width > 0 && sceneRect.height > 0)
            return sceneRect
        if (encodingSize.width <= 0 || encodingSize.height <= 0)
            return Qt.rect(0, 0, width, height)
        const margin = Math.min(22, width * 0.06, height * 0.06)
        const ratio = encodingSize.width / encodingSize.height
        const w = Math.max(0, Math.min(width - 2 * margin, (height - 2 * margin) * ratio))
        const h = w / ratio
        return Qt.rect((width - w) / 2, (height - h) / 2, w, h)
    }
    readonly property real originalAspect: preview.item ? (preview.item.previewSourceAspect ?? 0) : 0
    readonly property bool swapsAxes: (editState.quarterTurns & 1) !== 0
    readonly property real sourceAspect: originalAspect > 0
        ? (swapsAxes ? 1 / originalAspect : originalAspect) * editState.crop.width / editState.crop.height
        : originalAspect
    readonly property bool lockAspect: !fillPreview && sourceAspect > 0
    readonly property bool canEdit: previewAvailable && preview.status === Loader.Ready
        && (fillPreview || lockAspect)
    readonly property bool selected: (canEdit || sourceSelectionAllowed) && editState.selected && !editState.removed
    readonly property bool showSelectionGuides: selectionIsCurrent && selected && canEdit && !sourceHidden && !sourceLocked
    readonly property bool layerRemoved: editState.removed
    readonly property bool cropMode: editState.cropMode
    readonly property bool mirrored: editState.mirrored
    readonly property real minimumWidth: Math.max(0, Math.min(112, defaultRect().width))
    readonly property real minimumHeight: Math.max(0, Math.min(72, defaultRect().height))
    readonly property real snapDistance: 6
    readonly property real handleMargin: 9
    readonly property rect viewportRect: editState.customized ? resolveRect(editState.normalizedRect) : defaultRect()
    readonly property bool hasCrop: viewportRect.x < outputRect.x - 0.01 || viewportRect.y < outputRect.y - 0.01
        || viewportRect.x + viewportRect.width > outputRect.x + outputRect.width + 0.01
        || viewportRect.y + viewportRect.height > outputRect.y + outputRect.height + 0.01
    readonly property int leftGapPixels: gapPixels(viewportRect.x - outputRect.x,
        outputRect.width, encodingSize.width, false)
    readonly property int rightGapPixels: gapPixels(viewportRect.x + viewportRect.width - outputRect.x,
        outputRect.width, encodingSize.width, true)
    readonly property int topGapPixels: gapPixels(viewportRect.y - outputRect.y,
        outputRect.height, encodingSize.height, false)
    readonly property int bottomGapPixels: gapPixels(viewportRect.y + viewportRect.height - outputRect.y,
        outputRect.height, encodingSize.height, true)
    signal compositionChanged()

    QtObject {
        id: editState
        property bool selected: false
        property bool customized: false
        property rect normalizedRect
        property var gesture: null
        property rect crop: Qt.rect(0, 0, 1, 1)
        property bool cropMode: false
        property int quarterTurns: 0
        property bool mirrored: false
        property bool removed: false
    }

    onCanEditChanged: if (!canEdit && !sourceSelectionAllowed) clearSelection()
    onSourceHiddenChanged: { cancelGesture(); scheduleComposition() }
    onSourceLockedChanged: if (sourceLocked) { cancelGesture(); setCropMode(false) }
    onFillPreviewChanged: { resetGeometry(); scheduleComposition() }
    onViewportRectChanged: { if (overflow.visible) overflow.requestPaint(); scheduleComposition() }
    onOutputRectChanged: { editState.gesture = null; if (overflow.visible) overflow.requestPaint(); scheduleComposition() }
    Component.onCompleted: scheduleComposition()
    onOriginalAspectChanged: editState.gesture = null
    onWidthChanged: editState.gesture = null
    onHeightChanged: editState.gesture = null

    function defaultRect() {
        const surface = outputRect
        if (!lockAspect)
            return surface
        const w = Math.min(surface.width, surface.height * sourceAspect)
        const h = w / sourceAspect
        return Qt.rect(surface.x + (surface.width - w) / 2, surface.y + (surface.height - h) / 2, w, h)
    }

    function scheduleComposition() {
        Qt.callLater(root.compositionChanged)
    }

    function composition() {
        const bounds = outputRect
        const layer = viewportRect
        const base = defaultRect()
        if (bounds.width > 0 && bounds.height > 0 && base.width > 0) {
            const geometry = Qt.rect(
                (layer.x + layer.width / 2 - bounds.x) / bounds.width,
                (layer.y + layer.height / 2 - bounds.y) / bounds.height,
                layer.width / bounds.width, layer.height / bounds.height)
            return {id: layerId, fit: !fillPreview, geometry, scale: layer.width / base.width,
                crop: editState.crop, quarterTurns: editState.quarterTurns, mirrored: editState.mirrored,
                removed: editState.removed || root.sourceHidden}
        }
        return null
    }

    function clamp(value, low, high) {
        return Math.max(low, Math.min(value, high))
    }

    function gapPixels(edge, extent, pixels, trailing) {
        if (extent <= 0)
            return 0
        // 与 NV12 构图共用偶数像素边界的口径。
        const boundary = clamp(2 * Math.round(edge / extent * pixels / 2), 0, pixels)
        return trailing ? pixels - boundary : boundary
    }

    function resolveRect(normalized) {
        const b = outputRect
        let w = Math.min(normalized.width * b.width, b.width * 4)
        let h = Math.min(normalized.height * b.height, b.height * 4)
        if (lockAspect) {
            w = Math.min(normalized.width, 4) * defaultRect().width
            h = w / sourceAspect
        }
        return Qt.rect(b.x + normalized.x * b.width - w / 2, b.y + normalized.y * b.height - h / 2, w, h)
    }

    function snap(value, first, second) {
        const edge = Math.abs(value - first) <= Math.abs(value - second) ? first : second
        return Math.abs(value - edge) <= snapDistance ? edge : value
    }

    function hitDirection(x, y) {
        const r = viewportRect
        const left = x - r.x
        const right = r.x + r.width - x
        const top = y - r.y
        const bottom = r.y + r.height - y
        if (showSelectionGuides && Math.min(left, right, top, bottom) >= -handleMargin) {
            const horizontal = Math.min(Math.abs(left), Math.abs(right)) > handleMargin ? 0
                : Math.abs(left) <= Math.abs(right) ? -1 : 1
            const vertical = Math.min(Math.abs(top), Math.abs(bottom)) > handleMargin ? 0
                : Math.abs(top) <= Math.abs(bottom) ? -1 : 1
            if (horizontal || vertical)
                return Qt.point(horizontal, vertical)
        }
        return Math.min(left, right, top, bottom) >= 0 ? Qt.point(0, 0) : null
    }

    function cursorFor(direction) {
        if (!direction)
            return Qt.ArrowCursor
        if (direction.x && direction.y)
            return direction.x === direction.y ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
        if (direction.x)
            return Qt.SizeHorCursor
        return direction.y ? Qt.SizeVerCursor : Qt.SizeAllCursor
    }

    function clearSelection() {
        editState.selected = false
        editState.cropMode = false
        editState.gesture = null
    }

    function selectLayer() {
        if ((canEdit || sourceSelectionAllowed) && !layerRemoved)
            editState.selected = true
    }

    function placeNewLayer() {
        const base = defaultRect()
        if (base.width <= 0 || base.height <= 0)
            return
        const w = base.width * 0.56
        const h = base.height * 0.56
        const b = outputRect
        saveGeometry(b.x + (b.width - w) * 0.75, b.y + (b.height - h) * 0.68, w, h)
    }

    function resetGeometry() {
        editState.customized = false
        editState.gesture = null
    }

    function restoreComposition(saved) {
        const geometry = saved.geometry
        const crop = saved.crop
        editState.gesture = null
        fillPreview = !saved.fit
        editState.crop = Qt.rect(crop[0], crop[1], crop[2], crop[3])
        editState.quarterTurns = saved.quarterTurns
        editState.mirrored = saved.mirrored
        editState.normalizedRect = Qt.rect(geometry[0], geometry[1], saved.fit ? saved.scale : geometry[2], geometry[3])
        editState.customized = true
        scheduleComposition()
    }

    function setCropMode(active) {
        editState.cropMode = active
        editState.gesture = null
    }

    function rotateLeft() {
        setCropMode(false)
        const c = editState.crop
        editState.quarterTurns = (editState.quarterTurns + (editState.mirrored ? 1 : 3)) % 4
        editState.crop = Qt.rect(c.y, 1 - c.x - c.width, c.height, c.width)
        scheduleComposition()
    }

    function flipHorizontally() {
        setCropMode(false)
        const c = editState.crop
        editState.crop = Qt.rect(1 - c.x - c.width, c.y, c.width, c.height)
        editState.mirrored = !editState.mirrored
        scheduleComposition()
    }

    function resetLayer() {
        setCropMode(false)
        editState.quarterTurns = 0
        editState.mirrored = false
        editState.crop = Qt.rect(0, 0, 1, 1)
        editState.removed = false
        resetGeometry()
        scheduleComposition()
    }

    function removeLayer() {
        editState.removed = true
        clearSelection()
        scheduleComposition()
    }

    function restoreLayer() {
        editState.removed = false
        editState.selected = true
        scheduleComposition()
    }

    function saveGeometry(left, top, w, h) {
        const b = outputRect
        editState.normalizedRect = Qt.rect((left + w / 2 - b.x) / b.width, (top + h / 2 - b.y) / b.height,
                                          w / (lockAspect ? defaultRect().width : b.width), h / b.height)
        editState.customized = true
    }

    function cropGesture(gesture, dx, dy) {
        const r = gesture.bounds
        const c = gesture.crop
        const fullW = r.width / c.width
        const fullH = r.height / c.height
        const fullLeft = r.x - c.x * fullW
        const fullTop = r.y - c.y * fullH
        const minW = Math.min(r.width, Math.max(16, fullW * 0.01))
        const minH = Math.min(r.height, Math.max(16, fullH * 0.01))
        let left = r.x
        let top = r.y
        let right = r.x + r.width
        let bottom = r.y + r.height
        if (gesture.direction.x < 0) left = clamp(left + dx, fullLeft, right - minW)
        if (gesture.direction.x > 0) right = clamp(right + dx, left + minW, fullLeft + fullW)
        if (gesture.direction.y < 0) top = clamp(top + dy, fullTop, bottom - minH)
        if (gesture.direction.y > 0) bottom = clamp(bottom + dy, top + minH, fullTop + fullH)
        editState.crop = Qt.rect((left - fullLeft) / fullW, (top - fullTop) / fullH,
                                (right - left) / fullW, (bottom - top) / fullH)
        saveGeometry(left, top, right - left, bottom - top)
        scheduleComposition()
    }

    function resizeFitted(r, direction, dx, dy) {
        const horizontal = direction.x
        const vertical = direction.y
        const anchorX = r.x + (1 - horizontal) * r.width / 2
        const anchorY = r.y + (1 - vertical) * r.height / 2
        const b = outputRect
        const availableWidth = horizontal < 0 ? anchorX - b.x : horizontal > 0 ? b.x + b.width - anchorX
            : 2 * Math.min(anchorX - b.x, b.x + b.width - anchorX)
        const availableHeight = vertical < 0 ? anchorY - b.y : vertical > 0 ? b.y + b.height - anchorY
            : 2 * Math.min(anchorY - b.y, b.y + b.height - anchorY)
        const aspect = sourceAspect
        const limit = defaultRect().width * 4
        const minimum = Math.min(r.width, Math.max(minimumWidth, minimumHeight * aspect))
        // 边手柄沿自身轴跟随鼠标，角手柄将鼠标位移投影到保持比例的对角线上。
        const change = horizontal && vertical
            ? (horizontal * dx + vertical * dy / aspect) / (1 + 1 / (aspect * aspect))
            : horizontal ? horizontal * dx : vertical * dy * aspect
        let w = clamp(r.width + change, minimum, limit)
        const nearWidth = snap(w, availableWidth, availableHeight * aspect)
        if (nearWidth >= minimum && nearWidth <= limit)
            w = nearWidth
        const h = w / aspect
        return Qt.rect(anchorX - (1 - horizontal) * w / 2, anchorY - (1 - vertical) * h / 2, w, h)
    }

    function updateGesture(x, y) {
        const gesture = editState.gesture
        if (!gesture || width <= 0 || height <= 0)
            return
        const r = gesture.bounds
        const b = outputRect
        if (b.width <= 0 || b.height <= 0)
            return
        const dx = x - gesture.startX
        const dy = y - gesture.startY
        const horizontal = gesture.direction.x
        const vertical = gesture.direction.y
        if (cropMode) {
            if ((horizontal || vertical) && (dx || dy || gesture.moved)) {
                gesture.moved = true
                cropGesture(gesture, dx, dy)
            }
            return
        }
        let left = r.x
        let top = r.y
        let right = r.x + r.width
        let bottom = r.y + r.height
        if (!horizontal && !vertical) {
            if (dx) left = snap(left + dx, b.x, b.x + b.width - r.width)
            if (dy) top = snap(top + dy, b.y, b.y + b.height - r.height)
            right = left + r.width
            bottom = top + r.height
        } else if (lockAspect && (dx || dy)) {
            const fitted = resizeFitted(r, gesture.direction, dx, dy)
            left = fitted.x
            top = fitted.y
            right = fitted.x + fitted.width
            bottom = fitted.y + fitted.height
        } else if (!lockAspect) {
            const minW = Math.min(r.width, minimumWidth)
            const minH = Math.min(r.height, minimumHeight)
            if (horizontal < 0 && dx) left = clamp(snap(left + dx, b.x, b.x + b.width), right - b.width * 4, right - minW)
            if (horizontal > 0 && dx) right = clamp(snap(right + dx, b.x, b.x + b.width), left + minW, left + b.width * 4)
            if (vertical < 0 && dy) top = clamp(snap(top + dy, b.y, b.y + b.height), bottom - b.height * 4, bottom - minH)
            if (vertical > 0 && dy) bottom = clamp(snap(bottom + dy, b.y, b.y + b.height), top + minH, top + b.height * 4)
        }
        if (!editState.customized && left === r.x && top === r.y && right === r.x + r.width && bottom === r.y + r.height)
            return
        // 保留少量可抓取区域，方便把越界画面拖回。
        const w = right - left
        const h = bottom - top
        const gripX = Math.min(24, w, width)
        const gripY = Math.min(24, h, height)
        left = clamp(left, gripX - w, width - gripX)
        top = clamp(top, gripY - h, height - gripY)
        saveGeometry(left, top, w, h)
    }

    function beginGesture(x, y) {
        const direction = hitDirection(x, y)
        if (!direction || !canEdit || layerRemoved || sourceHidden)
            return false
        if (sourceLocked) {
            selectLayer()
            return false
        }
        selectLayer()
        const r = viewportRect
        const c = editState.crop
        // 保存值快照，拖动时布局及裁剪边界会变化。
        editState.gesture = {
            startX: x, startY: y, direction,
            bounds: {x: r.x, y: r.y, width: r.width, height: r.height},
            crop: {x: c.x, y: c.y, width: c.width, height: c.height}, moved: false
        }
        return true
    }

    function finishGesture(x, y) {
        updateGesture(x, y)
        cancelGesture()
    }

    function cancelGesture() { editState.gesture = null }

    clip: true
    Rectangle {
        objectName: "recordingEncodingCanvas"
        parent: root.canvasBackground
        visible: root.layerId === 0
        x: root.outputRect.x; y: root.outputRect.y
        width: root.outputRect.width; height: root.outputRect.height
        color: "black"
        border.color: "#56627696"
    }
    Item {
        id: viewport
        objectName: "recordingViewport"
        x: root.viewportRect.x; y: root.viewportRect.y
        width: root.viewportRect.width; height: root.viewportRect.height
        clip: true
        opacity: root.layerRemoved || root.sourceHidden ? 0 : 1
        Item {
            id: orientedSource
            readonly property rect crop: editState.crop
            x: -crop.x * width; y: -crop.y * height
            width: viewport.width / crop.width; height: viewport.height / crop.height
            transform: Scale { origin.x: orientedSource.width / 2; xScale: root.mirrored ? -1 : 1 }
            Loader {
                id: preview
                objectName: "recordingViewportLoader"
                anchors.centerIn: parent
                width: root.swapsAxes ? orientedSource.height : orientedSource.width
                height: root.swapsAxes ? orientedSource.width : orientedSource.height
                rotation: editState.quarterTurns * 90
                sourceComponent: root.sourceComponent
                opacity: root.previewAvailable ? 1 : 0
            }
        }
    }

    Binding {
        target: preview.item && preview.item.previewInfo ? preview.item.previewInfo : null
        property: "parent"
        value: viewport
        when: target !== null
    }

    Canvas {
        id: overflow
        objectName: "recordingViewportCropWarning"
        anchors.fill: parent
        visible: root.previewAvailable && !root.layerRemoved && !root.sourceHidden && root.hasCrop
        onVisibleChanged: if (visible) requestPaint()
        onPaint: {
            const c = getContext("2d")
            c.reset()
            c.clearRect(0, 0, width, height)
            const a = root.viewportRect
            const b = root.outputRect
            const x = Math.max(0, a.x)
            const y = Math.max(0, a.y)
            const r = Math.min(width, a.x + a.width)
            const d = Math.min(height, a.y + a.height)
            if (r <= x || d <= y)
                return
            const middleY = Math.max(y, b.y)
            const middleHeight = Math.min(d, b.y + b.height) - middleY
            const bottomY = Math.max(y, b.y + b.height)
            const rightX = Math.max(x, b.x + b.width)
            const paths = [[x, y, r - x, Math.min(d, b.y) - y], [x, bottomY, r - x, d - bottomY],
                [x, middleY, Math.min(r, b.x) - x, middleHeight], [rightX, middleY, r - rightX, middleHeight]]
            c.beginPath()
            let outside = false
            for (const p of paths) {
                if (p[2] > 0 && p[3] > 0) {
                    outside = true
                    c.rect(p[0], p[1], p[2], p[3])
                }
            }
            if (!outside)
                return
            c.clip()
            c.fillStyle = Qt.rgba(0.12, 0.14, 0.18, 0.44)
            c.fillRect(x, y, r - x, d - y)
            c.beginPath()
            for (let line = x - d + y; line < r; line += 12) {
                c.moveTo(line, d)
                c.lineTo(line + d - y, y)
            }
            c.strokeStyle = Qt.rgba(0.78, 0.8, 0.84, 0.36)
            c.lineWidth = 3
            c.stroke()
        }
    }
    Item {
        parent: root.overlayHost
        anchors.fill: parent
        visible: root.showSelectionGuides
        Rectangle {
            objectName: "recordingViewportSelection" + (root.layerId ? "_" + root.layerId : "")
            x: viewport.x; y: viewport.y; width: viewport.width; height: viewport.height
            color: "transparent"
            border.color: root.cropMode ? "#f4c989" : root.selectionColor
            border.width: 2
        }
        Repeater {
            model: ["Left", "Right", "Top", "Bottom"]
            delegate: Rectangle {
                required property string modelData
                id: guide
                readonly property bool horizontal: modelData === "Left" || modelData === "Right"
                readonly property bool leading: modelData === "Left" || modelData === "Top"
                readonly property int pixels: horizontal ? (leading ? root.leftGapPixels : root.rightGapPixels)
                    : (leading ? root.topGapPixels : root.bottomGapPixels)
                readonly property rect b: root.outputRect
                readonly property rect a: root.viewportRect
                readonly property real canvasStart: horizontal ? b.x : b.y
                readonly property real canvasEnd: canvasStart + (horizontal ? b.width : b.height)
                readonly property real layerStart: horizontal ? a.x : a.y
                readonly property real layerEnd: layerStart + (horizontal ? a.width : a.height)
                readonly property real lineStart: leading ? canvasStart : root.clamp(layerEnd, canvasStart, canvasEnd)
                readonly property real lineEnd: leading ? root.clamp(layerStart, canvasStart, canvasEnd) : canvasEnd
                readonly property real lineLength: lineEnd - lineStart
                visible: root.showSelectionGuides && pixels > 0 && (horizontal ? width : height) > 0
                x: horizontal ? lineStart : root.clamp(a.x + a.width / 2, b.x, b.x + b.width - 1)
                y: horizontal ? root.clamp(a.y + a.height / 2, b.y, b.y + b.height - 1) : lineStart
                width: horizontal ? lineLength : 1
                height: horizontal ? 1 : lineLength
                color: root.selectionColor
                Text {
                    objectName: "recording" + guide.modelData + "Gap"
                    x: root.clamp(guide.x + (guide.horizontal ? (guide.width - width) / 2 : 6),
                                  guide.b.x, guide.b.x + guide.b.width - width) - guide.x
                    y: root.clamp(guide.y + (guide.horizontal ? -height - 3 : (guide.height - height) / 2),
                                  guide.b.y, guide.b.y + guide.b.height - height) - guide.y
                    text: guide.pixels + " px"
                    font.family: root.fontFamily; font.pixelSize: root.guidePixelSize
                    color: root.handleColor
                    style: Text.Outline
                    styleColor: "#222035"
                }
            }
        }

        Repeater {
            model: [
                { name: "topLeft", x: 0, y: 0 }, { name: "top", x: 0.5, y: 0 }, { name: "topRight", x: 1, y: 0 },
                { name: "left", x: 0, y: 0.5 }, { name: "right", x: 1, y: 0.5 },
                { name: "bottomLeft", x: 0, y: 1 }, { name: "bottom", x: 0.5, y: 1 }, { name: "bottomRight", x: 1, y: 1 }
            ]
            delegate: Rectangle {
                required property var modelData
                readonly property real handleX: viewport.x + modelData.x * viewport.width
                readonly property real handleY: viewport.y + modelData.y * viewport.height
                objectName: "recordingViewportHandle_" + modelData.name + (root.layerId ? "_" + root.layerId : "")
                visible: root.showSelectionGuides && handleX >= 0 && handleX <= root.width && handleY >= 0 && handleY <= root.height
                x: root.clamp(handleX - width / 2, 0, root.width - width)
                y: root.clamp(handleY - height / 2, 0, root.height - height)
                width: 8; height: 8
                color: root.cropMode ? "#ffe2a9" : root.handleColor
                border.color: root.handleBorderColor
            }
        }
    }
}
