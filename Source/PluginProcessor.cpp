#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace divinity
{
//==============================================================================
DivinityProcessor::DivinityProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "PARAMETERS", createParameterLayout()),
      presets (apvts),
      engine (apvts, *bank, *factorySamples, userSample, userTables),
      fx (apvts),
      arp (apvts),
      stepSeq (apvts)
{
    apvts.state.setProperty (fxOrderProperty, defaultFxOrder().joinIntoString (","), nullptr);
    apvts.state.addListener (this);
    // Start on the first factory preset (Celestial Pad) rather than a bare init patch.
    presets.loadPreset (0);
    undo.clearUndoHistory();
    for (auto& c : ccMap) c.store (-1);
    applyMidiMapString (settings::midiMap());
    apvts.state.setProperty (midiMapProperty, midiMapString(), nullptr);
    startTimer (100);
}

//==============================================================================
juce::String DivinityProcessor::loadUserWavetable (int osc, const juce::File& file, bool selectIt)
{
    if (! juce::isPositiveAndBelow (osc, kNumOscs)) return "No such oscillator.";
    auto err = userTables[(size_t) osc].load (file);
    if (err.isNotEmpty()) return err;
    apvts.state.setProperty (userTableProperty (osc), file.getFullPathName(), nullptr);
    if (selectIt)
    {
        auto set = [this] (const juce::String& id, float real) { if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (real)); };
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ids::osc (osc, "table"))))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) (c->choices.size() - 1)));
        set (ids::osc (osc, "engine"), (float) OscEngine::Wavetable);
        set (ids::osc (osc, "on"), 1.0f);
    }
    return {};
}

void DivinityProcessor::clearUserWavetable (int osc)
{
    apvts.state.setProperty (userTableProperty (osc), juce::String(), nullptr);
}

//==============================================================================
// MIDI learn
void DivinityProcessor::startMidiLearn (const juce::String& paramID)
{
    const auto& ps = getParameters();
    for (int i = 0; i < ps.size(); ++i)
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ps[i]); rp != nullptr && rp->getParameterID() == paramID) { learnParam.store (i); return; }
}

bool DivinityProcessor::isLearning (const juce::String& paramID) const
{
    const int i = learnParam.load();
    if (i < 0) return false;
    auto* rp = dynamic_cast<juce::RangedAudioParameter*> (getParameters()[i]);
    return rp != nullptr && rp->getParameterID() == paramID;
}

int DivinityProcessor::getMidiCcFor (const juce::String& paramID) const
{
    const auto& ps = getParameters();
    for (int cc = 0; cc < 128; ++cc)
    {
        const int i = ccMap[(size_t) cc].load();
        if (i >= 0)
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ps[i]); rp != nullptr && rp->getParameterID() == paramID) return cc;
    }
    return -1;
}

void DivinityProcessor::forgetMidiMapping (const juce::String& paramID)
{
    const int cc = getMidiCcFor (paramID);
    if (cc >= 0) { ccMap[(size_t) cc].store (-1); midiMapDirty.store (true); }
}

void DivinityProcessor::clearAllMidiMappings()
{
    for (auto& c : ccMap) c.store (-1);
    midiMapDirty.store (true);
}

juce::String DivinityProcessor::midiMapString() const
{
    juce::StringArray items;
    const auto& ps = getParameters();
    for (int cc = 0; cc < 128; ++cc)
    {
        const int i = ccMap[(size_t) cc].load();
        if (i >= 0)
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ps[i]))
                items.add (juce::String (cc) + "=" + rp->getParameterID());
    }
    return items.joinIntoString (",");
}

void DivinityProcessor::applyMidiMapString (const juce::String& s)
{
    for (auto& c : ccMap) c.store (-1);
    const auto& ps = getParameters();
    for (auto& item : juce::StringArray::fromTokens (s, ",", ""))
    {
        const int cc = item.upToFirstOccurrenceOf ("=", false, false).getIntValue();
        const auto id = item.fromFirstOccurrenceOf ("=", false, false);
        if (! juce::isPositiveAndBelow (cc, 128)) continue;
        for (int i = 0; i < ps.size(); ++i)
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ps[i]); rp != nullptr && rp->getParameterID() == id) { ccMap[(size_t) cc].store (i); break; }
    }
    ++midiMapVersion;
}

//==============================================================================
int DivinityProcessor::wantedQuality() const
{
    const int q = juce::jlimit (0, 2, (int) std::lround (apvts.getRawParameterValue (ids::quality)->load()));
    if (isNonRealtime() && apvts.getRawParameterValue (ids::qualityRender)->load() > 0.5f) return (int) Quality::HQ;
    return q;
}

void DivinityProcessor::configureQuality (int q)
{
    // call with processing suspended (or from prepareToPlay)
    const bool hq = q == (int) Quality::HQ;
    if (hq)
    {
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampler->initProcessing ((size_t) baseBlock);
        engine.prepare (baseRate * 2.0, baseBlock * 2);
        hqMidi.ensureSize (4096);
        setLatencySamples (juce::roundToInt (oversampler->getLatencyInSamples()));
    }
    else
    {
        oversampler.reset();
        engine.prepare (baseRate, baseBlock);
        setLatencySamples (0);
    }
    runningQuality.store (q);
}

