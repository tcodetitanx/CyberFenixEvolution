// EvolutionWar core gameplay types.
#pragma once

#include "CoreMinimal.h"
#include "EWTypes.generated.h"

UENUM(BlueprintType)
enum class EEWTeam : uint8
{
	Cyborg	UMETA(DisplayName = "Cyborg"),
	Insect	UMETA(DisplayName = "Insect")
};

UENUM(BlueprintType)
enum class EEWBuildingType : uint8
{
	Base	UMETA(DisplayName = "Base"),
	Hive	UMETA(DisplayName = "Hive"),
	Depot	UMETA(DisplayName = "Generation Depot")
};

/** Player orbital support powers, targeted at a point on the lane. */
UENUM(BlueprintType)
enum class EEWOrbital : uint8
{
	Cannon	UMETA(DisplayName = "Cannon"),	// focused burst, small radius
	Bomb	UMETA(DisplayName = "Bomb"),	// wide area blast
	Laser	UMETA(DisplayName = "Laser"),	// narrow piercing column, heavy damage
	Shield	UMETA(DisplayName = "Shield"),	// heals friendlies in an area
	Count	UMETA(Hidden)
};

/**
 * Battlefield role. Drives formation behaviour, attack VFX and targeting:
 *  - Melee/Tank queue into ranks and hold a front line.
 *  - Ranged hold at distance and fire bolts (the only thing that can hit air).
 *  - Support hangs in the pack and pulses heals to nearby allies.
 *  - Flanker ignores the queue and rushes past the front toward the backline.
 *  - Aerial flies above the lane; only Ranged/Aerial (and orbitals) can hit it.
 * Role lives on the genome so it survives champion breeding / specialization.
 */
UENUM(BlueprintType)
enum class EEWRole : uint8
{
	Melee	UMETA(DisplayName = "Melee"),
	Ranged	UMETA(DisplayName = "Ranged"),
	Support	UMETA(DisplayName = "Support"),
	Flanker	UMETA(DisplayName = "Flanker"),
	Aerial	UMETA(DisplayName = "Aerial"),
	Tank	UMETA(DisplayName = "Tank")
};

/**
 * Variant rarity tier. Replaces per-unit hue scrambling: instead of recoloring a
 * unit's sprite, its strength is read off as a faint colored outline glow so the
 * artwork stays true and rarity is legible at a glance.
 */
UENUM(BlueprintType)
enum class EEWRarity : uint8
{
	Common		UMETA(DisplayName = "Common"),
	Uncommon	UMETA(DisplayName = "Uncommon"),
	Rare		UMETA(DisplayName = "Rare"),
	Epic		UMETA(DisplayName = "Epic"),
	Legendary	UMETA(DisplayName = "Legendary"),
	Mythic		UMETA(DisplayName = "Mythic")
};

/** True for roles that fight in the air (spawn at altitude, special targeting). */
FORCEINLINE bool IsAerialRole(EEWRole R) { return R == EEWRole::Aerial; }

/** True for roles whose attacks can strike aerial targets (ranged bolts + flyers). */
FORCEINLINE bool RoleCanHitAir(EEWRole R) { return R == EEWRole::Ranged || R == EEWRole::Aerial; }

/** True for roles that fire a travelling projectile rather than a melee strike. */
FORCEINLINE bool RoleIsShooter(EEWRole R) { return R == EEWRole::Ranged || R == EEWRole::Aerial; }

/** True for roles that queue into a holding front line (others bypass / hold back). */
FORCEINLINE bool RoleQueues(EEWRole R) { return R == EEWRole::Melee || R == EEWRole::Tank || R == EEWRole::Support; }

/**
 * Genetic blueprint for a unit. Every spawned unit carries a genome that is a
 * (possibly mutated) copy of a class base genome. Traits drive combat + visuals.
 */
