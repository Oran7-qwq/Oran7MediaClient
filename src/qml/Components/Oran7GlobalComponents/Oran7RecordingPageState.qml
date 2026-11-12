import QtQuick

// 页面只持有设置草稿，运行状态始终读取 C++ 控制器。
Item {
    id: root
    visible: false
    required property var service
    required property var controller
    readonly property var monitors: translatedMonitors
    property var captureDraft: ({})
    property var captureSaved: ({})
    property var outputDraft: ({})
    property var outputSaved: ({})
    property var captureRequestId: 0
    property bool usesCaptureRequestIds: false
    property var submittedCapture: null
    property bool applying: false
    property bool captureSaveQueued: false
    property bool outputSaveQueued: false
    property bool startAfterApply: false
    property string message: ""
    readonly property bool busy: !controller || controller.recordingBusy || controller.previewBusy === true || applying
    readonly property string previewHint: {
        if (!controller || controller.previewBusy !== true) return ""
        if (controller.previewError) return String(controller.previewError)
        if (controller.previewState === "Starting") return qsTranslate("Oran7Ui", "正在启动预览…")
        if (controller.previewState === "Stopping") return qsTranslate("Oran7Ui", "正在停止预览…")
        return qsTranslate("Oran7Ui", "正在恢复预览…")
    }
    readonly property bool captureDirty: JSON.stringify(captureDraft) !== JSON.stringify(captureSaved)
    readonly property bool outputDirty: JSON.stringify(outputDraft) !== JSON.stringify(outputSaved)
    readonly property int minimumCaptureFps: Number.isInteger(outputDraft.fps) && outputDraft.fps > 0 ? outputDraft.fps : 1
    readonly property int maximumOutputFps: Number.isInteger(captureDraft.fps) && captureDraft.fps > 0 ? captureDraft.fps : 1

    function validFrameRates() {
        if (!Number.isInteger(captureDraft.fps) || captureDraft.fps < 1 || captureDraft.fps > 240) {
            message = "采集帧率须为 1～240 的整数"
            return false
        }
        if (!Number.isInteger(outputDraft.fps) || outputDraft.fps < 1 || outputDraft.fps > 240) {
            message = "录制帧率须为 1～240 的整数"
            return false
        }
        if (outputDraft.fps > captureDraft.fps) {
            message = "录制帧率上限不能超过采集帧率，请提高左侧采集帧率或降低右侧录制帧率"
            return false
        }
        return true
    }
    readonly property int monitorRow: {
        for (var i = 0; i < monitors.length; ++i)
            if (monitors[i].outputIndex === captureDraft.outputIndex) return i
        return -1
    }

    function queueSave(capture, deferred) {
        if (capture) captureSaveQueued = !deferred
        else outputSaveQueued = !deferred
        if (captureSaveQueued || outputSaveQueued) autoSaveTimer.restart()
    }
    function commitCapture() { queueSave(true, false) }
    function commitOutput() { queueSave(false, false) }
    function editCapture(key, value, deferred) {
        var next = Object.assign({}, captureDraft)
        next[key] = value
        captureDraft = next
        queueSave(true, !!deferred)
    }
    function editOutput(key, value, deferred) {
        var next = Object.assign({}, outputDraft)
        next[key] = value
        outputDraft = next
        queueSave(false, !!deferred)
    }
    function flushSettings() {
        autoSaveTimer.stop()
        if (busy) return false // 异步应用完成或录制结束后继续处理已提交的变更。
        // 降帧率先保存录制端，升帧率先应用采集端，保持后端约束。
        if (outputSaveQueued && outputDirty && outputDraft.fps <= captureSaved.fps) {
            outputSaveQueued = false
            if (!saveOutput()) return false
        }
        if (captureSaveQueued) {
            captureSaveQueued = false
            if (captureDirty && !applyCapture()) return false
            if (applying) return false
        }
        if (outputSaveQueued) {
            outputSaveQueued = false
            if (outputDirty && !saveOutput()) return false
        }
        return !captureDirty && !outputDirty
    }
    Timer { id: autoSaveTimer; interval: 180; onTriggered: root.flushSettings() }
    onBusyChanged: {
        if (!busy && (captureSaveQueued || outputSaveQueued)) autoSaveTimer.restart()
    }
    property var monitorInputs: []
    readonly property var translatedMonitors: {
        var list = monitorInputs
        var next = []
        for (var i = 0; i < list.length; ++i) {
            var m = list[i]
            var name = qsTranslate("Oran7Ui", "显示器 ") + (m.outputIndex + 1)
            if (m.monitorName && String(m.monitorName).trim().length)
                name += "：" + String(m.monitorName).trim()
            next.push({outputIndex: m.outputIndex, attached: m.attached,
                name: name,
                details: m.width + " × " + m.height + (m.primary ? qsTranslate("Oran7Ui", " · 主屏") : "")
                    + (m.attached ? "" : qsTranslate("Oran7Ui", " · 未连接")),
                label: name + " · " + m.width + "×" + m.height
                    + (m.primary ? qsTranslate("Oran7Ui", " · 主屏") : "") + (m.attached ? "" : qsTranslate("Oran7Ui", " · 未连接"))})
        }
        return next
    }
    function refreshMonitors() {
        if (busy) return
        monitorInputs = service.screenCaptureMonitors()
    }
    function reloadCapture() {
        captureSaved = service.screenCaptureSettings()
        captureDraft = Object.assign({}, captureSaved)
    }
    function localOptions(values) {
        return {mode: values.mode, format: values.format, outputDirectory: values.outputDirectory, filePrefix: values.filePrefix,
            width: values.width, height: values.height, fps: values.fps,
            bitrateKbps: values.bitrateKbps, audioEnabled: values.audioEnabled}
    }
    function reloadOutput() {
        if (!controller) return
        outputSaved = localOptions(controller.recordingSettings())
        // 本页面用于本地文件录制；容器格式与 WHIP 输出模式分开。
        outputDraft = Object.assign({}, outputSaved, {mode: 0})
    }
    function reset() {
        if (busy) return
        autoSaveTimer.stop()
        captureSaveQueued = false
        outputSaveQueued = false
        startAfterApply = false
        reloadCapture()
        reloadOutput()
        message = "已恢复已保存设置"
    }
    function finishCapture(ok, msg) {
        // 完成回调只更新保存基线，保留应用期间新编辑的草稿。
        var draft = captureDraft
        var preserve = submittedCapture && JSON.stringify(draft) !== JSON.stringify(submittedCapture)
        reloadCapture()
        if (preserve) captureDraft = draft
        captureRequestId = 0
        submittedCapture = null
        applying = false
        message = String(msg)
        if (startAfterApply) {
            startAfterApply = false
            if (ok) start()
        }
    }
    function applyCapture() {
        if (busy || !captureDirty) return false
        if (monitorRow < 0 || !monitors[monitorRow].attached) {
            message = "请选择已连接的显示器"
            return false
        }
        if (!validFrameRates()) return false
        if (captureDraft.fps < outputSaved.fps) {
            message = "较低的录制帧率尚未保存，请先完成右侧帧率设置"
            return false
        }
        submittedCapture = Object.assign({}, captureDraft)
        applying = true
        message = "正在应用画面来源…"
        var result = service.applyScreenCaptureSettings(captureDraft.outputIndex,
            captureDraft.fps, captureDraft.drawMouse, captureDraft.debugLog)
        captureRequestId = result && result.requestId ? result.requestId : 0
        if (captureRequestId) usesCaptureRequestIds = true
        if (!result || !result.ok || !result.pending)
            finishCapture(result && result.ok, result ? result.msg : "应用失败")
        return result && result.ok
    }
    function saveOutput() {
        if (busy) return false
        if (!validFrameRates()) return false
        if (outputDraft.fps > captureSaved.fps) {
            message = "较高的采集帧率尚未生效，请先完成左侧帧率设置"
            return false
        }
        // 从后端取完整配置再合并，保留本页不编辑的 WHIP/信令参数。
        var values = Object.assign({}, controller.recordingSettings(), outputDraft, {mode: 0})
        var result = controller.applyRecordingSettings(values)
        message = result ? String(result.msg || (result.ok ? "录制设置已保存" : "保存失败")) : "保存失败"
        if (result && result.ok) reloadOutput()
        return result && result.ok
    }
    function start() {
        if (busy) return false
        if (!validFrameRates()) return false
        captureSaveQueued = captureDirty
        outputSaveQueued = outputDirty
        if (!flushSettings()) {
            if (applying) startAfterApply = true
            return false
        }
        message = ""
        var accepted = controller.startRecording()
        if (!accepted) message = "录制请求未接受，请检查当前状态"
        return accepted
    }
    Connections {
        target: root.service
        function onScreenCaptureApplyFinished(ok, msg) {
            if (!root.usesCaptureRequestIds && root.applying) root.finishCapture(ok, msg)
        }
    }
    Connections {
        target: root.controller
        ignoreUnknownSignals: true // 旧的嵌入宿主可继续使用无编号完成信号。
        function onCaptureRequestFinished(result) {
            if (!root.applying || result.requestId !== root.captureRequestId) return
            root.finishCapture(result.ok, result.msg)
        }
        function onStarted() {
            if (root.message === "正在重新连接预览…") root.message = "预览已连接"
            if (!root.busy) root.refreshMonitors()
        }
        function onErrorOccurred(err) { root.message = String(err) }
        function onCaptureSettingsChanged() {
            // 自动恢复可能重映射显示器序号；保留用户尚未应用的草稿。
            var dirty = root.captureDirty
            var draft = root.captureDraft
            root.reloadCapture()
            if (dirty) root.captureDraft = draft
            if (!root.busy) root.refreshMonitors()
        }
        function onRecordingSettingsChanged() {
            if (!root.outputDirty) root.reloadOutput()
        }
    }
    Component.onCompleted: {
        reloadCapture()
        reloadOutput()
        refreshMonitors()
    }
}
