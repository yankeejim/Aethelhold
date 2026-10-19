#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ValleyBuilder.generated.h"

class ALandscape;
class UTexture2D;
class UMaterialInterface;

/** Editor tools for building the founding map; called from Tools/build_valley.py. */
UCLASS()
class AETHELHOLDEDITOR_API UValleyBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Creates the valley landscape in the open editor level, from FValleyTerrain. */
	UFUNCTION(BlueprintCallable, Category = "Aethelhold")
	static ALandscape* BuildLandscape(UMaterialInterface* Material, int32 Seed);

	/** Bakes which way and how fast the river flows into a texture (/Game/Materials/T_RiverFlow)
	 *  for the water material. */
	UFUNCTION(BlueprintCallable, Category = "Aethelhold")
	static UTexture2D* BuildFlowMap(int32 Seed);

	/** Bakes FValleyTerrain::ForestDensity into a texture (/Game/Materials/T_ForestMask) so the
	 *  ground material puts forest floor, not meadow, where the trees stand. */
	UFUNCTION(BlueprintCallable, Category = "Aethelhold")
	static UTexture2D* BuildForestMask(int32 Seed);

	/** Plants the woods, riverside trees, lone meadow oaks and shrubs (Megaplants, with wind). */
	UFUNCTION(BlueprintCallable, Category = "Aethelhold")
	static AActor* BuildForest(int32 Seed);

	/** Scatters Megascans rocks: boulders in the woods and on steep ground, small mossy stones
	 *  along the river and here and there in the meadow. */
	UFUNCTION(BlueprintCallable, Category = "Aethelhold")
	static AActor* BuildRocks(int32 Seed);

	/** The terrain height in centimeters at a world position (for placing actors). */
	UFUNCTION(BlueprintCallable, Category = "Aethelhold")
	static float TerrainHeight(float WorldX, float WorldY, int32 Seed);

	/** Where the river runs across the given world Y, in world centimeters. */
	UFUNCTION(BlueprintCallable, Category = "Aethelhold")
	static float RiverCenterX(float WorldY, int32 Seed);

	/** Where the founders arrive, in world centimeters, on the ground. */
	UFUNCTION(BlueprintCallable, Category = "Aethelhold")
	static FVector StartSpot(int32 Seed);
};
