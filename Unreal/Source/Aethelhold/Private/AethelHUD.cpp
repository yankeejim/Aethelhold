#include "AethelHUD.h"
#include "Colony.h"
#include "Founder.h"
#include "Forest.h"
#include "Item.h"
#include "Building.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"

namespace
{
	const FLinearColor Panel(0.02f, 0.02f, 0.015f, 0.62f);
	const FLinearColor Ink(0.95f, 0.92f, 0.82f);
	const FLinearColor Dim(0.72f, 0.69f, 0.6f);
	const FLinearColor Gold(1.f, 0.82f, 0.4f);

	FString SkillName(ESkill S)
	{
		switch (S)
		{
		case ESkill::Woodcutter: return TEXT("woodcutter");
		case ESkill::Forager: return TEXT("forager");
		case ESkill::Hunter: return TEXT("hunter");
		case ESkill::Builder: return TEXT("builder");
		case ESkill::Healer: return TEXT("healer");
		case ESkill::Fisher: return TEXT("fisher");
		case ESkill::Herder: return TEXT("herder");
		}
		return FString();
	}

	FString TemperamentName(ETemperament T)
	{
		switch (T)
		{
		case ETemperament::Calm: return TEXT("calm");
		case ETemperament::Cheerful: return TEXT("cheerful");
		case ETemperament::HotHeaded: return TEXT("hot-headed");
		case ETemperament::Gloomy: return TEXT("gloomy");
		case ETemperament::Proud: return TEXT("proud");
		case ETemperament::Timid: return TEXT("timid");
		case ETemperament::Stubborn: return TEXT("stubborn");
		}
		return FString();
	}

	FLinearColor NeedColor(float V)
	{
		return V > 0.6f ? FLinearColor(0.45f, 0.75f, 0.35f) : V > 0.3f ? FLinearColor(0.9f, 0.7f, 0.25f) : FLinearColor(0.85f, 0.3f, 0.2f);
	}
}

void AAethelHUD::Select(AFounder* Founder)
{
	Selected = Founder;
}

AFounder* AAethelHUD::GetSelected() const
{
	return Selected.Get();
}

AColony* AAethelHUD::FindColony()
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

void AAethelHUD::DrawHUD()
{
	Super::DrawHUD();
	AColony* C = FindColony();
	if (!C || !Canvas)
	{
		return;
	}
	// For screenshots and testing: -select=N picks a founder at the start.
	int32 Pick = INDEX_NONE;
	if (!bPicked && FParse::Value(FCommandLine::Get(), TEXT("-select="), Pick) && C->GetFounders().IsValidIndex(Pick))
	{
		Selected = C->GetFounders()[Pick];
	}
	bPicked = true;
	DrawZones(C);
	DrawBuildings(C);
	DrawMarks(C);
	DrawNames(C);
	DrawTopBar(C);
	DrawNews(C);
	if (AFounder* F = Selected.Get())
	{
		DrawCard(F);
	}
}

void AAethelHUD::DrawTopBar(AColony* C)
{
	UFont* Font = GEngine->GetMediumFont();
	const int32 H = FMath::FloorToInt(C->GetHour());
	const int32 M = FMath::FloorToInt((C->GetHour() - H) * 60.f);
	const FString SpeedText = C->GetSpeed() == 0 ? TEXT("Paused") : FString::Printf(TEXT("Speed x%d"), C->GetSpeed());
	const FString Line = FString::Printf(TEXT("Day %d   %s   %02d:%02d   %.0f°C      Food %d meals (%d rations, %d nuts, %d fish)   Wood %d logs      %s"),
		C->GetDay(), *C->SeasonName(), H, M, C->Temperature(), C->GetFood(), C->GetRations(),
		C->CountStored(EItem::Nuts), C->CountStored(EItem::Fish), C->CountStored(EItem::Log), *SpeedText);

	DrawRect(Panel, 0.f, 0.f, Canvas->SizeX, 40.f);
	DrawText(Line, Ink, 20.f, 10.f, Font, 1.25f);
	const FString Help = TEXT("Space pause   1 2 3 speed   Click a founder   F follow   C mark trees   Z stockpiles   B build");
	float W, Hh;
	GetTextSize(Help, W, Hh, Font, 1.f);
	DrawText(Help, Dim, Canvas->SizeX - W - 20.f, 12.f, Font, 1.f);
}

