// A single combat unit (cyborg or insect). Visual is a camera-facing sprite plane.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWTypes.h"
#include "EWUnit.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class AEWGameMode;

UCLASS()
class EVOLUTIONWAR_API AEWUnit : public AActor
{
	GENERATED_BODY()

public:
	AEWUnit();

	/** Configure team, genome and sprite. Call right after spawn (before/at BeginPlay). */
	UFUNCTION(BlueprintCallable, Category = "EW|Unit")
	void InitUnit(EEWTeam InTeam, const FUnitGenome& InGenome, UTexture2D* InSprite, float InSpriteWorldHeight);

	UFUNCTION(BlueprintCallable, Category = "EW|Unit")
	void ReceiveDamage(float Amount, AActor* DamageInstigator);

	/** Restore health (capped at MaxHealth). Used by the Shield orbital. */
	UFUNCTION(BlueprintCallable, Category = "EW|Unit")
	void Heal(float Amount);

	UFUNCTION(BlueprintPure, Category = "EW|Unit")
	EEWTeam GetTeam() const { return Team; }

	UFUNCTION(BlueprintPure, Category = "EW|Unit")
	const FUnitGenome& GetGenome() const { return Genome; }

	/** Tactical role (from the genome), drives formation/attacks/targeting. */
	UFUNCTION(BlueprintPure, Category = "EW|Unit")
	EEWRole GetRole() const { return Genome.Role; }

	/** True if this unit flies above the lane (special targeting + altitude). */
	UFUNCTION(BlueprintPure, Category = "EW|Unit")
	bool IsAerial() const { return IsAerialRole(Genome.Role); }

	/** Which parallel depth file this unit marches in (0..N-1); -1 = unassigned. */
	int32 GetDepthLane() const { return DepthLane; }
	void SetDepthLane(int32 In) { DepthLane = In; }

	UFUNCTION(BlueprintPure, Category = "EW|Unit")
	float GetHealthFraction() const { return Genome.MaxHealth > 0.f ? Health / Genome.MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "EW|Unit")
	bool IsAlive() const { return Health > 0.f; }

	/** Accumulated combat performance (damage dealt + kill bonuses). Drives champion selection. */
	UFUNCTION(BlueprintPure, Category = "EW|Unit")
	float GetFitness() const { return Fitness; }

	UFUNCTION(BlueprintPure, Category = "EW|Unit")
	int32 GetKills() const { return Kills; }

	UTexture2D* GetSpriteTexture() const { return SpriteTexture; }
	float GetSpriteWorldHeight() const { return SpriteWorldHeight; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	void ApplyVisuals();
	void Die();
	float DistanceAlongLane(const AActor* Other) const;

	/** Whether a hit against Goal should be dealt now (and spawn the matching VFX). */
	void PerformAttack(AActor* Goal);

	/** Role-appropriate attack VFX toward Goal: bolt for shooters, slash for melee,
	 *  heavy shock for tanks. Replaces the old cyborg-only projectile. */
	void FireAttackVFX(AActor* Goal);

	/** Support roles pulse a heal + green sparkle to nearby allies on a timer. */
	void TickSupportAura(float DeltaSeconds);

	/** Procedural sprite animation: walk-bob while moving, lunge while attacking,
	 *  shake+flash when hit. Applied as relative rotation/scale + a Z offset so the
	 *  unit's gameplay X position is never disturbed. */
	void UpdateVisualAnim(float DeltaSeconds, bool bMoving);
	/** Plays the topple / shrink / sink death sequence, then destroys the actor. */
	void UpdateDeathAnim(float DeltaSeconds);
	/** Kick off a one-shot attack lunge (the "jump" toward the enemy). */
	void TriggerAttackAnim() { AttackAnimTime = 0.32f; }

	UPROPERTY(VisibleAnywhere, Category = "EW|Unit")
	TObjectPtr<UStaticMeshComponent> SpriteMesh;

	/** Master sprite material (unlit, masked, exposes BaseTex + Tint params). */
	UPROPERTY(EditAnywhere, Category = "EW|Unit")
	TObjectPtr<UMaterialInterface> SpriteMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SpriteMID;

	UPROPERTY(VisibleAnywhere, Category = "EW|Unit")
	EEWTeam Team = EEWTeam::Cyborg;

	UPROPERTY(VisibleAnywhere, Category = "EW|Unit")
	FUnitGenome Genome;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> SpriteTexture;

	UPROPERTY(VisibleAnywhere, Category = "EW|Unit")
	float Health = 100.f;

	float SpriteWorldHeight = 320.f;
	float AttackCooldown = 0.f;

	/** Parallel formation file (set by the GameMode at spawn). */
	int32 DepthLane = 0;
	/** Extra Z the aerial units float at above their ground rest height. */
	float AerialAltitude = 0.f;
	/** Countdown between support heal pulses. */
	float SupportPulseTime = 0.f;

	/** Combat performance, credited as this unit deals damage / lands kills. */
	float Fitness = 0.f;
	int32 Kills = 0;

	// ---- Procedural animation state -----------------------------------------
	float WalkPhase = 0.f;        // advances while marching -> bob/sway
	float IdlePhase = 0.f;        // always advancing -> gentle breathing
	float AttackAnimTime = 0.f;   // countdown for the attack lunge
	float HitShakeTime = 0.f;     // countdown for the damage shake + flash
	bool  bDying = false;         // playing the death animation (no longer fights)
	float DeathTime = 0.f;        // elapsed death-animation time
	float BaseActorZ = 0.f;       // rest Z the bob/jump offsets ride on top of
	float BaseScaleFactor = 1.f;  // rest uniform scale anims multiply

	UPROPERTY(Transient)
	TObjectPtr<AActor> CurrentTarget;

	UPROPERTY(Transient)
	TObjectPtr<AEWGameMode> GM;
};
