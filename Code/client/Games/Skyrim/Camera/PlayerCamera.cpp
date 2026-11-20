#include <Services/HelgenService.h>
#include <Interface/UI.h>

#include <Camera/PlayerCamera.h>
#include <NetImmerse/NiCamera.h>
#include <TiltedOnlinePCH.h>

PlayerCamera* PlayerCamera::Get() noexcept
{
    POINTER_SKYRIMSE(PlayerCamera*, s_instance, 400802);
    return *(s_instance.Get());
}

bool PlayerCamera::IsFirstPerson() noexcept
{
    TP_THIS_FUNCTION(TIsFirstPerson, void, PlayerCamera, void*, void*, double*);
    POINTER_SKYRIMSE(TIsFirstPerson, isFirstPerson, 21600);

    double firstPerson = 0.0;
    TiltedPhoques::ThisCall(isFirstPerson, this, nullptr, nullptr, &firstPerson);

    return firstPerson == 1.0;
}

NiPoint3 PlayerCamera::GetPosition() noexcept
{
    TP_THIS_FUNCTION(TGetPosition, void, PlayerCamera, NiPoint3*);
    POINTER_SKYRIMSE(TGetPosition, getPosition, 50786);

    NiPoint3 position{};
    TiltedPhoques::ThisCall(getPosition, this, &position);

    return position;
}

bool PlayerCamera::WorldPtToScreenPt3(const NiPoint3& in, NiPoint3& out, float zeroTolerance /* = 1e-5f */)
{
    auto* pCam = GetNiCamera();
    if (cameraNode && pCam)
    {
        return pCam->WorldPtToScreenPt3(in, out, zeroTolerance);
    }

    return false;
}

void PlayerCamera::ForceFirstPerson() noexcept
{
    TP_THIS_FUNCTION(TForceFirstPerson, void, PlayerCamera);
    POINTER_SKYRIMSE(TForceFirstPerson, forceFirstPerson, 50790);
    TiltedPhoques::ThisCall(forceFirstPerson, this);
}

void PlayerCamera::ForceThirdPerson() noexcept
{
    TP_THIS_FUNCTION(TForceThirdPerson, void, PlayerCamera);
    POINTER_SKYRIMSE(TForceThirdPerson, forceThirdPerson, 50796);
    TiltedPhoques::ThisCall(forceThirdPerson, this);
}

TP_THIS_FUNCTION(TUpdatePlayerCamera, void, PlayerCamera);
TUpdatePlayerCamera* RealUpdatePlayerCamera = nullptr;
void TP_MAKE_THISCALL(HookUpdatePlayerCamera, PlayerCamera)
{
    // RaceSexCamera owns the view during creation. Unpausing the world must not overwrite it.
    if (HelgenService::HoldScene(0xCD68E) && UI::Get()->GetMenuOpen(BSFixedString("RaceSex Menu")))
        return;
    TiltedPhoques::ThisCall(RealUpdatePlayerCamera, apThis);
}

struct InputEvent;
TP_THIS_FUNCTION(TCameraCanProcess, bool, void, InputEvent*);
TCameraCanProcess* RealThirdPersonCanProcess = nullptr;
TCameraCanProcess* RealFirstPersonCanProcess = nullptr;

// These take the zoom and POV events: the wheel and the view key (checked in 1.7.104, both compare the event against
// UserEvents' togglePOV and zoom names). Vanilla pauses the game for the menus the fork keeps running, so the camera
// never sees input there; here the wheel scrolling the Favorites list also zoomed, and left first person.
// Third person's handler also serves the horse, dragon and bleedout cameras.
bool TP_MAKE_THISCALL(HookThirdPersonCanProcess, void, InputEvent* apEvent)
{
    return TiltedPhoques::ThisCall(RealThirdPersonCanProcess, apThis, apEvent) && !UI::Get()->IsUnpausedMenuOpen();
}

bool TP_MAKE_THISCALL(HookFirstPersonCanProcess, void, InputEvent* apEvent)
{
    return TiltedPhoques::ThisCall(RealFirstPersonCanProcess, apThis, apEvent) && !UI::Get()->IsUnpausedMenuOpen();
}

static TiltedPhoques::Initializer s_creationCameraHook([] {
    POINTER_SKYRIMSE(TUpdatePlayerCamera, update, 50784);
    RealUpdatePlayerCamera = update.Get();
    TP_HOOK(&RealUpdatePlayerCamera, HookUpdatePlayerCamera);

    // PlayerInputHandler::CanProcess of ThirdPersonState and FirstPersonState, vtable slot 1 at offset 0x20
    POINTER_SKYRIMSE(TCameraCanProcess, thirdPersonCanProcess, 50905);
    POINTER_SKYRIMSE(TCameraCanProcess, firstPersonCanProcess, 50729);
    RealThirdPersonCanProcess = thirdPersonCanProcess.Get();
    RealFirstPersonCanProcess = firstPersonCanProcess.Get();
    TP_HOOK(&RealThirdPersonCanProcess, HookThirdPersonCanProcess);
    TP_HOOK(&RealFirstPersonCanProcess, HookFirstPersonCanProcess);
});
