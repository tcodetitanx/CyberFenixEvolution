#include "EWProjectile.h"
#include "EWParticleBurst.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AEWProjectile::AEWProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));

	// Both pieces are flat planes stood up to face the camera (-Y), like the units.
	Trail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Trail"));
	Trail->SetupAttachment(Root);
	Trail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Trail->SetCastShadow(false);
	Trail->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	// Streak trailing behind the head along the lane (cyborgs fire toward +X).
	Trail->SetRelativeLocation(FVector(-70.f, 0.f, 0.f));
	Trail->SetRelativeScale3D(FVector(1.4f, 0.14f, 1.f));

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Root);
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Head->SetCastShadow(false);
	Head->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	Head->SetRelativeScale3D(FVector(0.32f, 0.32f, 1.f));

	if (PlaneFinder.Succeeded())
	{
		Head->SetStaticMesh(PlaneFinder.Object);
		Trail->SetStaticMesh(PlaneFinder.Object);
	}
}

void AEWProjectile::Launch(const FVector& Start, AActor* InTarget, const FVector& FallbackEnd, const FLinearColor& InColor, float InSpeed)
{
	SetActorLocation(Start);
	Target = InTarget;
	EndPoint = FallbackEnd;
	Color = InColor;
	Speed = FMath::Max(800.f, InSpeed);
}

void AEWProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (!GlowMaterial)
	{
		GlowMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/M_Glow.M_Glow"));
	}
	if (GlowMaterial)
	{
		HeadMID = UMaterialInstanceDynamic::Create(GlowMaterial, this);
		TrailMID = UMaterialInstanceDynamic::Create(GlowMaterial, this);
		Head->SetMaterial(0, HeadMID);
		Trail->SetMaterial(0, TrailMID);
	}
	SetGlow(1.f);
}

void AEWProjectile::SetGlow(float Opacity)
{
	if (HeadMID)
	{
		HeadMID->SetVectorParameterValue(TEXT("GlowColor"), Color);
		HeadMID->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	}
	if (TrailMID)
	{
		// Trail is a touch dimmer than the bright core.
		TrailMID->SetVectorParameterValue(TEXT("GlowColor"), Color * 0.6f);
		TrailMID->SetScalarParameterValue(TEXT("Opacity"), Opacity * 0.8f);
	}
}

void AEWProjectile::BeginImpact()
{
	if (bImpact) { return; }
	bImpact = true;
	ImpactAge = 0.f;
	if (Trail)
	{
		Trail->SetVisibility(false);
	}
	// Spark burst where the bolt lands.
	AEWParticleBurst::Spawn(GetWorld(), GetActorLocation(), Color, 9, 900.f, 0.4f, 22.f);
}

void AEWProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	if (bImpact)
	{
		// Flash: the head puffs outward and fades, then the bolt is gone.
		ImpactAge += DeltaSeconds;
		const float T = FMath::Clamp(ImpactAge / ImpactDuration, 0.f, 1.f);
		if (Head)
		{
			const float S = FMath::Lerp(0.32f, 1.1f, T);
			Head->SetRelativeScale3D(FVector(S, S, 1.f));
		}
		SetGlow(1.f - T);
		if (T >= 1.f)
		{
			Destroy();
		}
		return;
	}

	// Steer toward the live target if it still exists, else the cached point.
	FVector Goal = EndPoint;
	if (Target)
	{
		Goal = Target->GetActorLocation();
		Goal.Z += 60.f; // aim at the body, not the feet
	}

	const FVector Loc = GetActorLocation();
	const FVector ToGoal = Goal - Loc;
	const float Dist = ToGoal.Size();
	const float StepLen = Speed * DeltaSeconds;

	if (Dist <= StepLen + 25.f || Age >= MaxLife)
	{
		SetActorLocation(Goal);
		BeginImpact();
		return;
	}

	const FVector Dir = ToGoal / FMath::Max(1.f, Dist);
	SetActorLocation(Loc + Dir * StepLen);
}
