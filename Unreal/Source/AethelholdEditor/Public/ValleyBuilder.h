#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ValleyBuilder.generated.h"

class ALandscape;
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

	/** The terrain height in centimeters at a world position (for placing actors). */
	UFUNCTION(BlueprintCallable, Category = "Aethelhold")
	static float TerrainHeight(float WorldX, float WorldY, int32 Seed);

	/** Where the founders arrive, in world centimeters, on the ground. */
	UFUNCTION(BlueprintCallable, Category = "Aethelhold")
	static FVector StartSpot(int32 Seed);
};
