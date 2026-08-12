#pragma once

#include <JuceHeader.h>
#include "ScratchEngine.h"

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

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static constexpr const char* speedParamId = "speed";
    static constexpr const char* midiOutParamId = "midiOut";

private:
    ScratchEngine engine;
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* speedParam = nullptr;
    std::atomic<float>* midiOutParam = nullptr;
    int lastMidiValue = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PalozebraVinylAudioProcessor)
};
