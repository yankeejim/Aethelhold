#include "StewardCamera.h"
#include "ValleyTerrain.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "AethelHUD.h"
#include "Colony.h"
#include "EngineUtils.h"
#include "Founder.h"
#include "Forest.h"

AStewardCamera::AStewardCamera()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Focus"));

	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Arm"));
	Arm->SetupAttachment(RootComponent);
	Arm->bDoCollisionTest = false;
	Arm->bEnableCameraLag = false;
	Arm->bUsePawnControlRotation = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Arm, USpringArmComponent::SocketName);
	Camera->SetFieldOfView(55.f);
}

void AStewardCamera::BeginPlay()
{
	Super::BeginPlay();
	Focus = TargetFocus = GetActorLocation();
	Yaw = TargetYaw = GetActorRotation().Yaw;

	// For screenshots and testing: -cam=X,Y,Yaw,Distance (meters and degrees).
	FString Cam;
	if (FParse::Value(FCommandLine::Get(), TEXT("-cam="), Cam, false))
	{
		TArray<FString> Parts;
		Cam.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() == 4)
		{
			Focus = TargetFocus = FVector(FCString::Atof(*Parts[0]) * 100.f, FCString::Atof(*Parts[1]) * 100.f, Focus.Z);
			Yaw = TargetYaw = FCString::Atof(*Parts[2]);
			TargetDistance = FMath::Clamp(FCString::Atof(*Parts[3]) * 100.f, MinDistance, MaxDistance);
		}
	}
	Distance = TargetDistance;
}

void AStewardCamera::BuildInput()
{
	Mapping = NewObject<UInputMappingContext>(this);

	MoveAction = NewObject<UInputAction>(this);
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	ZoomAction = NewObject<UInputAction>(this);
	ZoomAction->ValueType = EInputActionValueType::Axis1D;
	TurnAction = NewObject<UInputAction>(this);
	TurnAction->ValueType = EInputActionValueType::Axis1D;
	LookAction = NewObject<UInputAction>(this);
	LookAction->ValueType = EInputActionValueType::Axis2D;

	// Keys give +X by default; swizzle turns that into +Y, negate flips it.
	auto MapMove = [this](FKey Key, bool bForwardAxis, bool bNegative)
	{
		FEnhancedActionKeyMapping& M = Mapping->MapKey(MoveAction, Key);
		if (bForwardAxis)
		{
			M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		}
		if (bNegative)
		{
			M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		}
	};
	MapMove(EKeys::W, true, false);
	MapMove(EKeys::Up, true, false);
	MapMove(EKeys::S, true, true);
	MapMove(EKeys::Down, true, true);
	MapMove(EKeys::D, false, false);
	MapMove(EKeys::Right, false, false);
	MapMove(EKeys::A, false, true);
	MapMove(EKeys::Left, false, true);

	Mapping->MapKey(ZoomAction, EKeys::MouseWheelAxis);

	Mapping->MapKey(TurnAction, EKeys::E);
	FEnhancedActionKeyMapping& TurnLeft = Mapping->MapKey(TurnAction, EKeys::Q);
	TurnLeft.Modifiers.Add(NewObject<UInputModifierNegate>(this));

	Mapping->MapKey(LookAction, EKeys::Mouse2D);

	auto Button = [this](FKey Key)
	{
		UInputAction* Action = NewObject<UInputAction>(this);
		Action->ValueType = EInputActionValueType::Boolean;
		Mapping->MapKey(Action, Key);
		return Action;
	};
	SelectAction = Button(EKeys::LeftMouseButton);
	PauseAction = Button(EKeys::SpaceBar);
	SpeedActions[0] = Button(EKeys::One);
	SpeedActions[1] = Button(EKeys::Two);
	SpeedActions[2] = Button(EKeys::Three);
	FollowAction = Button(EKeys::F);
	MarkAction = Button(EKeys::C);
}

