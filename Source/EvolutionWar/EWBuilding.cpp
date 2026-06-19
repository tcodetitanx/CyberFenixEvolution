#include "EWBuilding.h"
#include "EWGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AEWBuilding::AEWBuilding()
{
	PrimaryActorTick.bCanEverTick = false;

	SpriteMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpriteMesh"));
	SetRootComponent(SpriteMesh);
	SpriteMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpriteMesh->SetCastShadow(false);
	// Stand the plane up facing the camera (-Y side).
	SpriteMesh->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded())
	{
		SpriteMesh->SetStaticMesh(PlaneFinder.Object);
	}
}

void AEWBuilding::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	if (AEWGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AEWGameMode>() : nullptr)
	{
		GM->RegisterBuilding(this);
	}
	ApplyVisuals();
}

void AEWBuilding::ApplyVisuals()
{
	if (!SpriteMaterial)
	{
		SpriteMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/M_Sprite.M_Sprite"));
	}
	if (SpriteMaterial && !SpriteMID)
	{
		SpriteMID = UMaterialInstanceDynamic::Create(SpriteMaterial, this);
		SpriteMesh->SetMaterial(0, SpriteMID);
	}
	if (SpriteMID && Sprite)
	{
		SpriteMID->SetTextureParameterValue(TEXT("BaseTex"), Sprite);
		SpriteMID->SetVectorParameterValue(TEXT("Tint"), FLinearColor::White);
	}

	const float Desired = FMath::Max(50.f, SpriteWorldHeight);
	// Negative X mirrors the billboard horizontally to match the mirrored world.
	const float SF = Desired / 100.f;
	SpriteMesh->SetRelativeScale3D(FVector(-SF, SF, SF));
	// The base actors are placed in the level, and a placed transform's rotation
	// overrides the constructor's. Force the sprite upright + facing the camera here
	// so a base can never end up edge-on (parallel to the camera's line of sight).
	SetActorRotation(FRotator(0.f, 0.f, 90.f));
	// SpriteMesh is the root component, so moving it moves the actor. Lift the
	// sprite so its base rests on the ground WITHOUT discarding the placed X/Y.
	const FVector Loc = GetActorLocation();
	SetActorLocation(FVector(Loc.X, Loc.Y, Desired * 0.5f));
}

void AEWBuilding::ReceiveDamage(float Amount, AActor* DamageInstigator)
{
	if (!IsAlive())
	{
		return;
	}
	Health = FMath::Max(0.f, Health - Amount);
	if (Health <= 0.f)
	{
		if (AEWGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AEWGameMode>() : nullptr)
		{
			GM->NotifyBuildingDestroyed(this);
		}
	}
}
