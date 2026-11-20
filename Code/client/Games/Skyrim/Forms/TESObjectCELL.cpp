#include <Forms/TESObjectCELL.h>
#include <TESObjectREFR.h>

Vector<TESObjectREFR*> TESObjectCELL::GetRefsByFormTypes(const Vector<FormType>& aFormTypes) const noexcept
{
    Vector<TESObjectREFR*> references{};

    if (!refData.refArray)
        return references;

    for (uint32_t i = 0; i < refData.capacity; i++)
    {
        TESObjectREFR* pRef = refData.refArray[i].Get();
        if (!pRef)
            continue;

        for (FormType formType : aFormTypes)
        {
            if (pRef->baseForm->formType == formType)
                references.push_back(pRef);
        }
    }

    return references;
}

void TESObjectCELL::GetCOCPlacementInfo(NiPoint3* aOutPos, NiPoint3* aOutRot, bool aAllowCellLoad) noexcept
{
    TP_THIS_FUNCTION(TGetCOCPlacementInfo, void, TESObjectCELL, NiPoint3*, NiPoint3*, bool);
    POINTER_SKYRIMSE(TGetCOCPlacementInfo, s_getCOCPlacementInfo, 19075);
    TiltedPhoques::ThisCall(s_getCOCPlacementInfo, this, aOutPos, aOutRot, aAllowCellLoad);
}

// The game counts queued loads on a cell's loaded data without checking the cell still has any: a reference queued while
// its cell detaches (moveto/resurrect into an unloaded holding cell, upstream issue 69) crashes on a null pointer. The
// decrement (19152) already skips a cell without loaded data, so the increment is skipped the same way.
TP_THIS_FUNCTION(TIncCellCount, void, TESObjectCELL);
TP_THIS_FUNCTION(TIncCellRefCount, void, TESObjectCELL, TESObjectREFR*);
static TIncCellCount* RealIncQueuedCount = nullptr;
static TIncCellCount* RealIncQueuedActorCount = nullptr;
static TIncCellRefCount* RealIncQueuedRefCount = nullptr;

// Form flag 13 marks a worldspace's persistent cell: 19159 then counts on the attached cell at the reference's position
// instead, which always has loaded data
static constexpr uint32_t kPersistentCellFlag = 1 << 13;

void TP_MAKE_THISCALL(HookIncQueuedCount, TESObjectCELL)
{
    if (apThis->loadedCellData)
        TiltedPhoques::ThisCall(RealIncQueuedCount, apThis);
}

void TP_MAKE_THISCALL(HookIncQueuedActorCount, TESObjectCELL)
{
    if (apThis->loadedCellData)
        TiltedPhoques::ThisCall(RealIncQueuedActorCount, apThis);
}

void TP_MAKE_THISCALL(HookIncQueuedRefCount, TESObjectCELL, TESObjectREFR* apRef)
{
    if (apThis->loadedCellData || (apThis->flags & kPersistentCellFlag))
        TiltedPhoques::ThisCall(RealIncQueuedRefCount, apThis, apRef);
}

static TiltedPhoques::Initializer s_cellCountHooks(
    []()
    {
        POINTER_SKYRIMSE(TIncCellCount, incQueuedCount, 19151);
        POINTER_SKYRIMSE(TIncCellCount, incQueuedActorCount, 19155);
        POINTER_SKYRIMSE(TIncCellRefCount, incQueuedRefCount, 19159);

        RealIncQueuedCount = incQueuedCount.Get();
        RealIncQueuedActorCount = incQueuedActorCount.Get();
        RealIncQueuedRefCount = incQueuedRefCount.Get();

        TP_HOOK(&RealIncQueuedCount, HookIncQueuedCount);
        TP_HOOK(&RealIncQueuedActorCount, HookIncQueuedActorCount);
        TP_HOOK(&RealIncQueuedRefCount, HookIncQueuedRefCount);
    });
