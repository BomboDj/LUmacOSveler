#include "PluginEditor.h"

namespace
{
class InfoDialogContent final : public juce::Component
{
public:
    explicit InfoDialogContent(const juce::String& message)
    {
        textEditor.setMultiLine(true);
        textEditor.setReadOnly(true);
        textEditor.setScrollbarsShown(true);
        textEditor.setCaretVisible(false);
        textEditor.setPopupMenuEnabled(false);
        textEditor.setText(message, false);
        textEditor.setFont(juce::FontOptions(14.0f));
        textEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff17191d));
        textEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white.withAlpha(0.86f));
        textEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(textEditor);

        closeButton.setButtonText("CLOSE");
        closeButton.onClick = [this]
        {
            if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState(0);
        };
        addAndMakeVisible(closeButton);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(14);
        closeButton.setBounds(bounds.removeFromBottom(32).withSizeKeepingCentre(90, 28));
        bounds.removeFromBottom(10);
        textEditor.setBounds(bounds);
    }

private:
    juce::TextEditor textEditor;
    juce::TextButton closeButton;
};
}

LUmacOSvelerAudioProcessorEditor::LUmacOSvelerAudioProcessorEditor(
    LUmacOSvelerAudioProcessor& processorToEdit)
    : AudioProcessorEditor(processorToEdit), audioProcessor(processorToEdit)
      , limiterReductionMeter(processorToEdit.getGainReductionDb(), limiterReductionLabel)
{
    setSize(750, 500);
    setResizable(true, true);
    setResizeLimits(520, 500, 1000, 720);

    targetLabel.setText("TARGET LEVEL", juce::dontSendNotification);
    targetLabel.setJustificationType(juce::Justification::centredLeft);
    targetLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    addAndMakeVisible(targetLabel);

    targetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    targetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 90, 26);
    targetSlider.setLookAndFeel(&sliderLookAndFeel);
    targetSlider.setTextValueSuffix(" LUFS");
    targetSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff9de46b).withAlpha(0.75f));
    targetSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffd8ffbf));
    targetSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    targetSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(targetSlider);

    targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, LUmacOSvelerAudioProcessor::targetLevelParameterId, targetSlider);

    auto configureSlider = [this](juce::Label& label, juce::Slider& slider, const char* text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
        addAndMakeVisible(label);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 90, 26);
        slider.setLookAndFeel(&sliderLookAndFeel);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff9de46b).withAlpha(0.75f));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffd8ffbf));
        addAndMakeVisible(slider);
    };

    configureSlider(maxGainLabel, maxGainSlider, "MAX GAIN");
    maxGainSlider.setTextValueSuffix(" dB");
    maxGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, LUmacOSvelerAudioProcessor::maxGainParameterId, maxGainSlider);

    configureSlider(truePeakLabel, truePeakSlider, "TRUE PEAK");
    truePeakSlider.setTextValueSuffix(" dBTP");
    truePeakAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, LUmacOSvelerAudioProcessor::truePeakParameterId, truePeakSlider);

    configureSlider(freezeLevelLabel, freezeLevelSlider, "FREEZE LEVEL");
    freezeLevelSlider.setTextValueSuffix(" LUFS");
    freezeLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, LUmacOSvelerAudioProcessor::freezeLevelParameterId, freezeLevelSlider);

    configureSlider(inputLevelLabel, inputLevelSlider, "INPUT LEVEL");
    inputLevelSlider.setTextValueSuffix(" LUFS");
    inputLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, LUmacOSvelerAudioProcessor::inputLevelParameterId, inputLevelSlider);

    configureSlider(correctionHighLabel, correctionHighSlider, "CORRECTION HIGH");
    correctionHighSlider.setTextValueSuffix(" %");
    correctionHighAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, LUmacOSvelerAudioProcessor::correctionHighParameterId,
        correctionHighSlider);

    configureSlider(correctionLowLabel, correctionLowSlider, "CORRECTION LOW");
    correctionLowSlider.setTextValueSuffix(" %");
    correctionLowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, LUmacOSvelerAudioProcessor::correctionLowParameterId,
        correctionLowSlider);

    correctionMixModeLabel.setText("CORRECTION MIX MODE", juce::dontSendNotification);
    correctionMixModeLabel.setJustificationType(juce::Justification::centredLeft);
    correctionMixModeLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    addAndMakeVisible(correctionMixModeLabel);
    correctionMixModeBox.addItem("Linear / Linear", 1);
    correctionMixModeBox.addItem("Linear / Log", 2);
    correctionMixModeBox.addItem("Log / Linear", 3);
    correctionMixModeBox.addItem("Log / Log", 4);
    correctionMixModeBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    correctionMixModeBox.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(correctionMixModeBox);
    correctionMixModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.parameters, LUmacOSvelerAudioProcessor::correctionMixModeParameterId,
        correctionMixModeBox);

    resetButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff30353b));
    resetButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
    resetButton.onClick = [this] { audioProcessor.resetParametersToDefaults(); };
    addAndMakeVisible(resetButton);

    inputLearnButton.setClickingTogglesState(true);
    inputLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff30353b));
    inputLearnButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
    inputLearnButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff9de46b));
    inputLearnButton.onClick = [this]
    {
        if (inputLearnButton.getToggleState())
        {
            inputLearnButton.setButtonText("STOP LEARN");
            audioProcessor.startInputLevelLearn();
        }
        else
        {
            inputLearnButton.setButtonText("LEARN INPUT");
            audioProcessor.stopInputLevelLearn();
        }
    };
    addAndMakeVisible(inputLearnButton);

    infoButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff30353b));
    infoButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
    infoButton.onClick = []
    {
        const juce::String infoText =
            "ITU-R BS.1770 / EBU R128\n\n"
            "LOUDNESS ANALYSIS\n"
            "K-weighting: high-pass 38.1358 Hz + high-shelf 1681.974 Hz / +4 dB\n"
            "Momentary: 400 ms | Short-term: 3 s\n"
            "Integrated blocks: 400 ms with 100 ms hop (75% overlap)\n"
            "Absolute gate: -70 LUFS (fixed) | Relative gate: -10 LU (fixed)\n\n"
            "GAIN CONTROL\n"
            "Target Level: default -20 LUFS\n"
            "Input Level: fixed integrated-loudness reference of the source\n"
            "LEARN INPUT captures gated integrated loudness and applies it when stopped\n"
            "Max Gain: total compensation limit from 0 to +30 dB\n"
            "Example: -23 LUFS to -16 LUFS needs 7 dB; allowing 10 dB more\n"
            "requires Max Gain = 17 dB\n"
            "Freeze Level: gain increases only above the input loudness threshold\n"
            "Correction High/Low: controlled gain ratio above/below Input Level\n"
            "Mix Mode: 0 linear/linear, 1 linear/log, 2 log/linear, 3 log/log\n\n"
            "TRUE PEAK\n"
            "Adjustable maximum: default -1.0 dBTP, 4x oversampling\n"
            "Internal safety margin: 0.2 dB\n\n"
            "LIMITER\n"
            "Fixed final limiter, always active\n"
            "Threshold: 0 dB | Output: 0 dB\n"
            "Lookahead: 0.1 ms | Knee: 0.1 dB | Release: 0.1 ms";

        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(new InfoDialogContent(infoText));
        options.dialogTitle = "LUmacOSveler - INFO";
        options.dialogBackgroundColour = juce::Colour(0xff17191d);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.useBottomRightCornerResizer = true;
        options.launchAsync();
    };
    addAndMakeVisible(infoButton);

    limiterLabel.setText("LIMITER", juce::dontSendNotification);
    limiterLabel.setJustificationType(juce::Justification::centredLeft);
    limiterLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    addAndMakeVisible(limiterLabel);
    limiterReductionLabel.setText("0.0 dB", juce::dontSendNotification);
    limiterReductionLabel.setJustificationType(juce::Justification::centredRight);
    limiterReductionLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
    addAndMakeVisible(limiterReductionLabel);
    addAndMakeVisible(limiterReductionMeter);
}

