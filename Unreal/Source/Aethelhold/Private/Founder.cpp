#include "Founder.h"
#include "Colony.h"
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

	// Needs: hunger over a day, tiredness over a waking day, warmth toward what the air allows.
	Food = FMath::Clamp(Food - GameHours / 24.f, 0.f, 1.f);
	const bool bAsleep = Activity == EActivity::Sleeping && !bHasTarget;
	Rest = FMath::Clamp(Rest + (bAsleep ? GameHours / 7.f : -GameHours / 17.f), 0.f, 1.f);
	const bool bByFire = FVector::Dist2D(GetActorLocation(), Colony->GetFire()) < 450.f;
	const float WarmthHere = bByFire ? 1.f : FMath::Clamp((Colony->Temperature() - 4.f) / 14.f, 0.f, 1.f);
	Warmth += (WarmthHere - Warmth) * FMath::Min(1.f, GameHours * (bByFire ? 1.5f : 0.5f));

	// What needs doing most: sleep at night, eat at mealtimes, warm up when cold.
	const int32 MealSlot = Colony->GetDay() * 2 + (Colony->GetHour() > 12.f ? 1 : 0);
	EActivity Want = EActivity::Wandering;
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

	const bool bUrgent = Want != EActivity::Wandering;
	if (bUrgent && Activity != Want)
	{
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

	if (bHasTarget)
	{
		PlayAnim(WalkAnim, WalkSpeed / WalkAnimSpeed);
		if (WalkTo(Target, GameHours))
		{
			bHasTarget = false;
			PlayAnim(IdleAnim, 1.f);
			if (Activity == EActivity::Sleeping)
			{
				// Face the fire, then lie down.
				SetActorRotation(FRotator(0.f, (Colony->GetFire() - GetActorLocation()).Rotation().Yaw, 0.f));
				LieDown(true);
			}
			else if (Activity == EActivity::Eating)
			{
				SetActorRotation(FRotator(0.f, (Colony->GetFire() - GetActorLocation()).Rotation().Yaw, 0.f));
				if (Colony->TakeMeal())
				{
					Food = FMath::Min(1.f, Food + 0.5f);
				}
				LastMealHour = MealSlot;
				Pause = 0.5f;
			}
			else if (Activity == EActivity::Warming)
			{
				SetActorRotation(FRotator(0.f, (Colony->GetFire() - GetActorLocation()).Rotation().Yaw, 0.f));
			}
			else if (Activity == EActivity::Talking && TalkingWith != INDEX_NONE)
			{
				const FVector Them = Colony->GetFounders()[TalkingWith]->GetActorLocation();
				SetActorRotation(FRotator(0.f, (Them - GetActorLocation()).Rotation().Yaw, 0.f));
				Pause = Rng.FRandRange(0.4f, 0.9f);
			}
			else
			{
				Pause = Rng.FRandRange(0.1f, 0.5f);
			}
		}
		return;
	}

	PlayAnim(IdleAnim, 1.f);
	Pause -= GameHours;
	if (Pause <= 0.f && !bUrgent)
	{
		Choose();
	}
}
