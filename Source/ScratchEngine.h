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

    // Re-align the virtual record with the current live input position without clearing history.
    // This is used only when the host transport explicitly seeks/rewinds/loops, not on wheel release.
    void syncToLive() noexcept
    {
        if (!prepared || bufferSize == 0)
            return;

        readPos = wrap(static_cast<double>(writePos) - static_cast<double>(baseDelaySamples));
        currentSpeed = 1.0;
        targetSpeed = 1.0;
    }

    void setTargetSpeed(double speed)
    {
        targetSpeed = std::clamp(speed, -4.0, 4.0);
    }

    double getCurrentSpeed() const noexcept { return currentSpeed; }
    std::size_t getLatencySamples() const noexcept { return baseDelaySamples; }

    // inputs/outputs are arrays of channel pointers. In-place operation is supported.
    // If speedCurve is supplied, each sample becomes the platter target speed for that frame.
    // This lets the plug-in replay a recorded wheel gesture sample-accurately.
    void process(const float* const* inputs,
                 float* const* outputs,
                 int numChannels,
                 int numSamples,
                 const float* speedCurve = nullptr)
    {
        if (!prepared || numSamples <= 0)
            return;

        const int chCount = std::min(channels, numChannels);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            if (speedCurve != nullptr)
                targetSpeed = std::clamp(static_cast<double>(speedCurve[sample]), -4.0, 4.0);

            // Smooth abrupt GUI/automation steps to avoid zipper clicks without killing scratch attack.
            const double smoothing = 1.0 - std::exp(-1.0 / (sampleRate * 0.0015));
            currentSpeed += (targetSpeed - currentSpeed) * smoothing;

            // Write incoming live audio first.
            for (int ch = 0; ch < chCount; ++ch)
                ring[static_cast<std::size_t>(ch)][writePos] = inputs[ch][sample];

            // Read the virtual record using cubic interpolation.
            for (int ch = 0; ch < chCount; ++ch)
                outputs[ch][sample] = readCubic(ch, readPos);

            // The platter moves only at the requested physical speed. When released at 1x,
            // it does not fast-forward to catch the DAW timeline.
            readPos = wrap(readPos + currentSpeed);
            writePos = (writePos + 1) % bufferSize;
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
