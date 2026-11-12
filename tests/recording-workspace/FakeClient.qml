pragma Singleton
import QtQuick
QtObject {
    id: root
    enum RenderObject { ScreenCaptureRender, Fit, Fill }
    property int attempts: 0
    property int previewStarts: 0
    property int scale: -1
    property bool attachAllowed: false
    property var captureValues: ({outputIndex: 0, fps: 60, drawMouse: true, debugLog: false})
    property QtObject capture: QtObject {
        property string fileFormat: "mp4"
        property bool recordingBusy: false
        property string recordingState: "Idle"
        property string recordingError: ""
        property string recordingOutputPath: ""
        signal recordingSettingsChanged()
        signal captureSettingsChanged()
        signal started()
        signal errorOccurred(string err)
        function restartPreview() { return !recordingBusy }
        function start() { root.previewStarts++ }
        function recordingSettings() { return {mode: 0, format: fileFormat, outputDirectory: "", filePrefix: "Oran7", width: 0, height: 0, fps: 60, bitrateKbps: 40000, audioEnabled: true} }
        function applyRecordingSettings(values) { if (values.format !== undefined) fileFormat = values.format; return {ok: true, msg: "已保存"} }
        function startRecording() { return true }
        function stopRecording() {}
    }
    signal screenCaptureApplyFinished(bool ok, string msg)
    signal videoRenderInfoUpdated(var info)
    function screenCapture() { return capture }
    function screenCaptureMonitors() { return [{outputIndex: 0, width: 1920, height: 1080, primary: true, attached: true}] }
    function screenCaptureSettings() { return captureValues }
    function applyScreenCaptureSettings(index, fps, mouse, debug) { return {ok: true, pending: false, msg: "已保存"} }
    function attachVideoItem(key, host) { attempts++; return attachAllowed }
    function setScaleMode(key, value) { scale = value }
}
