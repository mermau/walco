#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <algorithm>
#include <vector>

//==============================================================================
// Pick-up salvati dall'utente con il nome della chitarra ("Tele", "SG"...).
// Sono globali: un file nelle impostazioni dell'utente, condiviso da tutti i
// progetti e da tutte le istanze del plugin. Va usato solo dal thread dei messaggi.
class PickupLibrary
{
public:
    struct Entry { juce::String name; float rKohm, lHenry; };

    PickupLibrary()
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "FTG-1";
        o.folderName          = "Collateral Effects/FTG-1";
        o.filenameSuffix      = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        file = std::make_unique<juce::PropertiesFile> (o);

        // la prima volta riprende le chitarre già salvate in FYN-2
        if (! file->containsKey ("pickups"))
        {
            auto old = o; old.applicationName = "FYN-2"; old.folderName = "Collateral Effects/FYN-2";
            juce::PropertiesFile previous (old);
            if (auto xml = previous.getXmlValue ("pickups"))
            {
                file->setValue ("pickups", xml.get());
                file->saveIfNeeded();
            }
        }
        reload();
    }

    // rilegge il file (un'altra istanza potrebbe averlo modificato)
    void reload()
    {
        entries.clear();
        if (auto xml = file->getXmlValue ("pickups"))
            for (auto* e : xml->getChildWithTagNameIterator ("PICKUP"))
                entries.push_back ({ e->getStringAttribute ("name"),
                                     (float) e->getDoubleAttribute ("r", 8.0),
                                     (float) e->getDoubleAttribute ("l", 4.0) });
        std::sort (entries.begin(), entries.end(),
                   [] (const Entry& a, const Entry& b) { return a.name.compareNatural (b.name) < 0; });
        ++version;
    }

    const std::vector<Entry>& getEntries() const { return entries; }
    int getVersion() const { return version; }

    int indexOf (const juce::String& name) const
    {
        for (size_t i = 0; i < entries.size(); ++i)
            if (entries[i].name.equalsIgnoreCase (name)) return (int) i;
        return -1;
    }

    // aggiunge o sovrascrive
    void save (const juce::String& name, float rKohm, float lHenry)
    {
        reload();
        if (const int i = indexOf (name); i >= 0) entries[(size_t) i] = { name, rKohm, lHenry };
        else entries.push_back ({ name, rKohm, lHenry });
        write();
    }

    void remove (const juce::String& name)
    {
        reload();
        if (const int i = indexOf (name); i >= 0) entries.erase (entries.begin() + i);
        write();
    }

private:
    void write()
    {
        juce::XmlElement xml ("PICKUPS");
        for (auto& e : entries)
        {
            auto* p = xml.createNewChildElement ("PICKUP");
            p->setAttribute ("name", e.name);
            p->setAttribute ("r", juce::String (e.rKohm, 2));
            p->setAttribute ("l", juce::String (e.lHenry, 2));
        }
        file->setValue ("pickups", &xml);
        file->saveIfNeeded();
        reload();
    }

    std::unique_ptr<juce::PropertiesFile> file;
    std::vector<Entry> entries;
    int version = 0;
};
