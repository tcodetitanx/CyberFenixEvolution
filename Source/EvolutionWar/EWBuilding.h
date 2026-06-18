// A faction structure: Base / Hive (destroy to win) or Generation Depot.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWTypes.h"
#include "EWBuilding.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class AEWGameMode;

UCLASS()
class EVOLUTIONWAR_API AEWBuilding : public AActor
{
	GENERATED_BODY()

public:
	AEWBuilding();

	UFUNCTION(BlueprintCallable, Category = "EW|Building")
	void ReceiveDamage(float Amount, AActor* DamageInstigator);

	UFUNCTION(BlueprintPure, Category = "EW|Building")
	EEWTeam GetTeam() const { return Team; }

	UFUNCTION(BlueprintPure, Category = "EW|Building")
	EEWBuildingType GetBuildingType() const { return BuildingType; }

	UFUNCTION(BlueprintPure, Category = "EW|Building")
	float GetHealthFraction() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "EW|Building")
	bool IsAlive() const { return Health > 0.f; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Building")
	EEWTeam Team = EEWTeam::Cyborg;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Building")
	EEWBuildingType BuildingType = EEWBuildingType::Base;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Building")
	float MaxHealth = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Building")
	TObjectPtr<UTexture2D> Sprite = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Building")
	float SpriteWorldHeight = 1400.f;

protected:
	virtual void BeginPlay() override;
	void ApplyVisuals();

	UPROPERTY(VisibleAnywhere, Category = "EW|Building")
	TObjectPtr<UStaticMeshComponent> SpriteMesh;

	UPROPERTY(EditAnywhere, Category = "EW|Building")
	TObjectPtr<UMaterialInterface> SpriteMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SpriteMID;

	UPROPERTY(VisibleAnywhere, Category = "EW|Building")
	float Health = 5000.f;
};
