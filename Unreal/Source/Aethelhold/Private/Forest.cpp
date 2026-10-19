#include "Forest.h"
#include "ValleyTerrain.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "DynamicWindData.h"
#include "Engine/SkeletalMesh.h"

namespace
{
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

AForest::AForest()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AForest::Plant(const FValleyTerrain& Terrain, int32 Seed)
{
	// One spot every 5 m, jittered. Each spot decides from the land what (if anything) grows,
	// and a clumping pattern bunches trees into groves with glades between, never rows.
	constexpr float Cell = 5.f;
	const int32 Cells = FMath::FloorToInt(FValleyTerrain::SizeMeters / Cell);
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

			FTreeSpot& Spot = Trees.AddDefaulted_GetRef();
			Spot.Kind = Tree;
			Spot.Shape = Shape;
			// Sink the trunk a little so roots don't float on slopes.
			Spot.Transform = FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f),
				FVector(X * 100.f, Y * 100.f, (H - 0.3f) * 100.f), FVector(Scale));
			Spot.Forage = Spot.IsShrub() ? (Shape >= 2 ? 1 : 3) : 0;
		}
	}

	// One component per tree kind and shape; the trees sway with Unreal's dynamic wind, which
	// reads each tree's own skeleton.
	UTransformProviderData* Wind = NewObject<UTransformProviderData>(this, UDynamicWindData::StaticClass(), TEXT("Wind"));
	TMap<TPair<ETree, int32>, int32> GroupOf;
	for (FTreeSpot& Spot : Trees)
	{
		int32* Found = GroupOf.Find({ Spot.Kind, Spot.Shape });
		if (!Found)
		{
			USkeletalMesh* Mesh = LoadTree(Spot.Kind, Spot.Shape);
			UInstancedSkinnedMeshComponent* Group = NewObject<UInstancedSkinnedMeshComponent>(this);
			Group->SetupAttachment(RootComponent);
			Group->SetSkinnedAssetAndUpdate(Mesh);
			Group->SetTransformProvider(Wind);
			// No collision: nothing walks into trees yet, and the camera's ground probe must pass.
			Group->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Group->RegisterComponent();
			Found = &GroupOf.Add({ Spot.Kind, Spot.Shape }, Groups.Add(Group));
		}
		Spot.Group = *Found;
		Spot.Instance = Groups[Spot.Group]->AddInstance(Spot.Transform, 0, true);
	}
	UE_LOG(LogTemp, Display, TEXT("Forest: planted %d trees and shrubs"), Trees.Num());
}

int32 AForest::MarkArea(const FVector2D& A, const FVector2D& B, bool bMark)
{
	const FBox2D Area(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)));
	int32 Count = 0;
	for (FTreeSpot& Spot : Trees)
	{
		if (!Spot.bGone && Spot.Fall == 0.f && !Spot.IsShrub() && Area.IsInside(FVector2D(Spot.Base())))
		{
			if (Spot.bMarked != bMark && (bMark || !Spot.bClaimed))
			{
				Spot.bMarked = bMark;
				++Count;
			}
		}
	}
	return Count;
}

int32 AForest::NearestMarked(const FVector& From) const
{
	int32 Best = INDEX_NONE;
	float BestDist = TNumericLimits<float>::Max();
	for (int32 I = 0; I < Trees.Num(); ++I)
	{
		const FTreeSpot& Spot = Trees[I];
		if (Spot.bMarked && !Spot.bClaimed && !Spot.bGone)
		{
			const float D = FVector::DistSquared2D(From, Spot.Base());
			if (D < BestDist)
			{
				BestDist = D;
				Best = I;
			}
		}
	}
	return Best;
}

int32 AForest::NearestForage(const FVector& From, float Range) const
{
	int32 Best = INDEX_NONE;
	float BestDist = Range * Range;
	for (int32 I = 0; I < Trees.Num(); ++I)
	{
		const FTreeSpot& Spot = Trees[I];
		if (Spot.Forage > 0 && !Spot.bClaimed && !Spot.bGone)
		{
			const float D = FVector::DistSquared2D(From, Spot.Base());
			if (D < BestDist)
			{
				BestDist = D;
				Best = I;
			}
		}
	}
	return Best;
}

void AForest::Fell(int32 Index, const FVector& Feller)
{
	FTreeSpot& Spot = Trees[Index];
	// It falls away from whoever cut it.
	Spot.FallYaw = (Spot.Base() - Feller).Rotation().Yaw;
	Spot.Fall = 0.001f;
	Falling.Add(Index);
}

void AForest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (int32 K = Falling.Num() - 1; K >= 0; --K)
	{
		FTreeSpot& Spot = Trees[Falling[K]];
		// Slow at first, then crashing down; it lies a moment before it is cleared.
		Spot.Fall += DeltaSeconds * 0.45f;
		const float T = FMath::Min(Spot.Fall, 1.f);
		const FVector Dir = FRotator(0.f, Spot.FallYaw, 0.f).Vector();
		const FVector Axis = FVector::CrossProduct(FVector::UpVector, Dir);
		const FQuat Tilt(Axis, FMath::DegreesToRadians(86.f * T * T));
		FTransform Now = Spot.Transform;
		Now.SetRotation(Tilt * Spot.Transform.GetRotation());
		Groups[Spot.Group]->SetInstanceTransform(Spot.Instance, Now, true);
		if (Spot.Fall >= 1.6f)
		{
			Groups[Spot.Group]->RemoveInstance(Spot.Instance);
			Spot.bGone = true;
			Falling.RemoveAt(K);
		}
	}
}
