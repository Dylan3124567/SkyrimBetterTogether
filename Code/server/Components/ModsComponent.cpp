
#include <Components.h>
#include <es_loader/ESLoader.h>

#include <algorithm>
#include <cctype>

namespace
{
// Windows and the game don't care about case, and plugins.txt spellings differ between installs. Two spellings of a plugin must
// share an id, each client only learns the ids of its own list and everything else can't be resolved.
String GetModKey(const String& acFilename) noexcept
{
    String key = acFilename;
    std::transform(key.begin(), key.end(), key.begin(), [](const unsigned char aCharacter) { return static_cast<char>(std::tolower(aCharacter)); });
    return key;
}
} // namespace

uint32_t ModsComponent::AddStandard(const String& acpFilename) noexcept
{
    const String cKey = GetModKey(acpFilename);
    const auto itor = m_standardMods.find(cKey);
    if (itor != std::end(m_standardMods))
    {
        itor.value().refCount++;
        return itor->second.id;
    }

    const auto id = m_seed++;
    m_standardMods.emplace(cKey, Entry{id, 1});

    return id;
}

uint32_t ModsComponent::AddLite(const String& acpFilename) noexcept
{
    const String cKey = GetModKey(acpFilename);
    const auto itor = m_liteMods.find(cKey);
    if (itor != std::end(m_liteMods))
    {
        itor.value().refCount++;
        return itor->second.id;
    }

    const auto id = m_seed++;
    m_liteMods.emplace(cKey, Entry{id, 1});

    return id;
}

void ModsComponent::AddServerMod(const ESLoader::PluginData& acData)
{
    // kind of a hack since we want to store both, so we take the two byte value
    m_serverMods.emplace(acData.m_filename, Entry{acData.m_liteId, 1});
}

bool ModsComponent::IsInstalled(const String& acpFilename) const noexcept
{
    auto it = std::find_if(m_serverMods.begin(), m_serverMods.end(), [&](const TModList::value_type& aEntry) { return aEntry.first == acpFilename; });

    return it != m_serverMods.end();
}