void AStewardCamera::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInput();

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Input->AddMappingContext(Mapping, 0);
		}
	}

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AStewardCamera::OnMove);
		EIC->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AStewardCamera::OnZoom);
		EIC->BindAction(TurnAction, ETriggerEvent::Triggered, this, &AStewardCamera::OnTurn);
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AStewardCamera::OnLook);
		EIC->BindAction(SelectAction, ETriggerEvent::Started, this, &AStewardCamera::OnSelect);
		EIC->BindAction(PauseAction, ETriggerEvent::Started, this, &AStewardCamera::OnPause);
		EIC->BindAction(SpeedActions[0], ETriggerEvent::Started, this, &AStewardCamera::OnSpeed1);
		EIC->BindAction(SpeedActions[1], ETriggerEvent::Started, this, &AStewardCamera::OnSpeed2);
		EIC->BindAction(SpeedActions[2], ETriggerEvent::Started, this, &AStewardCamera::OnSpeed3);
		EIC->BindAction(FollowAction, ETriggerEvent::Started, this, &AStewardCamera::OnFollow);
		EIC->BindAction(MarkAction, ETriggerEvent::Started, this, &AStewardCamera::OnMarkTrees);
		EIC->BindAction(SelectAction, ETriggerEvent::Completed, this, &AStewardCamera::OnSelectReleased);
	}
}

void AStewardCamera::OnMove(const FInputActionValue& Value)
{
	PendingMove += Value.Get<FVector2D>();
	bFollow = false;
}

AColony* AStewardCamera::GetColony()
{
	if (!Colony.IsValid())
	{
		for (TActorIterator<AColony> It(GetWorld()); It; ++It)
		{
			Colony = *It;
			break;
		}
	}
	return Colony.Get();
}

bool AStewardCamera::GroundUnderCursor(FVector& Out) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	FHitResult Hit;
	if (PC && PC->GetHitResultUnderCursorForObjects({ UEngineTypes::ConvertToObjectType(ECC_WorldStatic) }, false, Hit))
	{
		Out = Hit.ImpactPoint;
		return true;
	}
	return false;
}

void AStewardCamera::OnSelect()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	AAethelHUD* Hud = PC ? Cast<AAethelHUD>(PC->GetHUD()) : nullptr;
	if (!Hud)
	{
		return;
	}
	// Marking trees: press, drag across them, let go.
	AColony* C = GetColony();
	if (C && C->IsMarkingTrees())
	{
		bDragging = GroundUnderCursor(DragStart);
		float X, Y;
		if (bDragging && PC->GetMousePosition(X, Y))
		{
			Hud->BeginDrag(FVector2D(X, Y));
		}
		return;
	}
	FHitResult Hit;
	PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit);
	AFounder* Founder = Cast<AFounder>(Hit.GetActor());
	Hud->Select(Founder);
	if (!Founder)
	{
		bFollow = false;
	}
}

void AStewardCamera::OnSelectReleased()
{
	if (!bDragging)
	{
		return;
	}
	bDragging = false;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (AAethelHUD* Hud = PC ? Cast<AAethelHUD>(PC->GetHUD()) : nullptr)
	{
		Hud->EndDrag();
	}
	FVector End;
	AColony* C = GetColony();
	if (!C || !GroundUnderCursor(End))
	{
		return;
	}
	// A click marks the trees around that spot; a drag marks everything inside. Shift unmarks.
	FVector2D A(DragStart), B(End);
	if (FVector2D::Distance(A, B) < 300.f)
	{
		A -= FVector2D(400.f);
		B += FVector2D(400.f);
	}
	const bool bUnmark = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
	const int32 Count = C->GetForest()->MarkArea(A, B, !bUnmark);
	if (Count > 0)
	{
		C->Say(bUnmark ? FString::Printf(TEXT("%d trees will be left standing."), Count)
			: FString::Printf(TEXT("%d trees marked for felling."), Count));
	}
}

void AStewardCamera::OnMarkTrees()
{
	if (AColony* C = GetColony())
	{
		C->SetMarkingTrees(!C->IsMarkingTrees());
	}
}

void AStewardCamera::OnPause()
{
	if (AColony* C = GetColony())
	{
		C->TogglePause();
	}
}

void AStewardCamera::SetSpeed(int32 Speed)
{
	if (AColony* C = GetColony())
	{
		C->SetSpeed(Speed);
	}
}

void AStewardCamera::OnFollow()
{
	bFollow = !bFollow;
}

void AStewardCamera::OnZoom(const FInputActionValue& Value)
{
	const float Wheel = Value.Get<float>();
	TargetDistance = FMath::Clamp(TargetDistance * FMath::Pow(0.82f, Wheel), MinDistance, MaxDistance);
}

