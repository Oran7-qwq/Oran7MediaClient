// Oran7ShaderTextMask.qml
// 歌词文字层的 shader 特效宿主。
//
// 设计目标：换特效 = 只替换 shaders/*.frag 文件，QML 一行都不用动。
// 做法：所有 shader 共用一份固定 uniform 契约（见 shaders/octgrams.frag 顶部的「契约头」），
//      本组件只负责把文字层渲染成纹理、按 shaderName 选 .qsb、喂 uniform；
//      「用文字笔画的 alpha 把特效裁出来」那一步在 shader 文件里做。
//
// 与 Oran7GradientMask 的区别：本组件**不含任何配色** —— 没有色板、没有颜色过渡动画，
// 特效长什么样完全由 shader 文件决定。
import QtQuick
import "../../Basic"

Item {
    id: root

    // 外部自定义内容区域（白色 Text/Image，其笔画 alpha 作为特效的形状）
    default property alias content: maskContent.data
    property alias contentItem: maskContent

    // 特效开关：false = 原样显示内容，不做特效
    property bool effectEnabled: true

    // 选哪个 shader：对应 shaderUrls 表的 key
    property string shaderName: "clouds"

    // 时间 uniform（秒），由下方 FrameAnimation 推进
    property real iTime: 0

    // shader 清单。加新特效只动这里：
    //   1. shaders/<name>.frag 按契约头写（正文可原样粘贴 Shadertoy）
    //   2. CMakeLists.txt 的 qt_add_shaders FILES 加一行，重跑 CMake
    //   3. 这里加一条
    readonly property var shaderUrls: ({
        "octgrams": "qrc:/shaders/octgrams.frag.qsb",
        "fractal":  "qrc:/shaders/fractal.frag.qsb",
        "clouds":   "qrc:/shaders/clouds.frag.qsb"
    })

    readonly property url shaderUrl:
        shaderUrls[shaderName] !== undefined ? shaderUrls[shaderName] : ""

    readonly property bool _shaderReady:
        root.effectEnabled && String(root.shaderUrl).length > 0

    // 内容容器：渲染到 layer，由 shader 用其 alpha 重新着色为特效。
    // 沿用 Oran7GradientMask 的写法 —— 文字只渲染这一层，shader 输出直接替换它，
    // 所以不会有白色垫底层、也不会出现描边阴影。
    Item {
        id: maskContent

        anchors.fill: parent

        layer.enabled: root._shaderReady
        layer.effect: ShaderEffect {
            // 属性名与类型必须与 shader 里 uniform buf 的成员一一对应。
            // iResolution 传真实像素尺寸；虚拟画布宽高比由 shader 文件自己决定
            // （见 octgrams.frag 的 SCENE_ASPECT）。
            property vector3d iResolution: Qt.vector3d(root.width, root.height, 1.0)
            property real     iTime: root.iTime

            fragmentShader: root.shaderUrl
        }
    }

    // 时间驱动：交给项目统一的动画总开关，关掉无限动画时特效静止。
    //
    // 用 Timer 递增而不是 FrameAnimation：
    //  1) 与 Oran7GradientMask 的 dynamicTimer 是同一套已验证写法（同样声明在
    //     default property alias 之后的位置），行为可预期；
    //  2) iTime 单调递增，不像 NumberAnimation 循环那样在回绕时产生跳变。
    Timer {
        interval: 16
        repeat: true
        running: root._shaderReady && BasicConfig.runningInfinitePropertyAnimation
        onTriggered: root.iTime = root.iTime + interval / 1000
    }
}
