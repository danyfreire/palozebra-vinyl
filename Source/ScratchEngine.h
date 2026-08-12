#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

// JUCE-independent core so the most important DSP can be tested by itself.
class ScratchEngine
{
public:
    void prepare(double newSampleRate, int numChannels, double historySeconds = 8.0)
    {
        sampleRate = std::max(1.0, newSampleRate);
        channels = std::max(1, numChannels);
        const auto wanted = static_cast<std::size_t>(std::ceil(sampleRate * historySeconds));
        bufferSize = std::max<std::size_t>(4096, wanted);
        ring.assign(static_cast<std::size_t>(channels), std::vector<float>(bufferSize, 0.0f));

        // Base latency gives the read head room to move both forwards and backwards.
        baseDelaySamples = std::min(bufferSize / 2, static_cast<std::size_t>(sampleRate * 0.75));
        writePos = 0;
        readPos = wrap(static_cast<double>(bufferSize - baseDelaySamples));
        currentSpeed = 1.0;
        targetSpeed = 1.0;
        prepared = true;
    }

    void reset()
    {
        for (auto& channel : ring)
            std::fill(channel.begin(), channel.end(), 0.0f);
        writePos = 0;
        readPos = wrap(static_cast<double>(bufferSize - baseDelaySamples));
        currentSpeed = targetSpeed = 1.0;
    }

    void setTargetSpeed(double speed)
    {
        targetSpeed = std::clamp(speed, -4.0, 4.0);
    }

    double getCurrentSpeed() const noexcept { return currentSpeed; }
    std::size_t getLatencySamples() const noexcept { return baseDelaySamples; }

    // inputs/outputs are arrays of channel pointers. In-place operation is supported.
    void process(const float* const* inputs, float* const* outputs, int numChannels, int numSamples)
    {
        if (!prepared || numSamples <= 0)
            return;

        const int chCount = std::min(channels, numChannels);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Smooth abrupt GUI/automation steps to avoid zipper clicks without killing scratch attack.
            const double smoothing = 1.0 - std::exp(-1.0 / (sampleRate * 0.0015));
            currentSpeed += (targetSpeed - currentSpeed) * smoothing;

            // Write incoming live audio first.
            for (int ch = 0; ch < chCount; ++ch)
                ring[static_cast<std::size_t>(ch)][writePos] = inputs[ch][sample];

            // Read the virtual record using cubic interpolation.
            for (int ch = 0; ch < chCount; ++ch)
                outputs[ch][sample] = readCubic(ch, readPos);

            readPos = wrap(readPos + currentSpeed);
            writePos = (writePos + 1) % bufferSize;

            // When the wheel is essentially back at 1x, gently servo the read head back
            // to the nominal live delay. This prevents a scratch from leaving a permanent
            // multi-second offset while keeping the manipulation natural.
            if (std::abs(targetSpeed - 1.0) < 0.0001)
                applyReturnToLiveServo();
        }
    }

private:
    double wrap(double x) const noexcept
    {
        const auto size = static_cast<double>(bufferSize);
        while (x < 0.0) x += size;
        while (x >= size) x -= size;
        return x;
    }

    std::size_t index(long long i) const noexcept
    {
        const auto n = static_cast<long long>(bufferSize);
        i %= n;
        if (i < 0) i += n;
        return static_cast<std::size_t>(i);
    }

    float readCubic(int channel, double position) const noexcept
    {
        const auto i1 = static_cast<long long>(std::floor(position));
        const float frac = static_cast<float>(position - std::floor(position));
        const auto& b = ring[static_cast<std::size_t>(channel)];

        const float y0 = b[index(i1 - 1)];
        const float y1 = b[index(i1)];
        const float y2 = b[index(i1 + 1)];
        const float y3 = b[index(i1 + 2)];

        // Catmull-Rom cubic interpolation.
        const float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        const float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float a2 = -0.5f * y0 + 0.5f * y2;
        const float a3 = y1;
        return ((a0 * frac + a1) * frac + a2) * frac + a3;
    }

    double signedDistance(double from, double to) const noexcept
    {
        double d = to - from;
        const double half = static_cast<double>(bufferSize) * 0.5;
        if (d > half) d -= static_cast<double>(bufferSize);
        if (d < -half) d += static_cast<double>(bufferSize);
        return d;
    }

    void applyReturnToLiveServo() noexcept
    {
        const double desired = wrap(static_cast<double>(writePos + bufferSize - baseDelaySamples));
        const double error = signedDistance(readPos, desired);

        // Small proportional correction. It catches up after a scratch but never jumps.
        // Clamp keeps the recovery from sounding like an absurd fast-forward.
        const double correction = std::clamp(error * 0.00035, -0.75, 1.5);
        readPos = wrap(readPos + correction);
    }

    double sampleRate = 44100.0;
    int channels = 2;
    std::size_t bufferSize = 0;
    std::size_t baseDelaySamples = 0;
    std::size_t writePos = 0;
    double readPos = 0.0;
    double currentSpeed = 1.0;
    double targetSpeed = 1.0;
    bool prepared = false;
    std::vector<std::vector<float>> ring;
};