void AAethelHUD::DrawMarks(AColony* C)
{
	// Marked trees carry a small orange tag at chest height, when the camera is near enough.
	const APlayerController* PC = GetOwningPlayerController();
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FLinearColor Mark(1.f, 0.55f, 0.15f, 0.9f);
	for (const FTreeSpot& Spot : C->GetForest()->GetTrees())
	{
		if (!Spot.bMarked || Spot.bGone || Spot.Fall > 0.f || FVector::DistSquared(CamLoc, Spot.Base()) > FMath::Square(40000.f))
		{
			continue;
		}
		const FVector Screen = Project(Spot.Base() + FVector(0.f, 0.f, 150.f));
		if (Screen.Z > 0.f)
		{
			DrawRect(Mark, Screen.X - 4.f, Screen.Y - 4.f, 8.f, 8.f);
		}
	}

	if (C->GetOrder() != AColony::EOrder::None)
	{
		UFont* Font = GEngine->GetMediumFont();
		FString Text;
		if (C->GetOrder() == AColony::EOrder::Build)
		{
			const ABuilding::FKind& K = ABuilding::Kind(C->GetBuildChoice());
			Text = FString::Printf(TEXT("Build a %s (%d logs): click to place, R to turn.   B for the next kind, or to stop"),
				*FString(K.Name).ToLower(), K.Logs);
		}
		else
		{
			Text = C->IsMarkingTrees()
				? TEXT("Marking trees to fell: drag across them (hold Shift to unmark).   C when done")
				: TEXT("Stockpiles: drag to lay one out, click one to change what it takes, Shift-drag to clear.   Z when done");
		}
		float W, H;
		GetTextSize(Text, W, H, Font, 1.2f);
		DrawRect(FLinearColor(0.35f, 0.15f, 0.02f, 0.75f), (Canvas->SizeX - W) * 0.5f - 16.f, 52.f, W + 32.f, H + 12.f);
		DrawText(Text, Ink, (Canvas->SizeX - W) * 0.5f, 58.f, Font, 1.2f);
	}
	float MX, MY;
	if (bDragging && PC->GetMousePosition(MX, MY))
	{
		const float X0 = FMath::Min(DragFrom.X, MX), Y0 = FMath::Min(DragFrom.Y, MY);
		const float W = FMath::Abs(MX - DragFrom.X), H = FMath::Abs(MY - DragFrom.Y);
		DrawRect(FLinearColor(1.f, 0.55f, 0.15f, 0.15f), X0, Y0, W, H);
		DrawLine(X0, Y0, X0 + W, Y0, Mark, 2.f);
		DrawLine(X0, Y0 + H, X0 + W, Y0 + H, Mark, 2.f);
		DrawLine(X0, Y0, X0, Y0 + H, Mark, 2.f);
		DrawLine(X0 + W, Y0, X0 + W, Y0 + H, Mark, 2.f);
	}
}

