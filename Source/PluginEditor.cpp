#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
std::unique_ptr<juce::Drawable> createTitleGraphic()
{
    const auto image = juce::ImageCache::getFromMemory(
        LUmacOSvelerIconData::LUmacOSvelerTitle_png,
        LUmacOSvelerIconData::LUmacOSvelerTitle_pngSize);
    return image.isValid() ? std::make_unique<juce::DrawableImage>(image) : nullptr;
}

class InfoDialogContent final : public juce::Component
{
public:
    explicit InfoDialogContent(const juce::String& message)
    {
        setSize(500, 490);
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
        icon = createTitleGraphic();

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
        bounds.removeFromTop(42);
        textEditor.setBounds(bounds);
    }

    void paint(juce::Graphics& graphics) override
    {
        if (icon != nullptr)
            icon->drawWithin(graphics, { 14.0f, 8.0f, 220.0f, 40.0f },
                             juce::RectanglePlacement::centred, 1.0f);
        graphics.setColour(juce::Colours::white.withAlpha(0.65f));
        graphics.setFont(juce::FontOptions(13.0f));
        graphics.drawText("Version 0.6.0", 14, 50, 220, 18,
                          juce::Justification::centredLeft);
    }

private:
    juce::TextEditor textEditor;
    juce::TextButton closeButton;
    std::unique_ptr<juce::Drawable> icon;
};
}

LUmacOSvelerAudioProcessorEditor::LUmacOSvelerAudioProcessorEditor(
    LUmacOSvelerAudioProcessor& processorToEdit)
    : AudioProcessorEditor(processorToEdit), audioProcessor(processorToEdit)
{
    setSize(750, 500);
    setResizable(true, true);
    setResizeLimits(620, 500, 1100, 670);


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
    truePeakSlider.setLookAndFeel(&sliderLookAndFeel);
    truePeakSlider.setTextValueSuffix(" dBTP");
    truePeakAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, LUmacOSvelerAudioProcessor::truePeakParameterId, truePeakSlider);

    configureSlider(lfeGainLabel, lfeGainSlider, "LFE GAIN (5.1)");
    lfeGainSlider.setTextValueSuffix(" dB");
    lfeGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, LUmacOSvelerAudioProcessor::lfeGainParameterId, lfeGainSlider);

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
    resetButton.onClick = [this]
    {
        audioProcessor.resetParametersToDefaults();
        audioProcessor.requestMeasurementReset();
    };
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
            "Version 0.6.0\n"
            "ITU-R BS.1770-5 measurement + EBU R 128 workflow\n\n"
            "SUPPORTED CONFIGURATIONS\n"
            "Mono, stereo and 5.1 channel layouts\n"
            "Sample rates: 8 kHz - 192 kHz\n"
            "LFE is excluded from loudness measurement in 5.1\n\n"
            "LOUDNESS MEASUREMENT\n"
            "K-weighting: ITU-R BS.1770-5 two-stage biquad filter\n"
            "Momentary window: 400 ms\n"
            "Short-term window: 3 seconds\n"
            "Integrated blocks: 400 ms, 100 ms hop, 75% overlap\n"
            "Absolute gate: -70 LUFS\n"
            "Relative gate: -10 LU below the absolute-gated average\n"
            "RESET restores all defaults and clears the programme loudness history\n\n"
            "LEVEL PARAMETERS\n"
            "Target Level [LUFS]: desired output loudness; EBU default -23 LUFS\n"
            "Input Level [LUFS]: known or learned integrated loudness of the source\n"
            "Max Gain [dB]: maximum total gain compensation, 0 to +30 dB\n"
            "Freeze Level [LUFS]: prevents quiet background noise from being raised\n\n"
            "DYNAMIC CORRECTION\n"
            "Correction High [%]: correction above Input Level\n"
            "Correction Low [%]: correction below Input Level\n"
            "Mix Mode: Linear/Linear, Linear/Log, Log/Linear or Log/Log\n"
            "LEARN INPUT measures the gated input loudness during playback\n"
            "and writes the result to Input Level when stopped\n\n"
            "TRUE PEAK / CLIPPER\n"
            "True Peak [dBTP]: user-facing ceiling\n"
            "Internal safety margin: 0.2 dB\n"
            "ITU Annex 2 FIR true-peak estimation, 4x oversampling\n"
            "Always-active hard clipper; no limiter, lookahead or release stage\n\n"
            "5.1 ONLY\n"
            "LFE Gain [dB]: fixed gain applied to the LFE channel\n"
            "The LFE remains excluded from loudness measurement but is clipped\n"
            "at the true-peak ceiling like the other output channels.";

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

}

void LUmacOSvelerAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff17191d));
    graphics.setColour(juce::Colour(0xff24282e));
    if (auto titleGraphic = createTitleGraphic())
        titleGraphic->drawWithin(graphics, { 34.0f, 16.0f, 220.0f, 40.0f },
                         juce::RectanglePlacement::centred, 1.0f);
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
    lfeGainLabel.setBounds(content.getX(), 310, labelWidth - 10, 26);
    lfeGainSlider.setBounds(sliderX, 302, sliderWidth, rowHeight);
    correctionHighLabel.setBounds(content.getX(), 358, labelWidth - 10, 26);
    correctionHighSlider.setBounds(sliderX, 350, sliderWidth, rowHeight);
    correctionLowLabel.setBounds(content.getX(), 406, labelWidth - 10, 26);
    correctionLowSlider.setBounds(sliderX, 398, sliderWidth, rowHeight);
    correctionMixModeLabel.setBounds(content.getX(), 454, labelWidth - 10, 26);
    correctionMixModeBox.setBounds(sliderX, 446, sliderWidth, rowHeight);
    inputLearnButton.setBounds(content.getRight() - 270, 24, 110, 28);
    resetButton.setBounds(content.getRight() - 150, 24, 70, 28);
    infoButton.setBounds(content.getRight() - 70, 24, 70, 28);
    const auto isFiveOne = audioProcessor.getTotalNumInputChannels() == 6;
    lfeGainLabel.setVisible(isFiveOne);
    lfeGainSlider.setVisible(isFiveOne);
}
