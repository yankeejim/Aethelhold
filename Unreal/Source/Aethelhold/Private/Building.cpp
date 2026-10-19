#include "Building.h"
#include "Storage.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// Engine shapes: the cylinder and cube are both 100 cm, the cylinder standing along Z.
	UStaticMesh* Cylinder() { return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); }
	UStaticMesh* Cube() { return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")); }
	UStaticMesh* Stone()
	{
		return LoadObject<UStaticMesh>(nullptr,
			TEXT("/Game/Fab/Megascans/3D/Mossy_Rocks_tjmudcfda/High/tjmudcfda_tier_1/StaticMeshes/tjmudcfda_tier_1.tjmudcfda_tier_1"));
	}
	UMaterialInterface* Bark()
	{
		return LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Megaplant_Library/Tree_English_Oak/Materials/MI_English_Oak_Bark_01.MI_English_Oak_Bark_01"));
	}
	UMaterialInterface* Thatch()
	{
		return LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Fab/Megascans/Surfaces/Dried_Grass_xbrgaba/High/xbrgaba_tier_1/Materials/MI_xbrgaba.MI_xbrgaba"));
	}

	const ABuilding::FKind Kinds[] = {
		// A lean-to of poles and thatch: quick shelter from the dew and wind for three.
		{ TEXT("Shelter"), FIntPoint(2, 1), 3, 3.f, 3 },
		// A family's log cabin.
		{ TEXT("Cabin"), FIntPoint(2, 3), 16, 16.f, 4 },
		// A small log store: food kept dry and shaded inside lasts much longer.
		{ TEXT("Storehouse"), FIntPoint(2, 2), 10, 10.f, 0 },
	};
}

ABuilding::ABuilding()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

const ABuilding::FKind& ABuilding::Kind(EBuilding InType)
{
	return Kinds[(int32)InType];
}

FIntPoint ABuilding::Footprint(EBuilding InType, int32 InTurns)
{
	const FIntPoint S = Kind(InType).Size;
	return InTurns % 2 == 0 ? S : FIntPoint(S.Y, S.X);
}

TArray<FIntPoint> ABuilding::Places() const
{
	TArray<FIntPoint> Out;
	const FIntPoint S = Footprint(Type, Turns);
	for (int32 X = 0; X < S.X; ++X)
	{
		for (int32 Y = 0; Y < S.Y; ++Y)
		{
			Out.Add(FirstPlace + FIntPoint(X, Y));
		}
	}
	return Out;
}

void ABuilding::Init(EBuilding InType, int32 InTurns, const FIntPoint& InFirstPlace)
{
	Type = InType;
	Turns = InTurns % 4;
	FirstPlace = InFirstPlace;
	Plan();

	// Stakes at the corners mark out the site until the first stones go down.
	const FIntPoint S = Kind(Type).Size;
	const float HX = S.X * Storage::PlaceSize * 0.5f - 20.f;
	const float HY = S.Y * Storage::PlaceSize * 0.5f - 20.f;
	for (int32 K = 0; K < 4; ++K)
	{
		UStaticMeshComponent* Stake = NewObject<UStaticMeshComponent>(this);
		Stake->SetupAttachment(RootComponent);
		Stake->SetStaticMesh(Cylinder());
		Stake->SetMaterial(0, Bark());
		Stake->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Stake->SetRelativeLocation(FVector(K % 2 ? HX : -HX, K / 2 ? HY : -HY, 40.f));
		Stake->SetRelativeScale3D(FVector(0.06f, 0.06f, 0.9f));
		Stake->RegisterComponent();
		Stakes.Add(Stake);
	}
	ShowPieces();
}

bool ABuilding::CanWork() const
{
	return !IsFinished() && Progress < (float)Delivered / GetKind().Logs - KINDA_SMALL_NUMBER;
}

void ABuilding::AddWork(float Hours)
{
	const float Cap = (float)Delivered / GetKind().Logs;
	Progress = FMath::Min(Cap, Progress + Hours / GetKind().Work);
	ShowPieces();
}

