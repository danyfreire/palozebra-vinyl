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

    // Keep a placed take restored from the project, but never resume transient transport states.
    gestureArmed.store(false);
    gestureStartPending.store(false);
    gestureRecording.store(false);
    timelineGestureActive.store(false);
    manualWheelTouch.store(false);
    gestureRecordPhase.store(1.0);
    releaseActive.store(false);
    effectiveSpeed.store(1.0f);
    hostPlaying.store(false);
    hostTimelineAvailable.store(false);
    lastTimelineSeconds = 0.0;
    lastTimelineBlockSamples = 0;
    lastTimelineValid = false;
}

bool PalozebraVinylAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void PalozebraVinylAudioProcessor::armGestureRecording() noexcept
{
    gestureRecording.store(false);
    gestureStartPending.store(false);
    gestureArmed.store(true);
    timelineGestureActive.store(false);
}

void PalozebraVinylAudioProcessor::stopGestureRecording() noexcept
{
    gestureArmed.store(false);
    gestureStartPending.store(false);
    gestureRecording.store(false);
}

void PalozebraVinylAudioProcessor::clearGesture() noexcept
{
    gestureArmed.store(false);
    gestureStartPending.store(false);
    gestureRecording.store(false);
    timelineGestureActive.store(false);
    gestureLength.store(0);
    gestureStartSeconds.store(0.0);
    gestureStartValid.store(false);
}

void PalozebraVinylAudioProcessor::beginManualWheelTouch() noexcept
{
    manualWheelTouch.store(true);
    cancelWheelRelease();

    // REC is an arm button. The first actual platter touch requests timeline placement.
    if (gestureArmed.load())
        gestureStartPending.store(true);
}

