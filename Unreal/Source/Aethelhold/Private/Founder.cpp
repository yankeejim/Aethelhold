#include "Founder.h"
#include "Colony.h"
#include "Forest.h"
#include "Item.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

namespace
{
	// Game hours pass at 1/30 of an hour per real second at normal speed, so a distance per
	// real second is that many times thirty per game hour.
	constexpr float SecondsPerGameHour = 30.f;
	constexpr float WalkSpeed = 160.f; // cm per real second at normal speed
	constexpr float WalkAnimSpeed = 170.f; // the walk animation's own stride speed
}

AFounder::AFounder()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Feet"));

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->SetupAttachment(RootComponent);
	Capsule->InitCapsuleSize(35.f, 90.f);
	Capsule->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
	// Clickable, but nothing bumps into founders yet.
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->SetCollisionObjectType(ECC_Pawn);
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	// The mannequin faces +Y; turn it to face the way the founder walks.
	Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
}

void AFounder::Init(AColony* InColony, int32 InIndex, const FFounderInfo& InInfo)
{
	Colony = InColony;
	Index = InIndex;
	Info = InInfo;
	Rng.Initialize(InColony->Seed * 131 + InIndex * 17 + 5);
#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("%s %s"), *Info.Name, *Info.Family));
#endif

	const TCHAR* Mesh = Info.bFemale
		? TEXT("/Game/Mannequin/Character/Mesh/SK_Mannequin_Female.SK_Mannequin_Female")
		: TEXT("/Game/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin");
	Body->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr, Mesh));
	IdleAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Mannequin/Animations/ThirdPersonIdle.ThirdPersonIdle"));
	WalkAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Mannequin/Animations/ThirdPersonWalk.ThirdPersonWalk"));

	// Children are smaller; the old stoop a little.
	const float Scale = Info.IsChild() ? FMath::GetMappedRangeValueClamped(FVector2D(7.f, 13.f), FVector2D(0.65f, 0.8f), (float)Info.Age)
		: (Info.bFemale ? Rng.FRandRange(0.93f, 1.f) : Rng.FRandRange(0.97f, 1.05f));
	SetActorScale3D(FVector(Scale));

	// Needs vary a little from one person to the next on arrival.
	Food = Rng.FRandRange(0.7f, 0.9f);
	Warmth = Rng.FRandRange(0.8f, 0.95f);
	Rest = Rng.FRandRange(0.55f, 0.8f);
	Pause = Rng.FRandRange(0.05f, 0.4f);
	PlayAnim(IdleAnim, 1.f);
	Body->SetPosition(Rng.FRandRange(0.f, 2.f));
}

float AFounder::Mood() const
{
	float Bias = 0.f;
	switch (Info.Temperament)
	{
	case ETemperament::Cheerful: Bias = 0.1f; break;
	case ETemperament::Gloomy: Bias = -0.1f; break;
	default: break;
	}
	return FMath::Clamp((Food + Warmth + Rest) / 3.f + Bias, 0.f, 1.f);
}

FString AFounder::MoodWord() const
{
	const float M = Mood();
	return M > 0.75f ? TEXT("content") : M > 0.55f ? TEXT("well enough") : M > 0.35f ? TEXT("uneasy") : TEXT("miserable");
}

