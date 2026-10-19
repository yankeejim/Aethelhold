#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Building.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UENUM()
enum class EBuilding : uint8 { Shelter, Cabin, Storehouse, Count };

/**
 * A building, from staked-out site to finished: the founders bring logs, then build it up
 * piece by piece (foundation stones, wall logs course by course, gables, roof), so it rises in
 * visible stages. Made from logs, stone and thatch until the building art comes.
 */
UCLASS()
class AETHELHOLD_API ABuilding : public AActor
{
	GENERATED_BODY()

public:
	ABuilding();

	/** What each kind costs and does. */
	struct FKind
	{
		const TCHAR* Name;
		/** Footprint in storage places (Storage::PlaceSize), before turning. */
		FIntPoint Size;
		int32 Logs;
		/** Hours of building work once the logs are there. */
		float Work;
		int32 Beds;
	};
	static const FKind& Kind(EBuilding Type);
	/** Footprint in places once turned by Turns quarter turns. */
	static FIntPoint Footprint(EBuilding Type, int32 Turns);

	void Init(EBuilding InType, int32 InTurns, const FIntPoint& InFirstPlace);

	EBuilding GetType() const { return Type; }
	const FKind& GetKind() const { return Kind(Type); }
	int32 GetTurns() const { return Turns; }
	/** The places of ground it stands on. */
	TArray<FIntPoint> Places() const;

	// --- construction ---
	bool IsFinished() const { return Progress >= 1.f; }
	/** Logs brought so far, and on their way. */
	int32 Delivered = 0;
	int32 Incoming = 0;
	int32 LogsWanted() const { return GetKind().Logs - Delivered - Incoming; }
	/** True when there is work to do with the logs already here. */
	bool CanWork() const;
	void AddWork(float Hours);
	float GetProgress() const { return Progress; }

	// --- use ---
	/** Where to stand outside the door. */
	FVector DoorOutside() const;
	FVector BedSpot(int32 Bed) const;
	/** Facing of a sleeper in a bed. */
	float BedYaw(int32 Bed) const;

private:
	struct FPiece
	{
		UStaticMesh* Mesh;
		UMaterialInterface* Material;
		FTransform Transform;
	};
	void Plan();
	void AddLog(const FVector& From, const FVector& To, float Diameter = 26.f);
	void AddPost(const FVector& Base, float Height, float Diameter = 22.f);
	void AddThatch(const FVector& Centre, float Length, float Width, float Pitch, float Yaw);
	void AddStone(const FVector& At, float Scale);
	/** A log house: walls of stacked logs, a door in the front, gables and a thatched roof. */
	void PlanLogHouse(float HalfX, float HalfY, int32 Courses);
	void PlanLeanTo();
	void ShowPieces();

	EBuilding Type = EBuilding::Shelter;
	int32 Turns = 0;
	FIntPoint FirstPlace;
	float Progress = 0.f;
	TArray<FPiece> Pieces;
	int32 Shown = 0;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Stakes;
};
