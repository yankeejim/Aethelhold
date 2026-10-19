#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Storage.h"
#include "ValleyTerrain.h"
#include "Colony.generated.h"

class AFounder;
class AForest;
class AItem;
class ABuilding;
enum class EBuilding : uint8;
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

	// --- items ---
	AItem* SpawnItem(EItem Type, int32 Portions, const FVector& At, float Yaw);
	/** The nearest item lying loose (not stored, not claimed) within range of a point, that some
	 *  stockpile has room for. */
	AItem* LooseItemNear(const FVector& From, float Range) const;
	void Drop(AItem* Item, const FVector& At);

	// --- stockpiles (Storage.h) ---
	/** Reserves room for an item in the nearest stockpile that takes it; invalid when full. */
	FStoreSpot Reserve(EItem Kind, const FVector& From);
	void Unreserve(const FStoreSpot& Spot);
	bool HasRoomFor(EItem Kind) const;
	FVector SpotLocation(const FStoreSpot& Spot, EItem Kind) const;
	void Store(AItem* Item, const FStoreSpot& Spot);
	/** Takes an item out of its stockpile place (taken, eaten, rotted, or its stockpile removed). */
	void Unstore(AItem* Item);
	/** Draws a new stockpile over a rectangle (world cm); returns how many places it got. */
	int32 AddZone(const FVector2D& A, const FVector2D& B);
	/** Removes stockpiles under a rectangle; what lay in them is left loose. */
	int32 RemoveZones(const FVector2D& A, const FVector2D& B);
	/** Changes what the stockpile under a point takes; false when there is none. */
	bool CycleZone(const FVector2D& At);
	const TArray<FStoreZone>& GetZones() const { return Zones; }
	const FStorePlace* FindPlace(const FIntPoint& Place) const { return Places.Find(Place); }
	FVector PlaceCenter(const FIntPoint& Place) const;

	// --- what the player is doing ---
	enum class EOrder : uint8 { None, MarkTrees, Stockpiles, Build };
	EOrder GetOrder() const { return Order; }
	void ToggleOrder(EOrder O) { Order = Order == O ? EOrder::None : O; }
	bool IsMarkingTrees() const { return Order == EOrder::MarkTrees; }
	/** Build mode: B steps through what to build (and back out); R turns it. */
	void NextBuildChoice();
	void TurnBuildChoice() { BuildTurns = (BuildTurns + 1) % 4; }
	EBuilding GetBuildChoice() const { return BuildChoice; }
	int32 GetBuildTurns() const { return BuildTurns; }

	// --- buildings (Building.h) ---
	/** The first place a footprint would start at, centred on a point of ground. */
	FIntPoint FootprintAt(EBuilding Type, int32 Turns, const FVector2D& Centre) const;
	/** Whether it can go there; Why says what's in the way. */
	bool CanPlace(EBuilding Type, int32 Turns, const FIntPoint& First, FString* Why = nullptr) const;
	ABuilding* PlaceBuilding(EBuilding Type, int32 Turns, const FVector2D& Centre);
	const TArray<TObjectPtr<ABuilding>>& GetBuildings() const { return Buildings; }
	/** The nearest unfinished building that still wants logs / has work to do. */
	ABuilding* SiteNeedingLogs(const FVector& From) const;
	ABuilding* SiteToWork(const FVector& From) const;
	/** The nearest log anyone may take (stored or lying loose). */
	AItem* LogToTake(const FVector& From) const;
	/** A log arrives at a building site. */
	void Deliver(AItem* Log, ABuilding* Site);
	/** Called as building work is done; sorts out what a finished building brings. */
	void OnBuilt(ABuilding* Building);
	/** The world X/Y of a place's corner nearest the origin. */
	FVector2D PlaceCorner(const FIntPoint& Place) const;

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
	void AssignHomes();
	/** How fast food rots here and now: 1 is the usual pace in the open. */
	float SpoilRate(const AItem* Item) const;

	FValleyTerrain Terrain{ 1 };
	FVector Fire = FVector::ZeroVector;

	UPROPERTY()
	TArray<TObjectPtr<AFounder>> Founders;

	UPROPERTY()
	TObjectPtr<AForest> Forest;

	UPROPERTY()
	TArray<TObjectPtr<AItem>> Items;

	TArray<FStoreZone> Zones;
	TMap<FIntPoint, FStorePlace> Places;
	EOrder Order = EOrder::None;
	EBuilding BuildChoice;
	int32 BuildTurns = 0;

	UPROPERTY()
	TArray<TObjectPtr<ABuilding>> Buildings;
	/** Places of ground under buildings, to the building's index. */
	TMap<FIntPoint, int32> Occupied;
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
