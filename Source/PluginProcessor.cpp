#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <array>
#include <algorithm>

PalozebraVinylAudioProcessor::PalozebraVinylAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
        .withInput("Source In", juce::AudioChannelSet::stereo(), false)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    speedParam = parameters.getRawParameterValue(speedParamId);
    midiOutParam = parameters.getRawParameterValue(midiOutParamId);
}

juce::AudioProcessorValueTreeState::ParameterLayout PalozebraVinylAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

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

void PalozebraVinylAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax(1.0, sampleRate);

    engine.prepare(currentSampleRate, juce::jmax(1, getMainBusNumOutputChannels()), 8.0);
    setLatencySamples(static_cast<int>(engine.getLatencySamples()));

    const auto maxGestureSamples = static_cast<std::size_t>(std::ceil(currentSampleRate * maxGestureSeconds));
    gestureBuffer.assign(maxGestureSamples, 1.0f);

    // JUCE notes that hosts may provide blocks larger than the estimate passed to prepareToPlay.
    // Reserve generously so normal playback never needs to allocate in the audio callback.
    const auto scratchSize = static_cast<std::size_t>(juce::jmax(samplesPerBlock, 65536));
    speedCurveScratch.assign(scratchSize, 1.0f);

    clearGesture();
    effectiveSpeed.store(1.0f);
    sourceLevel.store(0.0f);
}

bool PalozebraVinylAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (mainIn.isDisabled() || out.isDisabled())
        return false;

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (mainIn != out)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sourceIn = layouts.getChannelSet(true, 1);
        if (!sourceIn.isDisabled() && sourceIn != out)
            return false;
    }

    return true;
}

void PalozebraVinylAudioProcessor::startGestureRecording() noexcept
{
    gesturePlaying.store(false);
    gestureRecording.store(false);
    gesturePosition.store(0);
    gestureLength.store(0);
    gestureRecording.store(true);
}

void PalozebraVinylAudioProcessor::stopGestureRecording() noexcept
{
    gestureRecording.store(false);
}

void PalozebraVinylAudioProcessor::startGesturePlayback() noexcept
{
    if (!hasGesture())
        return;

    gestureRecording.store(false);
    gesturePlaying.store(false);
    gesturePosition.store(0);
    gesturePlaying.store(true);
}

void PalozebraVinylAudioProcessor::stopGesturePlayback() noexcept
{
    gesturePlaying.store(false);
    gesturePosition.store(0);
}

void PalozebraVinylAudioProcessor::clearGesture() noexcept
{
    gestureRecording.store(false);
    gesturePlaying.store(false);
    gesturePosition.store(0);
    gestureLength.store(0);
}

double PalozebraVinylAudioProcessor::getGestureLengthSeconds() const noexcept
{
    return static_cast<double>(gestureLength.load()) / juce::jmax(1.0, currentSampleRate);
}

void PalozebraVinylAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    auto* mainInputBus = getBus(true, 0);
    auto* outputBus = getBus(false, 0);
    auto* sourceBus = getBusCount(true) > 1 ? getBus(true, 1) : nullptr;

    if (mainInputBus == nullptr || outputBus == nullptr)
        return;

    const bool useAux = sourceBus != nullptr
        && sourceBus->isEnabled()
        && sourceBus->getNumberOfChannels() == outputBus->getNumberOfChannels();

    auto* selectedInputBus = useAux ? sourceBus : mainInputBus;
    usingSourceInput.store(useAux);

    const int numSamples = buffer.getNumSamples();
    const int channels = juce::jmin({ 2,
                                      selectedInputBus->getNumberOfChannels(),
                                      outputBus->getNumberOfChannels() });

    if (channels <= 0 || numSamples <= 0)
        return;

    std::array<const float*, 2> inputs{};
    std::array<float*, 2> outputs{};

    float peak = 0.0f;
    for (int ch = 0; ch < channels; ++ch)
    {
        const int inputIndex = selectedInputBus->getChannelIndexInProcessBlockBuffer(ch);
        const int outputIndex = outputBus->getChannelIndexInProcessBlockBuffer(ch);

        inputs[static_cast<std::size_t>(ch)] = buffer.getReadPointer(inputIndex);
        outputs[static_cast<std::size_t>(ch)] = buffer.getWritePointer(outputIndex);

        const auto* input = inputs[static_cast<std::size_t>(ch)];
        for (int sample = 0; sample < numSamples; ++sample)
            peak = juce::jmax(peak, std::abs(input[sample]));
    }
    sourceLevel.store(peak);

    if (speedCurveScratch.size() < static_cast<std::size_t>(numSamples))
        speedCurveScratch.resize(static_cast<std::size_t>(numSamples), 1.0f);

    const bool playing = gesturePlaying.load();
    const bool recording = gestureRecording.load();
    bool useSpeedCurve = false;
    float commandedSpeed = speedParam != nullptr ? speedParam->load() : 1.0f;

    if (playing)
    {
        useSpeedCurve = true;
        auto position = gesturePosition.load();
        const auto length = gestureLength.load();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            if (position < length)
            {
                speedCurveScratch[static_cast<std::size_t>(sample)] = gestureBuffer[position++];
            }
            else
            {
                speedCurveScratch[static_cast<std::size_t>(sample)] = 1.0f;
                gesturePlaying.store(false);
            }
        }

        gesturePosition.store(position);
        commandedSpeed = speedCurveScratch[static_cast<std::size_t>(numSamples - 1)];
    }
    else if (recording)
    {
        useSpeedCurve = true;
        auto length = gestureLength.load();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float liveSpeed = speedParam != nullptr ? speedParam->load() : 1.0f;
            speedCurveScratch[static_cast<std::size_t>(sample)] = liveSpeed;

            if (length < gestureBuffer.size())
                gestureBuffer[length++] = liveSpeed;
            else
                gestureRecording.store(false);
        }

        gestureLength.store(length);
        commandedSpeed = speedCurveScratch[static_cast<std::size_t>(numSamples - 1)];
    }
    else
    {
        engine.setTargetSpeed(commandedSpeed);
    }

    engine.process(inputs.data(),
                   outputs.data(),
                   channels,
                   numSamples,
                   useSpeedCurve ? speedCurveScratch.data() : nullptr);

    effectiveSpeed.store(static_cast<float>(engine.getCurrentSpeed()));

    // Optional telemetry as CC74. Internal gesture recording does not depend on MIDI or
    // on the DAW's automation system.
    if (midiOutParam != nullptr && midiOutParam->load() > 0.5f)
    {
        const auto normalised = juce::jlimit(0.0f, 1.0f, (commandedSpeed + 4.0f) / 8.0f);
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
