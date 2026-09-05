#pragma once

#include "PluginProcessor.h"

class LUmacOSvelerAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit LUmacOSvelerAudioProcessorEditor(LUmacOSvelerAudioProcessor&);
    ~LUmacOSvelerAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class RightAlignedSliderLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        juce::Label* createSliderTextBox(juce::Slider& slider) override
        {
            auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
            label->setJustificationType(juce::Justification::centredRight);
            return label;
        }
    };

    class LimiterReductionMeter final : public juce::Component, private juce::Timer
    {
    public:
        LimiterReductionMeter(const std::atomic<float>& reduction, juce::Label& reductionLabel)
            : reductionDb(reduction), reductionValueLabel(reductionLabel)
        {
            startTimerHz(30);
        }

        void paint(juce::Graphics& graphics) override;

    private:
        void timerCallback() override;
        const std::atomic<float>& reductionDb;
        juce::Label& reductionValueLabel;
        float heldReductionDb = 0.0f;
        double lastPeakTimeMs = 0.0;
    };

    LUmacOSvelerAudioProcessor& audioProcessor;
    RightAlignedSliderLookAndFeel sliderLookAndFeel;
    juce::Label targetLabel;
    juce::Slider targetSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> targetAttachment;
    juce::Label maxGainLabel;
    juce::Slider maxGainSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> maxGainAttachment;
    juce::Label truePeakLabel;
    juce::Slider truePeakSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> truePeakAttachment;
    juce::Label freezeLevelLabel;
    juce::Slider freezeLevelSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freezeLevelAttachment;
    juce::Label inputLevelLabel;
    juce::Slider inputLevelSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputLevelAttachment;
    juce::Label correctionHighLabel;
    juce::Slider correctionHighSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> correctionHighAttachment;
    juce::Label correctionLowLabel;
    juce::Slider correctionLowSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> correctionLowAttachment;
    juce::Label correctionMixModeLabel;
    juce::ComboBox correctionMixModeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> correctionMixModeAttachment;
    juce::TextButton resetButton { "RESET" };
    juce::TextButton inputLearnButton { "LEARN INPUT" };
    juce::TextButton infoButton { "INFO" };
    juce::Label limiterLabel;
    juce::Label limiterReductionLabel;
    LimiterReductionMeter limiterReductionMeter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUmacOSvelerAudioProcessorEditor)
};