void LUmacOSvelerAudioProcessorEditor::LimiterReductionMeter::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto currentReduction = juce::jlimit(0.0f, 12.0f, reductionDb.load());

    if (currentReduction >= heldReductionDb)
    {
        heldReductionDb = currentReduction;
        lastPeakTimeMs = now;
    }
    else if (now - lastPeakTimeMs >= 1000.0)
    {
        heldReductionDb = currentReduction;
        lastPeakTimeMs = now;
    }

    reductionValueLabel.setText(juce::String(juce::jlimit(0.0f, 12.0f, heldReductionDb), 1)
                                + " dB", juce::dontSendNotification);
    if (getParentComponent() != nullptr)
        getParentComponent()->repaint();
    repaint();
}

void LUmacOSvelerAudioProcessorEditor::LimiterReductionMeter::paint(juce::Graphics& graphics)
{
    const auto area = getLocalBounds().toFloat();
    const auto liveReduction = juce::jlimit(0.0f, 12.0f, reductionDb.load());
    const auto amount = liveReduction / 12.0f;
    const auto bar = area.withRight(area.getRight() - 90.0f)
                         .withY(area.getCentreY() - 3.0f)
                         .withHeight(6.0f);

    graphics.setColour(juce::Colour(0xff30353b));
    graphics.fillRoundedRectangle(bar, 4.0f);
    graphics.setColour(juce::Colour(0xfff07878));
    graphics.fillRoundedRectangle(bar.withLeft(bar.getRight() - bar.getWidth() * amount), 4.0f);
}

void LUmacOSvelerAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff17191d));
    graphics.setColour(juce::Colour(0xff24282e));
    graphics.setColour(juce::Colours::white);
    graphics.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    graphics.drawText("LUmacOSveler", 250, 24, getWidth() - 482, 26, juce::Justification::centred);
}

void LUmacOSvelerAudioProcessorEditor::resized()
{
    const auto content = getLocalBounds().reduced(34, 0);
    const auto labelWidth = 126;
    const auto sliderX = content.getX() + labelWidth;
    const auto sliderWidth = content.getWidth() - labelWidth;
    const auto rowHeight = 42;

    targetLabel.setBounds(content.getX(), 70, labelWidth - 10, 26);
    targetSlider.setBounds(sliderX, 62, sliderWidth, rowHeight);
    inputLevelLabel.setBounds(content.getX(), 118, labelWidth - 10, 26);
    inputLevelSlider.setBounds(sliderX, 110, sliderWidth, rowHeight);
    maxGainLabel.setBounds(content.getX(), 166, labelWidth - 10, 26);
    maxGainSlider.setBounds(sliderX, 158, sliderWidth, rowHeight);
    freezeLevelLabel.setBounds(content.getX(), 214, labelWidth - 10, 26);
    freezeLevelSlider.setBounds(sliderX, 206, sliderWidth, rowHeight);
    truePeakLabel.setBounds(content.getX(), 262, labelWidth - 10, 26);
    truePeakSlider.setBounds(sliderX, 254, sliderWidth, rowHeight);
    correctionHighLabel.setBounds(content.getX(), 310, labelWidth - 10, 26);
    correctionHighSlider.setBounds(sliderX, 302, sliderWidth, rowHeight);
    correctionLowLabel.setBounds(content.getX(), 358, labelWidth - 10, 26);
    correctionLowSlider.setBounds(sliderX, 350, sliderWidth, rowHeight);
    correctionMixModeLabel.setBounds(content.getX(), 406, labelWidth - 10, 26);
    correctionMixModeBox.setBounds(sliderX, 398, sliderWidth, rowHeight);
    resetButton.setBounds(content.getX(), 24, 70, 28);
    inputLearnButton.setBounds(content.getX() + 82, 24, 110, 28);
    infoButton.setBounds(content.getRight() - 70, 24, 70, 28);
    limiterLabel.setBounds(content.getX(), 454, labelWidth - 10, 30);
    limiterReductionMeter.setBounds(sliderX, 454, sliderWidth, 30);
    limiterReductionLabel.setBounds(content.getRight() - 90, 454, 90, 30);
}