void ABuilding::ShowPieces()
{
	// Pieces are planned in building order; show as many as the work done allows.
	const int32 Want = IsFinished() ? Pieces.Num() : FMath::FloorToInt(Progress * Pieces.Num());
	while (Shown < Want)
	{
		const FPiece& P = Pieces[Shown++];
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
		Part->SetupAttachment(RootComponent);
		Part->SetStaticMesh(P.Mesh);
		Part->SetMaterial(0, P.Material);
		Part->SetRelativeTransform(P.Transform);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->RegisterComponent();
		Parts.Add(Part);
	}
	if (Shown > 0)
	{
		for (UStaticMeshComponent* Stake : Stakes)
		{
			Stake->DestroyComponent();
		}
		Stakes.Empty();
	}
}

void ABuilding::AddLog(const FVector& From, const FVector& To, float Diameter)
{
	const FVector Along = To - From;
	FPiece& P = Pieces.AddDefaulted_GetRef();
	P.Mesh = Cylinder();
	P.Material = Bark();
	// The cylinder stands along Z; tip it to lie along the line from one end to the other.
	const FQuat Tip = FQuat::FindBetweenNormals(FVector::UpVector, Along.GetSafeNormal());
	P.Transform = FTransform(Tip, (From + To) * 0.5f, FVector(Diameter / 100.f, Diameter / 100.f, Along.Size() / 100.f));
}

void ABuilding::AddPost(const FVector& Base, float Height, float Diameter)
{
	AddLog(Base, Base + FVector(0.f, 0.f, Height), Diameter);
}

void ABuilding::AddThatch(const FVector& Centre, float Length, float Width, float Pitch, float Yaw)
{
	FPiece& P = Pieces.AddDefaulted_GetRef();
	P.Mesh = Cube();
	P.Material = Thatch();
	// Pitch tips the panel about its length: positive lifts the +X edge.
	P.Transform = FTransform(FRotator(Pitch, Yaw, 0.f), Centre, FVector(Width / 100.f, Length / 100.f, 0.22f));
}

void ABuilding::AddStone(const FVector& At, float Scale)
{
	FPiece& P = Pieces.AddDefaulted_GetRef();
	P.Mesh = Stone();
	P.Material = nullptr;
	P.Transform = FTransform(FRotator(0.f, At.X * 3.f + At.Y, 0.f), At, FVector(Scale));
}

void ABuilding::PlanLogHouse(float HX, float HY, int32 Courses)
{
	constexpr float D = 26.f;
	constexpr float Over = 30.f;
	// Corner stones first, sunk into the ground.
	for (int32 K = 0; K < 4; ++K)
	{
		AddStone(FVector(K % 2 ? HX : -HX, K / 2 ? HY : -HY, -12.f), 0.22f);
	}
	// Walls course by course: the side walls, then the end walls half a log higher, so the
	// corners interlock. The front (-Y) has a door gap in its lower courses.
	const float DoorHalf = 50.f;
	for (int32 C = 0; C < Courses; ++C)
	{
		const float ZSide = D * 0.5f + C * D;
		const float ZEnd = ZSide + D * 0.5f;
		AddLog(FVector(-HX, -HY - Over, ZSide), FVector(-HX, HY + Over, ZSide), D);
		AddLog(FVector(HX, -HY - Over, ZSide), FVector(HX, HY + Over, ZSide), D);
		AddLog(FVector(-HX - Over, HY, ZEnd), FVector(HX + Over, HY, ZEnd), D);
		if (ZEnd < 185.f)
		{
			AddLog(FVector(-HX - Over, -HY, ZEnd), FVector(-DoorHalf, -HY, ZEnd), D);
			AddLog(FVector(DoorHalf, -HY, ZEnd), FVector(HX + Over, -HY, ZEnd), D);
		}
		else
		{
			AddLog(FVector(-HX - Over, -HY, ZEnd), FVector(HX + Over, -HY, ZEnd), D);
		}
	}
	// Gables: shorter and shorter logs on the end walls, up to the ridge.
	const float Top = Courses * D + D * 0.5f;
	const float Rise = HX * 0.75f;
	const int32 GableCourses = FMath::FloorToInt(Rise / D);
	for (int32 J = 0; J < GableCourses; ++J)
	{
		const float Z = Top + D * 0.5f + J * D;
		const float Half = HX * (1.f - (J + 0.5f) * D / Rise);
		AddLog(FVector(-Half, -HY, Z), FVector(Half, -HY, Z), D);
		AddLog(FVector(-Half, HY, Z), FVector(Half, HY, Z), D);
	}
	// The ridge, then the thatch laid in strips from the eaves up, front to back.
	AddLog(FVector(0.f, -HY - 50.f, Top + Rise), FVector(0.f, HY + 50.f, Top + Rise), 30.f);
	const float Run = HX + 40.f;
	const float Slope = FMath::RadiansToDegrees(FMath::Atan2(Rise, HX));
	const float Width = FMath::Sqrt(Run * Run + Rise * Rise * (Run / HX) * (Run / HX)) + 20.f;
	const int32 Strips = 4;
	const float Length = (HY * 2.f + 100.f) / Strips;
	for (int32 S = 0; S < Strips; ++S)
	{
		const float Y = -HY - 50.f + Length * (S + 0.5f);
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const FVector Centre(Side * Run * 0.5f, Y, Top + Rise * 0.5f + 12.f - (Run - HX) * 0.5f * Rise / HX);
			AddThatch(Centre, Length + 4.f, Width, -Side * Slope, 0.f);
		}
	}
}