FString AFounder::Describe() const
{
	const bool bWalking = bHasTarget;
	switch (Activity)
	{
	case EActivity::Arriving: return TEXT("Looking around the new land");
	case EActivity::Wandering: return bWalking ? TEXT("Walking about the camp") : TEXT("Looking around");
	case EActivity::Resting: return TEXT("Resting");
	case EActivity::Talking:
		if (TalkingWith != INDEX_NONE)
		{
			const FString Other = Colony->GetFounders()[TalkingWith]->GetInfo().Name;
			return bWalking ? FString::Printf(TEXT("Going to talk with %s"), *Other) : FString::Printf(TEXT("Talking with %s"), *Other);
		}
		return TEXT("Talking");
	case EActivity::Eating: return bWalking ? TEXT("Going to the fire to eat") : TEXT("Eating by the fire");
	case EActivity::Warming: return bWalking ? TEXT("Cold, going to the fire") : TEXT("Warming up by the fire");
	case EActivity::Sleeping: return bWalking ? TEXT("Going to sleep") : TEXT("Asleep by the fire");
	case EActivity::Chopping:
		return bWalking ? TEXT("Going to fell a tree") : bWaitingForFall ? TEXT("Timber!") : TEXT("Felling a tree");
	case EActivity::Foraging:
		return Carried ? TEXT("Bringing hazelnuts to the stockpile") : bWalking ? TEXT("Going to gather hazelnuts") : TEXT("Gathering hazelnuts");
	case EActivity::Fishing:
		return Carried ? TEXT("Bringing the catch to the stockpile") : bWalking ? TEXT("Going down to the river to fish") : TEXT("Fishing");
	case EActivity::Hauling:
		return Carried ? FString::Printf(TEXT("Carrying a %s to the stockpile"), *Carried->Name())
			: Fetching ? FString::Printf(TEXT("Fetching a %s"), *Fetching->Name()) : TEXT("Hauling");
	}
	return FString();
}

void AFounder::PlayAnim(UAnimSequence* Anim, float Rate)
{
	if (Anim != CurrentAnim)
	{
		CurrentAnim = Anim;
		Body->PlayAnimation(Anim, true);
	}
	Body->SetPlayRate(Rate);
}

void AFounder::LieDown(bool bLie)
{
	if (bLie == bLying)
	{
		return;
	}
	bLying = bLie;
	// Lying on one side, curled toward the fire.
	Body->SetRelativeRotation(bLie ? FRotator(0.f, -90.f, 90.f) : FRotator(0.f, -90.f, 0.f));
	Body->SetRelativeLocation(bLie ? FVector(0.f, 0.f, 18.f) : FVector::ZeroVector);
}

FVector AFounder::SleepSpot() const
{
	const float Angle = Index * 2.f * PI / Colony->GetFounders().Num();
	const FVector Fire = Colony->GetFire();
	return Colony->OnGround(Fire.X + 330.f * FMath::Cos(Angle), Fire.Y + 330.f * FMath::Sin(Angle));
}

FVector AFounder::PickWanderSpot() const
{
	// Children keep near a parent; grown founders roam the land around the camp.
	FVector Center = Colony->GetFire();
	float Range = 4500.f;
	if (Info.IsChild())
	{
		const int32 Parent = Info.Mother != INDEX_NONE ? Info.Mother : Info.Father;
		if (Parent != INDEX_NONE)
		{
			Center = Colony->GetFounders()[Parent]->GetActorLocation();
			Range = 900.f;
		}
	}

	for (int32 Tries = 0; Tries < 20; ++Tries)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const float R = Rng.FRandRange(0.25f, 1.f) * Range;
		const FVector Spot = Colony->OnGround(Center.X + R * FMath::Cos(A), Center.Y + R * FMath::Sin(A));
		// Stay on dry, walkable land, and never walk through the river to get there.
		bool bDry = Colony->IsDryLand(Spot);
		for (int32 Step = 1; bDry && Step < 10; ++Step)
		{
			bDry = Colony->IsDryLand(FMath::Lerp(GetActorLocation(), Spot, Step / 10.f));
		}
		if (bDry)
		{
			return Spot;
		}
	}
	return Colony->GetFire();
}

