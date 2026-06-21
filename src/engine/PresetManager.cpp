#include "PresetManager.h"
#include "ProcessorRack.h"
#include "ModuleFactory.h"

PresetManager::PresetManager (ProcessorRack& rackRef) : rack (rackRef) {}

// ─────────────────────────────────────────────────────────────────────────────
//  Factory preset table
// ─────────────────────────────────────────────────────────────────────────────
const std::vector<std::pair<juce::String, juce::String>>& PresetManager::factoryTable ()
{
    // XML must match ProcessorRack::getState / setState format:
    //   <Rack>
    //     <Module type="N" uid="N" bypassed="0" wetDry="1.0" [module params...]/>
    //   </Rack>
    static const std::vector<std::pair<juce::String, juce::String>> table =
    {
        {
            "Init",
            R"(<Rack/>)"
        },
        {
            "Vocal Chain",
            R"(<Rack>
  <Module type="2" uid="1" bypassed="0" wetDry="1.0"
          threshold="-40.0" range="60.0" attack="5.0" hold="40.0" release="80.0"/>
  <Module type="3" uid="2" bypassed="0" wetDry="1.0"
          threshold="-20.0" ratio="3.0" knee="6.0" attack="10.0" release="120.0" makeup="4.0" mix="1.0" character="0"/>
  <Module type="11" uid="3" bypassed="0" wetDry="1.0"
          freq="8000.0" threshold="-20.0" range="8.0" attack="1.0" release="50.0"/>
  <Module type="4" uid="4" bypassed="0" wetDry="1.0"
          ceiling="-1.0" release="50.0" lookahead="2.0"/>
</Rack>)"
        },
        {
            "Noise Cleanup",
            R"(<Rack>
  <Module type="9" uid="1" bypassed="0" wetDry="1.0"/>
  <Module type="10" uid="2" bypassed="0" wetDry="1.0"/>
  <Module type="3" uid="3" bypassed="0" wetDry="1.0"
          threshold="-24.0" ratio="3.0" knee="6.0" attack="15.0" release="150.0" makeup="2.0" mix="1.0" character="0"/>
</Rack>)"
        },
        {
            "Podcast Ready",
            R"(<Rack>
  <Module type="9" uid="1" bypassed="0" wetDry="1.0"/>
  <Module type="10" uid="2" bypassed="0" wetDry="1.0"/>
  <Module type="3" uid="3" bypassed="0" wetDry="1.0"
          threshold="-18.0" ratio="4.0" knee="4.0" attack="8.0" release="100.0" makeup="4.0" mix="1.0" character="0"/>
  <Module type="4" uid="4" bypassed="0" wetDry="1.0"
          ceiling="-1.0" release="80.0" lookahead="2.0"/>
</Rack>)"
        },
        {
            "Master Bus",
            R"(<Rack>
  <Module type="3" uid="1" bypassed="0" wetDry="1.0"
          threshold="-12.0" ratio="2.0" knee="8.0" attack="20.0" release="200.0" makeup="2.0" mix="1.0" character="1"/>
  <Module type="4" uid="2" bypassed="0" wetDry="1.0"
          ceiling="-0.3" release="80.0" lookahead="2.0"/>
</Rack>)"
        },
    };
    return table;
}

// ─────────────────────────────────────────────────────────────────────────────
juce::File PresetManager::getPresetDirectory ()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("AuClear/Presets");
}

PresetManager::PresetLists PresetManager::getLists () const
{
    PresetLists result;

    for (const auto& [name, xml] : factoryTable ())
        result.factory.add (name);

    const juce::File dir = getPresetDirectory ();
    if (dir.isDirectory ())
    {
        for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.xml"))
            result.user.add (f.getFileNameWithoutExtension ());
        result.user.sort (true);
    }

    return result;
}

bool PresetManager::load (const juce::String& name)
{
    for (const auto& [n, xml] : factoryTable ())
    {
        if (n == name)
        {
            if (auto parsed = juce::XmlDocument::parse (xml))
            {
                const auto tree = juce::ValueTree::fromXml (*parsed);
                if (tree.isValid ())
                {
                    rack.setState (tree, makeModule);
                    return true;
                }
            }
            return false;
        }
    }

    const juce::File f = getPresetDirectory ().getChildFile (name + ".xml");
    if (!f.existsAsFile ())
        return false;

    if (auto parsed = juce::XmlDocument::parse (f))
    {
        const auto tree = juce::ValueTree::fromXml (*parsed);
        if (tree.isValid ())
        {
            rack.setState (tree, makeModule);
            return true;
        }
    }
    return false;
}

bool PresetManager::save (const juce::String& name)
{
    const juce::File dir = getPresetDirectory ();
    if (!dir.createDirectory ())
        return false;

    juce::ValueTree tree ("Rack");
    rack.getState (tree);

    if (auto xml = tree.createXml ())
        return xml->writeTo (dir.getChildFile (name + ".xml"), juce::XmlElement::TextFormat{});

    return false;
}

bool PresetManager::remove (const juce::String& name)
{
    const juce::File f = getPresetDirectory ().getChildFile (name + ".xml");
    return f.existsAsFile () && f.deleteFile ();
}