USTRUCT(BlueprintType)
struct FUnitGenome
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genome")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genome")
	float MoveSpeed = 220.f;        // uu/s along the lane

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genome")
	float Damage = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genome")
	float AttackInterval = 1.0f;    // seconds between attacks

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genome")
	float AttackRange = 130.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genome")
	float AggroRange = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genome")
	float Armor = 0.f;              // flat damage reduction per hit

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genome")
	float Scale = 1.0f;             // visual + roughly hit-size

	/** Mutation-driven accent color, multiplied into the sprite's emissive glow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genome")
	FLinearColor Tint = FLinearColor::White;

	/** Battlefield role: drives formation, attack style and targeting. Inherited
	 *  unchanged through breeding so a lineage keeps its tactical identity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Genome")
	EEWRole Role = EEWRole::Melee;

	/** A single scalar describing how "strong" this genome is (for champion ranking). */
	float Power() const
	{
		const float Dps = Damage / FMath::Max(0.1f, AttackInterval);
		return MaxHealth * 0.5f + Dps * 6.f + MoveSpeed * 0.15f + Armor * 4.f + AttackRange * 0.1f;
	}

	/** Returns a mutated copy. Amount ~0.0-1.0 = max +/- fractional swing per trait. */
	FUnitGenome Mutated(float Amount, FRandomStream& Rng) const
	{
		auto Vary = [&](float V, float Lo, float Hi)
		{
			const float Factor = 1.f + Rng.FRandRange(-Amount, Amount);
			return FMath::Clamp(V * Factor, Lo, Hi);
		};

		FUnitGenome G = *this;
		G.MaxHealth      = Vary(MaxHealth, 10.f, 100000.f);
		G.MoveSpeed      = Vary(MoveSpeed, 40.f, 1200.f);
		G.Damage         = Vary(Damage, 1.f, 100000.f);
		G.AttackInterval = Vary(AttackInterval, 0.15f, 6.f);
		G.AttackRange    = Vary(AttackRange, 60.f, 2500.f);
		G.AggroRange     = FMath::Max(G.AttackRange + 50.f, Vary(AggroRange, 100.f, 6000.f));
		G.Armor          = Vary(Armor + 1.f, 1.f, 100000.f) - 1.f;
		G.Scale          = Vary(Scale, 0.6f, 2.2f);

		// Tint is deliberately left untouched: a variant's strength now reads as a
		// rarity outline (see EEWRarity), not a scrambled hue, so the sprite keeps
		// its authored team colour and only the power changes between generations.
		return G;
	}
};

/** Map a genome's raw Power() onto a rarity tier. Stronger variants read rarer, so
 *  a beefy mutation visibly earns a brighter outline. Thresholds span the roster's
 *  natural ~140-600 power range with headroom for mutation swings. */
FORCEINLINE EEWRarity RarityFromPower(float P)
{
	if (P >= 480.f) { return EEWRarity::Mythic; }
	if (P >= 390.f) { return EEWRarity::Legendary; }
	if (P >= 310.f) { return EEWRarity::Epic; }
	if (P >= 240.f) { return EEWRarity::Rare; }
	if (P >= 175.f) { return EEWRarity::Uncommon; }
	return EEWRarity::Common;
}

FORCEINLINE EEWRarity RarityFromGenome(const FUnitGenome& G) { return RarityFromPower(G.Power()); }

/** Faint outline colour per rarity tier (classic loot palette). */
FORCEINLINE FLinearColor RarityColor(EEWRarity R)
{
	switch (R)
	{
	case EEWRarity::Uncommon:  return FLinearColor(0.30f, 1.00f, 0.38f, 1.f); // green
	case EEWRarity::Rare:      return FLinearColor(0.28f, 0.58f, 1.00f, 1.f); // blue
	case EEWRarity::Epic:      return FLinearColor(0.72f, 0.36f, 1.00f, 1.f); // purple
	case EEWRarity::Legendary: return FLinearColor(1.00f, 0.70f, 0.16f, 1.f); // gold
	case EEWRarity::Mythic:    return FLinearColor(1.00f, 0.26f, 0.42f, 1.f); // red/pink
	case EEWRarity::Common:
	default:                   return FLinearColor(0.62f, 0.66f, 0.72f, 1.f); // faint grey
	}
}

/** Outline glow strength per rarity: barely-there for common, brightest for mythic. */
FORCEINLINE float RarityHaloOpacity(EEWRarity R)
{
	switch (R)
	{
	case EEWRarity::Uncommon:  return 0.34f;
	case EEWRarity::Rare:      return 0.42f;
	case EEWRarity::Epic:      return 0.50f;
	case EEWRarity::Legendary: return 0.58f;
	case EEWRarity::Mythic:    return 0.66f;
	case EEWRarity::Common:
	default:                   return 0.16f;
	}
}

/** Design-time definition of a unit class: which sprite + base genome to spawn. */
USTRUCT(BlueprintType)
struct FUnitClassDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class")
	FName ClassName = TEXT("Grunt");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class")
	TObjectPtr<UTexture2D> Sprite = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class")
	FUnitGenome BaseGenome;

	/** World height in unreal units the sprite should occupy at Scale 1.0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class")
	float SpriteWorldHeight = 320.f;

	/** Resource cost when the player manually deploys this class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class")
	float DeployCost = 25.f;
};
