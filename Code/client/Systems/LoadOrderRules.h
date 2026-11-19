#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

/**
 * @brief Works out the contents of Plugins.txt. This has no game dependencies so it can be unit tested.
 *
 * The game drops the '*' that marks a plugin as active from Plugins.txt, so a mod pack has to be marked active again before
 * every start. Plugins that aren't part of the pack are left as they are: they belong to the player or their mod manager.
 */
namespace LoadOrderRules
{
inline std::string Trim(const std::string& acLine)
{
    const auto cIsSpace = [](const char aChar) { return std::isspace(static_cast<unsigned char>(aChar)) != 0; };
    const auto cBegin = std::find_if_not(acLine.begin(), acLine.end(), cIsSpace);
    const auto cEnd = std::find_if_not(acLine.rbegin(), std::make_reverse_iterator(cBegin), cIsSpace).base();
    return std::string(cBegin, cEnd);
}

inline bool IsPlugin(const std::string& acLine)
{
    return !acLine.empty() && acLine[0] != '#';
}

inline std::string PluginName(const std::string& acLine)
{
    return !acLine.empty() && acLine[0] == '*' ? acLine.substr(1) : acLine;
}

inline bool IsSamePlugin(const std::string& acLeft, const std::string& acRight)
{
    const auto cLower = [](const char aChar) { return std::tolower(static_cast<unsigned char>(aChar)); };
    return std::equal(acLeft.begin(), acLeft.end(), acRight.begin(), acRight.end(), [&](const char aLeft, const char aRight) { return cLower(aLeft) == cLower(aRight); });
}

/**
 * @param acRequired Lines of the pack's list, in load order.
 * @param acExisting Lines of the current Plugins.txt.
 * @return The new lines: comments, the pack's plugins marked active and in order, then everything else untouched.
 */
inline std::vector<std::string> Merge(const std::vector<std::string>& acRequired, const std::vector<std::string>& acExisting)
{
    std::vector<std::string> required;
    for (const auto& cLine : acRequired)
    {
        const auto cTrimmed = Trim(cLine);
        if (IsPlugin(cTrimmed))
            required.push_back(PluginName(cTrimmed));
    }

    std::vector<std::string> result;
    for (const auto& cLine : acExisting)
    {
        const auto cTrimmed = Trim(cLine);
        if (!cTrimmed.empty() && !IsPlugin(cTrimmed))
            result.push_back(cTrimmed);
    }

    for (const auto& cName : required)
        result.push_back("*" + cName);

    for (const auto& cLine : acExisting)
    {
        const auto cTrimmed = Trim(cLine);
        if (!IsPlugin(cTrimmed))
            continue;

        const auto cName = PluginName(cTrimmed);
        if (std::none_of(required.begin(), required.end(), [&cName](const std::string& acName) { return IsSamePlugin(acName, cName); }))
            result.push_back(cTrimmed);
    }

    return result;
}
} // namespace LoadOrderRules
