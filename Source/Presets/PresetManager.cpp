#include "PresetManager.h"

namespace divinity
{
//==============================================================================
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s)
{
    factory = createFactoryPresets();
    for (auto* p : apvts.processor.getParameters())
        p->addListener (this);
    loadSettings();
    rescan();
}

PresetManager::~PresetManager()
{
    for (auto* p : apvts.processor.getParameters())
        p->removeListener (this);
}

juce::File PresetManager::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
              .getChildFile ("TRIPLE SEVEN AUDIO").getChildFile ("DIVINITY").getChildFile ("Presets");
}

void PresetManager::rescan()
{
    presets.clear();
    for (int i = 0; i < (int) factory.size(); ++i)
    {
        PresetInfo p;
        p.name = factory[(size_t) i].name;
        p.category = factory[(size_t) i].category;
        p.tags = factory[(size_t) i].tags;
        p.description = factory[(size_t) i].description;
        p.author = "TRIPLE SEVEN AUDIO";
        p.isFactory = true;
        p.factoryIndex = i;
        presets.add (p);
    }

    auto folder = getUserPresetFolder();
    if (folder.isDirectory())
    {
        auto files = folder.findChildFiles (juce::File::findFiles, true, juce::String ("*") + kExtension);
        files.sort();
        for (auto& f : files)
        {
            if (auto xml = juce::XmlDocument::parse (f))
            {
                if (! xml->hasTagName ("DivinityPreset")) continue;
                PresetInfo p;
                p.name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
                p.category = xml->getStringAttribute ("category");
                if (p.category.isEmpty())
                    p.category = f.getParentDirectory() == folder ? "User" : f.getParentDirectory().getFileName();
                p.author = xml->getStringAttribute ("author", "User");
                p.tags = xml->getStringAttribute ("tags");
                p.description = xml->getStringAttribute ("description");
                p.isFactory = false;
                p.file = f;
                presets.add (p);
            }
        }
    }

    // keep the current index pointing at the same named preset
    currentIndex = -1;
    for (int i = 0; i < presets.size(); ++i)
        if (presets[i].name == currentName) { currentIndex = i; break; }

    sendChangeMessage();
}

//==============================================================================
void PresetManager::parameterValueChanged (int, float)
{
    if (! loading.load())
        modified = true;
}

void PresetManager::setAllToDefault()
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
}

void PresetManager::setParamReal (const juce::String& id, float realValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
    else
        jassertfalse;   // unknown parameter id
}

void PresetManager::applyFactoryPreset (const FactoryPreset& fp)
{
    setAllToDefault();
    for (const auto& [id, value] : fp.values)
    {
        if (value.isString())
        {
            if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
            {
                const int idx = c->choices.indexOf (value.toString());
                jassert (idx >= 0);
                if (idx >= 0) c->setValueNotifyingHost (c->convertTo0to1 ((float) idx));
            }
            else jassertfalse;
        }
        else
            setParamReal (id, (float) (double) value);
    }
}

bool PresetManager::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, presets.size())) return false;
    const auto& info = presets.getReference (index);

    bool ok = false;
    if (auto* um = apvts.undoManager) um->beginNewTransaction ("Load preset");
    loading = true;
    if (info.isFactory)
    {
        applyFactoryPreset (factory[(size_t) info.factoryIndex]);
        ok = true;
    }
    else if (auto xml = juce::XmlDocument::parse (info.file))
        ok = applyPresetXml (*xml);
    loading = false;

    if (ok)
    {
        currentIndex = index;
        currentName = info.name;
        modified = false;
    }
    sendChangeMessage();
    return ok;
}

void PresetManager::loadInit()
{
    if (auto* um = apvts.undoManager) um->beginNewTransaction ("Init patch");
    loading = true;
    setAllToDefault();
    loading = false;
    currentIndex = -1;
    currentName = "Init";
    modified = false;
    sendChangeMessage();
}

//==============================================================================
std::unique_ptr<juce::XmlElement> PresetManager::createPresetXml (const PresetInfo& info) const
{
    auto xml = std::make_unique<juce::XmlElement> ("DivinityPreset");
    xml->setAttribute ("version", kStateVersion);
    xml->setAttribute ("name", info.name);
    xml->setAttribute ("author", info.author.isNotEmpty() ? info.author : juce::String ("User"));
    xml->setAttribute ("category", info.category);
    xml->setAttribute ("tags", info.tags);
    xml->setAttribute ("description", info.description);
    auto* params = xml->createNewChildElement ("Params");
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            auto* e = params->createNewChildElement ("P");
            e->setAttribute ("id", rp->getParameterID());
            e->setAttribute ("v", (double) rp->convertFrom0to1 (rp->getValue()));
        }
    return xml;
}

