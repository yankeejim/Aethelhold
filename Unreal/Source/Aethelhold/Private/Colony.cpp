#include "Colony.h"
#include "Founder.h"
#include "Forest.h"
#include "Item.h"
#include "Building.h"
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

	// For testing: -order=1 starts marking trees, -order=2 laying out stockpiles.
	int32 StartOrder = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("-order="), StartOrder))
	{
		Order = static_cast<EOrder>(FMath::Clamp(StartOrder, 0, 2));
	}

	// For testing: -logs=N lays N logs in the stockpile; -sites=1 stakes out a shelter, a cabin
	// and a storehouse near the camp (clearing trees where they stand).
	int32 StartLogs = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("-logs="), StartLogs))
	{
		for (int32 K = 0; K < StartLogs; ++K)
		{
			AItem* Log = SpawnItem(EItem::Log, 0, Fire, 0.f);
			const FStoreSpot Spot = Reserve(EItem::Log, Fire);
			if (!Spot.IsValid())
			{
				break;
			}
			Store(Log, Spot);
		}
	}
	int32 StartSites = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("-sites="), StartSites) && StartSites > 0)
	{
		const TPair<EBuilding, FVector2D> Sites[] = {
			{ EBuilding::Shelter, FVector2D(Fire.X, Fire.Y + 700.f) },
			{ EBuilding::Cabin, FVector2D(Fire.X + 900.f, Fire.Y + 1200.f) },
			{ EBuilding::Storehouse, FVector2D(Fire.X + 1900.f, Fire.Y - 350.f) },
		};
		for (const TPair<EBuilding, FVector2D>& S : Sites)
		{
			for (int32 T = 0; T < Forest->GetTrees().Num(); ++T)
			{
				if (FVector2D::Distance(FVector2D(Forest->GetTrees()[T].Base()), S.Value) < 600.f)
				{
					Forest->Clear(T);
				}
			}
			PlaceBuilding(S.Key, S.Key == EBuilding::Storehouse ? 3 : 0, S.Value);
		}
	}

	// For testing: -mark=N marks the N trees nearest the camp for felling.
	int32 ToMark = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("-mark="), ToMark))
	{
		TArray<FTreeSpot>& Trees = Forest->GetTrees();
		TArray<int32> Nearest;
		for (int32 I = 0; I < Trees.Num(); ++I)
		{
			if (!Trees[I].IsShrub())
			{
				Nearest.Add(I);
			}
		}
		Nearest.Sort([&](int32 A, int32 B) { return FVector::DistSquared2D(Trees[A].Base(), Fire) < FVector::DistSquared2D(Trees[B].Base(), Fire); });
		for (int32 K = 0; K < FMath::Min(ToMark, Nearest.Num()); ++K)
		{
			Trees[Nearest[K]].bMarked = true;
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
	BuildCampfire();
	// They set their packs down a few steps back from the fire: the first stockpile.
	AddZone(FVector2D(Fire.X + 600.f, Fire.Y - 600.f), FVector2D(Fire.X + 1100.f, Fire.Y - 100.f));

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
			Unstore(Soonest);
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
		if (!Item->bStored && !Item->bClaimed && HasRoomFor(Item->Type))
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

void AColony::Drop(AItem* Item, const FVector& At)
{
	Item->SetActorLocation(OnGround(At.X, At.Y) + FVector(0.f, 0.f, 13.f));
	Item->bClaimed = false;
}

// --- stockpiles ---

FVector AColony::PlaceCenter(const FIntPoint& Place) const
{
	return OnGround((Place.X + 0.5f) * Storage::PlaceSize, (Place.Y + 0.5f) * Storage::PlaceSize);
}

FStoreSpot AColony::Reserve(EItem Kind, const FVector& From)
{
	// The nearest place that takes this kind and has a free slot: one already holding the same
	// kind, or an empty one.
	FStoreSpot Best;
	float BestDist = TNumericLimits<float>::Max();
	for (TPair<FIntPoint, FStorePlace>& Pair : Places)
	{
		FStorePlace& Place = Pair.Value;
		if (!Zones[Place.Zone].Takes(Kind) || (Place.Kind.IsSet() && Place.Kind.GetValue() != Kind && !Place.IsEmpty()))
		{
			continue;
		}
		const float D = FVector::DistSquared2D(From, PlaceCenter(Pair.Key));
		if (D >= BestDist)
		{
			continue;
		}
		for (int32 S = 0; S < Storage::Capacity(Kind); ++S)
		{
			const bool bTaken = Place.Slots.IsValidIndex(S) && (Place.Slots[S].IsValid() || Place.Reserved[S]);
			if (!bTaken)
			{
				Best.Place = Pair.Key;
				Best.Slot = S;
				BestDist = D;
				break;
			}
		}
	}
	if (Best.IsValid())
	{
		FStorePlace& Place = Places[Best.Place];
		if (Place.IsEmpty())
		{
			Place.Kind = Kind;
		}
		Place.Slots.SetNum(FMath::Max(Place.Slots.Num(), Storage::Capacity(Kind)));
		Place.Reserved.SetNum(Place.Slots.Num());
		Place.Reserved[Best.Slot] = true;
	}
	return Best;
}

void AColony::Unreserve(const FStoreSpot& Spot)
{
	if (FStorePlace* Place = Places.Find(Spot.Place))
	{
		if (Place->Reserved.IsValidIndex(Spot.Slot))
		{
			Place->Reserved[Spot.Slot] = false;
		}
	}
}

bool AColony::HasRoomFor(EItem Kind) const
{
	for (const TPair<FIntPoint, FStorePlace>& Pair : Places)
	{
		const FStorePlace& Place = Pair.Value;
		if (!Zones[Place.Zone].Takes(Kind))
		{
			continue;
		}
		if (Place.IsEmpty())
		{
			return true;
		}
		if (Place.Kind.IsSet() && Place.Kind.GetValue() == Kind)
		{
			for (int32 S = 0; S < Storage::Capacity(Kind); ++S)
			{
				if (!Place.Slots.IsValidIndex(S) || (!Place.Slots[S].IsValid() && !Place.Reserved[S]))
				{
					return true;
				}
			}
		}
	}
	return false;
}

FVector AColony::SpotLocation(const FStoreSpot& Spot, EItem Kind) const
{
	const FStorePlace* Place = Places.Find(Spot.Place);
	const bool bIndoor = Place && Zones[Place->Zone].bIndoor;
	return PlaceCenter(Spot.Place) + Storage::SlotOffset(Kind, Spot.Slot, bIndoor);
}

void AColony::Store(AItem* Item, const FStoreSpot& Spot)
{
	FStorePlace* Place = Places.Find(Spot.Place);
	if (!Place || !Place->Slots.IsValidIndex(Spot.Slot))
	{
		// The stockpile was taken away while it was being carried.
		Drop(Item, Item->GetActorLocation());
		return;
	}
	Place->Reserved[Spot.Slot] = false;
	Place->Slots[Spot.Slot] = Item;
	Item->SetActorLocation(SpotLocation(Spot, Item->Type));
	Item->SetActorRotation(FRotator(0.f, Item->Type == EItem::Log ? 0.f : 30.f, 0.f));
	Item->bStored = true;
	Item->bClaimed = false;
	Item->Spot = Spot;
}

void AColony::Unstore(AItem* Item)
{
	if (!Item->bStored)
	{
		return;
	}
	if (FStorePlace* Place = Places.Find(Item->Spot.Place))
	{
		if (Place->Slots.IsValidIndex(Item->Spot.Slot))
		{
			Place->Slots[Item->Spot.Slot] = nullptr;
		}
	}
	Item->bStored = false;
	Item->Spot = FStoreSpot();
}

int32 AColony::AddZone(const FVector2D& A, const FVector2D& B)
{
	const FIntPoint Lo = Storage::PlaceAt(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)));
	const FIntPoint Hi = Storage::PlaceAt(FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)));
	FStoreZone Zone;
	for (int32 X = Lo.X; X <= Hi.X; ++X)
	{
		for (int32 Y = Lo.Y; Y <= Hi.Y; ++Y)
		{
			const FIntPoint P(X, Y);
			// Not in the river or on a steep bank, and not where another stockpile already is.
			if (!Places.Contains(P) && !Occupied.Contains(P) && IsDryLand(PlaceCenter(P)))
			{
				Zone.Places.Add(P);
			}
		}
	}
	if (Zone.Places.IsEmpty())
	{
		return 0;
	}
	const int32 Index = Zones.Add(Zone);
	for (const FIntPoint& P : Zone.Places)
	{
		Places.Add(P).Zone = Index;
	}
	return Zone.Places.Num();
}

