#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "AethelGameMode.generated.h"

/** The player's controller: a free mouse cursor over the world. */
UCLASS()
class AETHELHOLD_API AAethelPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AAethelPlayerController();

protected:
	virtual void BeginPlay() override;
};

UCLASS()
class AETHELHOLD_API AAethelGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAethelGameMode();

	virtual void StartPlay() override;
};