void PalozebraVinylAudioProcessor::endManualWheelTouch() noexcept
{
    manualWheelTouch.store(false);
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

    if (speedCurveScratch.size() < static_cast<std::size_t>(numSamples))
        speedCurveScratch.resize(static_cast<std::size_t>(numSamples), 1.0f);

    // Read the host timeline for this callback. Seconds are preferred because the saved take
    // remains meaningful if the project is reopened at another sample rate.
    bool timelineValid = false;
    bool transportPlaying = false;
    double blockStartSeconds = 0.0;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            transportPlaying = position->getIsPlaying();

            if (const auto seconds = position->getTimeInSeconds())
            {
                blockStartSeconds = *seconds;
                timelineValid = true;
            }
            else if (const auto samples = position->getTimeInSamples())
            {
                blockStartSeconds = static_cast<double>(*samples) / currentSampleRate;
                timelineValid = true;
            }
        }
    }

    hostPlaying.store(transportPlaying);
    hostTimelineAvailable.store(timelineValid);

    // A host seek/rewind/loop is an explicit timeline jump. Re-align the record there so a
    // recorded scratch manipulates the same source material on every pass. Normal wheel release
    // still never catches up automatically.
    if (timelineValid && transportPlaying)
    {
        bool transportJump = !lastTimelineValid;

        if (lastTimelineValid)
        {
            const double expected = lastTimelineSeconds
                                  + static_cast<double>(lastTimelineBlockSamples) / currentSampleRate;
            const double blockSeconds = static_cast<double>(juce::jmax(lastTimelineBlockSamples, numSamples))
                                      / currentSampleRate;
            const double tolerance = juce::jmax(0.020, blockSeconds * 4.0);
            transportJump = std::abs(blockStartSeconds - expected) > tolerance;
        }

        if (transportJump)
        {
            engine.syncToLive();
            releaseActive.store(false);
            timelineGestureActive.store(false);
        }

        lastTimelineSeconds = blockStartSeconds;
        lastTimelineBlockSamples = numSamples;
        lastTimelineValid = true;
    }
    else
    {
        lastTimelineValid = false;
    }

    // REC is only armed until the first platter touch. The audio thread places the take at the
    // beginning of the first block that actually sees that touch while the host is playing.
    const bool touchingNow = manualWheelTouch.load();
    if (gestureStartPending.load() && gestureArmed.load())
    {
        if (touchingNow && timelineValid && transportPlaying)
        {
            gestureLength.store(0);
            gestureRecordPhase.store(1.0);
            gestureStartSeconds.store(blockStartSeconds);
            gestureStartValid.store(true);
            gestureRecording.store(true);
            gestureArmed.store(false);
            gestureStartPending.store(false);
        }
        else if (!touchingNow)
        {
            // A touch while the host was stopped/unavailable must not place itself later by accident.
            gestureStartPending.store(false);
        }
    }

    const bool recording = gestureRecording.load();
    const bool armed = gestureArmed.load();
    const auto gestureCount = gestureLength.load();
    const bool placedTake = gestureStartValid.load() && gestureCount > 0;
    const double takeStartSeconds = gestureStartSeconds.load();
    const double gestureStep = static_cast<double>(gestureRateHz) / currentSampleRate;

    double recordPhase = gestureRecordPhase.load();
    std::size_t recordLength = gestureLength.load();

    bool releasing = releaseActive.load();
    const float releaseStart = releaseStartSpeed.load();
    const int releaseTotal = juce::jmax(1, releaseTotalSamples.load());
    int releaseProcessed = releaseSamplesProcessed.load();

    const float liveParameterSpeed = speedParam != nullptr ? speedParam->load() : 1.0f;

    // Existing takes stay quiet while REC is armed, so the user can replace a take without the
    // old scratch firing underneath the new performance.
    const bool allowTimelineTake = !recording
                                && !armed
                                && !touchingNow
                                && placedTake
                                && timelineValid
                                && transportPlaying;

    bool anyTimelineSample = false;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float commandedSpeed = liveParameterSpeed;
        bool timelineOwnsSample = false;

        if (allowTimelineTake)
        {
            const double absoluteSeconds = blockStartSeconds
                                         + static_cast<double>(sample) / currentSampleRate;
            const double localPoint = (absoluteSeconds - takeStartSeconds)
                                    * static_cast<double>(gestureRateHz);

            if (localPoint >= 0.0 && localPoint < static_cast<double>(gestureCount))
            {
                const auto i0 = static_cast<std::size_t>(std::floor(localPoint));
                const auto i1 = juce::jmin(i0 + 1, gestureCount - 1);
                const float frac = static_cast<float>(localPoint - std::floor(localPoint));
                commandedSpeed = gestureBuffer[i0] + (gestureBuffer[i1] - gestureBuffer[i0]) * frac;
                timelineOwnsSample = true;
                anyTimelineSample = true;
            }
        }

        if (timelineOwnsSample)
        {
            // Timeline playback is deterministic and supersedes a stale manual-release envelope.
            releasing = false;
            releaseActive.store(false);
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

        // While recording, capture exactly the speed/pitch command being heard, including the
        // short motor release after the user lets go of the platter.
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

    timelineGestureActive.store(anyTimelineSample);
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

    // Saving in the middle of an active gesture is intentionally ignored; once STOP REC is pressed,
    // the placed take is stable and can be serialized safely with the project.
    const bool stableTake = !gestureRecording.load() && gestureStartValid.load();
    const auto length = stableTake ? juce::jmin(gestureLength.load(), gestureBuffer.size()) : 0u;

    state.setProperty("gestureLength", static_cast<int>(length), nullptr);
    state.setProperty("gestureRateHz", gestureRateHz, nullptr);
    state.setProperty("gestureTimelineVersion", 1, nullptr);

    if (length > 0)
    {
        state.setProperty("gestureStartSeconds", gestureStartSeconds.load(), nullptr);

        juce::StringArray values;
        for (std::size_t i = 0; i < length; ++i)
            values.add(juce::String(gestureBuffer[i], 5));
        state.setProperty("gestureData", values.joinIntoString(","), nullptr);
    }
    else
    {
        state.removeProperty("gestureStartSeconds", nullptr);
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

        gestureArmed.store(false);
        gestureStartPending.store(false);
        gestureRecording.store(false);
        timelineGestureActive.store(false);
        releaseActive.store(false);

        const int savedLength = static_cast<int>(state.getProperty("gestureLength", 0));
        const auto encoded = state.getProperty("gestureData").toString();
        const bool hasTimelinePlacement = state.hasProperty("gestureStartSeconds");

        std::size_t restored = 0;
        if (savedLength > 0 && encoded.isNotEmpty() && hasTimelinePlacement)
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
        gestureStartSeconds.store(hasTimelinePlacement
                                    ? static_cast<double>(state.getProperty("gestureStartSeconds"))
                                    : 0.0);
        gestureStartValid.store(restored > 0 && hasTimelinePlacement);
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