void AAethelHUD::DrawZones(AColony* C)
{
	// Each stockpile is outlined on the ground, coloured by what it takes, with a label.
	const APlayerController* PC = GetOwningPlayerController();
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const bool bEditing = C->GetOrder() == AColony::EOrder::Stockpiles;
	UFont* Font = GEngine->GetSmallFont();
	const float H = Storage::PlaceSize * 0.5f - 6.f;

	for (const FStoreZone& Zone : C->GetZones())
	{
		if (Zone.bRemoved)
		{
			continue;
		}
		const FLinearColor Color = Zone.Accepts == EStoreFilter::Wood ? FLinearColor(0.95f, 0.6f, 0.25f, bEditing ? 0.9f : 0.5f)
			: Zone.Accepts == EStoreFilter::Food ? FLinearColor(0.5f, 0.9f, 0.35f, bEditing ? 0.9f : 0.5f)
			: FLinearColor(0.95f, 0.9f, 0.75f, bEditing ? 0.9f : 0.45f);
		FVector Middle = FVector::ZeroVector;
		int32 Stored = 0;
		bool bNear = false;
		for (const FIntPoint& P : Zone.Places)
		{
			const FVector Centre = C->PlaceCenter(P) + FVector(0.f, 0.f, 8.f);
			Middle += Centre;
			if (const FStorePlace* Place = C->FindPlace(P))
			{
				Stored += Place->Count();
			}
			if (FVector::DistSquared(CamLoc, Centre) > FMath::Square(30000.f))
			{
				continue;
			}
			bNear = true;
			const FVector Corners[4] = { Centre + FVector(-H, -H, 0.f), Centre + FVector(H, -H, 0.f), Centre + FVector(H, H, 0.f), Centre + FVector(-H, H, 0.f) };
			FVector Screen[4];
			bool bVisible = true;
			for (int32 K = 0; K < 4; ++K)
			{
				Screen[K] = Project(Corners[K]);
				bVisible &= Screen[K].Z > 0.f;
			}
			if (bVisible)
			{
				for (int32 K = 0; K < 4; ++K)
				{
					DrawLine(Screen[K].X, Screen[K].Y, Screen[(K + 1) % 4].X, Screen[(K + 1) % 4].Y, Color, bEditing ? 2.f : 1.f);
				}
			}
		}
		if (!bNear || Zone.Places.IsEmpty())
		{
			continue;
		}
		Middle /= Zone.Places.Num();
		const FVector Screen = Project(Middle + FVector(0.f, 0.f, 120.f));
		if (Screen.Z > 0.f)
		{
			const TCHAR* What = Zone.Accepts == EStoreFilter::Wood ? TEXT("wood") : Zone.Accepts == EStoreFilter::Food ? TEXT("food") : TEXT("anything");
			const FString Label = FString::Printf(TEXT("Stockpile: %s  (%d)"), What, Stored);
			float W, Th;
			GetTextSize(Label, W, Th, Font, 1.1f);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), Screen.X - W * 0.5f - 6.f, Screen.Y - 2.f, W + 12.f, Th + 4.f);
			DrawText(Label, FLinearColor(Color.R, Color.G, Color.B, 1.f), Screen.X - W * 0.5f, Screen.Y, Font, 1.1f);
		}
	}
}

void AAethelHUD::Outline(const TArray<FVector>& Corners, const FLinearColor& Color, float Thickness)
{
	TArray<FVector> Screen;
	for (const FVector& P : Corners)
	{
		const FVector S = Project(P);
		if (S.Z <= 0.f)
		{
			return;
		}
		Screen.Add(S);
	}
	for (int32 K = 0; K < Screen.Num(); ++K)
	{
		const FVector& A = Screen[K];
		const FVector& B = Screen[(K + 1) % Screen.Num()];
		DrawLine(A.X, A.Y, B.X, B.Y, Color, Thickness);
	}
}

