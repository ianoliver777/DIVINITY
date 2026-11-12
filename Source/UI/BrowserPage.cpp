// BROWSER workspace: category tiles | search + preset list | details.
#include "Pages.h"
#include "../PluginProcessor.h"

namespace divinity::ui
{
using namespace colours;

namespace
{
    const juce::StringArray& browserCategories()
    {
        static const juce::StringArray c { "ALL", "REESE", "BASS", "LEAD", "SYNTH", "PAD", "TEXTURE", "SEQUENCE", "PERCUSSION", "FX", "WAVE", "TRAP", "TECHNO", "DUBSTEP", "GARAGE", "DARK GARAGE", "DNB", "CINEMATIC", "FAVOURITES", "USER" };
        return c;
    }

    class CategoryTiles : public Panel
    {
    public:
        explicit CategoryTiles (DivinityProcessor& p) : Panel ("CATEGORIES"), proc (p) {}
        std::function<void (const juce::String&)> onSelect;
        juce::String selected { "ALL" };
        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            const auto& cats = browserCategories();
            for (int i = 0; i < cats.size(); ++i)
            {
                auto r = tile (i).toFloat();
                const bool sel = cats[i] == selected;
                g.setColour (sel ? accentDeep : panel2.withAlpha (i == hover ? 0.95f : 0.55f)); g.fillRoundedRectangle (r, 6.0f);
                g.setColour (sel ? accent : border); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
                g.setColour (sel ? text : textDim);
                g.setFont (font (14.0f, Weight::Medium, 0.06f));
                g.drawText (cats[i], r.toNearestInt().reduced (14, 0), juce::Justification::centredLeft);
                g.setFont (font (11.5f));
                g.drawText (juce::String (count (cats[i])), r.toNearestInt().reduced (14, 0), juce::Justification::centredRight);
            }
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            for (int i = 0; i < browserCategories().size(); ++i)
                if (tile (i).contains (e.getPosition())) { selected = browserCategories()[i]; repaint(); if (onSelect) onSelect (selected); }
        }
        void mouseMove (const juce::MouseEvent& e) override
        {
            int h = -1;
            for (int i = 0; i < browserCategories().size(); ++i) if (tile (i).contains (e.getPosition())) h = i;
            if (h != hover) { hover = h; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    private:
        juce::Rectangle<int> tile (int i) const { const int h = (getHeight() - 50) / browserCategories().size(); return { 10, 44 + i * h, getWidth() - 20, h - 5 }; }
        int count (const juce::String& cat) const
        {
            int n = 0;
            for (auto& pr : proc.presets.getPresets())
                if (cat == "ALL" || (cat == "FAVOURITES" && proc.presets.isFavourite (pr)) || (cat == "USER" && ! pr.isFactory)
                    || pr.category.equalsIgnoreCase (cat)) ++n;
            return n;
        }
        DivinityProcessor& proc;
        int hover = -1;
    };

    class PresetTable : public Panel, private juce::ListBoxModel, private juce::ChangeListener
    {
    public:
        explicit PresetTable (DivinityProcessor& p) : Panel ("PRESETS"), proc (p)
        {
            search.setTextToShowWhenEmpty ("Search names, tags and descriptions...", textFaint);
            search.setFont (font (14.0f));
            search.setIndents (12, 8);
            search.onTextChange = [this] { refresh(); };
            addAndMakeVisible (search);
            list.setModel (this);
            list.setRowHeight (32);
            list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
            addAndMakeVisible (list);
            proc.presets.addChangeListener (this);
            refresh();
        }
        ~PresetTable() override { proc.presets.removeChangeListener (this); }
        std::function<void (int)> onHighlight;
        juce::String category { "ALL" };
        void refresh()
        {
            const auto& all = proc.presets.getPresets();
            const auto q = search.getText().trim();
            filtered.clearQuick();
            for (int i = 0; i < all.size(); ++i)
            {
                const auto& pr = all.getReference (i);
                if (category == "FAVOURITES" && ! proc.presets.isFavourite (pr)) continue;
                if (category == "USER" && pr.isFactory) continue;
                if (category != "ALL" && category != "FAVOURITES" && category != "USER" && ! pr.category.equalsIgnoreCase (category)) continue;
                if (q.isNotEmpty() && ! (pr.name.containsIgnoreCase (q) || pr.category.containsIgnoreCase (q)
                                         || pr.tags.containsIgnoreCase (q) || pr.description.containsIgnoreCase (q))) continue;
                filtered.add (i);
            }
            list.updateContent();
            list.repaint();
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            g.setColour (textDim);
            g.setFont (font (11.5f));
            g.drawText (juce::String (filtered.size()) + " presets", getWidth() - 220, 12, 200, 20, juce::Justification::centredRight);
            g.setFont (font (10.0f, Weight::Medium, 0.1f));
            g.drawText ("NAME", 24, 88, 200, 14, juce::Justification::centredLeft);
            g.drawText ("CATEGORY", getWidth() / 2 - 40, 88, 120, 14, juce::Justification::centredLeft);
            g.drawText ("CHARACTER", getWidth() / 2 + 90, 88, 200, 14, juce::Justification::centredLeft);
        }
        void resized() override
        {
            search.setBounds (14, 44, getWidth() - 28, 34);
            list.setBounds (8, 106, getWidth() - 16, getHeight() - 114);
        }
    private:
        int getNumRows() override { return filtered.size(); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selectedRow) override
        {
            if (! juce::isPositiveAndBelow (row, filtered.size())) return;
            const int index = filtered[row];
            const auto& pr = proc.presets.getPresets().getReference (index);
            const bool current = index == proc.presets.getCurrentIndex();
            if (current || selectedRow)
            {
                g.setColour ((current ? accentDeep : panel2).withAlpha (0.7f));
                g.fillRoundedRectangle (2.0f, 1.0f, (float) w - 4.0f, (float) h - 2.0f, 4.0f);
            }
            g.setFont (font (14.0f, current ? Weight::Medium : Weight::Regular));
            g.setColour (current ? text : text.withAlpha (0.88f));
            g.drawText (juce::String (index + 1).paddedLeft ('0', 3) + "   " + pr.name, 14, 0, w / 2 - 70, h, juce::Justification::centredLeft, true);
            g.setFont (font (12.0f, Weight::Medium, 0.05f));
            g.setColour (accent);
            g.drawText (pr.category.toUpperCase(), w / 2 - 48, 0, 120, h, juce::Justification::centredLeft, true);
            g.setColour (textDim);
            g.setFont (font (12.0f));
            g.drawText (pr.tags, w / 2 + 82, 0, w / 2 - 120, h, juce::Justification::centredLeft, true);
            if (proc.presets.isFavourite (pr))
            {
                auto heart = icons::heart();
                heart.applyTransform (heart.getTransformToScaleToFit (juce::Rectangle<float> ((float) w - 26.0f, (float) h * 0.5f - 7.0f, 14.0f, 14.0f), true));
                g.setColour (accent); g.fillPath (heart);
            }
        }
        void listBoxItemClicked (int row, const juce::MouseEvent& e) override
        {
            if (! juce::isPositiveAndBelow (row, filtered.size())) return;
            const int index = filtered[row];
            if (e.mods.isPopupMenu())
            {
                const auto pr = proc.presets.getPresets()[index];
                juce::PopupMenu m;
                m.addItem (1, proc.presets.isFavourite (pr) ? "Remove from favourites" : "Add to favourites");
                m.showMenuAsync ({}, [this, pr] (int r) { if (r == 1) proc.presets.toggleFavourite (pr); });
                return;
            }
            proc.presets.loadPreset (index);
            if (onHighlight) onHighlight (index);
        }
        void selectedRowsChanged (int row) override { if (juce::isPositiveAndBelow (row, filtered.size()) && onHighlight) onHighlight (filtered[row]); }
        void returnKeyPressed (int row) override { if (juce::isPositiveAndBelow (row, filtered.size())) proc.presets.loadPreset (filtered[row]); }
        void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }

        DivinityProcessor& proc;
        juce::TextEditor search;
        juce::ListBox list;
        juce::Array<int> filtered;
    };

