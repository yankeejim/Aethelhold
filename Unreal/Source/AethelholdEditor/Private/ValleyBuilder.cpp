#include "ValleyBuilder.h"
#include "ValleyTerrain.h"
#include "Editor.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"

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

namespace
{
	/** Creates or overwrites a texture asset under /Game/Materials from raw pixels. */
	UTexture2D* SaveTexture(const TCHAR* Name, int32 Size, ETextureSourceFormat Format, const TArray<uint8>& Pixels,
		TextureCompressionSettings Compression)
	{
		const FString Path = FString::Printf(TEXT("/Game/Materials/%s"), Name);
		UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("%s.%s"), *Path, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Tex)
		{
			UPackage* Package = CreatePackage(*Path);
			Tex = NewObject<UTexture2D>(Package, Name, RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Tex);
		}
		Tex->Source.Init(Size, Size, 1, 1, Format, Pixels.GetData());
		Tex->SRGB = false;
		Tex->CompressionSettings = Compression;
		Tex->AddressX = TA_Clamp;
		Tex->AddressY = TA_Clamp;
		Tex->PostEditChange();
		Tex->MarkPackageDirty();
		return Tex;
	}
}

UTexture2D* UValleyBuilder::BuildFlowMap(int32 Seed)
{
	// Which way the river runs at each spot, and how fast: quickest mid-channel, slow by the
	// banks. Red and green hold the direction, blue the speed.
	const FValleyTerrain Terrain(Seed);
	constexpr int32 N = 1024;
	const float Step = FValleyTerrain::SizeMeters / N;
	TArray<uint8> Pixels;
	Pixels.SetNumZeroed(N * N * 4);
	ParallelFor(N, [&](int32 Row)
	{
		const float Y = (Row + 0.5f) * Step;
		const FVector2D Down = FVector2D(Terrain.RiverCenterX(Y + 1.f) - Terrain.RiverCenterX(Y - 1.f), 2.f).GetSafeNormal();
		for (int32 Col = 0; Col < N; ++Col)
		{
			const float X = (Col + 0.5f) * Step;
			const float Edge = Terrain.DistanceFromWater(X, Y);
			const float Across = FMath::Clamp(-Edge / Terrain.RiverHalfWidth(X, Y), 0.f, 1.f);
			const float Speed = 0.25f + 0.75f * FMath::Sqrt(Across);
			uint8* Px = &Pixels[(Row * N + Col) * 4];
			Px[0] = static_cast<uint8>(FMath::RoundToInt(255.f * Speed));              // B
			Px[1] = static_cast<uint8>(FMath::RoundToInt(255.f * (Down.Y * 0.5f + 0.5f))); // G
			Px[2] = static_cast<uint8>(FMath::RoundToInt(255.f * (Down.X * 0.5f + 0.5f))); // R
			Px[3] = 255;
		}
	});
	return SaveTexture(TEXT("T_RiverFlow"), N, TSF_BGRA8, Pixels, TC_VectorDisplacementmap);
}

UTexture2D* UValleyBuilder::BuildForestMask(int32 Seed)
{
	const FValleyTerrain Terrain(Seed);
	constexpr int32 N = 1024;
	const float Step = FValleyTerrain::SizeMeters / N;
	TArray<uint8> Pixels;
	Pixels.SetNumZeroed(N * N);
	ParallelFor(N, [&](int32 Row)
	{
		for (int32 Col = 0; Col < N; ++Col)
		{
			const float F = Terrain.ForestDensity((Col + 0.5f) * Step, (Row + 0.5f) * Step);
			Pixels[Row * N + Col] = static_cast<uint8>(FMath::RoundToInt(F * 255.f));
		}
	});
	return SaveTexture(TEXT("T_ForestMask"), N, TSF_G8, Pixels, TC_Grayscale);
}

