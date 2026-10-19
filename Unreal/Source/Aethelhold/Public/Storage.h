#pragma once

#include "CoreMinimal.h"
#include "Storage.generated.h"

class AItem;
enum class EItem : uint8;

/**
 * Stockpile zones (docs/resources.md, Storage): the player draws them on the ground and says
 * what each takes. The ground is split into square places; each place holds one kind of item,
 * as many as physically fit (logs stack, baskets sit side by side).
 */
namespace Storage
{
	/** The side of one storage place, in cm: room for a 2 m log lying down. */
	constexpr float PlaceSize = 220.f;

	int32 Capacity(EItem Kind);
	/** Where the Slot-th item of a kind sits within a place, relative to its centre on the ground.
	 *  Indoors, things are packed closer to keep clear of the walls. */
	FVector SlotOffset(EItem Kind, int32 Slot, bool bIndoor = false);
	inline FIntPoint PlaceAt(const FVector2D& World)
	{
		return FIntPoint(FMath::FloorToInt(World.X / PlaceSize), FMath::FloorToInt(World.Y / PlaceSize));
	}
}

UENUM()
enum class EStoreFilter : uint8 { Everything, Wood, Food };

/** A reserved or used slot: which place, and which slot in it. */
struct FStoreSpot
{
	FIntPoint Place = FIntPoint(MAX_int32, MAX_int32);
	int32 Slot = INDEX_NONE;

	bool IsValid() const { return Slot != INDEX_NONE; }
};

/** One square of ground in a stockpile. */
struct FStorePlace
{
	int32 Zone = INDEX_NONE;
	/** The kind of item kept here, once the first one arrives. */
	TOptional<EItem> Kind;
	TArray<TWeakObjectPtr<AItem>> Slots;
	TArray<bool> Reserved;

	bool IsEmpty() const;
	int32 Count() const;
};

struct FStoreZone
{
	EStoreFilter Accepts = EStoreFilter::Everything;
	TArray<FIntPoint> Places;
	bool bRemoved = false;
	/** Inside a storehouse: dry and shaded, so food lasts longer. Not drawn or changed by hand. */
	bool bIndoor = false;

	bool Takes(EItem Kind) const;
};