void ABuilding::PlanLeanTo()
{
	// Two forked posts at the front hold a ridge pole; poles lean from it to the ground behind,
	// and thatch is laid over them. The open side faces -Y.
	const float HX = 190.f;
	const float H = 170.f;
	const float FrontY = -70.f;
	const float BackY = 100.f;
	AddPost(FVector(-HX + 20.f, FrontY, -10.f), H + 15.f);
	AddPost(FVector(HX - 20.f, FrontY, -10.f), H + 15.f);
	AddLog(FVector(-HX - 20.f, FrontY, H), FVector(HX + 20.f, FrontY, H), 18.f);
	for (int32 K = 0; K < 6; ++K)
	{
		const float X = -HX + 20.f + K * (2.f * HX - 40.f) / 5.f;
		AddLog(FVector(X, FrontY - 20.f, H + 10.f), FVector(X, BackY + 20.f, -5.f), 12.f);
	}
	const float Slope = FMath::RadiansToDegrees(FMath::Atan2(H, BackY - FrontY));
	const float Width = FMath::Sqrt(FMath::Square(BackY - FrontY) + H * H) + 30.f;
	for (int32 S = 0; S < 3; ++S)
	{
		const float X = -HX + (S + 0.5f) * (2.f * HX / 3.f);
		AddThatch(FVector(X, (FrontY + BackY) * 0.5f, H * 0.5f + 18.f), 2.f * HX / 3.f + 6.f, Width, -Slope, 90.f);
	}
}

void ABuilding::Plan()
{
	Pieces.Reset();
	switch (Type)
	{
	case EBuilding::Shelter: PlanLeanTo(); break;
	case EBuilding::Cabin: PlanLogHouse(190.f, 280.f, 8); break;
	case EBuilding::Storehouse: PlanLogHouse(165.f, 165.f, 7); break;
	default: break;
	}
}

FVector ABuilding::DoorOutside() const
{
	const float HY = Kind(Type).Size.Y * Storage::PlaceSize * 0.5f;
	return GetActorTransform().TransformPosition(FVector(0.f, -HY - 80.f, 0.f));
}

FVector ABuilding::BedSpot(int32 Bed) const
{
	FVector Local;
	if (Type == EBuilding::Shelter)
	{
		Local = FVector(-110.f + Bed * 110.f, 30.f, 0.f);
	}
	else
	{
		// Along the back half of the cabin, heads to the side walls.
		Local = FVector(Bed % 2 ? 90.f : -90.f, 40.f + (Bed / 2) * 130.f, 0.f);
	}
	return GetActorTransform().TransformPosition(Local);
}

float ABuilding::BedYaw(int32 Bed) const
{
	const float Local = Type == EBuilding::Shelter ? 0.f : (Bed % 2 ? 180.f : 0.f);
	return GetActorRotation().Yaw + Local;
}
