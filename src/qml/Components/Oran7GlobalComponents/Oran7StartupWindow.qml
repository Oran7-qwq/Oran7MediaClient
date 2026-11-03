import QtQuick
import QtQuick.Window

Window {
    id: root
    objectName: "__Oran7StartupWindow__"
    width: 460; height: 300
    x: Screen.virtualX + (Screen.width - width) / 2
    y: Screen.virtualY + (Screen.height - height) / 2
    visible: true
    color: "transparent"
    title: "Oran7MediaClient"
    flags: Qt.SplashScreen | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    // Quit 会先请求关闭顶层窗口；取消后仍拒绝 close 会反过来否决应用退出。
    onClosing: if (!Oran7Startup.finished) Oran7Startup.cancel()
    Oran7StartupSurface {
        width: root.width; height: root.height
        stageText: Oran7Startup.stageText
        errorText: Oran7Startup.error
        failed: Oran7Startup.failed
        animate: root.visible && Oran7Startup.active
        onCancelRequested: root.close()
    }
}