bool AFounder::WalkTo(const FVector& Goal, float GameHours)
{
	FVector Here = GetActorLocation();
	FVector Away = Goal - Here;
	Away.Z = 0.f;
	const float Left = Away.Size();
	const float Step = WalkSpeed * (Info.IsChild() ? 0.9f : 1.f) * GameHours * SecondsPerGameHour;
	if (Left <= FMath::Max(Step, 30.f))
	{
		SetActorLocation(Colony->OnGround(Goal.X, Goal.Y));
		return true;
	}
	const FVector Dir = Away / Left;
	Here += Dir * Step;
	SetActorLocation(Colony->OnGround(Here.X, Here.Y));

	const float TurnStep = 300.f * GameHours * SecondsPerGameHour;
	FRotator Facing = GetActorRotation();
	Facing.Yaw = FMath::FixedTurn(Facing.Yaw, Dir.Rotation().Yaw, TurnStep);
	SetActorRotation(Facing);
	return false;
}

void AFounder::Choose()
{
	TalkingWith = INDEX_NONE;
	const float Roll = Rng.FRand();
	if (Roll < 0.5f)
	{
		Activity = EActivity::Wandering;
		Target = PickWanderSpot();
		bHasTarget = true;
		return;
	}
	if (Roll < 0.75f)
	{
		// Seek out someone who isn't busy, kin first.
		TArray<int32> Free;
		for (int32 I = 0; I < Colony->GetFounders().Num(); ++I)
		{
			const AFounder* Other = Colony->GetFounders()[I];
			const EActivity A = Other->GetActivity();
			if (I != Index && (A == EActivity::Resting || A == EActivity::Wandering || A == EActivity::Arriving)
				&& FVector::Dist(Other->GetActorLocation(), GetActorLocation()) < 4000.f)
			{
				Free.Add(I);
				if (Other->GetInfo().Family == Info.Family)
				{
					Free.Add(I);
				}
			}
		}
		if (Free.Num() > 0)
		{
			TalkingWith = Free[Rng.RandRange(0, Free.Num() - 1)];
			Activity = EActivity::Talking;
			const FVector Them = Colony->GetFounders()[TalkingWith]->GetActorLocation();
			FVector Side = GetActorLocation() - Them;
			Side.Z = 0.f;
			Target = Colony->OnGround(Them.X + Side.GetSafeNormal().X * 120.f, Them.Y + Side.GetSafeNormal().Y * 120.f);
			bHasTarget = true;
			return;
		}
	}
	Activity = EActivity::Resting;
	Pause = Rng.FRandRange(0.2f, 0.8f);
}

FVector AFounder::FishingSpot() const
{
	// Somewhere along the near bank, within a short walk of camp.
	const FValleyTerrain& Land = Colony->GetTerrain();
	const FVector Fire = Colony->GetFire();
	for (int32 Tries = 0; Tries < 12; ++Tries)
	{
		const float Y = Fire.Y / 100.f + Rng.FRandRange(-60.f, 60.f);
		const float Center = Land.RiverCenterX(Y);
		const float Side = Fire.X / 100.f > Center ? 1.f : -1.f;
		for (float Out = 2.f; Out < 30.f; Out += 1.f)
		{
			const float X = Center + Side * (Land.RiverHalfWidth(Center, Y) + Out);
			const FVector Spot = Colony->OnGround(X * 100.f, Y * 100.f);
			if (Colony->IsDryLand(Spot))
			{
				return Spot;
			}
		}
	}
	return Colony->GetFire();
}

bool AFounder::CarryTo(AItem* Item)
{
	Fetching = nullptr;
	CarrySpot = Colony->Reserve(Item->Type, GetActorLocation());
	if (!CarrySpot.IsValid())
	{
		Item->bClaimed = false;
		Activity = EActivity::Resting;
		Pause = 0.1f;
		return false;
	}
	Carried = Item;
	Item->bClaimed = true;
	Target = Colony->SpotLocation(CarrySpot, Item->Type);
	bHasTarget = true;
	return true;
}

void AFounder::LetGo()
{
	if (Carried)
	{
		Colony->Unreserve(CarrySpot);
		CarrySpot = FStoreSpot();
		Colony->Drop(Carried, GetActorLocation() + GetActorForwardVector() * 60.f);
		Carried = nullptr;
	}
	if (Fetching)
	{
		Fetching->bClaimed = false;
		Fetching = nullptr;
	}
	if (Tree != INDEX_NONE && !bWaitingForFall)
	{
		Colony->GetForest()->GetTrees()[Tree].bClaimed = false;
		Tree = INDEX_NONE;
	}
	WorkLeft = 0.f;
}

