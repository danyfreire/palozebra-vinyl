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

    // Timeline recorder. REC arms first; touching the platter places the take on the DAW timeline.
    void armGestureRecording() noexcept;
    void stopGestureRecording() noexcept;
    void clearGesture() noexcept;

    void beginManualWheelTouch() noexcept;
    void endManualWheelTouch() noexcept;

    bool isGestureArmed() const noexcept { return gestureArmed.load(); }
    bool isGestureRecording() const noexcept { return gestureRecording.load(); }
    bool isTimelineGestureActive() const noexcept { return timelineGestureActive.load(); }
    bool hasGesture() const noexcept { return gestureStartValid.load() && gestureLength.load() > 0; }
    double getGestureLengthSeconds() const noexcept;
    double getGestureStartSeconds() const noexcept { return gestureStartSeconds.load(); }

    bool isHostPlaying() const noexcept { return hostPlaying.load(); }
    bool hasHostTimeline() const noexcept { return hostTimelineAvailable.load(); }

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

    // Preallocated fixed-capacity gesture storage keeps normal audio playback allocation-free.
    std::vector<float> gestureBuffer;
    std::vector<float> speedCurveScratch;
    std::atomic<std::size_t> gestureLength { 0 };
    std::atomic<double> gestureRecordPhase { 1.0 };
    std::atomic<double> gestureStartSeconds { 0.0 };
    std::atomic<bool> gestureStartValid { false };
    std::atomic<bool> gestureArmed { false };
    std::atomic<bool> gestureStartPending { false };
    std::atomic<bool> gestureRecording { false };
    std::atomic<bool> timelineGestureActive { false };
    std::atomic<bool> manualWheelTouch { false };

    // Last transport state published by the audio thread for the UI.
    std::atomic<bool> hostPlaying { false };
    std::atomic<bool> hostTimelineAvailable { false };

    // Short spin-up/spin-down envelope generated at audio rate on mouse release.
    std::atomic<bool> releaseActive { false };
    std::atomic<float> releaseStartSpeed { 1.0f };
    std::atomic<int> releaseTotalSamples { 1 };
    std::atomic<int> releaseSamplesProcessed { 0 };

    std::atomic<float> effectiveSpeed { 1.0f };
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PalozebraVinylAudioProcessor)
};
