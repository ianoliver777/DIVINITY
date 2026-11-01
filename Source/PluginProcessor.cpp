#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace divinity
{
//==============================================================================
DivinityProcessor::DivinityProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "PARAMETERS", createParameterLayout()),
      presets (apvts),
      engine (apvts, *bank),
      fx (apvts)
{
    // Start on the first factory preset (Celestial Pad) rather than a bare init patch.
    presets.loadPreset (0);
    undo.clearUndoHistory();
}

bool DivinityProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void DivinityProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    fx.prepare (sampleRate, samplesPerBlock);
    loadMeasurer.reset (sampleRate, samplesPerBlock);
    uiMidi.reset (sampleRate);
    uiMidiBuffer.ensureSize (2048);
    setLatencySamples (0);   // Phase 1 processing is zero-latency (ADAA saturation, no lookahead)
}

void DivinityProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::AudioProcessLoadMeasurer::ScopedTimer timer (loadMeasurer, buffer.getNumSamples());

    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    uiMidiBuffer.clear();
    uiMidi.removeNextBlockOfMessages (uiMidiBuffer, n);
    midi.addEvents (uiMidiBuffer, 0, n, 0);

    if (panicRequested.exchange (false))
    {
        engine.allNotesOff (true);
        fx.reset();
    }

    TransportInfo transport;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) transport.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) { transport.ppqAtBlockStart = *ppq; transport.hasPpq = true; }
            transport.isPlaying = pos->getIsPlaying();
        }

    engine.process (buffer, midi, transport);
    fx.process (buffer, transport.bpm, engine.getGlobalMods(), engine.getSnapshot().macro);

    // meters + scope
    const float* L = buffer.getReadPointer (0);
    const float* R = buffer.getReadPointer (buffer.getNumChannels() > 1 ? 1 : 0);
    const float decay = std::pow (0.5f, (float) n / (float) (getSampleRate() * 0.15 + 1.0));
    meterL = juce::jmax (meterL.load() * decay, buffer.getMagnitude (0, 0, n));
    meterR = juce::jmax (meterR.load() * decay, buffer.getMagnitude (buffer.getNumChannels() > 1 ? 1 : 0, 0, n));
    scope.push (L, R, n);
    cpuLoad = (float) loadMeasurer.getLoadAsProportion();
}

//==============================================================================
void DivinityProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("DIVINITY");
    root.setAttribute ("stateVersion", kStateVersion);
    root.setAttribute ("presetName", presets.getCurrentName());
    root.setAttribute ("presetModified", presets.isModified());
    root.setAttribute ("uiWorkspace", uiWorkspace);
    root.setAttribute ("uiScale", (double) uiScale);
    if (auto params = apvts.copyState().createXml())
        root.addChildElement (params.release());
    copyXmlToBinary (root, destData);
}

void DivinityProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("DIVINITY"))
        return;   // unknown / corrupt data: keep the current sound rather than wiping it

    auto* params = xml->getChildByName (apvts.state.getType());
    if (params == nullptr) return;

    // Parameter-by-parameter restore: anything missing from an older session
    // falls back to its default, anything unknown from a newer one is ignored.
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr) continue;
        float norm = rp->getDefaultValue();
        for (auto* e : params->getChildWithTagNameIterator ("PARAM"))
            if (e->getStringAttribute ("id") == rp->getParameterID())
            {
                norm = rp->convertTo0to1 ((float) e->getDoubleAttribute ("value"));
                break;
            }
        rp->setValueNotifyingHost (norm);
    }

    uiWorkspace = xml->getStringAttribute ("uiWorkspace", "DESIGN");
    uiScale = (float) juce::jlimit (0.6, 2.0, xml->getDoubleAttribute ("uiScale", 1.0));
    presets.setCurrentFromState (xml->getStringAttribute ("presetName", "Init"), xml->getBoolAttribute ("presetModified", false));
    undo.clearUndoHistory();
}

juce::AudioProcessorEditor* DivinityProcessor::createEditor()
{
    return new DivinityEditor (*this);
}

} // namespace divinity

//==============================================================================
#if ! DIVINITY_TESTS
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new divinity::DivinityProcessor();
}
#endif