void DivinityProcessor::timerCallback()
{
    if (++timerTicks % 20 == 0)
    {
        userSample.collectGarbage();
        for (auto& t : userTables) t.collectGarbage();
    }
    if (midiMapDirty.exchange (false))
    {
        const auto m = midiMapString();
        apvts.state.setProperty (midiMapProperty, m, nullptr);
        settings::setMidiMap (m);            // your controller mappings follow you into every DIVINITY instance
        ++midiMapVersion;
    }
    // quality switches need the engine re-prepared at the new rate: do it here with audio suspended
    const int want = wantedQuality();
    const bool hqNow = runningQuality.load() == (int) Quality::HQ, hqWant = want == (int) Quality::HQ;
    if (hqNow != hqWant && getSampleRate() > 0.0)
    {
        suspendProcessing (true);
        configureQuality (want);
        suspendProcessing (false);
    }
    else runningQuality.store (want);
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
    for (int o = 0; o < kNumOscs; ++o)
        if (tree == apvts.state && property == userTableProperty (o))
        {
            const auto path = tree.getProperty (property).toString();
            if (path.isEmpty()) userTables[(size_t) o].clear();
            else if (juce::File (path) != userTables[(size_t) o].getFile() && juce::File::isAbsolutePath (path))
                userTables[(size_t) o].load (juce::File (path));
        }
    if (tree == apvts.state && property == midiMapProperty && midiMapString() != tree.getProperty (property).toString())
        applyMidiMapString (tree.getProperty (property).toString());
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
    baseRate = sampleRate; baseBlock = juce::jmax (1, samplesPerBlock);
    configureQuality (wantedQuality());
    fx.prepare (sampleRate, samplesPerBlock);
    arp.prepare (sampleRate);
    stepSeq.prepare (sampleRate);
    loadMeasurer.reset (sampleRate, samplesPerBlock);
    uiMidi.reset (sampleRate);
    uiMidiBuffer.ensureSize (2048);
    // latency: 0, or the half-band filter's few samples in HQ (set by configureQuality)
}

void DivinityProcessor::renderSynth (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const TransportInfo& transport)
{
    if (oversampler == nullptr) { engine.process (buffer, midi, transport); return; }
    // HQ: render the voices at 2x into the oversampler's own buffer, then filter + decimate into `buffer`
    const int n = buffer.getNumSamples();
    for (int start = 0; start < n; start += baseBlock)
    {
        const int len = juce::jmin (baseBlock, n - start);
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) start, (size_t) len);
        block.clear();
        auto up = oversampler->processSamplesUp (block);
        float* chans[2] = { up.getChannelPointer (0), up.getChannelPointer (1) };
        juce::AudioBuffer<float> hq (chans, 2, (int) up.getNumSamples());
        hqMidi.clear();
        for (const auto meta : midi)
            if (meta.samplePosition >= start && meta.samplePosition < start + len)
                hqMidi.addEvent (meta.getMessage(), (meta.samplePosition - start) * 2);
        engine.process (hq, hqMidi, transport);
        oversampler->processSamplesDown (block);
    }
}

void DivinityProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::AudioProcessLoadMeasurer::ScopedTimer timer (loadMeasurer, buffer.getNumSamples());

    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    // MIDI learn: the next CC after "learn" claims the parameter; mapped CCs drive their parameter
    for (const auto meta : midi)
    {
        const auto& m = meta.getMessage();
        if (! m.isController() || m.getControllerNumber() >= 120) continue;
        const int cc = m.getControllerNumber();
        if (const int learn = learnParam.exchange (-1); learn >= 0)
        {
            for (auto& c : ccMap) if (c.load() == learn) c.store (-1);   // one CC per parameter
            ccMap[(size_t) cc].store (learn);
            midiMapDirty.store (true);
        }
        if (const int idx = ccMap[(size_t) cc].load(); idx >= 0)
            if (auto* p = getParameters()[idx])
                p->setValueNotifyingHost ((float) m.getControllerValue() / 127.0f);
    }
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
    renderSynth (buffer, midi, transport);
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
    auto st = apvts.copyState();
    st.setProperty (midiMapProperty, midiMapString(), nullptr);   // newest mappings, even before the UI timer wrote them
    if (auto params = st.createXml())
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
        for (int o = 0; o < kNumOscs; ++o)
            apvts.state.setProperty (userTableProperty (o), params->getStringAttribute (userTableProperty (o).toString()), nullptr);
        {
            // a project without its own mappings keeps your global controller map
            const auto m = params->getStringAttribute (midiMapProperty.toString());
            apvts.state.setProperty (midiMapProperty, m.isNotEmpty() ? m : settings::midiMap(), nullptr);
        }
    }

    setFxOrder (juce::StringArray::fromTokens (params->getStringAttribute (fxOrderProperty.toString(), defaultFxOrder().joinIntoString (",")), ",", ""), false);
    uiWorkspace = xml->getStringAttribute ("uiWorkspace", "CREATE");
    if (! juce::StringArray { "CREATE", "EFFECTS", "PLAY", "SOUL", "LAB" }.contains (uiWorkspace)) uiWorkspace = "CREATE";
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