bool AFounder::FindWork()
{
	AForest* Forest = Colony->GetForest();
	const FVector Camp = Colony->GetFire();
	const bool bChild = Info.IsChild();

	// Things lying about get carried in; children only fetch what's close to camp.
	AItem* Loose = Colony->LooseItemNear(bChild ? Camp : GetActorLocation(), bChild ? 3000.f : 15000.f);

	auto GoNear = [this](const FVector& Spot, float Distance)
	{
		FVector Side = GetActorLocation() - Spot;
		Side.Z = 0.f;
		const FVector Stand = Spot + Side.GetSafeNormal() * Distance;
		Target = Colony->OnGround(Stand.X, Stand.Y);
		bHasTarget = true;
	};
	auto Chop = [&]()
	{
		const int32 Marked = bChild ? INDEX_NONE : Forest->NearestMarked(Camp);
		if (Marked == INDEX_NONE)
		{
			return false;
		}
		Tree = Marked;
		Forest->GetTrees()[Tree].bClaimed = true;
		Activity = EActivity::Chopping;
		GoNear(Forest->GetTrees()[Tree].Base(), 110.f);
		return true;
	};
	auto Forage = [&]()
	{
		const int32 Shrub = Forest->NearestForage(bChild ? GetActorLocation() : Camp, bChild ? 4000.f : 25000.f);
		if (Shrub == INDEX_NONE)
		{
			return false;
		}
		Tree = Shrub;
		Forest->GetTrees()[Tree].bClaimed = true;
		Activity = EActivity::Foraging;
		GoNear(Forest->GetTrees()[Tree].Base(), 150.f);
		return true;
	};
	auto Fish = [&]()
	{
		Activity = EActivity::Fishing;
		Target = FishingSpot();
		bHasTarget = true;
		return true;
	};
	auto Haul = [&]()
	{
		if (!Loose)
		{
			return false;
		}
		Activity = EActivity::Hauling;
		Fetching = Loose;
		Loose->bClaimed = true;
		Target = Colony->OnGround(Loose->GetActorLocation().X, Loose->GetActorLocation().Y);
		bHasTarget = true;
		return true;
	};

	// Each turns first to their own trade, then lends a hand where it's needed.
	if (bChild)
	{
		return Haul() || Forage();
	}
	switch (Info.Skill)
	{
	case ESkill::Fisher: return Fish();
	case ESkill::Woodcutter:
	case ESkill::Builder: return Chop() || Haul() || Forage();
	case ESkill::Herder: return Haul() || Chop() || Forage();
	default: return Forage() || Haul() || Chop();
	}
}

