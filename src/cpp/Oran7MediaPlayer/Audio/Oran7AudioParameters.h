#pragma once

#include <cstdint>
#include <type_traits>

/**
 * @brief 播放器音效参数快照。
 *
 * 约束：
 * - 固定大小、可平凡复制；只使用标准定宽类型与 float。
 * - 不含 Qt 类型、字符串、容器、指针，可在音频线程安全读取。
 * - 由 GUI 线程发布，音频线程按块取最新完整快照。
 */
struct Oran7AudioParameters
{
    static constexpr int kBandCount = 10;
    static constexpr std::uint32_t kVersion = 2;

    std::uint32_t version = kVersion;
    ///< 每次发布递增；用于丢弃过期快照与测试并发一致性。
    std::uint32_t revision = 0;
    ///< 0=旁路，1=启用；不使用 bool，避免跨编译器布局差异。
    std::uint32_t enabled = 0;

    float preampDb = 0.0f;
    float outputGainDb = 0.0f;
    float limiterCeilingDb = -1.0f;
    float bandGainsDb[kBandCount] = {};

    // A2：滤波、低音架、立体声宽度/交叉馈送、压缩。
    float highPassHz = 20.0f;       // <= 20 视为关闭
    float lowPassHz = 20000.0f;     // >= 20000 视为关闭
    float bassBoostDb = 0.0f;
    float bassFreqHz = 120.0f;
    float stereoWidth = 1.0f;       // 0…2，1 为中性
    std::uint32_t crossfeed = 0;    // 0 关 / 1 轻 / 2 标准 / 3 强
    std::uint32_t compressorEnabled = 0;
    float compressorThresholdDb = -18.0f;
    float compressorRatio = 3.0f;
    float compressorAttackMs = 10.0f;
    float compressorReleaseMs = 150.0f;
    float compressorKneeDb = 3.0f;
    float compressorMakeupDb = 0.0f;

    bool isEnabled() const noexcept { return enabled != 0; }
};

static_assert(std::is_trivially_copyable_v<Oran7AudioParameters>,
              "Oran7AudioParameters 必须可平凡复制，才能跨线程按值传递");
static_assert(std::is_standard_layout_v<Oran7AudioParameters>,
              "Oran7AudioParameters 必须是标准布局");
