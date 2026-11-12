import QtQuick
import QtQuick.Controls
import "../../src/qml/Components/Oran7GlobalComponents"
Window {
    id: root
    width: 1320; height: 780; visible: true
    color: "#17181d"
    property alias workspace: workspace
    property alias fake: fake
    property int autoSaveCount: 0
    property int autoStartCount: 0
    QtObject {
        id: fake
        property bool recordingBusy: false
        property bool previewBusy: false
        property string previewState: "Running"
        property string previewError: ""
        property bool requestIds: false
        property int requestCounter: 0
        signal captureRequestFinished(var result)
        property string recordingState: "Idle"
        property string recordingError: ""
        property string recordingOutputPath: ""
        property int starts: 0
        property int stops: 0
        property int saves: 0
        property int captureApplies: 0
        property bool reject: false
        property bool asyncApply: false
        property var applied: ({})
        property var capture: ({outputIndex: 4, fps: 60, drawMouse: true, debugLog: false})
        property var options: ({mode: 1, format: "mp4", outputDirectory: "", filePrefix: "中文录像", width: 0,
            height: 0, fps: 60, bitrateKbps: 40000, audioEnabled: true,
            whipUrl: "https://example.test/whip", bearerToken: "retained", roomId: "room"})
        signal screenCaptureApplyFinished(bool ok, string msg)
        signal recordingSettingsChanged()
        signal captureSettingsChanged()
        signal started()
        signal errorOccurred(string err)
        function restartPreview() { return !recordingBusy }
        function screenCaptureMonitors() { return [{outputIndex: 1,width: 1920,height: 1080,monitorName: "LQ156T1JW05",primary: false,attached: true},
            {outputIndex: 4,width: 2560,height: 1440,monitorName: "X7IQ",primary: true,attached: true},
            {outputIndex: 7,width: 0,height: 0,primary: false,attached: false}] }
        function screenCaptureSettings() { return Object.assign({}, capture) }
        function applyScreenCaptureSettings(index, fps, mouse, debug) {
            captureApplies++
            applied = {outputIndex: index, fps: fps, drawMouse: mouse, debugLog: debug}
            if (reject || fps < options.fps) return {ok: false, pending: false, msg: "参数不合法"}
            if (!asyncApply) capture = applied
            return {ok: true, requestId: requestIds ? ++requestCounter : 0, pending: asyncApply, msg: asyncApply ? "应用中" : "已保存"}
        }
        function recordingSettings() { return Object.assign({}, options) }
        function applyRecordingSettings(values) {
            if (reject || recordingBusy || values.fps > capture.fps) return {ok: false, msg: "设置被拒绝"}
            saves++
            options = Object.assign({}, values)
            recordingSettingsChanged()
            return {ok: true, msg: "已保存"}
        }
        function startRecording() { starts++; recordingState = "Preparing"; recordingBusy = true; return true }
        function stopRecording() { stops++; recordingState = "Stopping" }
    }
    Oran7RecordingWorkspace {
        id: workspace
        objectName: "workspace"
        anchors.fill: parent
        service: fake
        controller: fake
        previewComponent: Component {
            Rectangle {
                color: "#101216"
                Label { anchors.centerIn: parent; text: "桌面画面预览"; color: "#aaaaaa" }
            }
        }
    }
    function runFrameRateChecks() {
        var state = workspace.pageState
        var previousCapture = Object.assign({}, fake.capture)
        var previousOutput = Object.assign({}, fake.options)
        var previousSaves = fake.saves
        var sizeEstimate = workspace.bitrateStorageEstimate
        var n = 0
        function ensure(value, label) { if (!value) throw new Error(label); return 1 }
        state.editOutput("fps", 30)
        n += ensure(state.saveOutput(), "先降低并保存录制帧率")
        state.editCapture("fps", 30)
        n += ensure(state.applyCapture() && state.captureSaved.fps === 30, "再降低采集帧率")
        state.editOutput("fps", 120)
        n += ensure(!state.saveOutput() && !state.start(), "右侧超过左侧时禁止保存或开始")
        state.editCapture("fps", 120)
        n += ensure(!state.saveOutput(), "左侧草稿未应用时不能保存较高录制帧率")
        n += ensure(state.applyCapture() && state.saveOutput(), "先提高采集再保存录制")
        n += ensure(workspace.bitrateStorageEstimate === sizeEstimate, "固定码率的体积估算不随帧率改变")
        state.editCapture("fps", 60)
        n += ensure(!state.applyCapture(), "不能直接把采集降到已保存录制帧率以下")
        state.editOutput("fps", 60)
        n += ensure(!state.applyCapture(), "录制草稿降低后仍需先保存")
        n += ensure(state.saveOutput() && state.applyCapture(), "保存较低录制帧率后可降低采集")
        fake.capture = previousCapture
        fake.options = previousOutput
        fake.saves = previousSaves
        state.reset()
        return n
    }
    function runChecks() {
        var state = workspace.pageState
        // 控件自动保存回归已提交本地模式，这里重置原手动编排用例的初始配置。
        fake.options = Object.assign({}, fake.options, {mode: 1})
        state.reset()
        function ensure(value, label) { if (!value) throw new Error(label); return 1 }
        var n = 0
        n += ensure(state.monitorRow === 1, "原始显示器序号映射")
        n += ensure(state.outputDirty && state.outputDraft.mode === 0 && fake.options.mode === 1, "本地草稿不提前改变旧模式")
        state.editOutput("fps", 50)
        state.editOutput("outputDirectory", "C:/录像/演示")
        n += ensure(state.saveOutput(), "聚合保存")
        n += ensure(fake.options.fps === 50 && fake.options.mode === 0 && !state.outputDirty, "保存后刷新草稿")
        n += ensure(fake.options.bearerToken === "retained" && fake.options.whipUrl === "https://example.test/whip", "保留推流参数")
        state.editCapture("debugLog", true)
        n += ensure(state.applyCapture() && !state.applying && !state.captureDirty, "同步成功不等待异步通知")
        state.editCapture("outputIndex", 1)
        fake.asyncApply = true
        n += ensure(state.applyCapture() && state.applying && state.busy && fake.applied.outputIndex === 1, "异步应用原始序号")
        n += ensure(!state.start() && fake.starts === 0, "应用中禁止开始")
        fake.capture = fake.applied
        fake.screenCaptureApplyFinished(true, "已切换")
        n += ensure(!state.applying && !state.captureDirty && state.monitorRow === 0, "异步成功读回")
        state.editCapture("fps", 120)
        state.applyCapture()
        fake.screenCaptureApplyFinished(false, "已回滚")
        n += ensure(state.captureDraft.fps === 60 && !state.captureDirty, "失败回滚读回")
        fake.reject = true
        state.editCapture("debugLog", false)
        n += ensure(!state.applyCapture() && !state.busy && !state.captureDirty, "同步拒绝解除忙状态")
        state.editOutput("filePrefix", "未保存")
        n += ensure(!state.start() && fake.starts === 0 && state.outputDirty, "保存失败阻止开始并保留草稿")
        fake.reject = false
        state.editCapture("fps", 24)
        n += ensure(!state.start() && fake.starts === 0, "未应用来源阻止开始")
        state.reset()
        n += ensure(!state.captureDirty && !state.outputDirty && state.outputDraft.filePrefix === "中文录像", "恢复所有草稿")
        state.editOutput("audioEnabled", false)
        n += ensure(state.start() && fake.starts === 1 && !fake.options.audioEnabled && state.busy, "保存设置后提交开始")
        n += ensure(!state.saveOutput() && !state.start(), "忙状态拒绝设置与重入")
        fake.stopRecording()
        n += ensure(state.busy && fake.recordingState === "Stopping", "保存期间仍忙")
        fake.recordingState = "Idle"
        fake.recordingBusy = false
        state.editCapture("outputIndex", 7)
        n += ensure(!state.applyCapture() && !state.busy, "未连接输出不可应用")
        state.reset()
        fake.options = Object.assign({}, fake.options, {fps: 25})
        fake.recordingSettingsChanged()
        n += ensure(state.outputDraft.fps === 25, "无草稿时同步外部配置")
        state.editOutput("fps", 30)
        fake.options = Object.assign({}, fake.options, {fps: 24})
        fake.recordingSettingsChanged()
        n += ensure(state.outputDraft.fps === 30, "外部通知不覆盖用户草稿")
        state.reset()
        fake.capture = Object.assign({}, fake.capture, {outputIndex: 4})
        fake.captureSettingsChanged()
        n += ensure(state.captureDraft.outputIndex === 4 && !state.captureDirty, "恢复重映射同步页面")
        state.editCapture("drawMouse", !state.captureSaved.drawMouse)
        fake.capture = Object.assign({}, fake.capture, {outputIndex: 1})
        fake.captureSettingsChanged()
        n += ensure(state.captureDraft.outputIndex === 4 && state.captureSaved.outputIndex === 1 && state.captureDirty, "重映射保留未应用草稿")
        state.message = "正在重新连接预览…"
        fake.started()
        n += ensure(state.message === "预览已连接", "重连成功反馈")
        fake.errorOccurred("重连失败")
        n += ensure(state.message === "重连失败", "重连错误反馈")
        state.reset()
        return n
    }
    // C++ 在各步骤之间处理事件循环，覆盖真实 Timer 与异步应用信号。
    function runAutoStep(step) {
        var state = workspace.pageState
        function ensure(value, label) { if (!value) throw new Error(label) }
        switch (step) {
        case 0:
            fake.asyncApply = false
            fake.reject = false
            fake.options = Object.assign({}, fake.options, {bitrateKbps: 40000, audioEnabled: true})
            state.reset()
            autoSaveCount = fake.saves
            state.editOutput("bitrateKbps", 8000)
            state.editOutput("audioEnabled", false)
            break
        case 1:
            ensure(fake.saves === autoSaveCount + 1 && fake.options.bitrateKbps === 8000 && !fake.options.audioEnabled, "连续设置合并自动保存")
            autoSaveCount = fake.saves
            state.editOutput("filePrefix", "输入中", true)
            break
        case 2:
            ensure(fake.saves === autoSaveCount && fake.options.filePrefix !== "输入中", "输入未结束不保存")
            state.commitOutput()
            break
        case 3:
            ensure(fake.options.filePrefix === "输入中" && !state.outputDirty, "编辑结束自动保存")
            fake.reject = true
            state.editOutput("filePrefix", "拒绝保存")
            break
        case 4:
            ensure(state.outputDirty && fake.options.filePrefix === "输入中" && !state.busy, "自动保存失败保留草稿并解除等待")
            fake.reject = false
            state.editOutput("filePrefix", "重新编辑")
            break
        case 5:
            ensure(fake.options.filePrefix === "重新编辑" && !state.outputDirty, "失败后再次修改自动提交")
            fake.asyncApply = true
            state.editCapture("drawMouse", !state.captureSaved.drawMouse)
            break
        case 6:
            ensure(state.applying && state.busy, "采集设置自动异步应用")
            state.editOutput("bitrateKbps", 12000)
            break
        case 7:
            ensure(fake.options.bitrateKbps === 8000, "采集应用期间暂缓录制配置保存")
            fake.capture = fake.applied
            fake.screenCaptureApplyFinished(true, "已应用")
            break
        case 8:
            ensure(fake.options.bitrateKbps === 12000 && !state.busy && !state.outputDirty, "异步完成后继续自动保存")
            autoStartCount = fake.starts
            state.editCapture("drawMouse", !state.captureSaved.drawMouse)
            state.start()
            ensure(state.applying && state.startAfterApply && fake.starts === autoStartCount, "开始录制等待采集设置完成")
            fake.capture = fake.applied
            fake.screenCaptureApplyFinished(true, "已应用")
            break
        case 9:
            ensure(fake.starts === autoStartCount + 1 && state.busy, "设置成功后自动继续开始录制")
            fake.recordingBusy = false
            fake.recordingState = "Idle"
            state.reset()
            state.editCapture("drawMouse", !state.captureSaved.drawMouse)
            state.start()
            fake.screenCaptureApplyFinished(false, "已回滚")
            break
        case 10:
            ensure(fake.starts === autoStartCount + 1 && !state.busy && !state.startAfterApply, "设置失败不开始录制")
            state.reset()
            state.editCapture("fps", 120)
            state.editOutput("fps", 120)
            break
        case 11:
            ensure(state.applying && fake.options.fps !== 120 && fake.applied.fps === 120, "升帧率先应用采集设置")
            fake.capture = fake.applied
            fake.screenCaptureApplyFinished(true, "已应用")
            break
        case 12:
            ensure(fake.capture.fps === 120 && fake.options.fps === 120 && !state.outputDirty, "采集升帧率后自动保存录制帧率")
            state.editOutput("fps", 30)
            state.editCapture("fps", 30)
            break
        case 13:
            ensure(state.applying && fake.options.fps === 30 && fake.applied.fps === 30, "降帧率先保存录制设置")
            fake.capture = fake.applied
            fake.screenCaptureApplyFinished(true, "已应用")
            break
        case 14:
            ensure(fake.capture.fps === 30 && fake.options.fps === 30 && !state.captureDirty && !state.outputDirty, "双侧帧率自动提交完成")
            fake.asyncApply = false
            state.reset()
            break
        }
        return true
    }
    function runRequestIdChecks() {
        function ensure(value, label) { if (!value) throw new Error(label) }
        var state = workspace.pageState
        fake.asyncApply = true
        fake.requestIds = true
        state.reset()
        state.editCapture("drawMouse", !state.captureSaved.drawMouse)
        ensure(state.applyCapture() && state.captureRequestId > 0, "新版请求记录编号")
        var oldId = state.captureRequestId
        fake.captureRequestFinished({requestId: oldId + 20, ok: false, msg: "旧错误"})
        ensure(state.applying, "无关结果不能结束当前请求")
        state.editCapture("debugLog", !state.captureSaved.debugLog, true)
        var nextDebug = state.captureDraft.debugLog
        fake.capture = fake.applied
        fake.captureRequestFinished({requestId: oldId, ok: true, msg: "已应用"})
        ensure(!state.applying && state.captureDraft.debugLog === nextDebug && state.captureDirty, "完成不覆盖后续输入")
        fake.screenCaptureApplyFinished(false, "迟到旧接口结果")
        ensure(state.captureDraft.debugLog === nextDebug && state.captureDirty, "兼容接口旧结果被忽略")
        ensure(state.applyCapture(), "新草稿可继续提交")
        fake.captureRequestFinished({requestId: oldId, ok: false, msg: "旧失败"})
        ensure(state.applying, "旧编号不能结束后续应用")
        fake.capture = fake.applied
        fake.captureRequestFinished({requestId: state.captureRequestId, ok: true, msg: "已应用"})
        ensure(!state.applying && !state.captureDirty, "匹配编号正常完成")
        fake.previewState = "Stopping"
        fake.previewBusy = true
        ensure(state.busy && !state.start() && !state.saveOutput(), "后端预览忙状态统一禁止录制与改输出")
        fake.previewBusy = false
        fake.previewState = "Running"
        fake.asyncApply = false
        fake.requestIds = false
        state.usesCaptureRequestIds = false
        state.reset()
        return 8
    }
}
