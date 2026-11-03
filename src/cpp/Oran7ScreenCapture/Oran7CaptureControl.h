#pragma once
#include <atomic>

// GUI 与采集线程只共享取消/诊断意图；控制器不跨线程访问 worker QObject。
struct Oran7CaptureControl
{
    std::atomic_bool stopRequested{false};
    std::atomic_bool debugLog{false};
};