void AFounder::Arrive()
{
	PlayAnim(IdleAnim, 1.f);
	auto Face = [this](const FVector& Where)
	{
		SetActorRotation(FRotator(0.f, (Where - GetActorLocation()).Rotation().Yaw, 0.f));
	};

	if (Carried)
	{
		// At the stockpile with something to put down.
		Colony->Store(Carried, CarrySpot);
		CarrySpot = FStoreSpot();
		Carried = nullptr;
		Activity = EActivity::Resting;
		Pause = Rng.FRandRange(0.05f, 0.2f);
		return;
	}
	switch (Activity)
	{
	case EActivity::Sleeping:
		Face(Colony->GetFire());
		LieDown(true);
		break;
	case EActivity::Eating:
		Face(Colony->GetFire());
		if (Colony->TakeMeal())
		{
			Food = FMath::Min(1.f, Food + 0.5f);
		}
		LastMealHour = Colony->GetDay() * 2 + (Colony->GetHour() > 12.f ? 1 : 0);
		Pause = 0.5f;
		break;
	case EActivity::Warming:
		Face(Colony->GetFire());
		break;
	case EActivity::Talking:
		if (TalkingWith != INDEX_NONE)
		{
			Face(Colony->GetFounders()[TalkingWith]->GetActorLocation());
		}
		Pause = Rng.FRandRange(0.4f, 0.9f);
		break;
	case EActivity::Chopping:
	{
		const FTreeSpot& Spot = Colony->GetForest()->GetTrees()[Tree];
		Face(Spot.Base());
		// Young trees come down in under an hour; grown ones take a good while.
		WorkLeft = Spot.Shape >= 2 ? 0.6f : 1.6f;
		break;
	}
	case EActivity::Foraging:
		Face(Colony->GetForest()->GetTrees()[Tree].Base());
		WorkLeft = 0.8f;
		break;
	case EActivity::Fishing:
	{
		const float Y = GetActorLocation().Y / 100.f;
		Face(FVector(Colony->GetTerrain().RiverCenterX(Y) * 100.f, GetActorLocation().Y, 0.f));
		WorkLeft = Rng.FRandRange(1.5f, 2.5f);
		break;
	}
	case EActivity::Hauling:
		if (Fetching)
		{
			CarryTo(Fetching);
		}
		break;
	default:
		Pause = Rng.FRandRange(0.1f, 0.5f);
		break;
	}
}

void AFounder::Work(float GameHours)
{
	AForest* Forest = Colony->GetForest();
	if (bWaitingForFall)
	{
		// Watch it come down; the logs lie where it fell.
		if (Forest->IsDown(Tree))
		{
			const FTreeSpot& Spot = Forest->GetTrees()[Tree];
			const FVector Dir = FRotator(0.f, Spot.FallYaw, 0.f).Vector();
			for (int32 L = 0; L < Spot.Logs(); ++L)
			{
				const FVector At = Spot.Base() + Dir * (250.f + L * 260.f) + FVector(Dir.Y, -Dir.X, 0.f) * Rng.FRandRange(-60.f, 60.f);
				Colony->SpawnItem(EItem::Log, 0, At, Spot.FallYaw + Rng.FRandRange(-10.f, 10.f));
			}
			bWaitingForFall = false;
			Tree = INDEX_NONE;
			Activity = EActivity::Resting;
			Pause = 0.1f;
		}
		return;
	}
	WorkLeft -= GameHours;
	if (WorkLeft > 0.f)
	{
		return;
	}
	switch (Activity)
	{
	case EActivity::Chopping:
		Forest->Fell(Tree, GetActorLocation());
		bWaitingForFall = true;
		break;
	case EActivity::Foraging:
	{
		FTreeSpot& Spot = Forest->GetTrees()[Tree];
		Spot.Forage -= 1;
		Spot.bClaimed = false;
		Tree = INDEX_NONE;
		CarryTo(Colony->SpawnItem(EItem::Nuts, 3, GetActorLocation(), GetActorRotation().Yaw));
		break;
	}
	case EActivity::Fishing:
	{
		// Some days the fish bite, some days they don't.
		const int32 Caught = Rng.RandRange(0, 3);
		if (Caught == 0)
		{
			Colony->Say(FString::Printf(TEXT("%s fished a while and caught nothing."), *Info.Name));
			Activity = EActivity::Resting;
			Pause = 0.1f;
		}
		else
		{
			if (Caught == 3)
			{
				Colony->Say(FString::Printf(TEXT("%s had a good catch: three fish."), *Info.Name));
			}
			CarryTo(Colony->SpawnItem(EItem::Fish, Caught * 2, GetActorLocation(), GetActorRotation().Yaw));
		}
		break;
	}
	default:
		break;
	}
}

