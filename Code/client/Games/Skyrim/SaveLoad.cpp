#include <TiltedOnlinePCH.h>

#include <SaveLoad.h>
#include <Interface/UI.h>
#include "immersive_launcher/stubs/DllBlocklist.h"
#include <mutex>

bool g_deferredManualSaveReady = false;

namespace
{
using TSaveGame = bool(BGSSaveLoadManager*, uint32_t, uint32_t, const char*);
TSaveGame* RealSaveGame = nullptr;
std::mutex s_saveMutex;
struct PendingSave
{
    BGSSaveLoadManager* Manager{};
    uint32_t Flags{};
    String Name;
} s_pendingSave;

bool HookSaveGame(BGSSaveLoadManager* apManager, uint32_t aType, uint32_t aFlags, const char* apName)
{
    // Type 2 is a manual save. The UI worker cannot synchronously save a running world.
    // Autosaves/quicksaves already use the engine's safe save-event phase.
    auto* ui = UI::Get();
    if (aType != 2 || !ui || ui->numPausesGame)
        return RealSaveGame(apManager, aType, aFlags, apName);
    {
        std::lock_guard lock(s_saveMutex);
        if (s_pendingSave.Manager)
            return false;
        s_pendingSave = {apManager, aFlags, apName ? apName : ""};
    }
    using TRequestSave = void(BGSSaveLoadManager*, uint32_t);
    POINTER_SKYRIMSE(TRequestSave, requestSave, 35769);
    requestSave.Get()(apManager, 0xF0004000);
    return true;
}

void CompleteManualSave()
{
    PendingSave save;
    {
        std::lock_guard lock(s_saveMutex);
        if (!s_pendingSave.Manager)
            return;
        save = s_pendingSave;
    }
    const bool saved = RealSaveGame(save.Manager, 2, save.Flags, save.Name.empty() ? nullptr : save.Name.c_str());
    spdlog::info("Deferred manual save completed: {}", saved);
    std::lock_guard lock(s_saveMutex);
    s_pendingSave = {};
}
}

static TiltedPhoques::Initializer s_manualSaveHook([]
{
    if (stubs::g_IsSoulsREActive)
        return;
    VersionDbPtr<uint8_t> processEvents(35772);
    // Verified against the installed 1.7.104 binary. Fail closed on a different dispatcher.
    constexpr uint8_t expected[]{0x81, 0xF9, 0x00, 0x08, 0x00, 0xF0, 0x0F, 0x85, 0x49, 0x01, 0x00, 0x00};
    if (std::memcmp(processEvents.Get() + 0x5D5, expected, sizeof(expected)) != 0)
    {
        spdlog::error("Manual save dispatcher changed; keeping Journal paused");
        return;
    }
    // Same safe event phase used by Skyrim Souls RE's save handler; preserve vanilla quicksave.
    struct SaveEvent : TiltedPhoques::CodeGenerator
    {
        SaveEvent(uint8_t* apProcess)
        {
            cmp(ecx, 0xF0000800);
            jne("manual");
            jmp_S(apProcess + 0x5E1);
            L("manual");
            cmp(ecx, 0xF0004000);
            jne("done");
            mov(rax, reinterpret_cast<uintptr_t>(&CompleteManualSave));
            call(rax);
            L("done");
            jmp_S(apProcess + 0x72A);
        }
    } code(processEvents.Get());
    POINTER_SKYRIMSE(TSaveGame, saveGame, 35727);
    RealSaveGame = saveGame.Get();
    TP_HOOK(&RealSaveGame, HookSaveGame);
    TiltedPhoques::Jump(processEvents.Get() + 0x5D5, code.getCode());
    g_deferredManualSaveReady = true;
});

void BGSSaveLoadManager::Save(SaveData* apData)
{
    apData->flags |= 4;

    const char* cSaveName = "";
    if (apData->saveName)
        cSaveName = apData->saveName;
}
