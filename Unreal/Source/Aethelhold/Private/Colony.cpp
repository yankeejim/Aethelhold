#include "Colony.h"
#include "Founder.h"
#include "Forest.h"
#include "Item.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	// One game day takes twelve real minutes at normal speed.
	constexpr float GameHoursPerSecond = 24.f / (12.f * 60.f);

	const TCHAR* MaleNames[] = { TEXT("Aldric"), TEXT("Bertram"), TEXT("Cuthbert"), TEXT("Edmund"), TEXT("Godwin"),
		TEXT("Hugh"), TEXT("Osric"), TEXT("Wulfric"), TEXT("Roland"), TEXT("Tobias"), TEXT("Alwin"), TEXT("Garrick") };
	const TCHAR* FemaleNames[] = { TEXT("Aelith"), TEXT("Brida"), TEXT("Edith"), TEXT("Elswyth"), TEXT("Gisela"),
		TEXT("Hilda"), TEXT("Maud"), TEXT("Rowena"), TEXT("Sybil"), TEXT("Wynne"), TEXT("Agnes"), TEXT("Isolde") };
	const TCHAR* Families[] = { TEXT("Ashby"), TEXT("Brook"), TEXT("Cole"), TEXT("Fairfax"), TEXT("Hale"),
		TEXT("Marsh"), TEXT("Thorne"), TEXT("Wren"), TEXT("Holt"), TEXT("Stone") };

	template <int32 N>
	const TCHAR* Pick(FRandomStream& Rng, const TCHAR* (&Names)[N], TSet<FString>& Used)
	{
		for (int32 Tries = 0; Tries < 50; ++Tries)
		{
			const TCHAR* Name = Names[Rng.RandRange(0, N - 1)];
			if (!Used.Contains(Name))
			{
				Used.Add(Name);
				return Name;
			}
		}
		return Names[0];
	}
}

AColony::AColony()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AColony::BeginPlay()
{
	Super::BeginPlay();
	Terrain = FValleyTerrain(Seed);
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		Sun = *It;
		if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			SunLight->SetForwardShadingPriority(1);
		}
		break;
	}
	// For screenshots and testing: -hour=H and -speed=S on the command line.
	FParse::Value(FCommandLine::Get(), TEXT("-hour="), Hour);
	int32 StartSpeed = 1;
	if (FParse::Value(FCommandLine::Get(), TEXT("-speed="), StartSpeed))
	{
		SetSpeed(StartSpeed);
	}
	// The woods grow from the land as the game starts.
	Forest = GetWorld()->SpawnActor<AForest>();
	Forest->Plant(Terrain, Seed);
	FoundTheColony();
	UpdateSun();

	// For testing: -mark=N marks the N trees nearest the camp for felling.
	int32 ToMark = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("-mark="), ToMark))
	{
		TArray<FTreeSpot>& Trees = Forest->GetTrees();
		TArray<int32> Order;
		for (int32 I = 0; I < Trees.Num(); ++I)
		{
			if (!Trees[I].IsShrub())
			{
				Order.Add(I);
			}
		}
		Order.Sort([&](int32 A, int32 B) { return FVector::DistSquared2D(Trees[A].Base(), Fire) < FVector::DistSquared2D(Trees[B].Base(), Fire); });
		for (int32 K = 0; K < FMath::Min(ToMark, Order.Num()); ++K)
		{
			Trees[Order[K]].bMarked = true;
		}
	}
}

FVector AColony::OnGround(float X, float Y) const
{
	return FVector(X, Y, Terrain.HeightAt(X / 100.f, Y / 100.f) * 100.f);
}

bool AColony::IsDryLand(const FVector& At) const
{
	const float X = At.X / 100.f;
	const float Y = At.Y / 100.f;
	return Terrain.HeightAt(X, Y) > 0.6f && Terrain.SlopeAt(X, Y) < 0.45f;
}

