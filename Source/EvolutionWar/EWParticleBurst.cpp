#include "EWParticleBurst.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

AEWParticleBurst::AEWParticleBurst()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

AEWParticleBurst* AEWParticleBurst::Spawn(UWorld* World, const FVector& Location, const FLinearColor& InColor,
	int32 Count, float Speed, float InLife, float Size)
{
	if (!World) { return nullptr; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AEWParticleBurst* Burst = World->SpawnActor<AEWParticleBurst>(AEWParticleBurst::StaticClass(), FTransform(Location), Params);
	if (Burst)
	{
		Burst->Configure(InColor, Count, Speed, InLife, Size);
	}
	return Burst;
}

void AEWParticleBurst::Configure(const FLinearColor& InColor, int32 Count, float Speed, float InLife, float Size)
{
	Color = InColor;
	Life = FMath::Max(0.1f, InLife);
	BaseSize = FMath::Max(4.f, Size);
	Count = FMath::Clamp(Count, 1, 40);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));

	if (!GlowMaterial)
	{
		GlowMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/M_Glow.M_Glow"));
	}
	if (GlowMaterial)
	{
		MID = UMaterialInstanceDynamic::Create(GlowMaterial, this);
		MID->SetVectorParameterValue(TEXT("GlowColor"), Color);
		MID->SetScalarParameterValue(TEXT("Opacity"), 1.f);
	}

	for (int32 i = 0; i < Count; ++i)
	{
		UStaticMeshComponent* Spark = NewObject<UStaticMeshComponent>(this);
		Spark->SetupAttachment(GetRootComponent());
		Spark->RegisterComponent();
		Spark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Spark->SetCastShadow(false);
		// Stand the flat plane up to face the camera (-Y), like the units.
		Spark->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
		const float S = BaseSize * FMath::FRandRange(0.5f, 1.2f) / 100.f; // plane is 100uu
		Spark->SetRelativeScale3D(FVector(S));
		if (PlaneFinder.Succeeded())
		{
			Spark->SetStaticMesh(PlaneFinder.Object);
		}
		if (MID)
		{
			Spark->SetMaterial(0, MID);
		}

		// Random outward velocity in the X-Z lane plane, biased upward.
		const float Ang = FMath::FRandRange(0.f, 2.f * PI);
		const float Spd = Speed * FMath::FRandRange(0.45f, 1.f);
		FVector Vel(FMath::Cos(Ang) * Spd, FMath::FRandRange(-40.f, 40.f), FMath::Sin(Ang) * Spd + Speed * 0.4f);
		Velocities.Add(Vel);
		Spins.Add(FMath::FRandRange(-540.f, 540.f));
		Sparks.Add(Spark);
	}
}

void AEWParticleBurst::BeginPlay()
{
	Super::BeginPlay();
}

void AEWParticleBurst::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / Life, 0.f, 1.f);

	if (MID)
	{
		// Fade out, and let the glow cool from its color toward dim near the end.
		MID->SetScalarParameterValue(TEXT("Opacity"), 1.f - T);
	}

	for (int32 i = 0; i < Sparks.Num(); ++i)
	{
		UStaticMeshComponent* Spark = Sparks[i];
		if (!Spark) { continue; }
		Velocities[i].Z -= Gravity * DeltaSeconds;
		Spark->AddRelativeLocation(Velocities[i] * DeltaSeconds);
		// Shrink as it ages.
		const float Scale = (BaseSize / 100.f) * FMath::Lerp(1.f, 0.1f, T);
		Spark->SetRelativeScale3D(FVector(Scale));
		FRotator Rot = Spark->GetRelativeRotation();
		Rot.Pitch += Spins[i] * DeltaSeconds;
		Spark->SetRelativeRotation(Rot);
	}

	if (T >= 1.f)
	{
		Destroy();
	}
}
