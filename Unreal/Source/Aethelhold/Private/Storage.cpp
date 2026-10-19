#include "Storage.h"
#include "Item.h"

int32 Storage::Capacity(EItem Kind)
{
	// Logs: five side by side, three layers high. Food: three by three on the ground.
	return Kind == EItem::Log ? 15 : 9;
}

FVector Storage::SlotOffset(EItem Kind, int32 Slot, bool bIndoor)
{
	const float Step = bIndoor ? 45.f : 70.f;
	if (Kind == EItem::Log)
	{
		return FVector(0.f, -80.f + 40.f * (Slot % 5), 13.f + 25.f * (Slot / 5));
	}
	return FVector(-Step + Step * (Slot % 3), -Step + Step * (Slot / 3), 13.f);
}

bool FStorePlace::IsEmpty() const
{
	for (int32 I = 0; I < Slots.Num(); ++I)
	{
		if (Slots[I].IsValid() || Reserved[I])
		{
			return false;
		}
	}
	return true;
}

int32 FStorePlace::Count() const
{
	int32 N = 0;
	for (const TWeakObjectPtr<AItem>& Item : Slots)
	{
		N += Item.IsValid() ? 1 : 0;
	}
	return N;
}

bool FStoreZone::Takes(EItem Kind) const
{
	switch (Accepts)
	{
	case EStoreFilter::Wood: return Kind == EItem::Log;
	case EStoreFilter::Food: return Kind != EItem::Log;
	default: return true;
	}
}
