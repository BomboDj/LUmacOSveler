#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float truePeakFirCoefficients[12][4] =
{
    { 0.0017089843750f, -0.0291748046875f, -0.0189208984375f, -0.0083007812500f },
    { 0.0109863281250f, 0.0292968750000f, 0.0330810546875f, 0.0148925781250f },
    { -0.0196533203125f, -0.0517578125000f, -0.0582275390625f, -0.0266113281250f },
    { 0.0332031250000f, 0.0891113281250f, 0.1015625000000f, 0.0476074218750f },
    { -0.0594482421875f, -0.1665039062500f, -0.2003173828125f, -0.1022949218750f },
    { 0.1373291015625f, 0.4650878906250f, 0.7797851562500f, 0.9721679687500f },
    { 0.9721679687500f, 0.7797851562500f, 0.4650878906250f, 0.1373291015625f },
    { -0.1022949218750f, -0.2003173828125f, -0.1665039062500f, -0.0594482421875f },
    { 0.0476074218750f, 0.1015625000000f, 0.0891113281250f, 0.0332031250000f },
    { -0.0266113281250f, -0.0582275390625f, -0.0517578125000f, -0.0196533203125f },
    { 0.0148925781250f, 0.0330810546875f, 0.0292968750000f, 0.0109863281250f },
    { -0.0083007812500f, -0.0189208984375f, -0.0291748046875f, 0.0017089843750f }
};

juce::dsp::IIR::Coefficients<float>::Ptr makeKWeightingStage1(double sampleRate)
{
    const auto k = std::tan(juce::MathConstants<double>::pi * 1681.974450955533 / sampleRate);
    const auto q = 0.7071752369554196;
    const auto vh = std::pow(10.0, 3.999843853973347 / 20.0);
    const auto vb = std::pow(vh, 0.4996667741545416);
    const auto denominator = 1.0 + k / q + k * k;
    const auto b0 = (vh + vb * k / q + k * k) / denominator;
    const auto b1 = 2.0 * (k * k - vh) / denominator;
    const auto b2 = (vh - vb * k / q + k * k) / denominator;
    const auto a1 = 2.0 * (k * k - 1.0) / denominator;
    const auto a2 = (1.0 - k / q + k * k) / denominator;
    return new juce::dsp::IIR::Coefficients<float>(static_cast<float>(b0), static_cast<float>(b1),
        static_cast<float>(b2), 1.0f, static_cast<float>(a1), static_cast<float>(a2));
}

juce::dsp::IIR::Coefficients<float>::Ptr makeKWeightingStage2(double sampleRate)
{
    const auto k = std::tan(juce::MathConstants<double>::pi * 38.13547087602444 / sampleRate);
    const auto q = 0.5003270373253953;
    const auto denominator = 1.0 + k / q + k * k;
    const auto b0 = 1.0 / denominator;
    const auto a1 = 2.0 * (k * k - 1.0) / denominator;
    const auto a2 = (1.0 - k / q + k * k) / denominator;
    return new juce::dsp::IIR::Coefficients<float>(static_cast<float>(b0),
        static_cast<float>(-2.0 * b0), static_cast<float>(b0), 1.0f,
        static_cast<float>(a1), static_cast<float>(a2));
}
}

float LUmacOSvelerAudioProcessor::processTruePeakSample(int channel, float sample) noexcept
{
    truePeakHistory[static_cast<size_t>(channel)][truePeakHistoryPosition] = sample;
    auto peak = 0.0f;
    for (int phase = 0; phase < 4; ++phase)
    {
        auto interpolated = 0.0f;
        for (int tap = 0; tap < truePeakFirTaps; ++tap)
        {
            const auto index = (truePeakHistoryPosition + truePeakFirTaps
                - static_cast<size_t>(tap)) % truePeakFirTaps;
            interpolated += truePeakFirCoefficients[tap][phase]
                * truePeakHistory[static_cast<size_t>(channel)][index];
        }
        peak = juce::jmax(peak, std::abs(interpolated));
    }
    return peak;
}