int32 AColony::RemoveZones(const FVector2D& A, const FVector2D& B)
{
	const FIntPoint Lo = Storage::PlaceAt(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)));
	const FIntPoint Hi = Storage::PlaceAt(FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)));
	int32 Removed = 0;
	for (int32 X = Lo.X; X <= Hi.X; ++X)
	{
		for (int32 Y = Lo.Y; Y <= Hi.Y; ++Y)
		{
			const FIntPoint P(X, Y);
			FStorePlace* Place = Places.Find(P);
			if (!Place || Zones[Place->Zone].bIndoor)
			{
				continue;
			}
			// Whatever lay there stays where it is, loose, to be carried somewhere else.
			for (const TWeakObjectPtr<AItem>& Item : Place->Slots)
			{
				if (Item.IsValid())
				{
					Item->bStored = false;
					Item->Spot = FStoreSpot();
				}
			}
			FStoreZone& Zone = Zones[Place->Zone];
			Zone.Places.Remove(P);
			Zone.bRemoved = Zone.Places.IsEmpty();
			Places.Remove(P);
			++Removed;
		}
	}
	return Removed;
}

bool AColony::CycleZone(const FVector2D& At)
{
	const FStorePlace* Place = Places.Find(Storage::PlaceAt(At));
	if (!Place || Zones[Place->Zone].bIndoor)
	{
		return false;
	}
	FStoreZone& Zone = Zones[Place->Zone];
	Zone.Accepts = static_cast<EStoreFilter>(((int32)Zone.Accepts + 1) % 3);
	return true;
}