float AColony::Temperature() const
{
	// Warmest at three in the afternoon, coolest before dawn.
	return 11.5f + 5.f * FMath::Cos((Hour - 15.f) / 24.f * 2.f * PI);
}

void AColony::FoundTheColony()
{
	FRandomStream Rng(Seed * 7919 + 3);
	const FVector2D Start = Terrain.StartSpot();
	// The fire goes a little back from the river, on dry flat ground.
	Fire = OnGround(Start.X * 100.f + 1500.f, Start.Y * 100.f);
	// The stockpile is a few steps further back from the water.
	Stockpile = OnGround(Fire.X + 900.f, Fire.Y - 300.f);
	BuildCampfire();

	// Seven founders: a family with a child, a young couple, an old widower and a youth.
	TSet<FString> UsedNames;
	TSet<FString> UsedFamilies;
	TArray<FFounderInfo> People;
	People.SetNum(7);
	const FString FamilyA = Pick(Rng, Families, UsedFamilies);
	const FString FamilyB = Pick(Rng, Families, UsedFamilies);
	const FString FamilyC = Pick(Rng, Families, UsedFamilies);
	const FString FamilyD = Pick(Rng, Families, UsedFamilies);

	auto Make = [&](int32 I, bool bFemale, int32 MinAge, int32 MaxAge, const FString& Family)
	{
		FFounderInfo& P = People[I];
		P.bFemale = bFemale;
		P.Name = bFemale ? Pick(Rng, FemaleNames, UsedNames) : Pick(Rng, MaleNames, UsedNames);
		P.Family = Family;
		P.Age = Rng.RandRange(MinAge, MaxAge);
		P.Temperament = static_cast<ETemperament>(Rng.RandRange(0, (int32)ETemperament::Stubborn));
	};
	Make(0, false, 31, 42, FamilyA);
	Make(1, true, 28, 38, FamilyA);
	Make(2, Rng.FRand() < 0.5f, 7, 12, FamilyA);
	Make(3, false, 21, 29, FamilyB);
	Make(4, true, 19, 27, FamilyB);
	Make(5, false, 56, 68, FamilyC);
	Make(6, Rng.FRand() < 0.5f, 16, 20, FamilyD);
	People[0].Spouse = 1; People[1].Spouse = 0;
	People[2].Father = 0; People[2].Mother = 1;
	People[3].Spouse = 4; People[4].Spouse = 3;
	People[5].bWidowed = true;

	// Every grown founder knows a trade; no two share one.
	TArray<ESkill> Skills = { ESkill::Woodcutter, ESkill::Forager, ESkill::Hunter, ESkill::Builder, ESkill::Healer,
		ESkill::Fisher, ESkill::Herder };
	for (int32 I = Skills.Num() - 1; I > 0; --I)
	{
		Skills.Swap(I, Rng.RandRange(0, I));
	}
	int32 NextSkill = 0;
	for (FFounderInfo& P : People)
	{
		P.Skill = P.IsChild() ? ESkill::Forager : Skills[NextSkill++];
	}

	// They arrive together, gathered near where the fire will be.
	for (int32 I = 0; I < People.Num(); ++I)
	{
		const float Angle = I * (2.f * PI / People.Num()) + Rng.FRandRange(-0.3f, 0.3f);
		const float Radius = Rng.FRandRange(300.f, 700.f);
		const FVector At = OnGround(Fire.X + Radius * FMath::Cos(Angle), Fire.Y + Radius * FMath::Sin(Angle));
		AFounder* Founder = GetWorld()->SpawnActor<AFounder>(At, FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f));
		Founders.Add(Founder);
	}
	for (int32 I = 0; I < People.Num(); ++I)
	{
		Founders[I]->Init(this, I, People[I]);
	}
}

