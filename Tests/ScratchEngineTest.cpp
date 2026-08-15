#include "ScratchEngine.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    constexpr int n = 48000;
    ScratchEngine engine;
    engine.prepare(48000.0, 1, 3.0);

    std::vector<float> input(n), output(n, 0.0f);
    for (int i = 0; i < n; ++i)
        input[i] = std::sin(2.0 * 3.141592653589793 * 440.0 * static_cast<double>(i) / 48000.0);

    const float* inPtrs[] = { input.data() };
    float* outPtrs[] = { output.data() };
    engine.setTargetSpeed(1.0);
    engine.process(inPtrs, outPtrs, 1, n);

    double energy = 0.0;
    for (float s : output)
    {
        assert(std::isfinite(s));
        energy += static_cast<double>(s) * s;
    }
    assert(energy > 1.0);

    // Reverse/stop/forward transitions must remain stable.
    engine.setTargetSpeed(-1.0);
    engine.process(inPtrs, outPtrs, 1, n / 4);
    engine.setTargetSpeed(0.0);
    engine.process(inPtrs, outPtrs, 1, n / 4);
    engine.setTargetSpeed(2.0);
    engine.process(inPtrs, outPtrs, 1, n / 4);

    for (int i = 0; i < n / 4; ++i)
        assert(std::isfinite(output[i]));

    // A recorded gesture curve can drive speed sample-by-sample.
    std::vector<float> gesture(n / 4, 1.0f);
    for (int i = 0; i < n / 16; ++i) gesture[static_cast<std::size_t>(i)] = 0.0f;
    for (int i = n / 16; i < n / 8; ++i) gesture[static_cast<std::size_t>(i)] = -1.0f;
    for (int i = n / 8; i < 3 * n / 16; ++i) gesture[static_cast<std::size_t>(i)] = 2.0f;

    engine.process(inPtrs, outPtrs, 1, n / 4, gesture.data());
    for (int i = 0; i < n / 4; ++i)
        assert(std::isfinite(output[i]));

    std::cout << "ScratchEngine smoke test passed. Latency="
              << engine.getLatencySamples() << " samples\n";
    return 0;
}
