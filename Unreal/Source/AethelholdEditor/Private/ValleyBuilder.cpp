#include "ValleyBuilder.h"
#include "ValleyTerrain.h"
#include "Editor.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "DynamicWindData.h"
#include "Engine/SkeletalMesh.h"
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

namespace
{
	enum class ETree : uint8 { Oak, Beech, Birch, Alder, Willow, Spruce, Hazel, Count };

	// Megaplants (docs/art.md); each comes as four shapes, A and B grown, C and D young.
	USkeletalMesh* LoadTree(ETree Tree, int32 Shape)
	{
		static const TCHAR* Names[][2] = {
			{ TEXT("Tree_English_Oak"), TEXT("Tree_English_Oak_Forest_01") },
			{ TEXT("Tree_European_Beech"), TEXT("Tree_European_Beech_01") },
			{ TEXT("Tree_Silver_Birch"), TEXT("Tree_Silver_Birch_01") },
			{ TEXT("Tree_Black_Alder"), TEXT("Tree_Black_Alder_01") },
			{ TEXT("Tree_Goat_Willow"), TEXT("Tree_Goat_Willow_01") },
			{ TEXT("Tree_Norway_Spruce"), TEXT("Tree_Norway_Spruce_01") },
			{ TEXT("Tree_Common_Hazel"), TEXT("Tree_Common_Hazel_01") },
		};
		const TCHAR Letter = TEXT('A') + Shape;
		const FString Path = FString::Printf(TEXT("/Game/Megaplant_Library/%s/%s/%s_%c.%s_%c"),
			Names[(int32)Tree][0], Names[(int32)Tree][1], Names[(int32)Tree][1], Letter, Names[(int32)Tree][1], Letter);
		return LoadObject<USkeletalMesh>(nullptr, *Path);
	}
}

AActor* UValleyBuilder::BuildForest(int32 Seed)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	const FValleyTerrain Terrain(Seed);

	// One spot every 5 m, jittered. Each spot decides from the land what (if anything) grows,
	// and a clumping pattern bunches trees into groves with glades between, never rows.
	constexpr float Cell = 5.f;
	const int32 Cells = FMath::FloorToInt(FValleyTerrain::SizeMeters / Cell);
	TMap<TPair<ETree, int32>, TArray<FTransform>> Planted;

	for (int32 J = 0; J < Cells; ++J)
	{
		for (int32 I = 0; I < Cells; ++I)
		{
			FRandomStream R(HashCombine(GetTypeHash(I * 7919 + J), GetTypeHash(Seed)));
			const float X = (I + R.FRandRange(0.f, 1.f)) * Cell;
			const float Y = (J + R.FRandRange(0.f, 1.f)) * Cell;
			const float H = Terrain.HeightAt(X, Y);
			if (H < 0.6f)
			{
				continue;
			}
			const float Water = Terrain.DistanceFromWater(X, Y);
			const float Forest = Terrain.ForestDensity(X, Y);
			const float Clump = Terrain.Clumping(X, Y);
			const float Roll = R.FRand();

			ETree Tree;
			int32 Shape = R.RandRange(0, 1);
			float Scale = R.FRandRange(0.85f, 1.35f);
			if (Water > 4.f && Water < 24.f && Roll < 0.16f * Clump * (1.f - Water / 26.f))
			{
				// Alders and willows line the river in clumps.
				Tree = R.FRand() < 0.6f ? ETree::Alder : ETree::Willow;
				Shape = R.RandRange(0, 3);
			}
			else if (Roll < Forest * (0.3f + 0.6f * Clump))
			{
				const float Pick = R.FRand();
				if (Forest < 0.45f)
				{
					// The ragged edge of a wood: birch, hazel and young oaks.
					Tree = Pick < 0.4f ? ETree::Birch : (Pick < 0.75f ? ETree::Hazel : ETree::Oak);
					Shape = R.RandRange(0, 3);
				}
				else if (H > 60.f)
				{
					Tree = Pick < 0.75f ? ETree::Spruce : ETree::Birch;
				}
				else if (Pick < 0.15f)
				{
					// Undergrowth: hazel and young trees beneath the canopy.
					Tree = R.FRand() < 0.6f ? ETree::Hazel : ETree::Beech;
					Shape = R.RandRange(2, 3);
				}
				else
				{
					Tree = Pick < 0.55f ? ETree::Oak : (Pick < 0.9f ? ETree::Beech : ETree::Birch);
					if (R.FRand() < 0.3f)
					{
						Shape = R.RandRange(2, 3);
					}
				}
			}
			else if (Forest < 0.05f && Water > 25.f && Roll > 0.9985f)
			{
				// Now and then a lone oak stands out in the meadow.
				Tree = ETree::Oak;
				Shape = 0;
				Scale = R.FRandRange(1.1f, 1.35f);
			}
			else
			{
				continue;
			}

			// Sink the trunk a little so roots don't float on slopes.
			const FVector Where(X * 100.f, Y * 100.f, (H - 0.3f) * 100.f);
			const FRotator Turn(0.f, R.FRandRange(0.f, 360.f), 0.f);
			Planted.FindOrAdd({ Tree, Shape }).Add(FTransform(Turn, Where, FVector(Scale)));
		}
	}

	AActor* Forest = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	USceneComponent* Root = NewObject<USceneComponent>(Forest, TEXT("Root"));
	Forest->SetRootComponent(Root);
	Forest->AddInstanceComponent(Root);
	Root->RegisterComponent();
	Forest->SetActorLabel(TEXT("Forest"));

	// The trees sway with Unreal's dynamic wind, which reads each tree's own skeleton.
	UTransformProviderData* Wind = NewObject<UTransformProviderData>(Forest, UDynamicWindData::StaticClass(), TEXT("Wind"));

	int32 Total = 0;
	for (const TPair<TPair<ETree, int32>, TArray<FTransform>>& Group : Planted)
	{
		USkeletalMesh* Mesh = LoadTree(Group.Key.Key, Group.Key.Value);
		if (!Mesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("BuildForest: missing tree %d shape %d"), (int32)Group.Key.Key, Group.Key.Value);
			continue;
		}
		UInstancedSkinnedMeshComponent* Trees = NewObject<UInstancedSkinnedMeshComponent>(Forest, *FString::Printf(TEXT("%s_Trees"), *Mesh->GetName()));
		Trees->SetupAttachment(Root);
		Trees->SetSkinnedAssetAndUpdate(Mesh);
		Trees->SetTransformProvider(Wind);
		// No collision yet: nothing walks into trees, and the camera's ground probe must pass through.
		Trees->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Forest->AddInstanceComponent(Trees);
		Trees->RegisterComponent();
		TArray<int32> Animations;
		Animations.SetNumZeroed(Group.Value.Num());
		Trees->AddInstances(Group.Value, Animations, false, true);
		Total += Group.Value.Num();
	}
	UE_LOG(LogTemp, Display, TEXT("BuildForest: planted %d trees and shrubs"), Total);
	return Forest;
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
