// A short-lived spark/ember burst built from glowing camera-facing quads.
// Purely cosmetic VFX spawned on projectile impacts and orbital strikes.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWParticleBurst.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class EVOLUTIONWAR_API AEWParticleBurst : public AActor
{
	GENERATED_BODY()

public:
	AEWParticleBurst();

	/** Spawn a burst at Location. Count quads fly outward, shrink and fade over Life seconds. */
	static AEWParticleBurst* Spawn(UWorld* World, const FVector& Location, const FLinearColor& Color,
		int32 Count, float Speed, float Life, float Size);

	void Configure(const FLinearColor& InColor, int32 Count, float Speed, float Life, float Size);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Sparks;

	TArray<FVector> Velocities;
	TArray<float> Spins;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> GlowMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID;

	FLinearColor Color = FLinearColor::White;
	float BaseSize = 24.f;
	float Gravity = 1400.f;
	float Age = 0.f;
	float Life = 0.45f;
};
