#include "AethelGameMode.h"
#include "StewardCamera.h"
#include "AethelHUD.h"
#include "Colony.h"
#include "Engine/World.h"

AAethelPlayerController::AAethelPlayerController()
{
	bShowMouseCursor = true;
}

void AAethelPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

AAethelGameMode::AAethelGameMode()
{
	DefaultPawnClass = AStewardCamera::StaticClass();
	PlayerControllerClass = AAethelPlayerController::StaticClass();
	HUDClass = AAethelHUD::StaticClass();
}

void AAethelGameMode::StartPlay()
{
	// The founders arrive as the game begins.
	GetWorld()->SpawnActor<AColony>();
	Super::StartPlay();
}