void AFounder::Live(float GameHours)
{
	if (!Colony)
	{
		return;
	}
	// Animation keeps pace with the game speed, and stops when paused.
	Body->GlobalAnimRateScale = (float)Colony->GetSpeed();
	if (GameHours <= 0.f)
	{
		return;
	}

	// Needs: hunger over a day, tiredness over a waking day (sooner when working), warmth
	// toward what the air allows.
	Food = FMath::Clamp(Food - GameHours / 24.f, 0.f, 1.f);
	const bool bAsleep = Activity == EActivity::Sleeping && !bHasTarget;
	const bool bWorking = Activity == EActivity::Chopping || Activity == EActivity::Hauling
		|| Activity == EActivity::Fishing || Activity == EActivity::Foraging;
	Rest = FMath::Clamp(Rest + (bAsleep ? GameHours / 7.f : -GameHours / (bWorking ? 13.f : 17.f)), 0.f, 1.f);
	const bool bByFire = FVector::Dist2D(GetActorLocation(), Colony->GetFire()) < 450.f;
	const float WarmthHere = bByFire ? 1.f : FMath::Clamp((Colony->Temperature() - 4.f) / 14.f, 0.f, 1.f);
	Warmth += (WarmthHere - Warmth) * FMath::Min(1.f, GameHours * (bByFire ? 1.5f : 0.5f));

	// What needs doing most: sleep at night, eat at mealtimes, warm up when cold. A tree
	// already toppling is seen through first.
	const int32 MealSlot = Colony->GetDay() * 2 + (Colony->GetHour() > 12.f ? 1 : 0);
	EActivity Want = EActivity::Wandering;
	if (!bWaitingForFall)
	{
		if (Colony->IsNight())
		{
			Want = EActivity::Sleeping;
		}
		else if (Colony->IsMealTime() && LastMealHour != MealSlot)
		{
			Want = EActivity::Eating;
		}
		else if (Warmth < 0.35f)
		{
			Want = EActivity::Warming;
		}
	}

	const bool bUrgent = Want != EActivity::Wandering;
	if (bUrgent && Activity != Want)
	{
		LetGo();
		Activity = Want;
		TalkingWith = INDEX_NONE;
		LieDown(false);
		const FVector Fire = Colony->GetFire();
		const float Angle = Index * 2.f * PI / Colony->GetFounders().Num() + 0.4f;
		Target = Want == EActivity::Sleeping ? SleepSpot()
			: Colony->OnGround(Fire.X + 190.f * FMath::Cos(Angle), Fire.Y + 190.f * FMath::Sin(Angle));
		bHasTarget = true;
	}
	else if (!bUrgent && (Activity == EActivity::Sleeping || Activity == EActivity::Warming
		|| (Activity == EActivity::Eating && Pause <= 0.f)))
	{
		LieDown(false);
		Activity = EActivity::Resting;
		Pause = 0.f;
	}

	if (Carried)
	{
		// Logs ride on the shoulder; food is held in front.
		const bool bLog = Carried->Type == EItem::Log;
		const float Scale = GetActorScale3D().Z;
		const FVector At = GetActorLocation() + FVector(0.f, 0.f, (bLog ? 150.f : 95.f) * Scale)
			+ GetActorRightVector() * (bLog ? 18.f : 0.f) + GetActorForwardVector() * (bLog ? 0.f : 35.f);
		Carried->SetActorLocationAndRotation(At, FRotator(0.f, GetActorRotation().Yaw + (bLog ? 0.f : 90.f), 0.f));
	}

	if (bHasTarget)
	{
		PlayAnim(WalkAnim, WalkSpeed / WalkAnimSpeed);
		if (WalkTo(Target, GameHours))
		{
			bHasTarget = false;
			Arrive();
		}
		return;
	}

	PlayAnim(IdleAnim, 1.f);
	if (bWaitingForFall || WorkLeft > 0.f)
	{
		Work(GameHours);
		return;
	}
	Pause -= GameHours;
	if (Pause <= 0.f && !bUrgent)
	{
		const float H = Colony->GetHour();
		const bool bWorkHours = H >= 8.f && H < 18.f;
		if (!(bWorkHours && FindWork()))
		{
			Choose();
		}
	}
}
