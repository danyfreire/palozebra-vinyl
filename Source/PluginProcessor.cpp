#include "PluginProcessor.h"
#include "PluginEditor.h"

PalozebraVinylAudioProcessor::PalozebraVinylAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    speedParam = parameters.getRawParameterValue(speedParamId);
    midiOutParam = parameters.getRawParameterValue(midiOutParamId);
}

juce::AudioProcessorValueTreeState::ParameterLayout PalozebraVinylAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Skew around 1x so the useful scratch range has more resolution near stop/normal speed.
    juce::NormalisableRange<float> speedRange(-4.0f, 4.0f, 0.0001f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ speedParamId, 1 }, "Wheel Speed", speedRange, 1.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction([](float v, int)
        {
            if (std::abs(v) < 0.005f) return juce::String("STOP");
            return juce::String(v, 2) + "x";
        })));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ midiOutParamId, 1 }, "MIDI CC Out", true));

    return layout;
}

void PalozebraVinylAudioProcessor::prepareToPlay(double sampleRate, int)
{
    engine.prepare(sampleRate, getTotalNumInputChannels(), 8.0);
    setLatencySamples(static_cast<int>(engine.getLatencySamples()));
}

bool PalozebraVinylAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void PalozebraVinylAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numIn = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();
    for (auto ch = numIn; ch < numOut; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    const float speed = speedParam != nullptr ? speedParam->load() : 1.0f;
    engine.setTargetSpeed(speed);

    std::array<const float*, 2> inputs{};
    std::array<float*, 2> outputs{};
    const int channels = juce::jmin(2, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        inputs[static_cast<std::size_t>(ch)] = buffer.getReadPointer(ch);
        outputs[static_cast<std::size_t>(ch)] = buffer.getWritePointer(ch);
    }

    engine.process(inputs.data(), outputs.data(), channels, buffer.getNumSamples());

    // Optional gesture telemetry as CC74. This is deliberately secondary to host automation.
    if (midiOutParam != nullptr && midiOutParam->load() > 0.5f)
    {
        const auto normalised = juce::jlimit(0.0f, 1.0f, (speed + 4.0f) / 8.0f);
        const int ccValue = juce::jlimit(0, 127, juce::roundToInt(normalised * 127.0f));
        if (ccValue != lastMidiValue)
        {
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, ccValue), 0);
            lastMidiValue = ccValue;
        }
    }
}

void PalozebraVinylAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void PalozebraVinylAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* PalozebraVinylAudioProcessor::createEditor()
{
    return new PalozebraVinylAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PalozebraVinylAudioProcessor();
}