LUmacOSvelerAudioProcessor::LUmacOSvelerAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
LUmacOSvelerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        targetLevelParameterId,
        "Target Level",
        juce::NormalisableRange<float>(-36.0f, -6.0f, 0.1f),
        -23.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("LUFS")
            .withStringFromValueFunction([](float value, int) { return juce::String(value, 1); })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        maxGainParameterId, "Max Gain", juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f),
        12.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction([](float value, int) { return juce::String(value, 1); })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        truePeakParameterId, "True Peak", juce::NormalisableRange<float>(-6.0f, 0.0f, 0.1f),
        -1.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dBTP")
            .withStringFromValueFunction([](float value, int) { return juce::String(value, 1); })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        freezeLevelParameterId, "Freeze Level", juce::NormalisableRange<float>(-60.0f, -6.0f, 0.1f),
        -30.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("LUFS")
            .withStringFromValueFunction([](float value, int) { return juce::String(value, 1); })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        inputLevelParameterId, "Input Level", juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f),
        -23.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("LUFS")
            .withStringFromValueFunction([](float value, int) { return juce::String(value, 1); })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        correctionHighParameterId, "Correction High", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        100.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction([](float value, int) { return juce::String(value, 1); })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        correctionLowParameterId, "Correction Low", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        100.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction([](float value, int) { return juce::String(value, 1); })));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        correctionMixModeParameterId, "Correction Mix Mode",
        juce::StringArray { "Linear / Linear", "Linear / Log", "Log / Linear", "Log / Log" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        lfeGainParameterId, "LFE Gain", juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    return layout;
}

void LUmacOSvelerAudioProcessor::prepareToPlay(double newSampleRate, int samplesPerBlock)
{
    // The host may use any rate in this range, including non-standard rates.
    // Clamp invalid host values to keep all DSP state safely initialised.
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    gainDb.reset(sampleRate, 1.0);
    gainDb.setCurrentAndTargetValue(0.0f);
    truePeakOversampler.reset();
    truePeakOversampler.initProcessing(static_cast<size_t>(juce::jmax(1, samplesPerBlock)));
    shortTermEnergy.assign(static_cast<size_t>(std::ceil(sampleRate * 3.0)), 0.0f);
    shortTermWritePosition = 0;
    shortTermSamples = 0;
    shortTermEnergySum = 0.0;
    momentaryEnergy.assign(static_cast<size_t>(std::ceil(sampleRate * 0.4)), 0.0f);
    momentaryWritePosition = 0;
    momentarySamples = 0;
    momentaryEnergySum = 0.0;
    gateHopSamples = juce::jmax(1, static_cast<int>(std::round(sampleRate * 0.1)));
    gateHopPosition = 0;
    gatedBlockEnergies.assign(3600000, 0.0f);
    gatedBlockCount = 0;

    for (auto channel = 0; channel < 6; ++channel)
    {
        highPassFilters[static_cast<size_t>(channel)].reset();
        highShelfFilters[static_cast<size_t>(channel)].reset();
        highPassFilters[static_cast<size_t>(channel)].coefficients = makeKWeightingStage2(sampleRate);
        highShelfFilters[static_cast<size_t>(channel)].coefficients = makeKWeightingStage1(sampleRate);
    }

    truePeakHistoryPosition = 0;
    for (auto& channelHistory : truePeakHistory)
        channelHistory.fill(0.0f);

    setLatencySamples(static_cast<int>(std::ceil(truePeakOversampler.getLatencyInSamples())));
    juce::ignoreUnused(samplesPerBlock);
}

void LUmacOSvelerAudioProcessor::releaseResources()
{
    gainDb.reset(sampleRate, 1.0);
    gainDb.setCurrentAndTargetValue(0.0f);
    truePeakOversampler.reset();
    shortTermEnergy.clear();
    shortTermWritePosition = 0;
    shortTermSamples = 0;
    shortTermEnergySum = 0.0;
    gatedBlockEnergies.clear();
    gatedBlockCount = 0;
    momentaryEnergy.clear();
    momentaryWritePosition = 0;
    momentarySamples = 0;
    momentaryEnergySum = 0.0;
    gateHopPosition = 0;
}

