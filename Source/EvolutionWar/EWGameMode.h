// Orchestrates the battle: spawns mutated units from each side, tracks bases,
// resources and win/lose. Designed to be data-driven via the unit class arrays.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EWTypes.h"
#include "EWGameMode.generated.h"

class AEWUnit;
class AEWBuilding;

/** Internal record of the best-performing genome a team has produced so far. */
struct FEWChampion
{
	FUnitGenome Genome;
	UTexture2D* Sprite = nullptr;     // aliases a catalog-owned (rooted) texture
	float SpriteWorldHeight = 320.f;
	float Fitness = 0.f;
	bool bValid = false;
};

/** A transient orbital impact the HUD animates as an expanding ring. */
struct FEWStrike
{
	float X = 0.f;
	float Radius = 100.f;
	float Age = 0.f;     // seconds since fired
	float Life = 0.7f;   // total display duration
	FLinearColor Color = FLinearColor::White;
};

UCLASS()
class EVOLUTIONWAR_API AEWGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AEWGameMode();

	// ---- Lane / world layout -------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Layout")
	float PlayerBaseX = -9000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Layout")
	float EnemyBaseX = 9000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Layout")
	float GroundZ = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Layout")
	float LaneY = 0.f;

	/** Spawned units get a small random Y spread so they don't perfectly overlap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Layout")
	float LaneDepthSpread = 160.f;

	/** Number of parallel ground files; up to this many units can fight abreast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Layout")
	int32 GroundFiles = 5;

	/** Depth (Y) spacing between adjacent files. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Layout")
	float FileSpacing = 120.f;

	// ---- Unit class catalogues (data-driven) --------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Units")
	TArray<FUnitClassDef> CyborgClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Units")
	TArray<FUnitClassDef> InsectClasses;

	// ---- Spawning behaviour --------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Spawning")
	float CyborgSpawnInterval = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Spawning")
	float InsectSpawnInterval = 2.0f;

	/** Mutation swing applied to each auto-spawned unit (chaotic early game). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Spawning")
	float MutationAmount = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Spawning")
	int32 MaxUnitsPerTeam = 60;

	// ---- Player economy ------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "EW|Economy")
	float Biomass = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Economy")
	float BiomassPerSecond = 12.f;

	// ---- Evolution / champions ----------------------------------------------
	/** Highest seed chance reached once a lineage is dominating (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Evolution")
	float MaxChampionSeedChance = 0.7f;

	/** Each crowned champion nudges its team's seed chance up by this much. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Evolution")
	float ChampionSeedChanceStep = 0.06f;

	/** Mutation multiplier shrinks toward this floor as generations advance (chaos -> focus). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Evolution")
	float MutationFloor = 0.08f;

	/** Per-generation decay applied to MutationAmount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Evolution")
	float MutationDecayPerGen = 0.96f;

	/** Champion fitness needed to spin its strain off into a new catalog class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Evolution")
	float SpecializeFitnessThreshold = 350.f;

	/** Cap on how many champion-derived strains a team's catalog may grow to hold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Evolution")
	int32 MaxStrainsPerTeam = 4;

	UFUNCTION(BlueprintPure, Category = "EW|Evolution")
	float GetChampionFitness(EEWTeam InTeam) const { return InTeam == EEWTeam::Cyborg ? CyborgChamp.Fitness : InsectChamp.Fitness; }

	UFUNCTION(BlueprintPure, Category = "EW|Evolution")
	int32 GetGeneration(EEWTeam InTeam) const { return InTeam == EEWTeam::Cyborg ? CyborgGeneration : InsectGeneration; }

	/** A finished unit offers its genome + performance to the team's gene pool. */
	void ReportFitness(EEWTeam InTeam, const FUnitGenome& InGenome, UTexture2D* InSprite, float InSpriteWorldHeight, float InFitness);

	// ---- Registry / queries used by units -----------------------------------
	void RegisterUnit(AEWUnit* Unit);
	void UnregisterUnit(AEWUnit* Unit);
	void RegisterBuilding(AEWBuilding* Building);

	/** Nearest living enemy (unit or base) to Loc within MaxRange, or null. When
	 *  bSeekerCanHitAir is false, aerial enemy units are ignored (ground melee can't
	 *  reach flyers). */
	AActor* FindNearestEnemy(EEWTeam SeekerTeam, const FVector& Loc, float MaxRange, bool bSeekerCanHitAir = true) const;

	/** Heal every friendly unit (of FriendTeam) within Radius of CenterX. Public so
	 *  support units can pulse heals into the pack around them. */
	void HealFriendliesInRange(EEWTeam FriendTeam, float CenterX, float Radius, float HealAmount);

	/** The enemy main structure for the given team (used as march target). */
	AEWBuilding* GetEnemyBase(EEWTeam SeekerTeam) const;

	/**
	 * X of the nearest friendly unit ahead of MyX in the DirSign direction (toward the
	 * enemy), excluding Self, within Lookahead, restricted to the SAME depth file so
	 * each of the N parallel files queues independently (=> up to N units abreast at the
	 * front). Returns false if none found.
	 */
	bool NearestFriendlyAheadX(EEWTeam Team, float MyX, float DirSign, const AEWUnit* Self, float Lookahead, float& OutX) const;

	void NotifyUnitDied(AEWUnit* Unit, AActor* Killer);
	void NotifyBuildingDestroyed(AEWBuilding* Building);

	/** Spawn one unit of a class for a team at its base; genome auto-mutated. */
	UFUNCTION(BlueprintCallable, Category = "EW|Spawning")
	AEWUnit* SpawnUnitOfClass(EEWTeam InTeam, const FUnitClassDef& Def, bool bMutate);

	/** Player manual deploy at a world X (spends Biomass). Returns null if unaffordable. */
	UFUNCTION(BlueprintCallable, Category = "EW|Player")
	AEWUnit* DeployPlayerUnit(int32 ClassIndex, float WorldX);

	UFUNCTION(BlueprintPure, Category = "EW|Economy")
	float GetBiomass() const { return Biomass; }

	// ---- HUD / presentation accessors ---------------------------------------
	UFUNCTION(BlueprintPure, Category = "EW|Match")
	bool IsMatchOver() const { return bMatchOver; }

	UFUNCTION(BlueprintPure, Category = "EW|Match")
	bool DidPlayerWin() const { return bPlayerWon; }

	/** 0..1 health of a team's main structure (Base/Hive), for the top HUD bars. */
	UFUNCTION(BlueprintPure, Category = "EW|Match")
	float GetBaseHealthFraction(EEWTeam InTeam) const;

	/** Lane X midway between the two armies' front lines (camera auto-follow target). */
	float GetCombatCenterX() const;

	const TArray<FEWStrike>& GetRecentStrikes() const { return RecentStrikes; }
	const TArray<TObjectPtr<AEWUnit>>& GetTeamUnits(EEWTeam InTeam) const { return InTeam == EEWTeam::Cyborg ? CyborgUnits : InsectUnits; }
	const TArray<TObjectPtr<AEWBuilding>>& GetBuildings() const { return Buildings; }

	// ---- Orbital boons -------------------------------------------------------
	/** Biomass cost to fire each orbital, indexed by EEWOrbital. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Orbital")
	TArray<float> OrbitalCosts;

	/** Cooldown (seconds) for each orbital, indexed by EEWOrbital. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EW|Orbital")
	TArray<float> OrbitalCooldowns;

	/** Fire an orbital boon centered on a lane X. Spends Biomass + starts cooldown. Returns false if blocked. */
	UFUNCTION(BlueprintCallable, Category = "EW|Orbital")
	bool TriggerOrbital(EEWOrbital Orbital, float WorldX);

	/** Remaining cooldown (seconds) for an orbital, for HUD display. */
	UFUNCTION(BlueprintPure, Category = "EW|Orbital")
	float GetOrbitalCooldownRemaining(EEWOrbital Orbital) const;

	UFUNCTION(BlueprintPure, Category = "EW|Orbital")
	float GetOrbitalCost(EEWOrbital Orbital) const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void AutoSpawn(EEWTeam InTeam);
	float GetBaseX(EEWTeam InTeam) const { return InTeam == EEWTeam::Cyborg ? PlayerBaseX : EnemyBaseX; }

	/** Populate the unit catalogs from the generated sprite roster (20 cyborg + 20
	 *  insect classes, each with a distinct genome archetype) unless a designer has
	 *  already supplied a rich catalog via Blueprint. */
	void BuildDefaultCatalog();

	bool bMatchOver = false;
	bool bPlayerWon = false;
	float CyborgTimer = 0.f;
	float InsectTimer = 0.f;
	FRandomStream Rng;

	/** Recent orbital impacts the HUD animates; aged out in Tick. */
	TArray<FEWStrike> RecentStrikes;

	// ---- Evolution runtime state --------------------------------------------
	FEWChampion CyborgChamp;
	FEWChampion InsectChamp;
	int32 CyborgGeneration = 0;
	int32 InsectGeneration = 0;
	float CyborgSeedChance = 0.f;
	float InsectSeedChance = 0.f;
	int32 CyborgStrains = 0;
	int32 InsectStrains = 0;

	/** Round-robin file cursors so consecutive ground spawns spread across files. */
	int32 CyborgLaneCursor = 0;
	int32 InsectLaneCursor = 0;

	/** Assign a depth file + compute the spawn Y for a fresh unit of InTeam. Aerials
	 *  always take the centre file. Returns the world Y; writes the file into OutLane. */
	float AssignFormationY(EEWTeam InTeam, EEWRole InRole, int32& OutLane);

	/** Damage every enemy unit (of EnemyTeam) within Radius of CenterX. Returns count hit. */
	int32 DamageEnemiesInRange(EEWTeam EnemyTeam, float CenterX, float Radius, float Damage);

	/** Lazily seed default orbital costs/cooldowns if the arrays are unset. */
	void EnsureOrbitalDefaults();

	/** Spawn the layered, per-orbital impact spectacle (rings + particle bursts). */
	void SpawnOrbitalVFX(EEWOrbital Orbital, float WorldX, float Radius, const FLinearColor& Color);

	/** Force AEWHUD onto local player controllers (guards against stale BP HUDClass). */
	void EnsurePlayerHUD();

	/** Per-orbital remaining cooldown, ticked down in Tick. */
	TArray<float> OrbitalCdRemaining;

	/** Mutation swing for a team this generation (decays from MutationAmount toward MutationFloor). */
	float EffectiveMutation(EEWTeam InTeam) const;
	/** Spawn one unit from a champion lineage (mutated offspring of the team's best). */
	AEWUnit* SpawnChampionOffspring(EEWTeam InTeam, const FEWChampion& Champ);
	/** Promote a strong champion into a permanent new catalog class (specialization). */
	void TrySpecialize(EEWTeam InTeam, const FEWChampion& Champ);

	UPROPERTY(Transient)
	TArray<TObjectPtr<AEWUnit>> CyborgUnits;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AEWUnit>> InsectUnits;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AEWBuilding>> Buildings;
};
