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
    bool producesMidi() const override { return true; }
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

    // Internal gesture recorder. This is intentionally independent of host automation.
    void startGestureRecording() noexcept;
    void stopGestureRecording() noexcept;
    void startGesturePlayback() noexcept;
    void stopGesturePlayback() noexcept;
    void clearGesture() noexcept;

    bool isGestureRecording() const noexcept { return gestureRecording.load(); }
    bool isGesturePlaying() const noexcept { return gesturePlaying.load(); }
    bool hasGesture() const noexcept { return gestureLength.load() > 0; }
    double getGestureLengthSeconds() const noexcept;

    float getEffectiveSpeed() const noexcept { return effectiveSpeed.load(); }
    float getSourceLevel() const noexcept { return sourceLevel.load(); }
    bool isUsingSourceInput() const noexcept { return usingSourceInput.load(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static constexpr const char* speedParamId = "speed";
    static constexpr const char* midiOutParamId = "midiOut";

private:
    static constexpr double maxGestureSeconds = 60.0;

    ScratchEngine engine;
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* speedParam = nullptr;
    std::atomic<float>* midiOutParam = nullptr;

    std::vector<float> gestureBuffer;
    std::vector<float> speedCurveScratch;
    std::atomic<std::size_t> gestureLength { 0 };
    std::atomic<std::size_t> gesturePosition { 0 };
    std::atomic<bool> gestureRecording { false };
    std::atomic<bool> gesturePlaying { false };

    std::atomic<float> effectiveSpeed { 1.0f };
    std::atomic<float> sourceLevel { 0.0f };
    std::atomic<bool> usingSourceInput { false };
    double currentSampleRate = 44100.0;
    int lastMidiValue = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PalozebraVinylAudioProcessor)
};
