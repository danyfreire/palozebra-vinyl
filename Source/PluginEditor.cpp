#include "PluginEditor.h"

VinylWheel::VinylWheel(PalozebraVinylAudioProcessor& p) : processor(p)
{
    speedParameter = processor.getParameters().getParameter(PalozebraVinylAudioProcessor::speedParamId);
    startTimerHz(60);
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
}

VinylWheel::~VinylWheel()
{
    stopTimer();
    if (dragging && speedParameter != nullptr)
        speedParameter->endChangeGesture();
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

    dragging = true;
    speedParameter->beginChangeGesture();
    lastPointerAngle = angleForPoint(e.position);
    lastDragTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    setSpeedFromGesture(0.0f); // grabbing a physical record stops it under your hand
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

    // One full revolution per second = 1x. Direction follows screen-angle convention.
    const float revolutionsPerSecond = delta / juce::MathConstants<float>::twoPi / static_cast<float>(dt);
    setSpeedFromGesture(revolutionsPerSecond);

    lastPointerAngle = nowAngle;
    lastDragTime = now;
}

void VinylWheel::mouseUp(const juce::MouseEvent&)
{
    if (!dragging || speedParameter == nullptr)
        return;

    // Release the record: normal platter motor takes over.
    setSpeedFromGesture(1.0f);
    speedParameter->endChangeGesture();
    dragging = false;
}

void VinylWheel::timerCallback()
{
    const float speed = processor.getSpeedParameter() != nullptr ? processor.getSpeedParameter()->load() : 1.0f;
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

    // Grooves
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

    // Rotating marker makes speed/direction obvious.
    const auto c = disc.getCentre();
    const float radius = diameter * 0.36f;
    juce::Point<float> marker(c.x + std::cos(visualAngle) * radius,
                              c.y + std::sin(visualAngle) * radius);
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.fillEllipse(marker.x - 3.5f, marker.y - 3.5f, 7.0f, 7.0f);

    const float speed = processor.getSpeedParameter() != nullptr ? processor.getSpeedParameter()->load() : 1.0f;
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    const auto text = std::abs(speed) < 0.005f ? juce::String("STOP") : juce::String(speed, 2) + "x";
    g.drawText(text, label.toNearestInt(), juce::Justification::centred);
}

PalozebraVinylAudioProcessorEditor::PalozebraVinylAudioProcessorEditor(PalozebraVinylAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), wheel(p)
{
    addAndMakeVisible(wheel);
    addAndMakeVisible(midiOutButton);
    midiAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.getParameters(), PalozebraVinylAudioProcessor::midiOutParamId, midiOutButton);

    setResizable(false, false);
    setSize(420, 500);
}

void PalozebraVinylAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(8, 8, 8));
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.drawText("PALOZEBRA // VINYL", 20, 12, getWidth() - 40, 36, juce::Justification::centred);

    g.setColour(juce::Colours::white.withAlpha(0.58f));
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("grab · drag · release · v0.1.1", 20, 450, getWidth() - 40, 24, juce::Justification::centred);
}

void PalozebraVinylAudioProcessorEditor::resized()
{
    wheel.setBounds(25, 55, getWidth() - 50, 380);
    midiOutButton.setBounds(getWidth() - 105, 438, 90, 28);
}
