#pragma once

#include "CoreMinimal.h"

/**
 * The founding map: a river valley, about 2 km across, with meadows by the water, wooded
 * hills behind and mountains at the edges. Heights come from one function so the landscape
 * and anything placed on it later agree. Distances are in meters, with (0,0) at a corner.
 */
struct AETHELHOLD_API FValleyTerrain
{
	/** Vertices per side; 2017 = 16 landscape components of 2x2 sections of 63 quads. */
	static constexpr int32 SizeVerts = 2017;
	static constexpr float SizeMeters = SizeVerts - 1;
	/** The river's surface height. */
	static constexpr float WaterLevel = 0.f;

	explicit FValleyTerrain(int32 InSeed = 1);

	float HeightAt(float X, float Y) const;
	float RiverCenterX(float Y) const;
	/** Where the founders arrive: a meadow on the river's east bank, mid-valley. */
	FVector2D StartSpot() const;

private:
	float Noise(float X, float Y, float Scale, int32 Octaves) const;

	int32 Seed;
	FVector2D Offset;
	float Phase1;
	float Phase2;
};
