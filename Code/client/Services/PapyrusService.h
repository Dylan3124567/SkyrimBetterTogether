#pragma once

#include <mutex>

#include <Misc/GameVM.h>

struct TESForm;
struct TESObjectREFR;
struct PapyrusFunctionRegisterEvent;

/**
 * @brief Handles registering and executing Papyrus functions.
 */
struct PapyrusService
{
    PapyrusService(entt::dispatcher& aDispatcher) noexcept;
    ~PapyrusService() noexcept = default;

    TP_NOCOPYMOVE(PapyrusService);

    const void* Get(const String& acNamespace, const String& acFunction) const noexcept;

    void HandlePapyrusFunctionEvent(const PapyrusFunctionRegisterEvent&) noexcept;

private:
    Map<String, void*> m_functions;
    // The VM registers natives on its own thread while game threads ask for them during a load
    mutable std::mutex m_lock;

    entt::scoped_connection m_papyrusFunctionRegisterConnection;
};

// Natives register with the VM as it starts, which on a save load is after the game may already call one of them (a
// resurrection during the load asked for IsDead). One asked for too early is looked up again on the next call.
inline const void* ResolvePapyrusFunction(const char* acNamespace, const char* acFunction) noexcept
{
    if (!acNamespace || !acFunction)
        return nullptr;

    return World::Get().ctx().at<PapyrusService>().Get(acNamespace, acFunction);
}

template <class Return, class Type, class... Args> struct PapyrusFunction
{
    using TFunction = Return(__fastcall*)(BSScript::IVirtualMachine*, uint32_t, const Type*, Args...);

    PapyrusFunction(const void* apAddress)
        : m_pFunction(reinterpret_cast<TFunction>(apAddress))
    {
    }

    PapyrusFunction(const char* acNamespace, const char* acFunction)
        : m_namespace(acNamespace)
        , m_function(acFunction)
    {
    }

    Return operator()(const Type* apThis, Args... args) const noexcept
    {
        if (!m_pFunction)
            m_pFunction = reinterpret_cast<TFunction>(ResolvePapyrusFunction(m_namespace, m_function));

        // Not registered yet: a call would jump to zero, so it does nothing until the next one
        auto* pVM = GameVM::Get();
        if (!m_pFunction || !pVM || !pVM->virtualMachine)
            return Return{};

        return m_pFunction(pVM->virtualMachine, 0, apThis, std::forward<Args>(args)...);
    }

private:
    const char* m_namespace{nullptr};
    const char* m_function{nullptr};
    mutable TFunction m_pFunction{nullptr};
};

template <class Return, class... Args> struct GlobalPapyrusFunction
{
    using TFunction = Return(__fastcall*)(BSScript::IVirtualMachine*, Args...);

    GlobalPapyrusFunction(const void* apAddress)
        : m_pFunction(reinterpret_cast<TFunction>(apAddress))
    {
    }

    GlobalPapyrusFunction(const char* acNamespace, const char* acFunction)
        : m_namespace(acNamespace)
        , m_function(acFunction)
    {
    }

    Return operator()(Args... args) const noexcept
    {
        if (!m_pFunction)
            m_pFunction = reinterpret_cast<TFunction>(ResolvePapyrusFunction(m_namespace, m_function));

        auto* pVM = GameVM::Get();
        if (!m_pFunction || !pVM || !pVM->virtualMachine)
            return Return{};

        return m_pFunction(pVM->virtualMachine, std::forward<Args>(args)...);
    }

private:
    const char* m_namespace{nullptr};
    const char* m_function{nullptr};
    mutable TFunction m_pFunction{nullptr};
};

struct RefrOrInventoryObj
{
    const TESObjectREFR* pRefr;
    TESForm* pInventoryForm;
    uint16_t itemCount;
};

template <class Return, class Type, class... Args> struct LatentPapyrusFunction
{
    using TFunction = Return(__fastcall*)(BSScript::IVirtualMachine*, uint32_t, const RefrOrInventoryObj&, Args...);

    LatentPapyrusFunction(const void* apAddress)
        : m_pFunction(reinterpret_cast<TFunction>(apAddress))
    {
    }

    LatentPapyrusFunction(const char* acNamespace, const char* acFunction)
        : m_namespace(acNamespace)
        , m_function(acFunction)
    {
    }

    Return operator()(const Type* apThis, Args... args) const noexcept
    {
        if (!m_pFunction)
            m_pFunction = reinterpret_cast<TFunction>(ResolvePapyrusFunction(m_namespace, m_function));

        auto* pVM = GameVM::Get();
        if (!m_pFunction || !pVM || !pVM->virtualMachine)
            return Return{};

        RefrOrInventoryObj self{apThis, nullptr, 0};

        return m_pFunction(pVM->virtualMachine, 0, self, std::forward<Args>(args)...);
    }

private:
    const char* m_namespace{nullptr};
    const char* m_function{nullptr};
    mutable TFunction m_pFunction{nullptr};
};

#define PAPYRUS_FUNCTION(returnType, scope, name, ...) static PapyrusFunction<returnType, scope, __VA_ARGS__> s_p##name(#scope, #name);
#define GLOBAL_PAPYRUS_FUNCTION(returnType, scope, name, ...) static GlobalPapyrusFunction<returnType, __VA_ARGS__> s_p##name(#scope, #name);
#define LATENT_PAPYRUS_FUNCTION(returnType, scope, name, ...) static LatentPapyrusFunction<returnType, scope, __VA_ARGS__> s_p##name(#scope, #name);
