#include "AIProcess.h"

std::atomic_uint32_t g_unloadedActivations{};

static TiltedPhoques::Initializer s_activationLifetimeGuard([]
{
    VersionDbPtr<uint8_t> activate(39408);
    constexpr uint8_t expected[]{0x48, 0x8B, 0x46, 0x10, 0xC6, 0x80, 0x70, 0x04, 0x00, 0x00, 0x00};
    if (std::memcmp(activate.Get() + 0x93, expected, sizeof(expected)) != 0)
    {
        spdlog::error("AI activation continuation changed; unload guard not installed");
        return;
    }
    // Activating a door can unload this NPC's process state before Activate returns.
    // Recheck it before clearing the queued-activation flag; retain vanilla handle cleanup.
    struct Guard : TiltedPhoques::CodeGenerator
    {
        Guard(uint8_t* apContinue)
        {
            mov(rax, ptr[rsi + 0x10]);
            test(rax, rax);
            jz("unloaded");
            mov(byte[rax + 0x470], 0);
            jmp_S(apContinue);
            L("unloaded");
            mov(rax, reinterpret_cast<uintptr_t>(&g_unloadedActivations));
            lock(); inc(dword[rax]);
            xor_(eax, eax);
            jmp_S(apContinue);
        }
    } code(activate.Get() + 0x9E);
    TiltedPhoques::Jump(activate.Get() + 0x93, code.getCode());
});

bhkCharacterController* AIProcess::GetCharController() noexcept
{
    TP_THIS_FUNCTION(TGetCharController, bhkCharacterController*, AIProcess);
    POINTER_SKYRIMSE(TGetCharController, getCharController, 39856);
    return TiltedPhoques::ThisCall(getCharController, this);
}

void AIProcess::KnockExplosion(Actor* apActor, const NiPoint3* aSourceLocation, float afMagnitude)
{
    TP_THIS_FUNCTION(TKnockExplosion, void, AIProcess, Actor*, const NiPoint3*, float);
    POINTER_SKYRIMSE(TKnockExplosion, knockExplosion, 39895);
    TiltedPhoques::ThisCall(knockExplosion, this, apActor, aSourceLocation, afMagnitude);
}
