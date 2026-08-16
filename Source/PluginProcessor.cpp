#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <cmath>

PalozebraVinylAudioProcessor::PalozebraVinylAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout()),
      gestureBuffer(maxGesturePoints, 1.0f),
      speedCurveScratch(65536, 1.0f)
{
    speedParam = parameters.getRawParameterValue(speedParamId);
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

    return layout;
}

void PalozebraVinylAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax(1.0, sampleRate);

    engine.prepare(currentSampleRate, juce::jmax(1, getMainBusNumOutputChannels()), 8.0);
    setLatencySamples(static_cast<int>(engine.getLatencySamples()));

    const auto wantedScratch = static_cast<std::size_t>(juce::jmax(samplesPerBlock, 65536));
    if (speedCurveScratch.size() < wantedScratch)
        speedCurveScratch.resize(wantedScratch, 1.0f);

    // Keep any gesture restored from the DAW project, but never resume a transient transport state.
    gestureRecording.store(false);
    gesturePlaying.store(false);
    gesturePlayPosition.store(0.0);
    gestureRecordPhase.store(1.0);
    releaseActive.store(false);
    effectiveSpeed.store(1.0f);
}

bool PalozebraVinylAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void PalozebraVinylAudioProcessor::startGestureRecording() noexcept
{
    gesturePlaying.store(false);
    gesturePlayPosition.store(0.0);
    gestureLength.store(0);
    gestureRecordPhase.store(1.0); // capture the very first audio sample as the first gesture point
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
    cancelWheelRelease();
    gesturePlayPosition.store(0.0);
    gesturePlaying.store(true);
}

void PalozebraVinylAudioProcessor::stopGesturePlayback() noexcept
{
    gesturePlaying.store(false);
    gesturePlayPosition.store(0.0);
    beginWheelRelease();
}

void PalozebraVinylAudioProcessor::clearGesture() noexcept
{
    gestureRecording.store(false);
    gesturePlaying.store(false);
    gesturePlayPosition.store(0.0);
    gestureLength.store(0);
}

double PalozebraVinylAudioProcessor::getGestureLengthSeconds() const noexcept
{
    return static_cast<double>(gestureLength.load()) / static_cast<double>(gestureRateHz);
}

void PalozebraVinylAudioProcessor::beginWheelRelease() noexcept
{
    const float start = juce::jlimit(-4.0f, 4.0f, effectiveSpeed.load());
    const float distance = std::abs(1.0f - start);

    if (distance < 0.01f)
    {
        releaseActive.store(false);
        return;
    }

    // Short platter-motor feel: ~45 ms near 1x, ~80 ms from stop, ~105 ms from reverse.
    // This changes speed/pitch only; it never changes gain and never catches up to the DAW timeline.
    const float reverseAmount = juce::jlimit(0.0f, 1.0f, -start);
    const float durationMs = juce::jlimit(40.0f, 120.0f,
                                         45.0f
                                         + 35.0f * juce::jmin(distance, 1.0f)
                                         + 25.0f * reverseAmount);

    releaseStartSpeed.store(start);
    releaseTotalSamples.store(juce::jmax(1, juce::roundToInt(currentSampleRate * durationMs * 0.001)));
    releaseSamplesProcessed.store(0);
    releaseActive.store(true);
}

void PalozebraVinylAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int channels = juce::jmin(2, juce::jmin(numIn, numOut));

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear(ch, 0, numSamples);

    if (channels <= 0 || numSamples <= 0)
        return;

    // Normal hosts stay well below this. Grow only for an unusually large offline block.
    if (speedCurveScratch.size() < static_cast<std::size_t>(numSamples))
        speedCurveScratch.resize(static_cast<std::size_t>(numSamples), 1.0f);

    const bool playing = gesturePlaying.load();
    const bool recording = gestureRecording.load();
    const auto gestureCount = gestureLength.load();
    const double gestureStep = static_cast<double>(gestureRateHz) / currentSampleRate;

    double playPosition = gesturePlayPosition.load();
    double recordPhase = gestureRecordPhase.load();
    std::size_t recordLength = gestureLength.load();

    bool releasing = releaseActive.load();
    const float releaseStart = releaseStartSpeed.load();
    const int releaseTotal = juce::jmax(1, releaseTotalSamples.load());
    int releaseProcessed = releaseSamplesProcessed.load();

    const float liveParameterSpeed = speedParam != nullptr ? speedParam->load() : 1.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float commandedSpeed = liveParameterSpeed;

        if (playing)
        {
            if (gestureCount == 0 || playPosition >= static_cast<double>(gestureCount))
            {
                commandedSpeed = 1.0f;
                gesturePlaying.store(false);
            }
            else
            {
                const auto i0 = static_cast<std::size_t>(std::floor(playPosition));
                const auto i1 = juce::jmin(i0 + 1, gestureCount - 1);
                const float frac = static_cast<float>(playPosition - std::floor(playPosition));
                commandedSpeed = gestureBuffer[i0] + (gestureBuffer[i1] - gestureBuffer[i0]) * frac;
                playPosition += gestureStep;
            }
        }
        else if (releasing)
        {
            const float progress = juce::jlimit(0.0f, 1.0f,
                static_cast<float>(releaseProcessed) / static_cast<float>(releaseTotal));

            // Cubic ease-out behaves like a small motor grabbing the platter: quick initial pull,
            // then a soft settle at exactly 1x. Pitch follows this curve naturally.
            const float oneMinus = 1.0f - progress;
            const float eased = 1.0f - oneMinus * oneMinus * oneMinus;
            commandedSpeed = releaseStart + (1.0f - releaseStart) * eased;

            ++releaseProcessed;
            if (releaseProcessed >= releaseTotal)
            {
                commandedSpeed = 1.0f;
                releasing = false;
                releaseActive.store(false);
            }
        }

        speedCurveScratch[static_cast<std::size_t>(sample)] = commandedSpeed;

        if (recording)
        {
            if (recordPhase >= 1.0)
            {
                if (recordLength < gestureBuffer.size())
                {
                    gestureBuffer[recordLength++] = commandedSpeed;
                    recordPhase -= 1.0;
                }
                else
                {
                    gestureRecording.store(false);
                }
            }

            recordPhase += gestureStep;
        }
    }

    gesturePlayPosition.store(playPosition);
    gestureRecordPhase.store(recordPhase);
    if (recording)
        gestureLength.store(recordLength);

    releaseSamplesProcessed.store(releaseProcessed);

    std::array<const float*, 2> inputs{};
    std::array<float*, 2> outputs{};
    for (int ch = 0; ch < channels; ++ch)
    {
        inputs[static_cast<std::size_t>(ch)] = buffer.getReadPointer(ch);
        outputs[static_cast<std::size_t>(ch)] = buffer.getWritePointer(ch);
    }

    engine.process(inputs.data(), outputs.data(), channels, numSamples, speedCurveScratch.data());
    effectiveSpeed.store(static_cast<float>(engine.getCurrentSpeed()));
}

void PalozebraVinylAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();

    const auto length = juce::jmin(gestureLength.load(), gestureBuffer.size());
    state.setProperty("gestureLength", static_cast<int>(length), nullptr);
    state.setProperty("gestureRateHz", gestureRateHz, nullptr);

    if (length > 0)
    {
        juce::StringArray values;
        for (std::size_t i = 0; i < length; ++i)
            values.add(juce::String(gestureBuffer[i], 5));
        state.setProperty("gestureData", values.joinIntoString(","), nullptr);
    }
    else
    {
        state.removeProperty("gestureData", nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void PalozebraVinylAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        if (!state.isValid())
            return;

        gestureRecording.store(false);
        gesturePlaying.store(false);
        releaseActive.store(false);
        gesturePlayPosition.store(0.0);

        const int savedLength = static_cast<int>(state.getProperty("gestureLength", 0));
        const auto encoded = state.getProperty("gestureData").toString();

        std::size_t restored = 0;
        if (savedLength > 0 && encoded.isNotEmpty())
        {
            juce::StringArray values;
            values.addTokens(encoded, ",", "");

            const auto wanted = juce::jmin<std::size_t>(
                static_cast<std::size_t>(juce::jmax(0, savedLength)),
                juce::jmin<std::size_t>(gestureBuffer.size(), static_cast<std::size_t>(values.size())));

            for (; restored < wanted; ++restored)
                gestureBuffer[restored] = static_cast<float>(values[static_cast<int>(restored)].getDoubleValue());
        }

        gestureLength.store(restored);
        parameters.replaceState(state);
    }
}

juce::AudioProcessorEditor* PalozebraVinylAudioProcessor::createEditor()
{
    return new PalozebraVinylAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PalozebraVinylAudioProcessor();
}