void AAethelHUD::DrawBuildings(AColony* C)
{
	const APlayerController* PC = GetOwningPlayerController();
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	UFont* Font = GEngine->GetSmallFont();

	// Building sites say how far along they are.
	for (const ABuilding* B : C->GetBuildings())
	{
		if (B->IsFinished() || FVector::DistSquared(CamLoc, B->GetActorLocation()) > FMath::Square(25000.f))
		{
			continue;
		}
		const FVector Screen = Project(B->GetActorLocation() + FVector(0.f, 0.f, 350.f));
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		const FString Label = FString::Printf(TEXT("%s: %d of %d logs, %d%% built"), B->GetKind().Name, B->Delivered,
			B->GetKind().Logs, FMath::FloorToInt(B->GetProgress() * 100.f));
		float W, H;
		GetTextSize(Label, W, H, Font, 1.1f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), Screen.X - W * 0.5f - 6.f, Screen.Y - 2.f, W + 12.f, H + 4.f);
		DrawText(Label, Gold, Screen.X - W * 0.5f, Screen.Y, Font, 1.1f);
	}

	// In build mode, the outline of what would go down under the cursor: green where it can,
	// red (and why not) where it can't.
	FHitResult Hit;
	if (C->GetOrder() != AColony::EOrder::Build
		|| !PC->GetHitResultUnderCursorForObjects({ UEngineTypes::ConvertToObjectType(ECC_WorldStatic) }, false, Hit))
	{
		return;
	}
	const EBuilding Type = C->GetBuildChoice();
	const int32 Turns = C->GetBuildTurns();
	const FIntPoint First = C->FootprintAt(Type, Turns, FVector2D(Hit.ImpactPoint));
	FString Why;
	const bool bOk = C->CanPlace(Type, Turns, First, &Why);
	const FIntPoint Size = ABuilding::Footprint(Type, Turns);
	const FVector2D Lo = C->PlaceCorner(First);
	const FVector2D Hi = C->PlaceCorner(First + Size);
	auto Ground = [C](float X, float Y) { return C->OnGround(X, Y) + FVector(0.f, 0.f, 10.f); };
	const FLinearColor Color = bOk ? FLinearColor(0.4f, 1.f, 0.4f, 0.95f) : FLinearColor(1.f, 0.3f, 0.2f, 0.95f);
	Outline({ Ground(Lo.X, Lo.Y), Ground(Hi.X, Lo.Y), Ground(Hi.X, Hi.Y), Ground(Lo.X, Hi.Y) }, Color, 2.5f);
	// A tick on the side the door will face.
	const FVector Centre = Ground((Lo.X + Hi.X) * 0.5f, (Lo.Y + Hi.Y) * 0.5f);
	const FVector Front = FRotator(0.f, Turns * 90.f, 0.f).RotateVector(FVector(0.f, -1.f, 0.f));
	const float Reach = (Turns % 2 == 0 ? Hi.Y - Lo.Y : Hi.X - Lo.X) * 0.5f;
	const FVector Door = Ground(Centre.X + Front.X * Reach, Centre.Y + Front.Y * Reach);
	const FVector DoorOut = Ground(Centre.X + Front.X * (Reach + 120.f), Centre.Y + Front.Y * (Reach + 120.f));
	Outline({ Door, DoorOut }, Color, 2.5f);
	if (!bOk)
	{
		const FVector S = Project(Centre + FVector(0.f, 0.f, 150.f));
		if (S.Z > 0.f)
		{
			float W, H;
			GetTextSize(Why, W, H, Font, 1.1f);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), S.X - W * 0.5f - 6.f, S.Y - 2.f, W + 12.f, H + 4.f);
			DrawText(Why, FLinearColor(1.f, 0.6f, 0.5f), S.X - W * 0.5f, S.Y, Font, 1.1f);
		}
	}
}

void AAethelHUD::DrawNews(AColony* C)
{
	// The last few things that happened, fading after a few game hours.
	UFont* Font = GEngine->GetSmallFont();
	const TArray<AColony::FNews>& News = C->GetNews();
	float Y = Canvas->SizeY - 30.f;
	int32 Shown = 0;
	for (int32 I = News.Num() - 1; I >= 0 && Shown < 6; --I)
	{
		const float Age = C->GetClock() - News[I].When;
		if (Age > 4.f)
		{
			break;
		}
		const float Alpha = FMath::Clamp(4.f - Age, 0.f, 1.f);
		float W, H;
		GetTextSize(News[I].Text, W, H, Font, 1.15f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f * Alpha), Canvas->SizeX - W - 36.f, Y - 3.f, W + 16.f, H + 6.f);
		DrawText(News[I].Text, FLinearColor(Ink.R, Ink.G, Ink.B, Alpha), Canvas->SizeX - W - 28.f, Y, Font, 1.15f);
		Y -= H + 10.f;
		++Shown;
	}
}

void AAethelHUD::DrawNames(AColony* C)
{
	UFont* Font = GEngine->GetSmallFont();
	const APlayerController* PC = GetOwningPlayerController();
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	for (AFounder* F : C->GetFounders())
	{
		const FVector Head = F->GetActorLocation() + FVector(0.f, 0.f, (F->IsLying() ? 70.f : 215.f) * F->GetActorScale3D().Z);
		const float Dist = FVector::Dist(CamLoc, Head);
		const bool bSelected = F == Selected.Get();
		if (Dist > 9000.f && !bSelected)
		{
			continue;
		}
		const FVector Screen = Project(Head);
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		const FString Name = F->GetInfo().Name;
		float W, Hh;
		GetTextSize(Name, W, Hh, Font, 1.2f);
		const float Alpha = bSelected ? 1.f : FMath::GetMappedRangeValueClamped(FVector2D(6000.f, 9000.f), FVector2D(1.f, 0.f), Dist);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f * Alpha), Screen.X - W * 0.5f - 6.f, Screen.Y - 2.f, W + 12.f, Hh + 4.f);
		FLinearColor Color = bSelected ? Gold : Ink;
		Color.A = Alpha;
		DrawText(Name, Color, Screen.X - W * 0.5f, Screen.Y, Font, 1.2f);
	}
}