AActor* UValleyBuilder::BuildRocks(int32 Seed)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	const FValleyTerrain Terrain(Seed);
	auto Load = [](const TCHAR* Code)
	{
		const FString Name = FString::Printf(TEXT("%s_tier_1"), Code);
		const TCHAR* Folder = FCString::Strcmp(Code, TEXT("wfstcdnaw")) == 0 ? TEXT("Mossy_Forest_Boulder") : TEXT("Mossy_Rocks");
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Fab/Megascans/3D/%s_%s/High/%s/StaticMeshes/%s.%s"),
			Folder, Code, *Name, *Name, *Name));
	};
	UStaticMesh* Meshes[] = { Load(TEXT("wfstcdnaw")), Load(TEXT("tjmudcfda")), Load(TEXT("tletbf2fa")) };

	AActor* Rocks = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	USceneComponent* Root = NewObject<USceneComponent>(Rocks, TEXT("Root"));
	Rocks->SetRootComponent(Root);
	Rocks->AddInstanceComponent(Root);
	Root->RegisterComponent();
	Rocks->SetActorLabel(TEXT("Rocks"));

	TArray<FTransform> Placed[3];
	constexpr float Cell = 6.f;
	const int32 Cells = FMath::FloorToInt(FValleyTerrain::SizeMeters / Cell);
	for (int32 J = 0; J < Cells; ++J)
	{
		for (int32 I = 0; I < Cells; ++I)
		{
			FRandomStream R(HashCombine(GetTypeHash(I * 104729 + J), GetTypeHash(Seed + 17)));
			const float X = (I + R.FRand()) * Cell;
			const float Y = (J + R.FRand()) * Cell;
			const float H = Terrain.HeightAt(X, Y);
			const float Water = Terrain.DistanceFromWater(X, Y);
			const float Slope = Terrain.SlopeAt(X, Y);
			const float Forest = Terrain.ForestDensity(X, Y);
			const float Roll = R.FRand();

			int32 Kind;
			float Scale;
			if (Water > -3.f && Water < 6.f && Roll < 0.05f)
			{
				// Stones at the water's edge, half sunk.
				Kind = R.RandRange(1, 2);
				Scale = R.FRandRange(0.6f, 1.4f);
			}
			else if (H > 1.f && Roll < 0.004f + 0.02f * Forest + 0.05f * FMath::Clamp(Slope - 0.4f, 0.f, 1.f))
			{
				// Boulders in the woods and on steep slopes; the odd one in the open.
				Kind = R.FRand() < 0.5f ? 0 : R.RandRange(1, 2);
				Scale = Kind == 0 ? R.FRandRange(0.5f, 1.6f) : R.FRandRange(1.f, 2.5f);
			}
			else
			{
				continue;
			}
			// Sink each rock about a quarter of its height so it sits in the ground, not on it.
			const float Sink = 25.f * Scale;
			const FRotator Turn(R.FRandRange(-8.f, 8.f), R.FRandRange(0.f, 360.f), R.FRandRange(-8.f, 8.f));
			Placed[Kind].Add(FTransform(Turn, FVector(X * 100.f, Y * 100.f, H * 100.f - Sink), FVector(Scale)));
		}
	}

	int32 Total = 0;
	for (int32 K = 0; K < 3; ++K)
	{
		if (!Meshes[K] || Placed[K].IsEmpty())
		{
			continue;
		}
		UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(Rocks, *FString::Printf(TEXT("Rocks_%d"), K));
		Ism->SetupAttachment(Root);
		Ism->SetStaticMesh(Meshes[K]);
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Rocks->AddInstanceComponent(Ism);
		Ism->RegisterComponent();
		Ism->AddInstances(Placed[K], false, true);
		Total += Placed[K].Num();
	}
	UE_LOG(LogTemp, Display, TEXT("BuildRocks: placed %d rocks"), Total);
	return Rocks;
}

float UValleyBuilder::TerrainHeight(float WorldX, float WorldY, int32 Seed)
{
	return FValleyTerrain(Seed).HeightAt(WorldX / 100.f, WorldY / 100.f) * 100.f;
}

float UValleyBuilder::RiverCenterX(float WorldY, int32 Seed)
{
	return FValleyTerrain(Seed).RiverCenterX(WorldY / 100.f) * 100.f;
}

FVector UValleyBuilder::StartSpot(int32 Seed)
{
	const FValleyTerrain Terrain(Seed);
	const FVector2D Spot = Terrain.StartSpot();
	return FVector(Spot.X * 100.f, Spot.Y * 100.f, Terrain.HeightAt(Spot.X, Spot.Y) * 100.f);
}