void AColony::BuildCampfire()
{
	// A ring of stones and a flickering light; flames and smoke come with the fire art.
	FireStones = NewObject<UInstancedStaticMeshComponent>(this, TEXT("FireStones"));
	FireStones->SetupAttachment(RootComponent);
	FireStones->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/Fab/Megascans/3D/Mossy_Rocks_tjmudcfda/High/tjmudcfda_tier_1/StaticMeshes/tjmudcfda_tier_1.tjmudcfda_tier_1")));
	FireStones->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FireStones->RegisterComponent();
	for (int32 I = 0; I < 9; ++I)
	{
		const float A = I * 2.f * PI / 9.f;
		const FVector At = Fire + FVector(70.f * FMath::Cos(A), 70.f * FMath::Sin(A), -5.f);
		FireStones->AddInstance(FTransform(FRotator(0.f, I * 47.f, 0.f), At, FVector(0.18f)), true);
	}

	FireLight = NewObject<UPointLightComponent>(this, TEXT("FireLight"));
	FireLight->SetupAttachment(RootComponent);
	FireLight->SetWorldLocation(Fire + FVector(0.f, 0.f, 60.f));
	FireLight->SetLightColor(FLinearColor(1.f, 0.55f, 0.22f));
	FireLight->SetAttenuationRadius(1800.f);
	FireLight->SetIntensityUnits(ELightUnits::Candelas);
	FireLight->SetIntensity(80.f);
	FireLight->SetCastShadows(true);
	FireLight->RegisterComponent();

	Moon = NewObject<UDirectionalLightComponent>(this, TEXT("Moon"));
	Moon->SetupAttachment(RootComponent);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetAtmosphereSunLight(true);
	Moon->SetAtmosphereSunLightIndex(1);
	Moon->SetLightColor(FLinearColor(0.62f, 0.72f, 1.f));
	Moon->SetIntensity(0.f);
	// The sun stays the main light for water, fog and glass.
	Moon->SetForwardShadingPriority(0);
	Moon->SetWorldRotation(FRotator(-35.f, 60.f, 0.f));
	Moon->RegisterComponent();
}

void AColony::SetSpeed(int32 NewSpeed)
{
	Speed = FMath::Max(0, NewSpeed);
	if (Speed > 0)
	{
		SpeedBeforePause = Speed;
	}
}

void AColony::TogglePause()
{
	SetSpeed(Speed == 0 ? SpeedBeforePause : 0);
}

int32 AColony::GetFood() const
{
	int32 Total = FoodPortions;
	for (const AItem* Item : Items)
	{
		if (Item->bStored && Item->IsFood())
		{
			Total += Item->Portions;
		}
	}
	return Total;
}

int32 AColony::CountStored(EItem Type) const
{
	int32 Count = 0;
	for (const AItem* Item : Items)
	{
		Count += Item->bStored && Item->Type == Type ? 1 : 0;
	}
	return Count;
}

bool AColony::TakeMeal()
{
	// Fresh food first, whatever will spoil soonest, then the rations they brought.
	AItem* Soonest = nullptr;
	for (AItem* Item : Items)
	{
		if (Item->bStored && Item->IsFood() && Item->Portions > 0 && (!Soonest || Item->HoursLeft < Soonest->HoursLeft))
		{
			Soonest = Item;
		}
	}
	if (Soonest)
	{
		if (--Soonest->Portions <= 0)
		{
			Items.Remove(Soonest);
			Soonest->Destroy();
		}
		return true;
	}
	if (FoodPortions <= 0)
	{
		return false;
	}
	--FoodPortions;
	return true;
}

AItem* AColony::SpawnItem(EItem Type, int32 Portions, const FVector& At, float Yaw)
{
	AItem* Item = GetWorld()->SpawnActor<AItem>(OnGround(At.X, At.Y) + FVector(0.f, 0.f, 13.f), FRotator(0.f, Yaw, 0.f));
	Item->Init(Type, Portions);
	Items.Add(Item);
	return Item;
}

