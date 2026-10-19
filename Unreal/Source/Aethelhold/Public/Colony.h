#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ValleyTerrain.h"
#include "Colony.generated.h"

class AFounder;
class AForest;
class AItem;
enum class EItem : uint8;
class ADirectionalLight;
class UPointLightComponent;
class UInstancedStaticMeshComponent;

/**
 * The settlement as a whole: the clock, the stores, the campfire and the founders. There is
 * one, spawned by the game mode where the founders arrive.
 */
UCLASS()
class AETHELHOLD_API AColony : public AActor
{
	GENERATED_BODY()

public:
	AColony();

	virtual void Tick(float DeltaSeconds) override;

	// --- time ---
	/** Game speed: 0 paused, otherwise a multiple of normal (one day in twelve minutes). */
	void SetSpeed(int32 NewSpeed);
	int32 GetSpeed() const { return Speed; }
	void TogglePause();
	int32 GetDay() const { return Day; }
	/** Hour of the day, 0..24. */
	float GetHour() const { return Hour; }
	/** Air temperature in degrees Celsius, from the hour (seasons come later). */
	float Temperature() const;
	bool IsNight() const { return Hour >= 21.f || Hour < 6.f; }
	bool IsMealTime() const { return (Hour >= 7.f && Hour < 8.f) || (Hour >= 18.5f && Hour < 19.5f); }
	FString SeasonName() const { return TEXT("Late summer"); }

	// --- place ---
	const FValleyTerrain& GetTerrain() const { return Terrain; }
	/** A point on the ground (world cm) at the given world X/Y. */
	FVector OnGround(float X, float Y) const;
	bool IsDryLand(const FVector& At) const;
	FVector GetFire() const { return Fire; }

	// --- people and stores ---
	const TArray<TObjectPtr<AFounder>>& GetFounders() const { return Founders; }
	AForest* GetForest() const { return Forest; }
	/** All the food there is to eat: the rations they brought plus what's in the stockpile. */
	int32 GetFood() const;
	int32 GetRations() const { return FoodPortions; }
	int32 CountStored(EItem Type) const;
	/** Takes one meal from the stores, the food closest to spoiling first; false when empty. */
	bool TakeMeal();

	// --- items and the stockpile ---
	AItem* SpawnItem(EItem Type, int32 Portions, const FVector& At, float Yaw);
	/** The nearest item lying loose (not stored, not claimed) within range of a point. */
	AItem* LooseItemNear(const FVector& From, float Range) const;
	/** Where in the stockpile the next item of this kind goes. */
	FVector StockSpot(EItem Type) const;
	FVector GetStockpile() const { return Stockpile; }
	void Store(AItem* Item);
	void Drop(AItem* Item, const FVector& At);

	// --- what the player has told them ---
	bool IsMarkingTrees() const { return bMarkingTrees; }
	void SetMarkingTrees(bool bOn) { bMarkingTrees = bOn; }

	// --- news, as the founders would tell it ---
	void Say(const FString& Text);
	struct FNews { FString Text; float When; };
	const TArray<FNews>& GetNews() const { return News; }
	/** Game hours since the founding. */
	float GetClock() const { return (Day - 1) * 24.f + Hour; }

	UPROPERTY(EditAnywhere, Category = "Colony")
	int32 Seed = 1;

private:
	virtual void BeginPlay() override;
	void FoundTheColony();
	void BuildCampfire();
	void UpdateSun();
	void Spoil(float GameHours);

	FValleyTerrain Terrain{ 1 };
	FVector Fire = FVector::ZeroVector;

	UPROPERTY()
	TArray<TObjectPtr<AFounder>> Founders;

	UPROPERTY()
	TObjectPtr<AForest> Forest;

	UPROPERTY()
	TArray<TObjectPtr<AItem>> Items;

	FVector Stockpile = FVector::ZeroVector;
	bool bMarkingTrees = false;
	TArray<FNews> News;

	UPROPERTY()
	TObjectPtr<ADirectionalLight> Sun;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> FireLight;

	/** A faint second sun for night, so the valley isn't pitch black. */
	UPROPERTY()
	TObjectPtr<class UDirectionalLightComponent> Moon;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> FireStones;

	int32 Speed = 1;
	int32 SpeedBeforePause = 1;
	int32 Day = 1;
	float Hour = 9.f;
	/** The founders brought ten days of food: two meals a day for seven. */
	int32 FoodPortions = 140;
	float FlickerTime = 0.f;
};