    class PresetDetails : public Panel, private juce::ChangeListener
    {
    public:
        explicit PresetDetails (DivinityProcessor& p) : Panel ("DETAILS"), proc (p), scope (p)
        {
            fav.setButtonText ("FAVOURITE");
            fav.onClick = [this] { if (juce::isPositiveAndBelow (shown, proc.presets.getPresets().size())) proc.presets.toggleFavourite (proc.presets.getPresets()[shown]); repaint(); };
            load.setButtonText ("LOAD");
            load.onClick = [this] { if (shown >= 0) proc.presets.loadPreset (shown); };
            for (auto* b : { &fav, &load }) { b->getProperties().set ("fontSize", 13.0); addAndMakeVisible (b); }
            addAndMakeVisible (scope);
            proc.presets.addChangeListener (this);
            show (proc.presets.getCurrentIndex());
        }
        ~PresetDetails() override { proc.presets.removeChangeListener (this); }
        void show (int index) { shown = index; repaint(); }
        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            const auto& all = proc.presets.getPresets();
            if (! juce::isPositiveAndBelow (shown, all.size())) return;
            const auto& pr = all.getReference (shown);
            g.setColour (text);
            g.setFont (font (24.0f, Weight::SemiBold));
            g.drawFittedText (pr.name, 18, 46, getWidth() - 36, 34, juce::Justification::centredLeft, 1);
            g.setColour (accent);
            g.setFont (font (13.0f, Weight::Medium, 0.1f));
            g.drawText (pr.category.toUpperCase() + (pr.isFactory ? "   -   FACTORY" : "   -   USER"), 18, 82, getWidth() - 36, 18, juce::Justification::centredLeft);
            g.setColour (textDim);
            g.setFont (font (12.5f));
            g.drawFittedText (pr.tags, 18, 106, getWidth() - 36, 20, juce::Justification::centredLeft, 1);
            g.setColour (text.withAlpha (0.9f));
            g.setFont (font (13.5f));
            g.drawFittedText (pr.description, 18, 134, getWidth() - 36, 120, juce::Justification::topLeft, 7);
        }
        void resized() override
        {
            scope.setBounds (14, getHeight() - 230, getWidth() - 28, 160);
            fav.setBounds (14, getHeight() - 56, (getWidth() - 40) / 2, 40);
            load.setBounds (26 + (getWidth() - 40) / 2, getHeight() - 56, (getWidth() - 40) / 2, 40);
        }
    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override { show (proc.presets.getCurrentIndex()); }
        DivinityProcessor& proc;
        ScopeDisplay scope;
        juce::TextButton fav, load;
        int shown = -1;
    };
}

struct BrowserPage::Impl
{
    Impl (DivinityProcessor& p, juce::Component& owner) : tiles (p), table (p), details (p)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &tiles, &table, &details }) owner.addAndMakeVisible (c);
        tiles.onSelect = [this] (const juce::String& c) { table.category = c; table.refresh(); };
        table.onHighlight = [this] (int i) { details.show (i); };
    }
    CategoryTiles tiles;
    PresetTable table;
    PresetDetails details;
};

BrowserPage::BrowserPage (DivinityProcessor& p) : impl (std::make_unique<Impl> (p, *this)) {}
BrowserPage::~BrowserPage() = default;
void BrowserPage::paint (juce::Graphics&) {}
void BrowserPage::resized()
{
    auto& m = *impl;
    const int w = getWidth(), h = getHeight();
    m.tiles.setBounds (0, 0, 240, h);
    m.details.setBounds (w - 380, 0, 380, h);
    m.table.setBounds (252, 0, w - 252 - 392, h);
}

} // namespace divinity::ui
