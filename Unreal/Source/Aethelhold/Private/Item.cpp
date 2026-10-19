#include "Item.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

AItem::AItem()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FString AItem::Name() const
{
	switch (Type)
	{
	case EItem::Log: return TEXT("log");
	case EItem::Nuts: return TEXT("basket of hazelnuts");
	case EItem::Fish: return Portions > 2 ? TEXT("string of fish") : TEXT("fish");
	}
	return FString();
}

void AItem::Init(EItem InType, int32 InPortions)
{
	Type = InType;
	Portions = InPortions;
	// Stand-in shapes until the item art comes: a bark-covered log, a heap of nuts, a fish.
	auto LoadMesh = [](const TCHAR* Path) { return LoadObject<UStaticMesh>(nullptr, Path); };
	auto LoadMat = [](const TCHAR* Path) { return LoadObject<UMaterialInterface>(nullptr, Path); };
	switch (Type)
	{
	case EItem::Log:
		HoursLeft = -1.f;
		Mesh->SetStaticMesh(LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		Mesh->SetMaterial(0, LoadMat(TEXT("/Game/Megaplant_Library/Tree_English_Oak/Materials/MI_English_Oak_Bark_01.MI_English_Oak_Bark_01")));
		// Lying along the actor's forward axis, about 2 m long and 25 cm thick.
		Mesh->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
		Mesh->SetRelativeScale3D(FVector(0.25f, 0.25f, 2.f));
		break;
	case EItem::Nuts:
		// Hazelnuts keep for weeks.
		HoursLeft = 24.f * 20.f;
		Mesh->SetStaticMesh(LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Mesh->SetMaterial(0, LoadMat(TEXT("/Game/Fab/Megascans/Surfaces/Forest_Floor_vktfeilaw/High/vktfeilaw_tier_1/Materials/MI_vktfeilaw.MI_vktfeilaw")));
		Mesh->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.25f));
		break;
	case EItem::Fish:
		// Fresh fish rots in two days.
		HoursLeft = 48.f;
		Mesh->SetStaticMesh(LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Mesh->SetMaterial(0, LoadMat(TEXT("/Game/Fab/Megascans/Surfaces/Small_Pebbles_Ground_xbliajz/High/xbliajz_tier_1/Materials/MI_xbliajz.MI_xbliajz")));
		Mesh->SetRelativeScale3D(FVector(0.5f, 0.14f, 0.12f));
		break;
	}
}
