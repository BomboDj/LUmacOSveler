#include "PluginProcessor.h"
#include "PluginEditor.h"

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
        -20.0f,
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
        -20.0f,
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
    return layout;
}

void LUmacOSvelerAudioProcessor::prepareToPlay(double newSampleRate, int samplesPerBlock)
{
    // The host may use any rate in this range, including non-standard rates.
    // Clamp invalid host values to keep all DSP state safely initialised.
    sampleRate = juce::jlimit(minimumSampleRate, maximumSampleRate, newSampleRate);
    gainDb.reset(sampleRate, 1.0);
    gainDb.setCurrentAndTargetValue(0.0f);
    truePeakOversampler.reset();
    truePeakOversampler.initProcessing(static_cast<size_t>(samplesPerBlock));
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
    gatedBlockEnergies.assign(9000, 0.0f);
    gatedBlockCount = 0;

    for (auto channel = 0; channel < 2; ++channel)
    {
        highPassFilters[static_cast<size_t>(channel)].reset();
        highShelfFilters[static_cast<size_t>(channel)].reset();
        highPassFilters[static_cast<size_t>(channel)].coefficients =
            juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 38.1358f, 0.5f);
        highShelfFilters[static_cast<size_t>(channel)].coefficients =
            juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 1681.974f, 0.7071f,
            juce::Decibels::decibelsToGain(4.0f));
    }

    const auto oversamplingFactor = static_cast<double>(truePeakOversampler.getOversamplingFactor());
    const auto lookAheadSamples = juce::jmax(1, static_cast<int>(
        std::round(sampleRate * 0.0001 * oversamplingFactor)));
    limiterDelay.assign(static_cast<size_t>(lookAheadSamples + 1), { 0.0f, 0.0f });
    limiterWritePosition = 0;
    limiterGainReductionDb = 0.0f;
    gainReductionDb.store(0.0f);
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
    limiterDelay.clear();
    limiterWritePosition = 0;
    limiterGainReductionDb = 0.0f;
    gainReductionDb.store(0.0f);
}

bool LUmacOSvelerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainInput = layouts.getChannelSet(true, 0);
    const auto& mainOutput = layouts.getChannelSet(false, 0);
    const auto isSupportedInput = mainInput == juce::AudioChannelSet::mono()
                               || mainInput == juce::AudioChannelSet::stereo();
    return isSupportedInput && mainInput == mainOutput;
}

void LUmacOSvelerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

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
                sampleEnergy += static_cast<double>(value) * value;
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
        const auto shortTermLufs = -0.691f
            + 10.0f * std::log10(static_cast<float>(juce::jmax(1.0e-12, meanSquare)));

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
    }

    auto limitingBlock = juce::dsp::AudioBlock<float>(buffer);
    const auto isOversampled = truePeakOversampler.getOversamplingFactor() > 1;
    if (isOversampled)
        limitingBlock = truePeakOversampler.processSamplesUp(limitingBlock);

    // Fixed end-of-chain limiter: 0 dB output, 0.1 ms lookahead, 0.1 dB knee
    // and 0.1 ms release. The detector is linked across channels. The true
    // peak parameter moves the preceding threshold below the fixed 0 dB wall.
    if (!limiterDelay.empty() && limitingBlock.getNumSamples() > 0)
    {
        const auto delaySize = limiterDelay.size();
        constexpr auto truePeakSafetyMarginDb = 0.2f;
        const auto truePeakTargetDb = parameters.getRawParameterValue(truePeakParameterId)->load()
            - truePeakSafetyMarginDb;
        const auto limiterSampleRate = sampleRate * (isOversampled ? 4.0 : 1.0);
        const auto releaseCoefficient = std::exp(static_cast<float>(
            -1.0 / (0.0001 * limiterSampleRate)));
        float blockReductionDb = 0.0f;

        for (size_t sample = 0; sample < limitingBlock.getNumSamples(); ++sample)
        {
            const auto readPosition = (limiterWritePosition + 1) % delaySize;
            auto peak = 0.0f;
            for (auto channel = 0; channel < numChannels; ++channel)
            {
                auto* channelSamples = limitingBlock.getChannelPointer(static_cast<size_t>(channel));
                limiterDelay[limiterWritePosition][static_cast<size_t>(channel)] = channelSamples[sample];
                peak = juce::jmax(peak, std::abs(channelSamples[sample]));
            }

            const auto peakDb = juce::Decibels::gainToDecibels(peak, -160.0f);
            constexpr auto kneeWidthDb = 0.1f;
            const auto halfKneeDb = kneeWidthDb * 0.5f;
            const auto distanceFromTargetDb = peakDb - truePeakTargetDb;
            float desiredReductionDb = 0.0f;

            if (distanceFromTargetDb > halfKneeDb)
                desiredReductionDb = distanceFromTargetDb;
            else if (distanceFromTargetDb > -halfKneeDb)
            {
                const auto distanceIntoKnee = distanceFromTargetDb + halfKneeDb;
                desiredReductionDb = (distanceIntoKnee * distanceIntoKnee)
                    / (2.0f * kneeWidthDb);
            }

            if (desiredReductionDb > limiterGainReductionDb)
                limiterGainReductionDb = desiredReductionDb;
            else
                limiterGainReductionDb *= releaseCoefficient;

            blockReductionDb = juce::jmax(blockReductionDb, limiterGainReductionDb);
            const auto limiterGain = juce::Decibels::decibelsToGain(-limiterGainReductionDb);
            for (auto channel = 0; channel < numChannels; ++channel)
            {
                auto* channelSamples = limitingBlock.getChannelPointer(static_cast<size_t>(channel));
                channelSamples[sample] = juce::jlimit(-1.0f, 1.0f,
                    limiterDelay[readPosition][static_cast<size_t>(channel)] * limiterGain);
            }

            limiterWritePosition = (limiterWritePosition + 1) % delaySize;
        }

        gainReductionDb.store(juce::jlimit(0.0f, 12.0f, blockReductionDb));
    }

    if (isOversampled)
    {
        auto outputBlock = juce::dsp::AudioBlock<float>(buffer);
        truePeakOversampler.processSamplesDown(outputBlock);
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
        correctionMixModeParameterId
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
