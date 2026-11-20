#include <Games/Skyrim/Interface/IMenu.h>
#include <Games/Skyrim/Interface/UI.h>
#include <Misc/BSFixedString.h>
#include <TiltedOnlinePCH.h>
#include "immersive_launcher/stubs/DllBlocklist.h"

#include <World.h>
#include <Services/OverlayService.h>
#include <SaveLoad.h>

void UI::NotifyPauseChange(bool aPaused)
{
    // UI::ProcessMessages (82082) sends this event when numPausesGame crosses zero.
    // Layout and dispatcher checked against the installed 1.7.104 binary and CommonLibSSE.
    struct MenuModeChangeEvent
    {
        BSFixedString menu;
        uint8_t mode;
    } event{BSFixedString("Shared Helgen"), static_cast<uint8_t>(aPaused)};
    static_assert(offsetof(MenuModeChangeEvent, mode) == 8);
    TP_THIS_FUNCTION(TSendModeChange, void, void, const MenuModeChangeEvent*);
    POINTER_SKYRIMSE(TSendModeChange, s_sendModeChange, 82141);
    TiltedPhoques::ThisCall(s_sendModeChange.Get(), menuModeEvents, &event);
}

UI* UI::Get()
{
    POINTER_SKYRIMSE(UI*, s_instance, 400327);
    return *s_instance.Get();
}

bool UI::GetMenuOpen(const BSFixedString& acName) const
{
    if (acName.data == nullptr)
        return false;

    TP_THIS_FUNCTION(TMenuSystem_IsOpen, bool, const UI, const BSFixedString&);
    POINTER_SKYRIMSE(TMenuSystem_IsOpen, s_isMenuOpen, 82074);

    return TiltedPhoques::ThisCall(s_isMenuOpen.Get(), this, acName);
}

void UI::CloseAllMenus()
{
    TP_THIS_FUNCTION(TUI_CloseAll, void, const UI);
    POINTER_SKYRIMSE(TUI_CloseAll, s_CloseAll, 82088);

    TiltedPhoques::ThisCall(s_CloseAll.Get(), this);
}

BSFixedString* UI::LookupMenuNameByInstance(IMenu* apMenu)
{
    for (auto& it : menuMap)
    {
        if (it.value.spMenu == apMenu)
            return &it.key;
    }
    return nullptr;
}

IMenu* UI::FindMenuByName(const BSFixedString& acName)
{
    for (const auto& it : menuMap)
    {
        if (it.key == acName)
            return it.value.spMenu;
    }
    return nullptr;
}

void UI::DebugLogAllMenus()
{
    for (auto& e : menuStack)
    {
        spdlog::info("Menu {}", e->uiMenuFlags);
    }
}

static void UnfreezeMenu(IMenu* apEntry)
{
    if (apEntry->PausesGame())
        apEntry->ClearFlag(IMenu::kPausesGame);

    if (apEntry->FreezesBackground())
        apEntry->ClearFlag(IMenu::kFreezeFrameBackground);

    if (apEntry->FreezesFramePause())
        apEntry->ClearFlag(IMenu::kFreezeFramePause);
}

static constexpr const char* kAllowList[] = {
    "TweenMenu",     "MagicMenu",     "StatsMenu",     "InventoryMenu", "MessageBoxMenu",
    "ContainerMenu", "FavoritesMenu", "Tutorial Menu"
    // Console remains paused; manual Journal saves are deferred to the save-event phase below.
    //"MapMenu", // MapMenu is disabled till we find a proper fix for first person.
};

// Unpaused elsewhere: the lock menu once its models load, the journal while a manual save waits.
static constexpr const char* kAlsoUnpaused[] = {"Lockpicking Menu", "Journal Menu"};

bool UI::IsUnpausedMenuOpen() const
{
    // Paused, they don't matter: player controls don't run while the game is paused.
    for (const char* name : kAllowList)
        if (GetMenuOpen(BSFixedString(name)))
            return true;
    for (const char* name : kAlsoUnpaused)
        if (GetMenuOpen(BSFixedString(name)))
            return true;
    return false;
}

static void* (*UI_AddToActiveQueue)(UI*, IMenu*, void*);

