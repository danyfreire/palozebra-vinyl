#include "PluginEditor.h"

namespace
{
juce::String formatTimelineTime(double seconds)
{
    const auto totalMs = juce::jmax(0, juce::roundToInt(seconds * 1000.0));
    const int millis = totalMs % 1000;
    const int totalSeconds = totalMs / 1000;
    const int secs = totalSeconds % 60;
    const int totalMinutes = totalSeconds / 60;
    const int mins = totalMinutes % 60;
    const int hours = totalMinutes / 60;

    if (hours > 0)
        return juce::String::formatted("%d:%02d:%02d.%03d", hours, mins, secs, millis);

    return juce::String::formatted("%02d:%02d.%03d", mins, secs, millis);
}
}

VinylWheel::VinylWheel(PalozebraVinylAudioProcessor& p) : processor(p)
{
    speedParameter = processor.getParameters().getParameter(PalozebraVinylAudioProcessor::speedParamId);
    startTimerHz(60);
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
}

VinylWheel::~VinylWheel()
{
    stopTimer();
    if (dragging)
    {
        if (speedParameter != nullptr)
            speedParameter->endChangeGesture();
        processor.endManualWheelTouch();
    }
}

float VinylWheel::angleForPoint(juce::Point<float> p) const
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return std::atan2(p.y - c.y, p.x - c.x);
}

void VinylWheel::setSpeedFromGesture(float speed)
{
    if (speedParameter == nullptr)
        return;

    speed = juce::jlimit(-4.0f, 4.0f, speed);
    speedParameter->setValueNotifyingHost(speedParameter->convertTo0to1(speed));
}

void VinylWheel::mouseDown(const juce::MouseEvent& e)
{
    if (speedParameter == nullptr)
        return;

    // Manual touch takes control. If REC is armed, this touch also places TAKE 01 on the host timeline.
    processor.beginManualWheelTouch();

    dragging = true;
    speedParameter->beginChangeGesture();
    lastPointerAngle = angleForPoint(e.position);
    lastDragTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    setSpeedFromGesture(0.0f);
}

void VinylWheel::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragging || speedParameter == nullptr)
        return;

    const float nowAngle = angleForPoint(e.position);
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const double dt = juce::jmax(0.001, now - lastDragTime);

    float delta = nowAngle - lastPointerAngle;
    while (delta > juce::MathConstants<float>::pi) delta -= juce::MathConstants<float>::twoPi;
    while (delta < -juce::MathConstants<float>::pi) delta += juce::MathConstants<float>::twoPi;

    const float revolutionsPerSecond = delta / juce::MathConstants<float>::twoPi / static_cast<float>(dt);
    setSpeedFromGesture(revolutionsPerSecond);

    lastPointerAngle = nowAngle;
    lastDragTime = now;
}

void VinylWheel::mouseUp(const juce::MouseEvent&)
{
    if (!dragging || speedParameter == nullptr)
        return;

    // Release uses the short platter-motor pitch bend already established in v0.3.
    processor.beginWheelRelease();
    setSpeedFromGesture(1.0f);
    speedParameter->endChangeGesture();
    dragging = false;
    processor.endManualWheelTouch();
}

void VinylWheel::timerCallback()
{
    const float speed = processor.getEffectiveSpeed();
    visualAngle += speed * juce::MathConstants<float>::twoPi / 60.0f;
    visualAngle = std::fmod(visualAngle, juce::MathConstants<float>::twoPi);
    repaint();
}

void VinylWheel::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(10.0f);
    const float diameter = juce::jmin(r.getWidth(), r.getHeight());
    auto disc = juce::Rectangle<float>(diameter, diameter).withCentre(r.getCentre());

    g.setColour(juce::Colour::fromRGB(18, 18, 18));
    g.fillEllipse(disc);

    for (int i = 1; i <= 9; ++i)
    {
        const float inset = static_cast<float>(i) * diameter * 0.035f;
        g.setColour(juce::Colour::fromRGBA(255, 255, 255, static_cast<juce::uint8>(16 + i * 2)));
        g.drawEllipse(disc.reduced(inset), 1.0f);
    }

    auto label = disc.withSizeKeepingCentre(diameter * 0.34f, diameter * 0.34f);
    g.setColour(juce::Colour::fromRGB(235, 235, 225));
    g.fillEllipse(label);
    g.setColour(juce::Colour::fromRGB(28, 28, 28));
    g.fillEllipse(label.withSizeKeepingCentre(diameter * 0.045f, diameter * 0.045f));

    const auto c = disc.getCentre();
    const float radius = diameter * 0.36f;
    juce::Point<float> marker(c.x + std::cos(visualAngle) * radius,
                              c.y + std::sin(visualAngle) * radius);
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.fillEllipse(marker.x - 3.5f, marker.y - 3.5f, 7.0f, 7.0f);

    const float speed = processor.getEffectiveSpeed();
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    const auto text = std::abs(speed) < 0.005f ? juce::String("STOP") : juce::String(speed, 2) + "x";
    g.drawText(text, label.toNearestInt(), juce::Justification::centred);
}