bool LUmacOSvelerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainInput = layouts.getChannelSet(true, 0);
    const auto& mainOutput = layouts.getChannelSet(false, 0);
    const auto isSupportedInput = mainInput == juce::AudioChannelSet::mono()
                               || mainInput == juce::AudioChannelSet::stereo()
                               || mainInput == juce::AudioChannelSet::create5point1();
    return isSupportedInput && mainInput == mainOutput;
}

void LUmacOSvelerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (resetMeasurementRequest.exchange(false))
    {
        shortTermWritePosition = 0;
        shortTermSamples = 0;
        shortTermEnergySum = 0.0;
        momentaryWritePosition = 0;
        momentarySamples = 0;
        momentaryEnergySum = 0.0;
        gateHopPosition = 0;
        gatedBlockCount = 0;
    }

    bool finishInputLearning = false;
    switch (inputLearnRequest.exchange(0))
    {
        case 1:
            gatedBlockCount = 0;
            gateHopPosition = 0;
            momentaryWritePosition = 0;
            momentarySamples = 0;
            momentaryEnergySum = 0.0;
            std::fill(momentaryEnergy.begin(), momentaryEnergy.end(), 0.0f);
            inputLearning.store(true);
            break;
        case 2:
            finishInputLearning = inputLearning.load();
            break;
        default:
            break;
    }

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin(getTotalNumInputChannels(), buffer.getNumChannels());
    if (numChannels > 0 && numSamples > 0 && !shortTermEnergy.empty())
    {
        for (auto sample = 0; sample < numSamples; ++sample)
        {
            double sampleEnergy = 0.0;
            for (auto channel = 0; channel < numChannels; ++channel)
            {
                auto value = buffer.getReadPointer(channel)[sample];
                value = highPassFilters[static_cast<size_t>(channel)].processSample(value);
                value = highShelfFilters[static_cast<size_t>(channel)].processSample(value);
                // JUCE 5.1 order is L, R, C, LFE, Ls, Rs. BS.1770 excludes LFE.
                const auto isLfe = numChannels == 6 && channel == 3;
                constexpr auto surroundWeight = 1.4125375; // +1.5 dB
                const auto channelWeight = isLfe ? 0.0 :
                    (numChannels == 6 && (channel == 4 || channel == 5)
                         ? surroundWeight : 1.0);
                sampleEnergy += channelWeight * static_cast<double>(value) * value;
            }

            const auto energy = static_cast<float>(sampleEnergy);

            if (shortTermSamples == shortTermEnergy.size())
                shortTermEnergySum -= shortTermEnergy[shortTermWritePosition];
            else
                ++shortTermSamples;

            shortTermEnergy[shortTermWritePosition] = energy;
            shortTermEnergySum += sampleEnergy;
            shortTermWritePosition = (shortTermWritePosition + 1) % shortTermEnergy.size();

            if (momentarySamples == momentaryEnergy.size())
                momentaryEnergySum -= momentaryEnergy[momentaryWritePosition];
            else
                ++momentarySamples;

            momentaryEnergy[momentaryWritePosition] = energy;
            momentaryEnergySum += sampleEnergy;
            momentaryWritePosition = (momentaryWritePosition + 1) % momentaryEnergy.size();

            if (++gateHopPosition >= gateHopSamples && momentarySamples == momentaryEnergy.size())
            {
                if (gatedBlockCount < gatedBlockEnergies.size())
                    gatedBlockEnergies[gatedBlockCount++] = static_cast<float>(
                        momentaryEnergySum / static_cast<double>(momentarySamples));

                gateHopPosition = 0;
            }
        }

        const auto meanSquare = shortTermEnergySum / static_cast<double>(shortTermSamples);
        const auto shortTermLufs = shortTermSamples == shortTermEnergy.size()
            ? -0.691f + 10.0f * std::log10(static_cast<float>(
                juce::jmax(1.0e-12, meanSquare)))
            : -std::numeric_limits<float>::infinity();

        // The 3-second window matches the R128 short-term time constant. The
        // K-weighting and R128 gating stages will replace this RMS estimator.
        const auto minimumMeasurementSamples = static_cast<size_t>(
            juce::jmin(0.4 * sampleRate, static_cast<double>(shortTermEnergy.size())));
        // BS.1770/R128 gate values are normative and intentionally not exposed
        // as user controls: absolute gate -70 LUFS, relative gate -10 LU.
        constexpr auto absoluteGate = -70.0f;
        constexpr auto relativeGate = -10.0f;
        auto integratedLufs = -std::numeric_limits<float>::infinity();

        if (gatedBlockCount > 0)
        {
            double ungatedEnergy = 0.0;
            size_t ungatedCount = 0;
            const auto absoluteEnergy = std::pow(10.0, (absoluteGate + 0.691) / 10.0);
            for (size_t index = 0; index < gatedBlockCount; ++index)
                if (gatedBlockEnergies[index] >= absoluteEnergy)
                {
                    ungatedEnergy += gatedBlockEnergies[index];
                    ++ungatedCount;
                }

            if (ungatedCount > 0)
            {
                const auto relativeEnergy = (ungatedEnergy / static_cast<double>(ungatedCount))
                    * std::pow(10.0, relativeGate / 10.0);
                double gatedEnergy = 0.0;
                size_t gatedCount = 0;
                for (size_t index = 0; index < gatedBlockCount; ++index)
                    if (gatedBlockEnergies[index] >= juce::jmax(absoluteEnergy, relativeEnergy))
                    {
                        gatedEnergy += gatedBlockEnergies[index];
                        ++gatedCount;
                    }
                if (gatedCount > 0)
                    integratedLufs = -0.691f + 10.0f * std::log10(
                        static_cast<float>(gatedEnergy / static_cast<double>(gatedCount)));
            }
        }

        if (finishInputLearning && std::isfinite(integratedLufs))
        {
            if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(
                    parameters.getParameter(inputLevelParameterId)))
            {
                parameter->setValueNotifyingHost(parameter->convertTo0to1(integratedLufs));
                inputLearning.store(false);
            }
        }
        else if (finishInputLearning)
        {
            inputLearning.store(false);
        }

        if (std::isfinite(shortTermLufs) && shortTermSamples >= minimumMeasurementSamples
            && shortTermLufs >= absoluteGate && (std::isinf(integratedLufs) || shortTermLufs >= integratedLufs + relativeGate))
        {
            const auto targetLevel = getTargetLevelLUFS();
            const auto inputLevel = parameters.getRawParameterValue(inputLevelParameterId)->load();
            const auto fixedGainDb = targetLevel - inputLevel;
            const auto controlledCorrectionDb = inputLevel - shortTermLufs;
            const auto isHighCorrection = shortTermLufs > inputLevel;
            const auto correctionPercent = parameters.getRawParameterValue(
                isHighCorrection ? correctionHighParameterId : correctionLowParameterId)->load() * 0.01f;
            const auto mixMode = juce::jlimit(0, 3, static_cast<int>(std::round(
                parameters.getRawParameterValue(correctionMixModeParameterId)->load() * 3.0f)));
            const auto logarithmicCurve = isHighCorrection ? (mixMode == 2 || mixMode == 3)
                                                            : (mixMode == 1 || mixMode == 3);
            const auto mixRatio = logarithmicCurve
                ? std::log10(1.0f + 9.0f * correctionPercent)
                : correctionPercent;
            const auto correctionDb = juce::jlimit(-24.0f,
                                                   parameters.getRawParameterValue(maxGainParameterId)->load(),
                                                   fixedGainDb + mixRatio * controlledCorrectionDb);
            const auto freezeLevel = parameters.getRawParameterValue(freezeLevelParameterId)->load();
            // Quiet input may be attenuated, but cannot increase the existing
            // program gain until its loudness crosses FREEZE LEVEL.
            if (shortTermLufs >= freezeLevel || correctionDb <= gainDb.getTargetValue())
                gainDb.setTargetValue(correctionDb);
        }
    }

    for (auto sample = 0; sample < numSamples; ++sample)
    {
        const auto gain = juce::Decibels::decibelsToGain(gainDb.getNextValue());
        for (auto channel = 0; channel < numChannels; ++channel)
            buffer.getWritePointer(channel)[sample] *= gain;
        if (numChannels == 6)
            buffer.getWritePointer(3)[sample] *= juce::Decibels::decibelsToGain(
                parameters.getRawParameterValue(lfeGainParameterId)->load());
    }

    const auto internalCeiling = juce::Decibels::decibelsToGain(
        parameters.getRawParameterValue(truePeakParameterId)->load() - 0.2f);
    for (auto sample = 0; sample < numSamples; ++sample)
    {
        auto estimatedPeak = 0.0f;
        for (auto channel = 0; channel < numChannels; ++channel)
            estimatedPeak = juce::jmax(estimatedPeak,
                processTruePeakSample(channel, buffer.getReadPointer(channel)[sample]));
        truePeakHistoryPosition = (truePeakHistoryPosition + 1) % truePeakFirTaps;
        const auto peakGain = estimatedPeak > internalCeiling
            ? internalCeiling / estimatedPeak : 1.0f;
        for (auto channel = 0; channel < numChannels; ++channel)
            buffer.getWritePointer(channel)[sample] *= peakGain;
    }

    auto clippingBlock = juce::dsp::AudioBlock<float>(buffer);
    const auto isOversampled = truePeakOversampler.getOversamplingFactor() > 1;
    if (isOversampled)
        clippingBlock = truePeakOversampler.processSamplesUp(clippingBlock);

    const auto ceiling = juce::Decibels::decibelsToGain(
        parameters.getRawParameterValue(truePeakParameterId)->load());

    // Fixed hard clipper at the selected true-peak ceiling.
    if (clippingBlock.getNumSamples() > 0)
    {
        for (size_t sample = 0; sample < clippingBlock.getNumSamples(); ++sample)
        {
            for (auto channel = 0; channel < numChannels; ++channel)
            {
                auto* channelSamples = clippingBlock.getChannelPointer(static_cast<size_t>(channel));
                channelSamples[sample] = juce::jlimit(-internalCeiling, internalCeiling,
                    channelSamples[sample]);
            }
        }
    }

    if (isOversampled)
    {
        auto outputBlock = juce::dsp::AudioBlock<float>(buffer);
        truePeakOversampler.processSamplesDown(outputBlock);
        for (auto channel = 0; channel < numChannels; ++channel)
            for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
                buffer.getWritePointer(channel)[sample] = juce::jlimit(-ceiling, ceiling,
                    buffer.getReadPointer(channel)[sample]);
    }

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
}

float LUmacOSvelerAudioProcessor::getTargetLevelLUFS() const noexcept
{
    return parameters.getRawParameterValue(targetLevelParameterId)->load();
}

void LUmacOSvelerAudioProcessor::resetParametersToDefaults()
{
    constexpr const char* parameterIds[] =
    {
        targetLevelParameterId, maxGainParameterId, truePeakParameterId, freezeLevelParameterId,
        inputLevelParameterId, correctionHighParameterId, correctionLowParameterId,
        correctionMixModeParameterId, lfeGainParameterId
    };

    for (const auto* parameterId : parameterIds)
    {
        if (auto* parameter = parameters.getParameter(parameterId))
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
    }
}

juce::AudioProcessorEditor* LUmacOSvelerAudioProcessor::createEditor()
{
    return new LUmacOSvelerAudioProcessorEditor(*this);
}

void LUmacOSvelerAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    const auto state = parameters.copyState();
    const auto xml = state.createXml();
    copyXmlToBinary(*xml, destination);
}

void LUmacOSvelerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LUmacOSvelerAudioProcessor();
}
