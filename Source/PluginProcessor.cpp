#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace divinity
{
//==============================================================================
DivinityProcessor::DivinityProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "PARAMETERS", createParameterLayout()),
      presets (apvts),
      engine (apvts, *bank, *factorySamples, userSample),
      fx (apvts),
      arp (apvts),
      stepSeq (apvts)
{
    apvts.state.setProperty (fxOrderProperty, defaultFxOrder().joinIntoString (","), nullptr);
    apvts.state.addListener (this);
    // Start on the first factory preset (Celestial Pad) rather than a bare init patch.
    presets.loadPreset (0);
    undo.clearUndoHistory();
    startTimer (2000);
}

juce::String DivinityProcessor::loadUserSample (const juce::File& file, bool selectIt)
{
    auto err = userSample.load (file);
    if (err.isNotEmpty()) return err;
    apvts.state.setProperty (userSampleProperty, file.getFullPathName(), nullptr);
    if (selectIt)
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ids::smpSample)))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) (c->choices.size() - 1)));
        if (auto* on = apvts.getParameter (ids::smpOn)) on->setValueNotifyingHost (1.0f);
    }
    return {};
}

juce::StringArray DivinityProcessor::getFxOrder() const
{
    return sanitiseFxOrder (juce::StringArray::fromTokens (apvts.state.getProperty (fxOrderProperty).toString(), ",", ""));
}

void DivinityProcessor::setFxOrder (const juce::StringArray& keys, bool undoable)
{
    if (undoable) undo.beginNewTransaction ("Reorder effects");
    apvts.state.setProperty (fxOrderProperty, sanitiseFxOrder (keys).joinIntoString (","), undoable ? &undo : nullptr);
}

void DivinityProcessor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree == apvts.state && property == fxOrderProperty)
    {
        fx.setOrder (juce::StringArray::fromTokens (tree.getProperty (property).toString(), ",", ""));
        if (! presets.isLoading()) presets.markModified();
    }
    if (tree == apvts.state && property == userSampleProperty)
    {
        const auto path = tree.getProperty (property).toString();
        if (path.isEmpty()) userSample.clear();
        else if (juce::File (path) != userSample.getFile() && juce::File::isAbsolutePath (path))
            userSample.load (juce::File (path));   // a missing file just leaves the slot empty (the UI shows it)
    }
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
    arp.prepare (sampleRate);
    stepSeq.prepare (sampleRate);
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
        arp.reset();
        stepSeq.reset();
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

    // STEP sequencer in TRIGGER mode replaces the arpeggiator while it runs
    const bool trig = stepSeq.isTriggering();
    if (trig != wasStepTriggering) { engine.allNotesOff (false); arp.reset(); wasStepTriggering = trig; }
    stepSeq.process (midi, n, transport);
    if (! trig) arp.process (midi, n, transport);
    hostBpm.store (transport.bpm);
    midiPreview.process (midi, n, getSampleRate(), transport.bpm, *factorySamples);   // MIDI generator audition
    engine.process (buffer, midi, transport);
    scopePre.push (buffer.getReadPointer (0), buffer.getReadPointer (buffer.getNumChannels() > 1 ? 1 : 0), n);
    fx.process (buffer, transport.bpm, engine.getGlobalMods(), engine.getSnapshot().macro);
    midiPreview.renderDrums (buffer);

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
    root.setAttribute ("uiAngelTips", uiAngelTips);
    root.setAttribute ("uiAngelOpen", uiAngelOpen);
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

    // Sessions saved before Sound Engine 2.0 have no engine_mode: keep them on the
    // Classic engine so old projects play back exactly as they were made.
    bool hasEngineMode = false;
    for (auto* e : params->getChildWithTagNameIterator ("PARAM"))
        if (e->getStringAttribute ("id") == ids::engineMode) { hasEngineMode = true; break; }
    if (! hasEngineMode)
        if (auto* em = apvts.getParameter (ids::engineMode))
            em->setValueNotifyingHost (em->convertTo0to1 ((float) EngineMode::Classic));
    applyPre07Defaults (*params, "PARAM", "id", apvts);

    // the user's own sample (if any) travels with the session as a file path
    {
        const auto path = params->getStringAttribute (userSampleProperty.toString());
        apvts.state.setProperty (userSampleProperty, path, nullptr);
    }

    setFxOrder (juce::StringArray::fromTokens (params->getStringAttribute (fxOrderProperty.toString(), defaultFxOrder().joinIntoString (",")), ",", ""), false);
    uiWorkspace = xml->getStringAttribute ("uiWorkspace", "CREATE");
    if (! juce::StringArray { "CREATE", "EFFECTS", "PLAY", "SOUL", "LAB" }.contains (uiWorkspace)) uiWorkspace = "CREATE";
    uiScale = (float) juce::jlimit (0.5, 2.0, xml->getDoubleAttribute ("uiScale", 1.0));
    uiAngelTips = xml->getBoolAttribute ("uiAngelTips", true);
    uiAngelOpen = xml->getBoolAttribute ("uiAngelOpen", false);
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