static void (*RealInitializeLockModels)(IMenu*);
static void InitializeLockModels(IMenu* apMenu)
{
    RealInitializeLockModels(apMenu);
    // Native lock models initialize only on the main thread. Unpausing on menu
    // insertion moves that work to the UI worker and leaves the interface blank.
    auto* ui = UI::Get();
    if (!stubs::g_IsSoulsREActive && World::Get().GetTransport().IsConnected() &&
        ui && apMenu->PausesGame() && ui->GetMenuOpen("Lockpicking Menu"))
    {
        UnfreezeMenu(apMenu);
        if (ui->numPausesGame && --ui->numPausesGame == 0)
            ui->NotifyPauseChange(false);
    }
}

static void* UI_AddToActiveQueue_Hook(UI* apSelf, IMenu* apMenu, void* apFoundItem /*In reality a reference*/)
{
    // if the menu is empty we let the real function handle it.
    if (!apMenu || !World::Get().GetTransport().IsConnected() || stubs::g_IsSoulsREActive)
        return UI_AddToActiveQueue(apSelf, apMenu, apFoundItem);

#if 0
        if (auto* pName = apSelf->LookupMenuNameByInstance(apEntry))
        {
            spdlog::info("Menu requested {}", pName->AsAscii());
        }
#endif

    // NOTE(Force): could also compare by RTTI later on...
    if (g_deferredManualSaveReady && apSelf->FindMenuByName("Journal Menu") == apMenu)
        UnfreezeMenu(apMenu);
    for (const char* item : kAllowList)
    {
        if (auto* pMenu = apSelf->FindMenuByName(item))
        {
            if (pMenu == apMenu)
                UnfreezeMenu(apMenu);
        }
    }

    return UI_AddToActiveQueue(apSelf, apMenu, apFoundItem);
}

using TCallback = void(void*, const BSFixedString*, UIMessage::UI_MESSAGE_TYPE, void*);
static TCallback* UIMessageQueue__AddMessage_Real;

// A menu's show message comes a frame or more before the menu is open, enough for the overlay to draw what goes with it.
void UIMessageQueue__AddMessage(void* a1, const BSFixedString* a2, UIMessage::UI_MESSAGE_TYPE a3, void* a4)
{
    if (a3 == UIMessage::kShow && a2)
        World::Get().GetOverlayService().PrepareMenuShortcut(*a2);
    UIMessageQueue__AddMessage_Real(a1, a2, a3, a4);
}

static TiltedPhoques::Initializer s_s(
    []()
    {
        POINTER_SKYRIMSE(void(IMenu*), initializeLockModels, 51960);
        RealInitializeLockModels = initializeLockModels.Get();
        TP_HOOK(&RealInitializeLockModels, InitializeLockModels);
        // pray that this doesnt fail!
        VersionDbPtr<uint8_t> ProcessHook(82082);
        TiltedPhoques::SwapCall(ProcessHook.Get() + 0x682, UI_AddToActiveQueue, &UI_AddToActiveQueue_Hook);

        // Ignore startup movie
        // TODO: Move me later.
        VersionDbPtr<uint8_t> MainInit(36548);
        TiltedPhoques::Put<uint8_t>(MainInit.Get() + 0xFE, 0xEB);

        // Credits to Skyrim Souls RE for this fix.
        // Allows the favorites menu to be numbered during connect.
        VersionDbPtr<uint8_t> FavoritesCanProcess(51538);
        TiltedPhoques::Put<uint16_t>(FavoritesCanProcess.Get() + 0x15, 0x9090);

        // UIMessageQueue::AddMessage(menu, type, data), checked against 1.7.104: it appends to the queue's 64 slots
        POINTER_SKYRIMSE(TCallback, s_addMessage, 13631);
        UIMessageQueue__AddMessage_Real = s_addMessage.Get();
        TP_HOOK(&UIMessageQueue__AddMessage_Real, UIMessageQueue__AddMessage);

        // Some experiments:

        // This kills the loading spinner
        // TiltedPhoques::Put<uint8_t>(0x1405D51C1, 0xEB);
        // TiltedPhoques::Nop(0x1405D51A2, 5);

        // use 8 threads by default!
        // TiltedPhoques::Put<uint8_t>(0x141E45770, 8);
    });
