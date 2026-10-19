#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Item.generated.h"

class UStaticMeshComponent;

UENUM()
enum class EItem : uint8 { Log, Nuts, Fish };

/**
 * A thing in the world (docs/resources.md: items are physical). It lies where it was made
 * until someone carries it to the stockpile. Food spoils unless eaten in time.
 */
UCLASS()
class AETHELHOLD_API AItem : public AActor
{
	GENERATED_BODY()

public:
	AItem();

	void Init(EItem InType, int32 InPortions);

	EItem Type = EItem::Log;
	/** Meals in it (food only). */
	int32 Portions = 0;
	/** Game hours before it rots; negative for things that keep. */
	float HoursLeft = -1.f;
	/** Lying in the stockpile (rather than where it was made or dropped). */
	bool bStored = false;
	/** Someone is on their way to fetch it, or carrying it. */
	bool bClaimed = false;

	bool IsFood() const { return Type != EItem::Log; }
	FString Name() const;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
};
