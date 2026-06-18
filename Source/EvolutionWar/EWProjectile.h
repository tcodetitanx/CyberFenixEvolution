// A cosmetic energy bolt fired by cyborg units. Flies to its target and bursts.
// Damage is applied instantly by the firing unit; this actor is purely VFX.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWProjectile.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class EVOLUTIONWAR_API AEWProjectile : public AActor
{
	GENERATED_BODY()

public:
	AEWProjectile();

	/** Fire from Start toward Target (homing). Color is the bolt's glow. */
	void Launch(const FVector& Start, AActor* InTarget, const FVector& FallbackEnd, const FLinearColor& InColor, float InSpeed);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void SetGlow(float Opacity);
	void BeginImpact();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Trail;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> GlowMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HeadMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TrailMID;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Target;

	FVector EndPoint = FVector::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	float Speed = 5200.f;
	float Age = 0.f;
	float MaxLife = 1.4f;
	bool bImpact = false;
	float ImpactAge = 0.f;
	float ImpactDuration = 0.14f;
};
