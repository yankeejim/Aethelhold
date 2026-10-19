#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "StewardCamera.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * The Steward's eye: a camera that orbits a point on the ground, from high above the valley
 * down to eye level among the founders, like Manor Lords. WASD pans, the wheel zooms,
 * Q/E or the middle mouse button turns, Shift pans faster. Close in, the view tilts toward
 * the horizon; far out, it looks down on the map.
 */
UCLASS()
class AETHELHOLD_API AStewardCamera : public APawn
{
	GENERATED_BODY()

public:
	AStewardCamera();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;

private:
	void OnMove(const FInputActionValue& Value);
	void OnZoom(const FInputActionValue& Value);
	void OnTurn(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnSelect();
	void OnSelectReleased();
	void OnMarkTrees();
	void OnStockpiles();
	bool GroundUnderCursor(FVector& Out) const;
	void OnPause();
	void OnSpeed1() { SetSpeed(1); }
	void OnSpeed2() { SetSpeed(3); }
	void OnSpeed3() { SetSpeed(10); }
	void OnFollow();
	void SetSpeed(int32 Speed);
	class AColony* GetColony();

	/** 0 when fully zoomed in, 1 when fully out. */
	float ZoomAlpha(float Distance) const;
	float GroundHeight(const FVector& At) const;
	void BuildInput();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> Arm;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MinDistance = 450.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MaxDistance = 140000.f;

	/** Pitch when fully zoomed in and fully out, in degrees below the horizon. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float NearPitch = 8.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float FarPitch = 62.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float TurnSpeed = 90.f;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> Mapping;

	UPROPERTY()
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY()
	TObjectPtr<UInputAction> ZoomAction;

	UPROPERTY()
	TObjectPtr<UInputAction> TurnAction;

	UPROPERTY()
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY()
	TObjectPtr<UInputAction> SelectAction;

	UPROPERTY()
	TObjectPtr<UInputAction> PauseAction;

	UPROPERTY()
	TObjectPtr<UInputAction> SpeedActions[3];

	UPROPERTY()
	TObjectPtr<UInputAction> FollowAction;

	UPROPERTY()
	TObjectPtr<UInputAction> MarkAction;

	UPROPERTY()
	TObjectPtr<UInputAction> StockpileAction;

	/** Where a drag to mark trees began, on the ground. */
	FVector DragStart = FVector::ZeroVector;
	bool bDragging = false;

	TWeakObjectPtr<class AColony> Colony;
	/** Keep the selected founder centred until the player pans away. */
	bool bFollow = false;

	// Where the camera is heading; the view eases toward these each frame.
	FVector TargetFocus;
	float TargetYaw = 0.f;
	float TargetDistance = 6000.f;
	float PitchOffset = 0.f;

	FVector Focus;
	float Yaw = 0.f;
	float Distance = 6000.f;

	FVector2D PendingMove = FVector2D::ZeroVector;
	float PendingTurn = 0.f;
};
