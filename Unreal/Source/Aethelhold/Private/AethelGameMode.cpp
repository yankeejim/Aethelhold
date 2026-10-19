#include "AethelGameMode.h"
#include "StewardCamera.h"

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
}
