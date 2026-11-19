#include <Games/Skyrim/Interface/Menus/CompassMarkers.h>

#include <PlayerCharacter.h>

namespace CompassMarkers
{
static std::mutex s_lock;
static std::array<Target, kMaxTargets> s_targets;
static size_t s_targetCount = 0;
static int32_t s_localColour = -1;
static Status s_status{};

void SetTargets(const Target* apTargets, size_t aCount, int32_t aLocalColour) noexcept
{
    std::lock_guard lock(s_lock);
    s_targetCount = std::min(aCount, kMaxTargets);
    std::copy_n(apTargets, s_targetCount, s_targets.begin());
    s_localColour = aLocalColour;
}

Status GetStatus() noexcept
{
    std::lock_guard lock(s_lock);
    return s_status;
}

namespace
{
// A Scaleform value as the game hands it to ActionScript, checked against how the compass update builds its arguments in
// 1.7.104: types 2 bool, 3 number, 4 string, 8 display object, and 0x40 marks a value holding a reference to release.
struct GFxValue
{
    void* pObjectInterface = nullptr;
    uint32_t Type = 0;
    uint32_t Padding = 0;
    union
    {
        double Number;
        const char* String;
        void* pData;
    } Value{};

    static GFxValue Of(double aNumber) noexcept
    {
        GFxValue value;
        value.Type = 3;
        value.Value.Number = aNumber;
        return value;
    }

    static GFxValue Of(const char* apString) noexcept
    {
        GFxValue value;
        value.Type = 4;
        value.Value.String = apString;
        return value;
    }

    [[nodiscard]] bool IsDisplayObject() const noexcept { return (Type & 0x8F) == 8; }

    // GFxValue::ObjectInterface::Invoke, the call the compass update makes for SetCompassMarkers
    bool Invoke(const char* apName, const GFxValue* apArgs, size_t aCount, GFxValue* apResult = nullptr) const noexcept
    {
        TP_THIS_FUNCTION(TInvoke, bool, void, void* apData, GFxValue* apResult, const char* apName, const GFxValue* apArgs, uint64_t aCount, bool aIsDisplayObject);
        POINTER_SKYRIMSE(TInvoke, s_invoke, 82256);
        return pObjectInterface && TiltedPhoques::ThisCall(s_invoke.Get(), pObjectInterface, Value.pData, apResult, apName, apArgs, static_cast<uint64_t>(aCount), IsDisplayObject());
    }