PalozebraVinylAudioProcessorEditor::PalozebraVinylAudioProcessorEditor(PalozebraVinylAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), wheel(p)
{
    addAndMakeVisible(wheel);
    addAndMakeVisible(recordButton);
    addAndMakeVisible(clearButton);
    addAndMakeVisible(takeLabel);

    takeLabel.setJustificationType(juce::Justification::centred);
    takeLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.72f));

    recordButton.onClick = [this]
    {
        if (processor.isGestureRecording() || processor.isGestureArmed())
            processor.stopGestureRecording();
        else
            processor.armGestureRecording();

        refreshTransportUi();
    };

    clearButton.onClick = [this]
    {
        processor.clearGesture();
        refreshTransportUi();
    };

    setResizable(false, false);
    setSize(440, 545);
    startTimerHz(20);
    refreshTransportUi();
}

PalozebraVinylAudioProcessorEditor::~PalozebraVinylAudioProcessorEditor()
{
    stopTimer();
}

void PalozebraVinylAudioProcessorEditor::timerCallback()
{
    refreshTransportUi();
}

void PalozebraVinylAudioProcessorEditor::refreshTransportUi()
{
    const bool armed = processor.isGestureArmed();
    const bool recording = processor.isGestureRecording();
    const bool active = processor.isTimelineGestureActive();
    const bool hasTake = processor.hasGesture();

    if (recording)
        recordButton.setButtonText("STOP REC");
    else if (armed)
        recordButton.setButtonText("CANCEL");
    else
        recordButton.setButtonText("REC");

    clearButton.setEnabled(hasTake || armed || recording);

    if (recording)
    {
        takeLabel.setText("RECORDING  ·  " + formatTimelineTime(processor.getGestureStartSeconds()),
                          juce::dontSendNotification);
    }
    else if (armed)
    {
        if (processor.isHostPlaying() && processor.hasHostTimeline())
            takeLabel.setText("REC ARMED  ·  TOUCH THE RECORD", juce::dontSendNotification);
        else
            takeLabel.setText("REC ARMED  ·  START DAW PLAYBACK", juce::dontSendNotification);
    }
    else if (hasTake)
    {
        juce::String text = "TAKE 01  ·  " + formatTimelineTime(processor.getGestureStartSeconds())
                          + "  ·  " + juce::String(processor.getGestureLengthSeconds(), 2) + " s";
        if (active)
            text += "  ·  PLAYING";
        takeLabel.setText(text, juce::dontSendNotification);
    }
    else
    {
        takeLabel.setText("NO TAKE", juce::dontSendNotification);
    }
}

void PalozebraVinylAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(8, 8, 8));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.drawText("PALOZEBRA // VINYL", 20, 12, getWidth() - 40, 36, juce::Justification::centred);

    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.drawLine(24.0f, 438.0f, static_cast<float>(getWidth() - 24), 438.0f, 1.0f);

    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.setFont(juce::FontOptions(12.0f));
    const juce::String footer = juce::String("REC arm → touch platter → replay in place  ·  v")
                              + JucePlugin_VersionString;
    g.drawText(footer, 15, 515, getWidth() - 30, 20, juce::Justification::centred);
}

void PalozebraVinylAudioProcessorEditor::resized()
{
    wheel.setBounds(30, 52, getWidth() - 60, 376);
    takeLabel.setBounds(20, 448, getWidth() - 40, 24);

    recordButton.setBounds(100, 480, 118, 36);
    clearButton.setBounds(226, 480, 114, 36);
}
