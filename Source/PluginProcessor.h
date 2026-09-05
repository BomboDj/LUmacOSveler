#pragma once

#include <JuceHeader.h>

class LUmacOSvelerAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr double minimumSampleRate = 8000.0;
    static constexpr double maximumSampleRate = 192000.0;
    static constexpr auto targetLevelParameterId = "targetLevel";
    static constexpr auto maxGainParameterId = "maxGain";
    static constexpr auto truePeakParameterId = "truePeak";
    static constexpr auto freezeLevelParameterId = "freezeLevel";
    static constexpr auto inputLevelParameterId = "inputLevel";
    static constexpr auto correctionHighParameterId = "correctionHigh";
    static constexpr auto correctionLowParameterId = "correctionLow";
    static constexpr auto correctionMixModeParameterId = "correctionMixMode";

    LUmacOSvelerAudioProcessor();
    ~LUmacOSvelerAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LUmacOSveler"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    float getTargetLevelLUFS() const noexcept;
    const std::atomic<float>& getGainReductionDb() const noexcept { return gainReductionDb; }
    void resetParametersToDefaults();
    void startInputLevelLearn() noexcept { inputLearnRequest.store(1); }
    void stopInputLevelLearn() noexcept { inputLearnRequest.store(2); }
    bool isInputLevelLearning() const noexcept { return inputLearning.load(); }

    static bool isSupportedSampleRate(double rate) noexcept
    {
        return rate >= minimumSampleRate && rate <= maximumSampleRate;
    }

    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    double sampleRate = 44100.0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainDb;
    juce::dsp::Oversampling<float> truePeakOversampler {
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };
    std::array<juce::dsp::IIR::Filter<float>, 2> highPassFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> highShelfFilters;
    std::vector<float> shortTermEnergy;
    size_t shortTermWritePosition = 0;
    size_t shortTermSamples = 0;
    double shortTermEnergySum = 0.0;
    std::vector<float> momentaryEnergy;
    size_t momentaryWritePosition = 0;
    size_t momentarySamples = 0;
    double momentaryEnergySum = 0.0;
    int gateHopSamples = 0;
    int gateHopPosition = 0;
    std::vector<float> gatedBlockEnergies;
    size_t gatedBlockCount = 0;
    std::vector<std::array<float, 2>> limiterDelay;
    size_t limiterWritePosition = 0;
    float limiterGainReductionDb = 0.0f;
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<int> inputLearnRequest { 0 };
    std::atomic<bool> inputLearning { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUmacOSvelerAudioProcessor)
};
