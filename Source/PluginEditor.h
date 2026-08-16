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

class PalozebraVinylAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                  private juce::Timer
{
public:
    explicit PalozebraVinylAudioProcessorEditor(PalozebraVinylAudioProcessor&);
    ~PalozebraVinylAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshTransportUi();

    PalozebraVinylAudioProcessor& processor;
    VinylWheel wheel;

    juce::TextButton recordButton { "REC" };
    juce::TextButton playButton { "PLAY" };
    juce::TextButton clearButton { "CLEAR" };
    juce::Label takeLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PalozebraVinylAudioProcessorEditor)
};
