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

	// FObjectFinder is only valid inside a constructor; resolve the mesh once here
	// and reuse the cached reference at runtime in Configure().
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded())
	{
		PlaneMesh = PlaneFinder.Object;
	}
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
	// Enhanced: denser bursts.
	Count = FMath::Clamp(FMath::RoundToInt((float)Count * 1.7f), 1, 90);

	if (!GlowMaterial)
	{
		GlowMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/M_Glow.M_Glow"));
	}
	if (GlowMaterial)
	{
		MID = UMaterialInstanceDynamic::Create(GlowMaterial, this);
		// Hotter core for a punchier flash.
		MID->SetVectorParameterValue(TEXT("GlowColor"), Color * 1.4f);
		MID->SetScalarParameterValue(TEXT("Opacity"), 1.f);
	}

	for (int32 i = 0; i < Count; ++i)
	{
		UStaticMeshComponent* Spark = NewObject<UStaticMeshComponent>(this);
		Spark->SetupAttachment(GetRootComponent());
		Spark->RegisterComponent();
		Spark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Spark->SetCastShadow(false);
		// Stand the flat plane up to face the camera (-Y side), like the units.
		Spark->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
		// Wider size spread, with the occasional big ember for variety.
		float Mag = FMath::FRandRange(0.35f, 1.3f);
		if (FMath::FRand() < 0.15f) { Mag *= 1.9f; }
		const float S = BaseSize * Mag / 100.f; // plane is 100uu
		Spark->SetRelativeScale3D(FVector(S));
		if (PlaneMesh)
		{
			Spark->SetStaticMesh(PlaneMesh);
		}
		if (MID)
		{
			Spark->SetMaterial(0, MID);
		}

		// More energetic outward velocity in the X-Z lane plane, with a stronger
		// upward bias so the burst plumes up before raining down.
		const float Ang = FMath::FRandRange(0.f, 2.f * PI);
		const float Spd = Speed * FMath::FRandRange(0.5f, 1.35f);
		FVector Vel(FMath::Cos(Ang) * Spd, FMath::FRandRange(-60.f, 60.f), FMath::Sin(Ang) * Spd + Speed * 0.55f);
		Velocities.Add(Vel);
		Spins.Add(FMath::FRandRange(-720.f, 720.f));
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
		// Punchy fade: hold the brightness then fall off, with a fast flicker so the
		// embers sparkle as they die.
		const float Flicker = 0.86f + 0.14f * FMath::Sin(Age * 60.f);
		MID->SetScalarParameterValue(TEXT("Opacity"), FMath::Pow(1.f - T, 0.7f) * Flicker);
	}

	// Multiplicative shrink preserves each spark's individual size as it fades.
	const float Keep = FMath::Pow(0.07f, DeltaSeconds / Life);
	const float Drag = FMath::Pow(0.55f, DeltaSeconds); // air resistance for nicer arcs
	for (int32 i = 0; i < Sparks.Num(); ++i)
	{
		UStaticMeshComponent* Spark = Sparks[i];
		if (!Spark) { continue; }
		Velocities[i].Z -= Gravity * DeltaSeconds;
		Velocities[i] *= Drag;
		Spark->AddRelativeLocation(Velocities[i] * DeltaSeconds);
		Spark->SetRelativeScale3D(Spark->GetRelativeScale3D() * Keep);
		FRotator Rot = Spark->GetRelativeRotation();
		Rot.Pitch += Spins[i] * DeltaSeconds;
		Spark->SetRelativeRotation(Rot);
	}

	if (T >= 1.f)
	{
		Destroy();
	}
}