void AAethelHUD::Bar(float X, float Y, const FString& Label, float Value)
{
	UFont* Font = GEngine->GetSmallFont();
	DrawText(Label, Dim, X, Y, Font, 1.1f);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.12f), X + 70.f, Y + 4.f, 200.f, 10.f);
	DrawRect(NeedColor(Value), X + 70.f, Y + 4.f, 200.f * Value, 10.f);
}

void AAethelHUD::DrawCard(AFounder* F)
{
	const FFounderInfo& I = F->GetInfo();
	UFont* Big = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const float X = 20.f;
	const float W = 380.f;
	const float H = 250.f;
	const float Y = Canvas->SizeY - H - 20.f;
	DrawRect(Panel, X, Y, W, H);

	float Line = Y + 12.f;
	DrawText(FString::Printf(TEXT("%s %s"), *I.Name, *I.Family), Gold, X + 16.f, Line, Big, 1.4f);
	Line += 34.f;
	const FString Who = I.IsChild()
		? FString::Printf(TEXT("%s, %d, a %s child"), I.bFemale ? TEXT("Girl") : TEXT("Boy"), I.Age, *TemperamentName(I.Temperament))
		: FString::Printf(TEXT("%s, %d, %s %s"), I.bFemale ? TEXT("Woman") : TEXT("Man"), I.Age, *TemperamentName(I.Temperament), *SkillName(I.Skill));
	DrawText(Who, Ink, X + 16.f, Line, Small, 1.15f);
	Line += 22.f;

	// Family, as the founders would put it.
	const TArray<TObjectPtr<AFounder>>& All = FindColony()->GetFounders();
	FString Kin;
	if (I.Spouse != INDEX_NONE)
	{
		Kin = FString::Printf(TEXT("%s of %s"), I.bFemale ? TEXT("Wife") : TEXT("Husband"), *All[I.Spouse]->GetInfo().Name);
	}
	else if (I.Mother != INDEX_NONE)
	{
		Kin = FString::Printf(TEXT("Child of %s and %s"), *All[I.Father]->GetInfo().Name, *All[I.Mother]->GetInfo().Name);
	}
	else if (I.bWidowed)
	{
		Kin = I.bFemale ? TEXT("Widow") : TEXT("Widower");
	}
	else
	{
		Kin = TEXT("Unwed, came alone");
	}
	for (int32 J = 0; J < All.Num(); ++J)
	{
		const FFounderInfo& O = All[J]->GetInfo();
		if ((O.Father != INDEX_NONE && All[O.Father] == F) || (O.Mother != INDEX_NONE && All[O.Mother] == F))
		{
			Kin += FString::Printf(TEXT(", parent of %s"), *O.Name);
		}
	}
	if (const ABuilding* Home = F->GetHome())
	{
		Kin += FString::Printf(TEXT("; sleeps in the %s"), *FString(Home->GetKind().Name).ToLower());
	}
	DrawText(Kin, Dim, X + 16.f, Line, Small, 1.1f);
	Line += 30.f;

	Bar(X + 16.f, Line, TEXT("Food"), F->Food);
	Line += 22.f;
	Bar(X + 16.f, Line, TEXT("Warmth"), F->Warmth);
	Line += 22.f;
	Bar(X + 16.f, Line, TEXT("Rest"), F->Rest);
	Line += 30.f;
	DrawText(FString::Printf(TEXT("Feels %s"), *F->MoodWord()), Ink, X + 16.f, Line, Small, 1.15f);
	Line += 22.f;
	DrawText(F->Describe(), Gold, X + 16.f, Line, Small, 1.15f);
}
