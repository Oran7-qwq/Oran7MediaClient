#include "Oran7AudioVolumeRamp.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

int checks = 0;

void check(bool ok, const char* message)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
    ++checks;
}

void testUnityIsCopy()
{
    Oran7AudioVolumeRamp ramp;
    ramp.prepare(48000);
    ramp.reset(1.0f);

    float input[64];
    float output[64];
    for (int i = 0; i < 64; ++i)
    {
        input[i] = static_cast<float>(i) * 0.01f;
        output[i] = 99.0f;
    }
    ramp.applyFloat(output, input, 64);
    for (int i = 0; i < 64; ++i)
        check(output[i] == input[i], "unity gain is bit exact");
    check(ramp.current() == 1.0f, "unity current stays 1");
}

void testRampDownTiming()
{
    Oran7AudioVolumeRamp ramp;
    ramp.prepare(48000); // 20ms => 960 samples full scale
    ramp.reset(1.0f);
    ramp.setTarget(0.0f);

    float input[960];
    float output[960];
    for (int i = 0; i < 960; ++i)
        input[i] = 1.0f;

    ramp.applyFloat(output, input, 480); // 10ms
    check(ramp.current() > 0.45f && ramp.current() < 0.55f, "half ramp after 10ms");
    ramp.applyFloat(output, input, 480); // 再 10ms
    // 浮点累加会残留极小值（约 -107dB），按听感/数值都视为已到零。
    check(std::fabs(ramp.current()) < 1e-4f, "reaches zero after 20ms");
    check(std::fabs(output[959]) < 1e-4f, "last sample muted");

    // 反向斜坡
    ramp.setTarget(1.0f);
    ramp.applyFloat(output, input, 960);
    check(std::fabs(ramp.current() - 1.0f) < 1e-6f, "reaches unity after 20ms");
}

void testS16Saturation()
{
    Oran7AudioVolumeRamp ramp;
    ramp.prepare(44100);
    ramp.reset(1.0f);

    short input[4] = {30000, -30000, 32767, -32768};
    short output[4] = {};
    ramp.applyS16(output, input, 4);
    check(output[0] == 30000, "positive s16 unchanged at unity");
    check(output[1] == -30000, "negative s16 unchanged at unity");
    check(output[2] == 32767, "max s16 unchanged");
    check(output[3] == -32768, "min s16 unchanged");

    ramp.reset(0.5f);
    ramp.applyS16(output, input, 4);
    check(output[0] == 15000, "s16 gain applied and rounded");
    check(output[1] == -15000, "negative s16 gain applied");
    check(output[2] == 16384, "positive saturation rounds");
}

void testResetAndInvalid()
{
    Oran7AudioVolumeRamp ramp;
    check(!ramp.prepared(), "new ramp not prepared");
    ramp.setTarget(0.5f);
    check(ramp.current() == 0.5f && ramp.target() == 0.5f, "target snaps before prepare");

    ramp.prepare(0);
    check(!ramp.prepared(), "zero sample rate not prepared");

    ramp.prepare(48000);
    ramp.reset(2.0f);
    check(ramp.current() == 1.0f, "reset clamps above 1");
    ramp.reset(-1.0f);
    check(ramp.current() == 0.0f, "reset clamps below 0");

    ramp.applyFloat(nullptr, nullptr, 16);
    ramp.applyS16(nullptr, nullptr, 16);
    ramp.applyFloat(nullptr, nullptr, 0);
    check(ramp.current() == 0.0f, "invalid calls leave state unchanged");

    // 静音输入仍推进斜坡
    float silence[480] = {};
    ramp.setTarget(1.0f);
    ramp.applyFloat(silence, nullptr, 480);
    check(ramp.current() > 0.45f && ramp.current() < 0.55f, "silent input advances ramp");
}

} // namespace

int main()
{
    testUnityIsCopy();
    testRampDownTiming();
    testS16Saturation();
    testResetAndInvalid();

    std::printf("audio volume ramp checks passed: %d\n", checks);
    return 0;
}
