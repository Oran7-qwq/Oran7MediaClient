# WaveBarChart 组件使用说明

## 功能描述
一个三柱状图组件，支持静态和动态两种状态：
- **静态状态**：固定高度 [10, 30, 20]（从左到右）
- **动态状态**：波浪跃动效果，高度范围 5-30，波浪从左到右循环

## 基本使用

### 1. 简单示例
```qml
import QtQuick 2.15
import "."

Rectangle {
    WaveBarChart {
        anchors.centerIn: parent
        animationEnabled: false // 静态状态
    }
}
```

### 2. 启用动画
```qml
WaveBarChart {
    id: myWaveBar
    animationEnabled: true // 动态状态

    // 可选自定义参数
    barWidth: 10        // 柱子宽度（默认：10）
    barSpacing: 3       // 柱子间隔（默认：3）
    maxHeight: 30       // 最大高度（默认：30）
    minHeight: 5        // 最小高度（默认：5）
}
```

### 3. 动态控制
```qml
WaveBarChart {
    id: waveBar
    animationEnabled: isPlaying // 绑定到播放状态
}

// 通过按钮控制
Button {
    onClicked: {
        isPlaying = !isPlaying
        waveBar.toggleAnimation(isPlaying)
    }
}
```

## 属性说明

| 属性名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `animationEnabled` | bool | false | 动画开关：true=动态，false=静态 |
| `barWidth` | int | 10 | 柱子宽度（像素） |
| `barSpacing` | int | 3 | 柱子间隔（像素） |
| `maxHeight` | int | 30 | 柱子最大高度（像素） |
| `minHeight` | int | 5 | 柱子最小高度（像素） |
| `staticHeights` | array | [10, 30, 20] | 静态状态下的高度值 |

## 方法说明

| 方法名 | 参数 | 说明 |
|--------|------|------|
| `toggleAnimation(bool enable)` | enable: true/false | 启用/禁用动画 |
| `startWaveAnimation()` | 无 | 启动波浪动画 |
| `stopWaveAnimation()` | 无 | 停止波浪动画，恢复静态 |

## 在你的项目中使用

### 与播放状态绑定
```qml
// 在 Oran7MusicPlaylistView.qml 中
WaveBarChart {
    id: playingIndicator
    anchors.right: parent.right
    anchors.rightMargin: 20
    anchors.verticalCenter: titleFlow.verticalCenter
    animationEnabled: root.isPlaying // 绑定播放状态
}
```

### 自定义样式
```qml
WaveBarChart {
    id: customWave
    barWidth: 15        // 更宽的柱子
    barSpacing: 5       // 更大的间隔
    maxHeight: 40       // 更高的柱子
    minHeight: 10       // 更高的底部

    // 如果需要改变颜色，可以修改源文件中的柱子颜色
}
```

## 文件位置
- 主组件：`Src/Components/WaveBarChart.qml`
- 演示示例：`Src/Components/WaveBarChartDemo.qml`
- 使用说明：`Src/Components/WaveBarChart_README.md`

## 技术细节
- 使用 `SequentialAnimation` 和 `ParallelAnimation` 实现波浪效果
- 柱子位置通过锚点和计算公式精确定位
- 动画使用 `Easing.InOutSine` 缓动函数，平滑过渡
- 支持 `Behavior` 平滑过渡效果

## 注意事项
1. 确保 `WaveBarChart.qml` 与调用文件在同一目录或在导入路径中
2. 修改静态高度需要修改 `staticHeights` 数组
3. 动画循环时间可通过修改 `SequentialAnimation` 的持续时间调整