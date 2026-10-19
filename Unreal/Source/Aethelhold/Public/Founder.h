#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Founder.generated.h"

class AColony;
class UAnimSequence;
class AItem;
class UCapsuleComponent;
class USkeletalMeshComponent;

UENUM()
enum class ESkill : uint8 { Woodcutter, Forager, Hunter, Builder, Healer, Fisher, Herder };

UENUM()
enum class ETemperament : uint8 { Calm, Cheerful, HotHeaded, Gloomy, Proud, Timid, Stubborn };

/** What a founder is doing, which is also what the card says about them. */
UENUM()
enum class EActivity : uint8 { Arriving, Wandering, Resting, Talking, Eating, Warming, Sleeping,
	Chopping, Foraging, Fishing, Hauling };

/** Who a founder is: fixed at the founding, apart from age. */
USTRUCT()
struct FFounderInfo
{
	GENERATED_BODY()

	FString Name;
	FString Family;
	int32 Age = 30;
	bool bFemale = false;
	ESkill Skill = ESkill::Forager;
	ETemperament Temperament = ETemperament::Calm;
	/** Indexes into the colony's founders; INDEX_NONE when there is none. */
	int32 Spouse = INDEX_NONE;
	int32 Mother = INDEX_NONE;
	int32 Father = INDEX_NONE;
	/** Founders who had a spouse who died before the founding. */
	bool bWidowed = false;

	bool IsChild() const { return Age < 14; }
};

/**
 * One of the founders, living by simple rules (the crowd layer, docs/concept.md): needs that
 * fall over time, and a few things they do to meet them, chosen moment to moment.
 */
UCLASS()
class AETHELHOLD_API AFounder : public AActor
{
	GENERATED_BODY()

public:
	AFounder();

	void Init(AColony* InColony, int32 InIndex, const FFounderInfo& InInfo);
	/** Advances this founder by some game hours (zero while paused). */
	void Live(float GameHours);

	const FFounderInfo& GetInfo() const { return Info; }
	EActivity GetActivity() const { return Activity; }
	/** The founder this one is talking with, or INDEX_NONE. */
	int32 GetTalkingWith() const { return TalkingWith; }
	FString Describe() const;
	bool IsLying() const { return bLying; }
	FString MoodWord() const;

	// Needs, 0 (desperate) to 1 (content).
	float Food = 0.85f;
	float Warmth = 0.9f;
	float Rest = 0.8f;
	float Mood() const;

private:
	void Choose();
	/** Picks a job for working hours; false when there is nothing to do. */
	bool FindWork();
	/** Lets go of whatever this founder holds or has claimed (an item, a tree, a shrub). */
	void LetGo();
	void Arrive();
	void Work(float GameHours);
	void CarryTo(AItem* Item);
	FVector FishingSpot() const;
	bool WalkTo(const FVector& Target, float GameHours);
	FVector PickWanderSpot() const;
	FVector SleepSpot() const;
	void PlayAnim(UAnimSequence* Anim, float Rate);
	void LieDown(bool bLie);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY()
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY()
	TObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY()
	TObjectPtr<UAnimSequence> CurrentAnim;

	UPROPERTY()
	TObjectPtr<AColony> Colony;

	int32 Index = INDEX_NONE;
	FFounderInfo Info;
	FRandomStream Rng;

	EActivity Activity = EActivity::Arriving;
	FVector Target = FVector::ZeroVector;
	bool bHasTarget = false;
	/** Game hours left before deciding again while standing still. */
	float Pause = 0.f;
	int32 TalkingWith = INDEX_NONE;
	bool bLying = false;
	int32 LastMealHour = -100;

	// Work in hand.
	UPROPERTY()
	TObjectPtr<AItem> Carried;
	/** The item being fetched, before it is picked up. */
	UPROPERTY()
	TObjectPtr<AItem> Fetching;
	int32 Tree = INDEX_NONE;
	float WorkLeft = 0.f;
	bool bWaitingForFall = false;
};