    // GFxValue::ObjectInterface::ObjectRelease, which the game calls before reusing a value that holds a reference
    void Release() noexcept
    {
        TP_THIS_FUNCTION(TRelease, void, void, GFxValue* apValue, void* apData);
        POINTER_SKYRIMSE(TRelease, s_release, 82270);
        if (Type & 0x40)
            TiltedPhoques::ThisCall(s_release.Get(), pObjectInterface, this, Value.pData);
        pObjectInterface = nullptr;
        Type = 0;
    }
};
static_assert(sizeof(GFxValue) == 0x18);

uint32_t& MarkerCount() noexcept
{
    POINTER_SKYRIMSE(uint32_t, s_markerCount, 406159);
    return *s_markerCount.Get();
}

uint32_t PlayerSetFrame() noexcept
{
    POINTER_SKYRIMSE(uint32_t, s_playerSetFrame, 406120);
    return *s_playerSetFrame.Get();
}

// HUDMarkerManager::AddMarker (51728) needs a reference, which a player outside the loaded cells doesn't have,
// so this writes the same slots directly. The manager starts with four GFxValues per marker.
size_t AddTargets(const Target* apTargets, size_t aCount) noexcept
{
    enum MarkerValue : size_t { kHeading, kAlpha, kFrame, kScale, kValueCount };
    constexpr size_t kGFxValueSize = 0x18;
    constexpr uint32_t kMaxMarkers = 0x30;

    TP_THIS_FUNCTION(TSetNumber, void, void, double);
    POINTER_SKYRIMSE(TSetNumber, s_setNumber, 51076);
    POINTER_SKYRIMSE(uint8_t, s_markers, 406154);

    const auto* pPlayer = PlayerCharacter::Get();
    if (!pPlayer)
        return 0;

    size_t added = 0;
    for (; added < aCount && MarkerCount() < kMaxMarkers; ++added)
    {
        const NiPoint3& target = apTargets[added].Position;
        const auto marker = UiRules::MakeCompassMarker(target.x - pPlayer->position.x, target.y - pPlayer->position.y, target.z - pPlayer->position.z);

        auto* pValues = s_markers.Get() + MarkerCount() * kValueCount * kGFxValueSize;
        const auto setValue = [pValues](MarkerValue aValue, double aNumber) {
            TiltedPhoques::ThisCall(s_setNumber.Get(), static_cast<void*>(pValues + aValue * kGFxValueSize), aNumber);
        };
        setValue(kHeading, marker.Heading);
        setValue(kAlpha, marker.Alpha);
        // Players get a disc of their own from the HUD helper, which hides this. Without the helper they look like waypoints.
        setValue(kFrame, static_cast<double>(PlayerSetFrame()));
        setValue(kScale, marker.Scale);

        ++MarkerCount();
    }
    return added;
}

// Loads the HUD helper into the HUD menu that holds the compass. Loading takes a few frames, so this waits before trying
// again, and gives up on a helper that isn't installed.
void LoadHelper(const GFxValue& acHud) noexcept
{
    static const void* s_pHud = nullptr;
    static int s_attempts = 0;
    static std::chrono::steady_clock::time_point s_nextAttempt{};

    if (s_pHud != acHud.Value.pData)
    {
        s_pHud = acHud.Value.pData;
        s_attempts = 0;
    }

    const auto cNow = std::chrono::steady_clock::now();
    if (s_attempts >= 3 || cNow < s_nextAttempt)
        return;
    ++s_attempts;
    s_nextAttempt = cNow + std::chrono::seconds(5);

    GFxValue depth;
    if (!acHud.Invoke("getNextHighestDepth", nullptr, 0, &depth) || depth.Type != 3)
    {
        depth.Release();
        return;
    }

    const GFxValue cArgs[] = {GFxValue::Of("SBT_Compass"), GFxValue::Of(depth.Value.Number)};
    GFxValue clip;
    if (acHud.Invoke("createEmptyMovieClip", cArgs, 2, &clip) && clip.IsDisplayObject())
    {
        // Relative to hudmenu.swf: Data/Interface/SkyrimBetterTogether/compass.swf
        const GFxValue cUrl = GFxValue::Of("SkyrimBetterTogether/compass.swf");
        clip.Invoke("loadMovie", &cUrl, 1);
        spdlog::info("Loading the compass helper into the HUD, attempt {}", s_attempts);
        std::lock_guard lock(s_lock);
        ++s_status.HelperLoads;
    }
    clip.Release();
}

// The HUD helper draws other players as coloured discs and tints each waypoint in its player's colour, once the game has
// made a clip for every marker. Arguments: the local player's colour, the index of the first of these markers, the
// custom marker's frame, then the colour and kind of each.
void Decorate(void* apCompass, uint32_t aFirst, const Target* apTargets, size_t aCount, int32_t aLocalColour) noexcept
{
    // The compass's HUD object, the clip the game calls SetCompassMarkers on
    const auto& hud = *reinterpret_cast<const GFxValue*>(static_cast<uint8_t*>(apCompass) + 0x10);
    if (!hud.IsDisplayObject())
        return;

    std::array<GFxValue, 3 + 2 * kMaxTargets> args;
    args[0] = GFxValue::Of(static_cast<double>(aLocalColour));
    args[1] = GFxValue::Of(static_cast<double>(aFirst));
    args[2] = GFxValue::Of(static_cast<double>(PlayerSetFrame()));
    for (size_t i = 0; i < aCount; ++i)
    {
        args[3 + i * 2] = GFxValue::Of(static_cast<double>(apTargets[i].Colour));
        args[4 + i * 2] = GFxValue::Of(static_cast<double>(apTargets[i].Kind));
    }

    const bool cDecorated = hud.Invoke("SBT_Tint", args.data(), 3 + 2 * aCount);
    if (!cDecorated && (aCount > 0 || aLocalColour >= 0))
        LoadHelper(hud);

    std::lock_guard lock(s_lock);
    s_status.Targets = aCount;
    s_status.Decorated = cDecorated ? aCount : 0;
    s_status.LocalColour = aLocalColour;
}

void (*RealCompassUpdate)(void*);

// HUDMenu fills the marker list, updates each HUD object and then clears the list, so this is the last point to add to it.
void HookCompassUpdate(void* apCompass)
{
    std::array<Target, kMaxTargets> targets;
    size_t count;
    int32_t localColour;
    {
        std::lock_guard lock(s_lock);
        count = s_targetCount;
        std::copy_n(s_targets.begin(), count, targets.begin());
        localColour = s_localColour;
    }

    const uint32_t cFirst = MarkerCount();
    const size_t cAdded = AddTargets(targets.data(), count);
    RealCompassUpdate(apCompass);
    Decorate(apCompass, cFirst, targets.data(), cAdded, localColour);
}

TiltedPhoques::Initializer s_compassHooks(
    []()
    {
        POINTER_SKYRIMSE(void(void*), compassUpdate, 51668);
        RealCompassUpdate = compassUpdate.Get();
        TP_HOOK(&RealCompassUpdate, HookCompassUpdate);
    });
} // namespace
} // namespace CompassMarkers