AItem* AColony::LooseItemNear(const FVector& From, float Range) const
{
	AItem* Best = nullptr;
	float BestDist = Range * Range;
	for (AItem* Item : Items)
	{
		if (!Item->bStored && !Item->bClaimed)
		{
			const float D = FVector::DistSquared2D(From, Item->GetActorLocation());
			if (D < BestDist)
			{
				BestDist = D;
				Best = Item;
			}
		}
	}
	return Best;
}

FVector AColony::StockSpot(EItem Type) const
{
	// Logs are stacked in a pile, five to a layer; food is set out in rows beside them.
	const int32 N = Type == EItem::Log ? CountStored(EItem::Log) : CountStored(EItem::Nuts) + CountStored(EItem::Fish);
	if (Type == EItem::Log)
	{
		return Stockpile + FVector(0.f, (N % 5) * 28.f - 56.f, 13.f + (N / 5) * 24.f);
	}
	return Stockpile + FVector(-150.f - (N / 6) * 55.f, (N % 6) * 55.f - 140.f, 13.f);
}

void AColony::Store(AItem* Item)
{
	Item->SetActorLocation(StockSpot(Item->Type));
	Item->SetActorRotation(FRotator(0.f, Item->Type == EItem::Log ? 0.f : 30.f, 0.f));
	Item->bStored = true;
	Item->bClaimed = false;
}

void AColony::Drop(AItem* Item, const FVector& At)
{
	Item->SetActorLocation(OnGround(At.X, At.Y) + FVector(0.f, 0.f, 13.f));
	Item->bClaimed = false;
}

void AColony::Say(const FString& Text)
{
	News.Add({ Text, GetClock() });
	if (News.Num() > 30)
	{
		News.RemoveAt(0);
	}
}

void AColony::Spoil(float GameHours)
{
	int32 Rotted = 0;
	for (int32 I = Items.Num() - 1; I >= 0; --I)
	{
		AItem* Item = Items[I];
		if (Item->HoursLeft < 0.f || Item->bClaimed)
		{
			continue;
		}
		Item->HoursLeft -= GameHours;
		if (Item->HoursLeft <= 0.f)
		{
			Rotted += Item->Portions;
			Items.RemoveAt(I);
			Item->Destroy();
		}
	}
	if (Rotted > 0)
	{
		Say(FString::Printf(TEXT("Food has gone bad: %d meals thrown away."), Rotted));
	}
}

void AColony::UpdateSun()
{
	if (!Sun)
	{
		return;
	}
	// The sun rises at six, sets at nine, and swings from east to west across the south.
	const float Day01 = (Hour - 6.f) / 15.f;
	const float Elevation = Day01 >= 0.f && Day01 <= 1.f
		? 4.f + 46.f * FMath::Sin(Day01 * PI)
		: -25.f;
	const float Azimuth = FMath::Lerp(-20.f, 200.f, FMath::Clamp(Day01, 0.f, 1.f));
	Sun->SetActorRotation(FRotator(-Elevation, Azimuth + 180.f, 0.f));
	if (Moon)
	{
		// Moonlight fades in as the sun goes down.
		Moon->SetIntensity(FMath::GetMappedRangeValueClamped(FVector2D(2.f, -8.f), FVector2D(0.f, 0.06f), Elevation));
	}
}

void AColony::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float GameHours = DeltaSeconds * Speed * GameHoursPerSecond;
	Hour += GameHours;
	if (Hour >= 24.f)
	{
		Hour -= 24.f;
		++Day;
	}
	UpdateSun();

	// The fire flickers in real time, and is the brightest thing around at night.
	FlickerTime += DeltaSeconds;
	const float Flicker = 0.85f + 0.1f * FMath::Sin(FlickerTime * 13.f) + 0.05f * FMath::Sin(FlickerTime * 31.f + 1.3f);
	FireLight->SetIntensity((IsNight() ? 45.f : 20.f) * Flicker);

	Spoil(GameHours);
	for (AFounder* Founder : Founders)
	{
		Founder->Live(GameHours);
	}
}