void AStewardCamera::OnTurn(const FInputActionValue& Value)
{
	PendingTurn += Value.Get<float>();
}

void AStewardCamera::OnLook(const FInputActionValue& Value)
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsInputKeyDown(EKeys::MiddleMouseButton))
	{
		return;
	}
	const FVector2D Delta = Value.Get<FVector2D>();
	TargetYaw += Delta.X * 0.35f;
	PitchOffset = FMath::Clamp(PitchOffset - Delta.Y * 0.25f, -20.f, 20.f);
}

float AStewardCamera::ZoomAlpha(float D) const
{
	return FMath::Clamp(FMath::Loge(D / MinDistance) / FMath::Loge(MaxDistance / MinDistance), 0.f, 1.f);
}

float AStewardCamera::GroundHeight(const FVector& At) const
{
	FHitResult Hit;
	const FVector Start(At.X, At.Y, 200000.f);
	const FVector End(At.X, At.Y, -50000.f);
	// Only the land counts, not founders or anything else standing on it.
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		return FMath::Max(Hit.ImpactPoint.Z, FValleyTerrain::WaterLevel * 100.f);
	}
	return At.Z;
}

void AStewardCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const APlayerController* PC = Cast<APlayerController>(GetController());
	const bool bFast = PC && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));

	// Pan along the ground, faster the farther out the camera is.
	if (!PendingMove.IsNearlyZero())
	{
		const FVector2D Dir = PendingMove.GetSafeNormal();
		const FRotator Heading(0.f, Yaw, 0.f);
		const FVector Forward = Heading.RotateVector(FVector::ForwardVector);
		const FVector Right = Heading.RotateVector(FVector::RightVector);
		const float Speed = (300.f + Distance * 0.9f) * (bFast ? 3.f : 1.f);
		TargetFocus += (Forward * Dir.Y + Right * Dir.X) * Speed * DeltaSeconds;
	}
	PendingMove = FVector2D::ZeroVector;

	TargetYaw += PendingTurn * TurnSpeed * DeltaSeconds;
	PendingTurn = 0.f;

	if (bFollow)
	{
		const APlayerController* FollowPC = Cast<APlayerController>(GetController());
		const AAethelHUD* Hud = FollowPC ? Cast<AAethelHUD>(FollowPC->GetHUD()) : nullptr;
		if (const AFounder* Followed = Hud ? Hud->GetSelected() : nullptr)
		{
			TargetFocus = Followed->GetActorLocation();
		}
	}

	const float Margin = 5000.f;
	const float Max = FValleyTerrain::SizeMeters * 100.f - Margin;
	TargetFocus.X = FMath::Clamp(TargetFocus.X, Margin, Max);
	TargetFocus.Y = FMath::Clamp(TargetFocus.Y, Margin, Max);
	TargetFocus.Z = GroundHeight(TargetFocus);

	// Ease toward the targets so movement feels weighty, not twitchy.
	Focus = FMath::VInterpTo(Focus, TargetFocus, DeltaSeconds, 8.f);
	Yaw = FMath::FInterpTo(Yaw, TargetYaw, DeltaSeconds, 10.f);
	Distance = FMath::Exp(FMath::FInterpTo(FMath::Loge(Distance), FMath::Loge(TargetDistance), DeltaSeconds, 7.f));

	const float Alpha = ZoomAlpha(Distance);
	float Pitch = FMath::Clamp(FMath::Lerp(NearPitch, FarPitch, FMath::Pow(Alpha, 1.6f)) + PitchOffset, 3.f, 85.f);
	// Close in, look at head height rather than the founders' feet.
	const FVector Pivot = Focus + FVector(0.f, 0.f, FMath::Lerp(170.f, 0.f, Alpha));

	// Rather than letting a hill behind the camera push it in, look down more steeply until
	// the camera clears the ground.
	for (int32 Step = 0; Step < 40 && Pitch < 85.f; ++Step)
	{
		const FVector Eye = Pivot - FRotator(-Pitch, Yaw, 0.f).Vector() * Distance;
		if (Eye.Z > GroundHeight(Eye) + 300.f)
		{
			break;
		}
		Pitch += 2.f;
	}

	SetActorLocation(Focus);
	Arm->SetWorldRotation(FRotator(-Pitch, Yaw, 0.f));
	Arm->TargetArmLength = Distance;
	Arm->SetWorldLocation(Pivot);
}
