#pragma once

#include <juce_core/juce_core.h>

#include "BeatProtocol.h"
#include "L10n.h"

namespace dt
{

// ---------------------------------------------------------------------------
// Settings shared between the overlay (the only writer of settings.xml) and the
// companion VST, which lives in the host's process and only reads it.
//
// The VST does not use PropertiesFile here on purpose: it must not rewrite a
// file it does not own. A tolerant read of the few keys it needs is enough.
// ---------------------------------------------------------------------------
inline juce::File danceOverlayDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("DanceOverlay");
}

inline juce::File overlaySettingsFile()
{
    return danceOverlayDir().getChildFile ("settings.xml");
}

// Language lives in its own one-line file: both processes may write it, and
// PropertiesFile would rewrite the whole settings.xml on every save.
inline juce::File overlayLanguageFile()
{
    return danceOverlayDir().getChildFile ("language.txt");
}

struct AppConfig
{
    int udpPort       = defaultPort;
    int hostTimeoutMs = (int) dt::hostTimeoutMs;
    int refreshHz     = 60;
    int systemRetries = 3;
};

// PropertiesFile stores its entries as
//   <PROPERTIES><VALUE name="key" val="value"/>...</PROPERTIES>
inline juce::String readSettingsValue (const juce::XmlElement& root, const juce::String& key)
{
    for (auto* child : root.getChildIterator())
        if (child->hasTagName ("VALUE") && child->getStringAttribute ("name") == key)
            return child->getStringAttribute ("val");

    return {};
}

inline AppConfig loadAppConfig()
{
    AppConfig c;

    const auto file = overlaySettingsFile();
    if (! file.existsAsFile())
        return c;

    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return c;

    const auto port = readSettingsValue (*xml, "udpPort").getIntValue();
    if (port >= 1024 && port <= 65535)
        c.udpPort = port;

    const auto timeout = readSettingsValue (*xml, "hostTimeoutMs").getIntValue();
    if (timeout >= 50 && timeout <= 5000)
        c.hostTimeoutMs = timeout;

    const auto hz = readSettingsValue (*xml, "refreshHz").getIntValue();
    if (hz >= 15 && hz <= 240)
        c.refreshHz = hz;

    const auto retries = readSettingsValue (*xml, "systemRetries").getIntValue();
    if (retries >= 0 && retries <= 20)
        c.systemRetries = retries;

    return c;
}

// Body may be empty (first run) -> follow the OS locale.
inline Language loadLanguage()
{
    const auto file = overlayLanguageFile();

    if (! file.existsAsFile())
        return systemLanguage();

    const auto tag = file.loadFileAsString().trim().toLowerCase();
    if (tag.startsWith ("zh")) return Language::Zh;
    if (tag.startsWith ("ja")) return Language::Ja;
    if (tag.startsWith ("en")) return Language::En;

    return systemLanguage();
}

inline void storeLanguage (Language l)
{
    danceOverlayDir().createDirectory();
    overlayLanguageFile().replaceWithText (juce::String (languageTag (l)) + "\n");
}

} // namespace dt
