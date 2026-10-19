#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AethelHUD.generated.h"

class AColony;
class AFounder;

/**
 * The on-screen information: the clock and stores along the top, founders' names over their
 * heads when the camera is close, and a card for the founder the player has clicked.
 */
UCLASS()
class AETHELHOLD_API AAethelHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	void Select(AFounder* Founder);
	AFounder* GetSelected() const;
	void BeginDrag(const FVector2D& Screen) { DragFrom = Screen; bDragging = true; }
	void EndDrag() { bDragging = false; }

private:
	AColony* FindColony();
	void DrawTopBar(AColony* Colony);
	void DrawNames(AColony* Colony);
	void DrawCard(AFounder* Founder);
	void Bar(float X, float Y, const FString& Label, float Value);
	void DrawMarks(AColony* Colony);
	void DrawNews(AColony* Colony);

	TWeakObjectPtr<AColony> Colony;
	TWeakObjectPtr<AFounder> Selected;
	bool bPicked = false;
	FVector2D DragFrom = FVector2D::ZeroVector;
	bool bDragging = false;
};
