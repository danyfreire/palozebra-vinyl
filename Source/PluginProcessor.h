#pragma once

#include <JuceHeader.h>
#include "ScratchEngine.h"

#include <atomic>
#include <cstddef>
#include <vector>

class PalozebraVinylAudioProcessor final : public juce::AudioProcessor
{
public:
    PalozebraVinylAudioProcessor();
    ~PalozebraVinylAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }
    std::atomic<float>* getSpeedParameter() noexcept { return speedParam; }

    // Internal gesture recorder: captures the platter movement, not the audio.
    void startGestureRecording() noexcept;
    void stopGestureRecording() noexcept;
    void startGesturePlayback() noexcept;
    void stopGesturePlayback() noexcept;
    void clearGesture() noexcept;

    bool isGestureRecording() const noexcept { return gestureRecording.load(); }
    bool isGesturePlaying() const noexcept { return gesturePlaying.load(); }
    bool hasGesture() const noexcept { return gestureLength.load() > 0; }
    double getGestureLengthSeconds() const noexcept;

    // Physical release: a short motor-like pitch bend back to 1x, never a timeline catch-up.
    void beginWheelRelease() noexcept;
    void cancelWheelRelease() noexcept { releaseActive.store(false); }

    float getEffectiveSpeed() const noexcept { return effectiveSpeed.load(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static constexpr const char* speedParamId = "speed";

private:
    static constexpr double maxGestureSeconds = 60.0;
    static constexpr int gestureRateHz = 200;
    static constexpr std::size_t maxGesturePoints =
        static_cast<std::size_t>(maxGestureSeconds * gestureRateHz) + 2;

    ScratchEngine engine;
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* speedParam = nullptr;

    // Preallocated fixed-capacity gesture storage keeps the audio callback allocation-free.
    std::vector<float> gestureBuffer;
    std::vector<float> speedCurveScratch;
    std::atomic<std::size_t> gestureLength { 0 };
    std::atomic<double> gesturePlayPosition { 0.0 };
    std::atomic<double> gestureRecordPhase { 1.0 };
    std::atomic<bool> gestureRecording { false };
    std::atomic<bool> gesturePlaying { false };

    // Short spin-up/spin-down envelope generated at audio rate on mouse release.
    std::atomic<bool> releaseActive { false };
    std::atomic<float> releaseStartSpeed { 1.0f };
    std::atomic<int> releaseTotalSamples { 1 };
    std::atomic<int> releaseSamplesProcessed { 0 };

    std::atomic<float> effectiveSpeed { 1.0f };
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PalozebraVinylAudioProcessor)
};
