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

    // The scratch engine processes the selected source bus, which is constrained to match
    // the mono/stereo main output layout.
    engine.prepare(currentSampleRate, juce::jmax(1, getMainBusNumOutputChannels()), 8.0);
    setLatencySamples(static_cast<int>(engine.getLatencySamples()));

    const auto maxGestureSamples = static_cast<std::size_t>(std::ceil(currentSampleRate * maxGestureSeconds));
    gestureBuffer.assign(maxGestureSamples, 1.0f);

    // Hosts may occasionally deliver blocks larger than the estimate passed here. Reserve
    // a generous scratch curve up-front and grow only if an unusual host requires it.
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

    auto mainInput = getBusBuffer(buffer, true, 0);
    auto output = getBusBuffer(buffer, false, 0);

    const bool sourceBusEnabled = getBusCount(true) > 1
        && getBus(true, 1) != nullptr
        && getBus(true, 1)->isEnabled();

    auto selectedInput = mainInput;
    if (sourceBusEnabled)
    {
        auto sourceInput = getBusBuffer(buffer, true, 1);
        if (sourceInput.getNumChannels() == output.getNumChannels())
            selectedInput = sourceInput;
    }

    const bool usingAux = sourceBusEnabled && selectedInput.getNumChannels() == output.getNumChannels();
    usingSourceInput.store(usingAux);

    float peak = 0.0f;
    for (int ch = 0; ch < selectedInput.getNumChannels(); ++ch)
        peak = juce::jmax(peak, selectedInput.getMagnitude(ch, 0, selectedInput.getNumSamples()));
    sourceLevel.store(peak);

    const int numSamples = output.getNumSamples();
    const int channels = juce::jmin({ 2, selectedInput.getNumChannels(), output.getNumChannels() });

    if (channels <= 0 || numSamples <= 0)
    {
        output.clear();
        return;
    }

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
                // A take ends on normal platter speed. There is no automatic catch-up.
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

    std::array<const float*, 2> inputs{};
    std::array<float*, 2> outputs{};
    for (int ch = 0; ch < channels; ++ch)
    {
        inputs[static_cast<std::size_t>(ch)] = selectedInput.getReadPointer(ch);
        outputs[static_cast<std::size_t>(ch)] = output.getWritePointer(ch);
    }

    engine.process(inputs.data(),
                   outputs.data(),
                   channels,
                   numSamples,
                   useSpeedCurve ? speedCurveScratch.data() : nullptr);

    effectiveSpeed.store(static_cast<float>(engine.getCurrentSpeed()));

    // Optional telemetry as CC74 remains secondary; the internal gesture recorder no longer
    // depends on MIDI or host automation.
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
