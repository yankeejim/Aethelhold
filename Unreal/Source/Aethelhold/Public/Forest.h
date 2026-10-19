#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstanceDataTypes.h"
#include "Forest.generated.h"

class UInstancedSkinnedMeshComponent;
struct FValleyTerrain;

UENUM()
enum class ETree : uint8 { Oak, Beech, Birch, Alder, Willow, Spruce, Hazel, Count };

/** One tree or shrub standing in the valley. */
struct FTreeSpot
{
	ETree Kind = ETree::Oak;
	/** Megaplants come in four shapes: 0 and 1 grown, 2 and 3 young. */
	int32 Shape = 0;
	FTransform Transform;
	/** Which component draws it, and its instance there. */
	int32 Group = INDEX_NONE;
	FPrimitiveInstanceId Instance;

	bool bMarked = false;
	bool bClaimed = false;
	bool bGone = false;
	/** Nuts and berries left on a shrub this season. */
	int32 Forage = 0;
	/** 0 standing, rising to 1 as it falls. */
	float Fall = 0.f;
	float FallYaw = 0.f;

	bool IsShrub() const { return Kind == ETree::Hazel; }
	/** Logs a felled tree gives: young trees one, grown ones three. */
	int32 Logs() const { return IsShrub() ? 0 : (Shape >= 2 ? 1 : 3); }
	FVector Base() const { return Transform.GetLocation(); }
};

/**
 * The valley's trees and shrubs, planted from the terrain when the game starts (so every seed
 * grows its own woods), drawn as Megaplants swaying in Unreal's dynamic wind. Trees can be
 * marked for felling and cut down; hazel shrubs give nuts to foragers.
 */
UCLASS()
class AETHELHOLD_API AForest : public AActor
{
	GENERATED_BODY()

public:
	AForest();

	void Plant(const FValleyTerrain& Terrain, int32 Seed);
	virtual void Tick(float DeltaSeconds) override;

	TArray<FTreeSpot>& GetTrees() { return Trees; }
	const TArray<FTreeSpot>& GetTrees() const { return Trees; }

	/** Marks (or unmarks) every standing tree inside a world-space rectangle; returns how many. */
	int32 MarkArea(const FVector2D& A, const FVector2D& B, bool bMark);
	/** The nearest marked, unclaimed tree to a point, or INDEX_NONE. */
	int32 NearestMarked(const FVector& From) const;
	/** The nearest shrub with forage left within a range, or INDEX_NONE. */
	int32 NearestForage(const FVector& From, float Range) const;
	/** Starts a tree toppling, away from the given point. */
	void Fell(int32 Index, const FVector& Feller);
	/** Takes a tree away at once, with no logs (for clearing a test site). */
	void Clear(int32 Index);
	/** True once a felled tree has hit the ground and been cleared away. */
	bool IsDown(int32 Index) const { return Trees[Index].bGone; }

private:
	UPROPERTY()
	TArray<TObjectPtr<UInstancedSkinnedMeshComponent>> Groups;

	TArray<FTreeSpot> Trees;
	/** Trees currently falling (their indexes). */
	TArray<int32> Falling;
};