void AColony::Say(const FString& Text)
{
	News.Add({ Text, GetClock() });
	if (News.Num() > 30)
	{
		News.RemoveAt(0);
	}
}

float AColony::SpoilRate(const AItem* Item) const
{
	// Heat speeds the rot and cold slows it right down; a storehouse slows it further.
	const float T = Temperature();
	const float Heat = T > 12.f ? 1.f + (T - 12.f) * 0.08f : FMath::GetMappedRangeValueClamped(FVector2D(12.f, 2.f), FVector2D(1.f, 0.3f), T);
	if (Item->bStored)
	{
		const FStorePlace* Place = Places.Find(Item->Spot.Place);
		if (Place && Zones[Place->Zone].bIndoor)
		{
			// Dry and shaded in the storehouse: about three times as long.
			return Heat * 0.35f;
		}
	}
	return Heat;
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
		Item->HoursLeft -= GameHours * SpoilRate(Item);
		if (Item->HoursLeft <= 0.f)
		{
			Rotted += Item->Portions;
			Unstore(Item);
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

// --- buildings ---

void AColony::NextBuildChoice()
{
	if (Order != EOrder::Build)
	{
		Order = EOrder::Build;
		BuildChoice = EBuilding::Shelter;
		return;
	}
	const int32 Next = (int32)BuildChoice + 1;
	if (Next >= (int32)EBuilding::Count)
	{
		Order = EOrder::None;
		return;
	}
	BuildChoice = static_cast<EBuilding>(Next);
}

FVector2D AColony::PlaceCorner(const FIntPoint& Place) const
{
	return FVector2D(Place.X * Storage::PlaceSize, Place.Y * Storage::PlaceSize);
}

FIntPoint AColony::FootprintAt(EBuilding Type, int32 Turns, const FVector2D& Centre) const
{
	const FIntPoint Size = ABuilding::Footprint(Type, Turns);
	const FVector2D Corner = Centre - FVector2D(Size) * Storage::PlaceSize * 0.5f;
	return FIntPoint(FMath::RoundToInt(Corner.X / Storage::PlaceSize), FMath::RoundToInt(Corner.Y / Storage::PlaceSize));
}

bool AColony::CanPlace(EBuilding Type, int32 Turns, const FIntPoint& First, FString* Why) const
{
	auto Fail = [Why](const TCHAR* Reason)
	{
		if (Why)
		{
			*Why = Reason;
		}
		return false;
	};
	const FIntPoint Size = ABuilding::Footprint(Type, Turns);
	float Low = TNumericLimits<float>::Max();
	float High = TNumericLimits<float>::Lowest();
	for (int32 X = 0; X < Size.X; ++X)
	{
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			const FIntPoint P = First + FIntPoint(X, Y);
			if (!IsDryLand(PlaceCenter(P)))
			{
				return Fail(TEXT("too wet or steep here"));
			}
			if (Places.Contains(P))
			{
				return Fail(TEXT("a stockpile is in the way"));
			}
			if (Occupied.Contains(P))
			{
				return Fail(TEXT("something is already built here"));
			}
		}
	}
	const FVector2D Lo = PlaceCorner(First);
	const FVector2D Hi = PlaceCorner(First + Size);
	for (const FVector2D& Corner : { Lo, Hi, FVector2D(Lo.X, Hi.Y), FVector2D(Hi.X, Lo.Y) })
	{
		const float H = OnGround(Corner.X, Corner.Y).Z;
		Low = FMath::Min(Low, H);
		High = FMath::Max(High, H);
	}
	if (High - Low > 90.f)
	{
		return Fail(TEXT("the ground isn't level enough"));
	}
	const FBox2D Area(Lo - FVector2D(60.f), Hi + FVector2D(60.f));
	if (Area.ExpandBy(250.f).IsInside(FVector2D(Fire)))
	{
		return Fail(TEXT("too close to the fire"));
	}
	for (const FTreeSpot& Tree : Forest->GetTrees())
	{
		if (!Tree.bGone && Area.IsInside(FVector2D(Tree.Base())))
		{
			return Fail(TEXT("trees are in the way: mark them for felling first"));
		}
	}
	return true;
}

ABuilding* AColony::PlaceBuilding(EBuilding Type, int32 Turns, const FVector2D& Centre)
{
	const FIntPoint First = FootprintAt(Type, Turns, Centre);
	FString Why;
	if (!CanPlace(Type, Turns, First, &Why))
	{
		Say(FString::Printf(TEXT("Can't build a %s there: %s."), *FString(ABuilding::Kind(Type).Name).ToLower(), *Why));
		return nullptr;
	}
	// It stands level, at the height of its highest corner, so no wall hangs in the air.
	const FIntPoint Size = ABuilding::Footprint(Type, Turns);
	const FVector2D Lo = PlaceCorner(First);
	const FVector2D Hi = PlaceCorner(First + Size);
	float Top = OnGround((Lo.X + Hi.X) * 0.5f, (Lo.Y + Hi.Y) * 0.5f).Z;
	for (const FVector2D& Corner : { Lo, Hi, FVector2D(Lo.X, Hi.Y), FVector2D(Hi.X, Lo.Y) })
	{
		Top = FMath::Max(Top, OnGround(Corner.X, Corner.Y).Z);
	}
	const FVector At((Lo.X + Hi.X) * 0.5f, (Lo.Y + Hi.Y) * 0.5f, Top);
	ABuilding* Building = GetWorld()->SpawnActor<ABuilding>(At, FRotator(0.f, Turns * 90.f, 0.f));
	Building->Init(Type, Turns, First);
	const int32 Index = Buildings.Add(Building);
	for (const FIntPoint& P : Building->Places())
	{
		Occupied.Add(P, Index);
	}
	Say(FString::Printf(TEXT("A %s is staked out: it needs %d logs."), *FString(Building->GetKind().Name).ToLower(), Building->GetKind().Logs));
	return Building;
}

ABuilding* AColony::SiteNeedingLogs(const FVector& From) const
{
	ABuilding* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (ABuilding* B : Buildings)
	{
		if (!B->IsFinished() && B->LogsWanted() > 0)
		{
			const float D = FVector::DistSquared2D(From, B->GetActorLocation());
			if (D < BestDist)
			{
				BestDist = D;
				Best = B;
			}
		}
	}
	return Best;
}

ABuilding* AColony::SiteToWork(const FVector& From) const
{
	ABuilding* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (ABuilding* B : Buildings)
	{
		if (B->CanWork())
		{
			const float D = FVector::DistSquared2D(From, B->GetActorLocation());
			if (D < BestDist)
			{
				BestDist = D;
				Best = B;
			}
		}
	}
	return Best;
}

AItem* AColony::LogToTake(const FVector& From) const
{
	AItem* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (AItem* Item : Items)
	{
		if (Item->Type == EItem::Log && !Item->bClaimed)
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

void AColony::Deliver(AItem* Log, ABuilding* Site)
{
	Unstore(Log);
	Items.Remove(Log);
	Log->Destroy();
	Site->Incoming = FMath::Max(0, Site->Incoming - 1);
	Site->Delivered += 1;
}

void AColony::OnBuilt(ABuilding* Building)
{
	if (!Building->IsFinished())
	{
		return;
	}
	const FString Name = FString(Building->GetKind().Name).ToLower();
	if (Building->GetType() == EBuilding::Storehouse)
	{
		// The floor inside becomes a food store, kept dry and shaded.
		FStoreZone Zone;
		Zone.Accepts = EStoreFilter::Food;
		Zone.bIndoor = true;
		Zone.Places = Building->Places();
		const int32 Index = Zones.Add(Zone);
		for (const FIntPoint& P : Zone.Places)
		{
			Places.Add(P).Zone = Index;
		}
		Say(TEXT("The storehouse is finished: food kept inside will last much longer."));
		return;
	}
	Say(FString::Printf(TEXT("The %s is finished."), *Name));
	AssignHomes();
}

void AColony::AssignHomes()
{
	// ByFamily stay together: the biggest family takes the first cabin with room, then the
	// rest find a place, cabins before shelters.
	TArray<ABuilding*> Homes;
	for (ABuilding* B : Buildings)
	{
		if (B->IsFinished() && B->GetKind().Beds > 0)
		{
			Homes.Add(B);
		}
	}
	Homes.Sort([](const ABuilding& A, const ABuilding& B) { return A.GetKind().Beds > B.GetKind().Beds; });
	TArray<int32> Free;
	for (ABuilding* B : Homes)
	{
		Free.Add(B->GetKind().Beds);
	}

	TMap<FString, TArray<AFounder*>> ByFamily;
	for (AFounder* F : Founders)
	{
		ByFamily.FindOrAdd(F->GetInfo().Family).Add(F);
	}
	TArray<TArray<AFounder*>> Groups;
	ByFamily.GenerateValueArray(Groups);
	Groups.Sort([](const TArray<AFounder*>& A, const TArray<AFounder*>& B) { return A.Num() > B.Num(); });

	TArray<FString> MovedIn;
	for (const TArray<AFounder*>& Family : Groups)
	{
		int32 Home = INDEX_NONE;
		for (int32 H = 0; H < Homes.Num() && Home == INDEX_NONE; ++H)
		{
			if (Free[H] >= Family.Num())
			{
				Home = H;
			}
		}
		for (AFounder* F : Family)
		{
			int32 Pick = Home;
			if (Pick == INDEX_NONE || Free[Pick] <= 0)
			{
				Pick = INDEX_NONE;
				for (int32 H = 0; H < Homes.Num() && Pick == INDEX_NONE; ++H)
				{
					Pick = Free[H] > 0 ? H : INDEX_NONE;
				}
			}
			ABuilding* Was = F->GetHome();
			if (Pick == INDEX_NONE)
			{
				F->SetHome(nullptr, INDEX_NONE);
				continue;
			}
			const int32 Bed = Homes[Pick]->GetKind().Beds - Free[Pick];
			--Free[Pick];
			F->SetHome(Homes[Pick], Bed);
			if (Was != Homes[Pick] && Homes[Pick]->GetType() == EBuilding::Cabin && !MovedIn.Contains(F->GetInfo().Family))
			{
				MovedIn.Add(F->GetInfo().Family);
				Say(FString::Printf(TEXT("The %s family moves into the cabin."), *F->GetInfo().Family));
			}
		}
	}
}
