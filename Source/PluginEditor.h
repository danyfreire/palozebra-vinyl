#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class VinylWheel final : public juce::Component,
                         private juce::Timer
{
public:
    explicit VinylWheel(PalozebraVinylAudioProcessor& p);
    ~VinylWheel() override;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float angleForPoint(juce::Point<float> p) const;
    void setSpeedFromGesture(float speed);

    PalozebraVinylAudioProcessor& processor;
    juce::RangedAudioParameter* speedParameter = nullptr;
    float visualAngle = 0.0f;
    float lastPointerAngle = 0.0f;
    double lastDragTime = 0.0;
    bool dragging = false;
};

class PalozebraVinylAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PalozebraVinylAudioProcessorEditor(PalozebraVinylAudioProcessor&);
    ~PalozebraVinylAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PalozebraVinylAudioProcessor& processor;
    VinylWheel wheel;
    juce::ToggleButton midiOutButton { "MIDI CC" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PalozebraVinylAudioProcessorEditor)
};