bool PresetManager::applyPresetXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName ("DivinityPreset")) return false;
    // version > kStateVersion: newer file — load what we recognise, ignore the rest.
    auto* params = xml.getChildByName ("Params");
    if (params == nullptr) return false;

    const bool wasLoading = loading.exchange (true);
    setAllToDefault();
    for (auto* e : params->getChildWithTagNameIterator ("P"))
        if (auto* p = apvts.getParameter (e->getStringAttribute ("id")))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) e->getDoubleAttribute ("v")));
    loading = wasLoading;
    return true;
}

juce::String PresetManager::saveUserPreset (const juce::String& rawName, const juce::String& category)
{
    const auto name = rawName.trim();
    if (name.isEmpty()) return "Please give the preset a name.";

    auto folder = getUserPresetFolder();
    if (! folder.createDirectory()) return "Could not create the preset folder:\n" + folder.getFullPathName();

    PresetInfo info;
    info.name = name;
    info.category = category.isNotEmpty() ? category : juce::String ("User");
    info.author = "User";
    info.isFactory = false;

    auto file = folder.getChildFile (juce::File::createLegalFileName (name) + kExtension);
    auto xml = createPresetXml (info);
    if (! xml->writeTo (file)) return "Could not write the preset file:\n" + file.getFullPathName();

    currentName = name;
    modified = false;
    rescan();
    return {};
}

juce::String PresetManager::importPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("DivinityPreset"))
        return "\"" + file.getFileName() + "\" is not a DIVINITY preset.";

    auto folder = getUserPresetFolder();
    folder.createDirectory();
    auto dest = folder.getChildFile (file.getFileName());
    if (dest != file && ! file.copyFileTo (dest))
        return "Could not copy the preset into the user folder.";

    rescan();
    for (int i = 0; i < presets.size(); ++i)
        if (! presets[i].isFactory && presets[i].file == dest)
            return loadPreset (i) ? juce::String() : juce::String ("The preset could not be loaded.");
    return {};
}

juce::String PresetManager::exportCurrent (const juce::File& file) const
{
    PresetInfo info;
    info.name = currentName;
    info.category = juce::isPositiveAndBelow (currentIndex, presets.size()) ? presets[currentIndex].category : juce::String ("User");
    auto xml = createPresetXml (info);
    return xml->writeTo (file) ? juce::String() : "Could not write " + file.getFullPathName();
}

void PresetManager::setCurrentFromState (const juce::String& name, bool wasModified)
{
    currentName = name.isNotEmpty() ? name : juce::String ("Init");
    currentIndex = -1;
    for (int i = 0; i < presets.size(); ++i)
        if (presets[i].name == currentName) { currentIndex = i; break; }
    modified = wasModified;
    sendChangeMessage();
}

//==============================================================================
juce::String PresetManager::favouriteKey (const PresetInfo& p) const
{
    return (p.isFactory ? "F:" : "U:") + p.name;
}

bool PresetManager::isFavourite (const PresetInfo& p) const { return favourites.contains (favouriteKey (p)); }

void PresetManager::toggleFavourite (const PresetInfo& p)
{
    const auto key = favouriteKey (p);
    if (favourites.contains (key)) favourites.removeString (key);
    else favourites.add (key);
    saveSettings();
    sendChangeMessage();
}

void PresetManager::loadSettings()
{
    auto f = getUserPresetFolder().getParentDirectory().getChildFile ("settings.xml");
    if (auto xml = juce::XmlDocument::parse (f))
        for (auto* e : xml->getChildWithTagNameIterator ("Favourite"))
            favourites.addIfNotAlreadyThere (e->getStringAttribute ("key"));
}

void PresetManager::saveSettings()
{
    auto dir = getUserPresetFolder().getParentDirectory();
    dir.createDirectory();
    juce::XmlElement xml ("DivinitySettings");
    for (auto& k : favourites)
        xml.createNewChildElement ("Favourite")->setAttribute ("key", k);
    xml.writeTo (dir.getChildFile ("settings.xml"));
}

} // namespace divinity
