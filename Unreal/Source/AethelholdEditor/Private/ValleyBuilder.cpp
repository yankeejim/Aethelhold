#include "ValleyBuilder.h"
#include "ValleyTerrain.h"
#include "Editor.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// Landscape heights are 16-bit; with a Z scale of 200, one unit is 200/128 cm,
	// so the land can span about -512 m to +512 m.
	constexpr float ZScale = 200.f;
	constexpr int32 NumSubsections = 2;
	constexpr int32 SubsectionSizeQuads = 63;
}

ALandscape* UValleyBuilder::BuildLandscape(UMaterialInterface* Material, int32 Seed)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	const FValleyTerrain Terrain(Seed);
	const int32 N = FValleyTerrain::SizeVerts;
	TArray<uint16> Heights;
	Heights.SetNumUninitialized(N * N);
	ParallelFor(N, [&](int32 Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			const float Meters = Terrain.HeightAt(X, Y);
			const float Units = 32768.f + Meters * 100.f * 128.f / ZScale;
			Heights[Y * N + X] = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(Units), 0, 65535));
		}
	});

	ALandscape* Landscape = World->SpawnActor<ALandscape>(FVector::ZeroVector, FRotator::ZeroRotator);
	Landscape->SetActorScale3D(FVector(100.f, 100.f, ZScale));
	Landscape->LandscapeMaterial = Material;
	Landscape->SetActorLabel(TEXT("Valley"));

	TMap<FGuid, TArray<uint16>> HeightsPerLayer;
	HeightsPerLayer.Add(FGuid(), MoveTemp(Heights));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayers;
	MaterialLayers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

	Landscape->Import(FGuid::NewGuid(), 0, 0, N - 1, N - 1, NumSubsections, SubsectionSizeQuads,
		HeightsPerLayer, nullptr, MaterialLayers, ELandscapeImportAlphamapType::Additive,
		MakeArrayView<const FLandscapeLayer>({}));
	Landscape->CreateLandscapeInfo();
	// Edit layers are merged into the drawn heightmaps on the GPU, normally over the next
	// editor frames; do it now so a script can save straight away.
	Landscape->ForceUpdateLayersContent();
	return Landscape;
}

float UValleyBuilder::TerrainHeight(float WorldX, float WorldY, int32 Seed)
{
	return FValleyTerrain(Seed).HeightAt(WorldX / 100.f, WorldY / 100.f) * 100.f;
}

FVector UValleyBuilder::StartSpot(int32 Seed)
{
	const FValleyTerrain Terrain(Seed);
	const FVector2D Spot = Terrain.StartSpot();
	return FVector(Spot.X * 100.f, Spot.Y * 100.f, Terrain.HeightAt(Spot.X, Spot.Y) * 100.f);
}
