#include "EWGameMode.h"
#include "EWUnit.h"
#include "EWBuilding.h"
#include "EWCameraPawn.h"
#include "EWHUD.h"
#include "EWParticleBurst.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

AEWGameMode::AEWGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AEWCameraPawn::StaticClass();
	HUDClass = AEWHUD::StaticClass();
}

void AEWGameMode::BeginPlay()
{
	Super::BeginPlay();
	Rng.Initialize(FPlatformTime::Cycles());
	CyborgTimer = 1.0f;
	InsectTimer = 1.0f;
	bMatchOver = false;
	BuildDefaultCatalog();
	EnsureOrbitalDefaults();
	EnsurePlayerHUD();
}

namespace
{
	/** Compact archetype row used to seed a unit class from a generated sprite. */
	struct FClassSeed
	{
		const TCHAR* SpritePath;
		const TCHAR* Name;
		float HP, Speed, Damage, Interval, Range, Armor, Scale, Height, Cost;
		FLinearColor Tint;
		EEWRole Role;
	};

	FUnitClassDef SeedToDef(const FClassSeed& S)
	{
		FUnitClassDef Def;
		Def.ClassName = FName(S.Name);
		Def.Sprite = LoadObject<UTexture2D>(nullptr, S.SpritePath);
		Def.SpriteWorldHeight = S.Height;
		Def.DeployCost = S.Cost;

		FUnitGenome G;
		G.MaxHealth = S.HP;
		G.MoveSpeed = S.Speed;
		G.Damage = S.Damage;
		G.AttackInterval = S.Interval;
		G.AttackRange = S.Range;
		G.AggroRange = FMath::Max(700.f, S.Range + 200.f);
		G.Armor = S.Armor;
		G.Scale = S.Scale;
		G.Tint = S.Tint;
		G.Role = S.Role;
		Def.BaseGenome = G;
		return Def;
	}

	// Cool neon tints for the cyborgs, warm/organic tints for the insects.
	const FLinearColor TCyan(0.45f, 0.95f, 1.15f, 1.f);
	const FLinearColor TBlue(0.45f, 0.65f, 1.25f, 1.f);
	const FLinearColor TTeal(0.40f, 1.05f, 0.85f, 1.f);
	const FLinearColor TIce(0.80f, 0.95f, 1.20f, 1.f);
	const FLinearColor TViolet(0.75f, 0.55f, 1.20f, 1.f);
	const FLinearColor TAmber(1.20f, 0.75f, 0.30f, 1.f);
	const FLinearColor TRed(1.25f, 0.45f, 0.35f, 1.f);
	const FLinearColor TGreen(0.65f, 1.10f, 0.45f, 1.f);
	const FLinearColor TOrange(1.25f, 0.60f, 0.25f, 1.f);
	const FLinearColor TLime(0.85f, 1.15f, 0.40f, 1.f);

	// ---- Cyborg roster ----------------------------------------------------------
	const FClassSeed GCyborgSeeds[] = {
		{ TEXT("/Game/Art/Source/cyborg_sentinel.cyborg_sentinel"),   TEXT("Sentinel"),   140, 200, 14, 1.0f, 150, 3, 1.05f, 300, 28, TCyan,   EEWRole::Melee },
		{ TEXT("/Game/Art/Source/cyborg_ranger.cyborg_ranger"),       TEXT("Ranger"),      90, 240, 16, 0.8f, 420, 0, 0.95f, 290, 30, TCyan,   EEWRole::Ranged },
		{ TEXT("/Game/Art/Source/cyborg_lancer.cyborg_lancer"),       TEXT("Lancer"),     110, 320, 20, 1.1f, 160, 1, 1.00f, 300, 30, TBlue,   EEWRole::Flanker },
		{ TEXT("/Game/Art/Source/cyborg_artillery.cyborg_artillery"), TEXT("Artillery"),  120, 120, 45, 2.2f, 700, 2, 1.25f, 340, 55, TViolet, EEWRole::Ranged },
		{ TEXT("/Game/Art/Source/cyborg_medic.cyborg_medic"),         TEXT("Medic"),      130, 230,  7, 1.2f, 220, 2, 1.00f, 295, 26, TTeal,   EEWRole::Support },
		{ TEXT("/Game/Art/Source/cyborg_assassin.cyborg_assassin"),   TEXT("Assassin"),    70, 380, 34, 0.7f, 160, 0, 0.90f, 280, 34, TViolet, EEWRole::Flanker },
		{ TEXT("/Game/Art/Source/cyborg_juggernaut.cyborg_juggernaut"),TEXT("Juggernaut"),320, 140, 22, 1.6f, 170, 8, 1.50f, 380, 60, TBlue,   EEWRole::Tank },
		{ TEXT("/Game/Art/Source/cyborg_striker.cyborg_striker"),     TEXT("Striker"),    150, 260, 22, 1.0f, 190, 3, 1.05f, 305, 34, TCyan,   EEWRole::Melee },
		{ TEXT("/Game/Art/Source/cyborg_engineer.cyborg_engineer"),   TEXT("Engineer"),    90, 220, 10, 1.1f, 200, 1, 0.90f, 280, 20, TTeal,   EEWRole::Support },
		{ TEXT("/Game/Art/Source/cyborg_vanguard.cyborg_vanguard"),   TEXT("Vanguard"),   240, 180, 18, 1.3f, 170, 6, 1.30f, 350, 46, TIce,    EEWRole::Tank },
		{ TEXT("/Game/Art/Source/cyborg_railgunner.cyborg_railgunner"),TEXT("Railgunner"), 85, 170, 60, 2.6f, 950, 0, 1.05f, 305, 58, TIce,    EEWRole::Ranged },
		{ TEXT("/Game/Art/Source/cyborg_pyro.cyborg_pyro"),           TEXT("Pyro"),       120, 230, 14, 0.5f, 230, 1, 1.00f, 300, 36, TOrange, EEWRole::Melee },
		{ TEXT("/Game/Art/Source/cyborg_orbdrone.cyborg_orbdrone"),   TEXT("OrbDrone"),    80, 420, 12, 0.8f, 320, 0, 0.85f, 270, 30, TCyan,   EEWRole::Aerial },
		{ TEXT("/Game/Art/Source/cyborg_warden.cyborg_warden"),       TEXT("Warden"),     260, 160, 16, 1.4f, 180, 7, 1.30f, 350, 44, TBlue,   EEWRole::Tank },
		{ TEXT("/Game/Art/Source/cyborg_berserker.cyborg_berserker"), TEXT("Berserker"),  130, 330, 30, 0.9f, 160, 1, 1.05f, 305, 38, TRed,    EEWRole::Flanker },
		{ TEXT("/Game/Art/Source/cyborg_marksman.cyborg_marksman"),   TEXT("Marksman"),    95, 210, 26, 1.3f, 580, 0, 1.00f, 300, 40, TIce,    EEWRole::Ranged },
		{ TEXT("/Game/Art/Source/cyborg_titan.cyborg_titan"),         TEXT("Titan"),      520, 120, 40, 1.8f, 210, 12,1.90f, 460, 90, TViolet, EEWRole::Tank },
		{ TEXT("/Game/Art/Source/cyborg_scout.cyborg_scout"),         TEXT("Scout"),       70, 460,  8, 0.7f, 150, 0, 0.80f, 260, 18, TTeal,   EEWRole::Aerial },
		{ TEXT("/Game/Art/Source/cyborg_guardian.cyborg_guardian"),   TEXT("Guardian"),   200, 190, 18, 1.2f, 180, 5, 1.20f, 330, 40, TIce,    EEWRole::Melee },
		{ TEXT("/Game/Art/Source/cyborg_destroyer.cyborg_destroyer"), TEXT("Destroyer"),  200, 170, 38, 1.4f, 300, 4, 1.35f, 360, 62, TViolet, EEWRole::Ranged },
	};

	// ---- Insect roster ----------------------------------------------------------
	const FClassSeed GInsectSeeds[] = {
		{ TEXT("/Game/Art/Source/insect_soldier.insect_soldier"),     TEXT("Soldier"),    130, 230, 14, 1.0f, 130, 2, 1.00f, 300, 25, TAmber,  EEWRole::Melee },
		{ TEXT("/Game/Art/Source/insect_spitter.insect_spitter"),     TEXT("Spitter"),     90, 200, 16, 0.9f, 360, 0, 0.95f, 290, 25, TGreen,  EEWRole::Ranged },
		{ TEXT("/Game/Art/Source/insect_burrower.insect_burrower"),   TEXT("Burrower"),   180, 170, 16, 1.2f, 120, 5, 1.15f, 320, 25, TOrange, EEWRole::Flanker },
		{ TEXT("/Game/Art/Source/insect_queen.insect_queen"),         TEXT("Queen"),      480, 130, 34, 1.6f, 150, 10,1.80f, 450, 25, TRed,    EEWRole::Tank },
		{ TEXT("/Game/Art/Source/insect_workerbug.insect_workerbug"), TEXT("Worker"),      80, 240,  8, 1.1f, 110, 0, 0.85f, 270, 25, TLime,   EEWRole::Support },
		{ TEXT("/Game/Art/Source/insect_mantis.insect_mantis"),       TEXT("Mantis"),     120, 340, 26, 0.9f, 140, 1, 1.05f, 305, 25, TGreen,  EEWRole::Flanker },
		{ TEXT("/Game/Art/Source/insect_scorpion.insect_scorpion"),   TEXT("Scorpion"),   160, 220, 24, 1.1f, 150, 4, 1.10f, 315, 25, TAmber,  EEWRole::Melee },
		{ TEXT("/Game/Art/Source/insect_wasp.insect_wasp"),           TEXT("Wasp"),        80, 430, 12, 0.8f, 150, 0, 0.85f, 270, 25, TLime,   EEWRole::Aerial },
		{ TEXT("/Game/Art/Source/insect_carapace.insect_carapace"),   TEXT("Carapace"),   300, 150, 18, 1.4f, 130, 9, 1.40f, 360, 25, TOrange, EEWRole::Tank },
		{ TEXT("/Game/Art/Source/insect_swarmer.insect_swarmer"),     TEXT("Swarmer"),     60, 300,  7, 0.7f, 110, 0, 0.75f, 250, 25, TGreen,  EEWRole::Melee },
		{ TEXT("/Game/Art/Source/insect_weaver.insect_weaver"),       TEXT("Weaver"),     120, 210, 12, 1.0f, 160, 2, 1.00f, 300, 25, TLime,   EEWRole::Support },
		{ TEXT("/Game/Art/Source/insect_charger.insect_charger"),     TEXT("Charger"),    150, 380, 22, 1.0f, 140, 1, 1.10f, 315, 25, TRed,    EEWRole::Flanker },
		{ TEXT("/Game/Art/Source/insect_locust.insect_locust"),       TEXT("Locust"),      70, 360,  9, 0.8f, 120, 0, 0.80f, 265, 25, TLime,   EEWRole::Aerial },
		{ TEXT("/Game/Art/Source/insect_centipede.insect_centipede"), TEXT("Centipede"),  220, 200, 20, 1.1f, 160, 4, 1.30f, 350, 25, TAmber,  EEWRole::Tank },
		{ TEXT("/Game/Art/Source/insect_hornet.insect_hornet"),       TEXT("Hornet"),      90, 400, 18, 0.85f,160, 0, 0.90f, 285, 25, TOrange, EEWRole::Aerial },
		{ TEXT("/Game/Art/Source/insect_brute.insect_brute"),         TEXT("Brute"),      260, 160, 30, 1.5f, 150, 6, 1.45f, 370, 25, TRed,    EEWRole::Tank },
		{ TEXT("/Game/Art/Source/insect_widow.insect_widow"),         TEXT("Widow"),       80, 360, 36, 0.8f, 150, 0, 0.95f, 290, 25, TViolet, EEWRole::Flanker },
		{ TEXT("/Game/Art/Source/insect_ravager.insect_ravager"),     TEXT("Ravager"),    240, 180, 34, 1.4f, 150, 4, 1.35f, 360, 25, TOrange, EEWRole::Melee },
		{ TEXT("/Game/Art/Source/insect_moth.insect_moth"),           TEXT("Moth"),       100, 320, 10, 1.0f, 180, 1, 1.00f, 300, 25, TLime,   EEWRole::Aerial },
		{ TEXT("/Game/Art/Source/insect_behemoth.insect_behemoth"),   TEXT("Behemoth"),   600, 110, 44, 1.9f, 170, 14,2.00f, 470, 25, TRed,    EEWRole::Tank },
	};
}

void AEWGameMode::BuildDefaultCatalog()
{
	// Respect a designer-authored Blueprint roster if one is already richly populated;
	// otherwise seed the full generated sprite roster so every unit type is in play.
	if (CyborgClasses.Num() < 8)
	{
		CyborgClasses.Reset();
		for (const FClassSeed& S : GCyborgSeeds)
		{
			CyborgClasses.Add(SeedToDef(S));
		}
	}
	if (InsectClasses.Num() < 8)
	{
		InsectClasses.Reset();
		for (const FClassSeed& S : GInsectSeeds)
		{
			InsectClasses.Add(SeedToDef(S));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[EvolutionWar] Catalog seeded: %d cyborg classes, %d insect classes"),
		CyborgClasses.Num(), InsectClasses.Num());
}

void AEWGameMode::EnsurePlayerHUD()
{
	// A Blueprint GameMode child created before AEWHUD existed can serialize a
	// stale HUDClass, leaving the player with a do-nothing base AHUD. Force our
	// Canvas HUD onto every local controller so the UI actually draws.
	UWorld* World = GetWorld();
	if (!World) { return; }
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (!PC->GetHUD() || !PC->GetHUD()->IsA(AEWHUD::StaticClass()))
			{
				PC->ClientSetHUD(AEWHUD::StaticClass());
			}
		}
	}
}

void AEWGameMode::EnsureOrbitalDefaults()
{
	const int32 N = (int32)EEWOrbital::Count;
	if (OrbitalCosts.Num() != N)
	{
		// Cannon, Bomb, Laser, Shield
		OrbitalCosts = { 40.f, 70.f, 90.f, 55.f };
	}
	if (OrbitalCooldowns.Num() != N)
	{
		OrbitalCooldowns = { 4.f, 9.f, 7.f, 8.f };
	}
	OrbitalCdRemaining.SetNumZeroed(N);
}

void AEWGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bMatchOver)
	{
		return;
	}

	Biomass += BiomassPerSecond * DeltaSeconds;

	for (float& Cd : OrbitalCdRemaining)
	{
		if (Cd > 0.f) { Cd = FMath::Max(0.f, Cd - DeltaSeconds); }
	}

	// Age out orbital impact rings.
	for (int32 i = RecentStrikes.Num() - 1; i >= 0; --i)
	{
		RecentStrikes[i].Age += DeltaSeconds;
		if (RecentStrikes[i].Age >= RecentStrikes[i].Life)
		{
			RecentStrikes.RemoveAtSwap(i);
		}
	}

	CyborgTimer -= DeltaSeconds;
	if (CyborgTimer <= 0.f)
	{
		CyborgTimer = CyborgSpawnInterval;
		AutoSpawn(EEWTeam::Cyborg);
	}

	InsectTimer -= DeltaSeconds;
	if (InsectTimer <= 0.f)
	{
		InsectTimer = InsectSpawnInterval;
		AutoSpawn(EEWTeam::Insect);
	}
}

void AEWGameMode::AutoSpawn(EEWTeam InTeam)
{
	const TArray<FUnitClassDef>& Catalog = (InTeam == EEWTeam::Cyborg) ? CyborgClasses : InsectClasses;
	if (Catalog.Num() == 0)
	{
		return;
	}
	const TArray<TObjectPtr<AEWUnit>>& Live = (InTeam == EEWTeam::Cyborg) ? CyborgUnits : InsectUnits;
	if (Live.Num() >= MaxUnitsPerTeam)
	{
		return;
	}

	// Each spawn cycle is a new "generation"; mutation cools down as it climbs.
	int32& Gen = (InTeam == EEWTeam::Cyborg) ? CyborgGeneration : InsectGeneration;
	++Gen;

	// Evolutionary pressure: once a champion exists, an increasing share of new
	// units are bred from it instead of the base catalog (winning traits spread).
	const FEWChampion& Champ = (InTeam == EEWTeam::Cyborg) ? CyborgChamp : InsectChamp;
	const float SeedChance = (InTeam == EEWTeam::Cyborg) ? CyborgSeedChance : InsectSeedChance;
	if (Champ.bValid && Rng.FRand() < SeedChance)
	{
		SpawnChampionOffspring(InTeam, Champ);
		return;
	}

	const int32 Idx = Rng.RandRange(0, Catalog.Num() - 1);
	SpawnUnitOfClass(InTeam, Catalog[Idx], true);
}

float AEWGameMode::AssignFormationY(EEWTeam InTeam, EEWRole InRole, int32& OutLane)
{
	const int32 Files = FMath::Max(1, GroundFiles);
	int32 Lane;
	if (IsAerialRole(InRole))
	{
		// Flyers ride the centre file so they sweep over the melee scrum.
		Lane = Files / 2;
	}
	else
	{
		int32& Cursor = (InTeam == EEWTeam::Cyborg) ? CyborgLaneCursor : InsectLaneCursor;
		Lane = Cursor % Files;
		++Cursor;
	}
	OutLane = Lane;

	// Centre the files around LaneY: file i sits at (i - (Files-1)/2) * spacing.
	const float Centered = (float)Lane - 0.5f * (float)(Files - 1);
	const float Jitter = Rng.FRandRange(-FileSpacing * 0.18f, FileSpacing * 0.18f);
	return LaneY + Centered * FileSpacing + Jitter;
}

float AEWGameMode::EffectiveMutation(EEWTeam InTeam) const
{
	const int32 Gen = (InTeam == EEWTeam::Cyborg) ? CyborgGeneration : InsectGeneration;
	return FMath::Max(MutationFloor, MutationAmount * FMath::Pow(MutationDecayPerGen, (float)Gen));
}

AEWUnit* AEWGameMode::SpawnUnitOfClass(EEWTeam InTeam, const FUnitClassDef& Def, bool bMutate)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FUnitGenome Genome = bMutate ? Def.BaseGenome.Mutated(EffectiveMutation(InTeam), Rng) : Def.BaseGenome;

	const float SpawnOffset = (InTeam == EEWTeam::Cyborg) ? 600.f : -600.f;
	const float X = GetBaseX(InTeam) + SpawnOffset;
	int32 Lane = 0;
	const float Y = AssignFormationY(InTeam, Genome.Role, Lane);
	const FTransform Xform(FRotator::ZeroRotator, FVector(X, Y, GroundZ));

	AEWUnit* Unit = World->SpawnActorDeferred<AEWUnit>(AEWUnit::StaticClass(), Xform);
	if (!Unit)
	{
		return nullptr;
	}
	Unit->SetDepthLane(Lane);
	Unit->InitUnit(InTeam, Genome, Def.Sprite, Def.SpriteWorldHeight);
	Unit->FinishSpawning(Xform);
	return Unit;
}

AEWUnit* AEWGameMode::SpawnChampionOffspring(EEWTeam InTeam, const FEWChampion& Champ)
{
	UWorld* World = GetWorld();
	if (!World || !Champ.bValid)
	{
		return nullptr;
	}

	// Offspring refine the champion rather than scrambling it: a tighter mutation
	// swing so the lineage converges on what already works.
	const FUnitGenome Genome = Champ.Genome.Mutated(EffectiveMutation(InTeam) * 0.6f, Rng);

	const float SpawnOffset = (InTeam == EEWTeam::Cyborg) ? 600.f : -600.f;
	const float X = GetBaseX(InTeam) + SpawnOffset;
	int32 Lane = 0;
	const float Y = AssignFormationY(InTeam, Genome.Role, Lane);
	const FTransform Xform(FRotator::ZeroRotator, FVector(X, Y, GroundZ));

	AEWUnit* Unit = World->SpawnActorDeferred<AEWUnit>(AEWUnit::StaticClass(), Xform);
	if (!Unit)
	{
		return nullptr;
	}
	Unit->SetDepthLane(Lane);
	Unit->InitUnit(InTeam, Genome, Champ.Sprite, Champ.SpriteWorldHeight);
	Unit->FinishSpawning(Xform);
	return Unit;
}

void AEWGameMode::ReportFitness(EEWTeam InTeam, const FUnitGenome& InGenome, UTexture2D* InSprite, float InSpriteWorldHeight, float InFitness)
{
	FEWChampion& Champ = (InTeam == EEWTeam::Cyborg) ? CyborgChamp : InsectChamp;
	if (Champ.bValid && InFitness <= Champ.Fitness)
	{
		return; // not an improvement on the reigning champion
	}

	Champ.Genome = InGenome;
	Champ.Sprite = InSprite;
	Champ.SpriteWorldHeight = InSpriteWorldHeight;
	Champ.Fitness = InFitness;
	Champ.bValid = true;

	// Each new champion tightens this team's grip on the gene pool.
	float& SeedChance = (InTeam == EEWTeam::Cyborg) ? CyborgSeedChance : InsectSeedChance;
	SeedChance = FMath::Min(MaxChampionSeedChance, SeedChance + ChampionSeedChanceStep);

	UE_LOG(LogTemp, Log, TEXT("[EvolutionWar] New %s champion: fitness=%.0f seedChance=%.2f gen=%d"),
		(InTeam == EEWTeam::Cyborg) ? TEXT("Cyborg") : TEXT("Insect"), InFitness, SeedChance,
		(InTeam == EEWTeam::Cyborg) ? CyborgGeneration : InsectGeneration);

	TrySpecialize(InTeam, Champ);
}

void AEWGameMode::TrySpecialize(EEWTeam InTeam, const FEWChampion& Champ)
{
	int32& Strains = (InTeam == EEWTeam::Cyborg) ? CyborgStrains : InsectStrains;
	if (Strains >= MaxStrainsPerTeam)
	{
		return;
	}
	// Each successive strain must clear a higher bar than the last.
	const float Threshold = SpecializeFitnessThreshold * FMath::Pow(1.6f, (float)Strains);
	if (Champ.Fitness < Threshold)
	{
		return;
	}

	TArray<FUnitClassDef>& Catalog = (InTeam == EEWTeam::Cyborg) ? CyborgClasses : InsectClasses;
	FUnitClassDef Strain;
	Strain.ClassName = FName(*FString::Printf(TEXT("Strain-%s%d"),
		(InTeam == EEWTeam::Cyborg) ? TEXT("C") : TEXT("I"), Strains + 1));
	Strain.Sprite = Champ.Sprite;
	Strain.BaseGenome = Champ.Genome;
	Strain.SpriteWorldHeight = Champ.SpriteWorldHeight;
	Strain.DeployCost = 25.f + Champ.Genome.Power() * 0.05f;
	Catalog.Add(Strain);
	++Strains;

	UE_LOG(LogTemp, Warning, TEXT("[EvolutionWar] %s evolved a new strain '%s' (catalog now %d classes)"),
		(InTeam == EEWTeam::Cyborg) ? TEXT("Cyborg") : TEXT("Insect"), *Strain.ClassName.ToString(), Catalog.Num());
}

AEWUnit* AEWGameMode::DeployPlayerUnit(int32 ClassIndex, float WorldX)
{
	if (!CyborgClasses.IsValidIndex(ClassIndex))
	{
		return nullptr;
	}
	const FUnitClassDef& Def = CyborgClasses[ClassIndex];
	if (Biomass < Def.DeployCost)
	{
		return nullptr;
	}
	Biomass -= Def.DeployCost;

	const FUnitGenome Genome = Def.BaseGenome.Mutated(MutationAmount * 0.5f, Rng);
	int32 Lane = 0;
	const float Y = AssignFormationY(EEWTeam::Cyborg, Genome.Role, Lane);
	const FTransform Xform(FRotator::ZeroRotator, FVector(WorldX, Y, GroundZ));
	if (UWorld* World = GetWorld())
	{
		AEWUnit* Unit = World->SpawnActorDeferred<AEWUnit>(AEWUnit::StaticClass(), Xform);
		if (Unit)
		{
			Unit->SetDepthLane(Lane);
			Unit->InitUnit(EEWTeam::Cyborg, Genome, Def.Sprite, Def.SpriteWorldHeight);
			Unit->FinishSpawning(Xform);
			return Unit;
		}
	}
	return nullptr;
}

float AEWGameMode::GetOrbitalCooldownRemaining(EEWOrbital Orbital) const
{
	const int32 Idx = (int32)Orbital;
	return OrbitalCdRemaining.IsValidIndex(Idx) ? OrbitalCdRemaining[Idx] : 0.f;
}

float AEWGameMode::GetOrbitalCost(EEWOrbital Orbital) const
{
	const int32 Idx = (int32)Orbital;
	return OrbitalCosts.IsValidIndex(Idx) ? OrbitalCosts[Idx] : 0.f;
}

int32 AEWGameMode::DamageEnemiesInRange(EEWTeam EnemyTeam, float CenterX, float Radius, float Damage)
{
	const TArray<TObjectPtr<AEWUnit>>& Enemies = (EnemyTeam == EEWTeam::Cyborg) ? CyborgUnits : InsectUnits;
	// Iterate a copy of the pointers: ReceiveDamage -> Die can mutate the live array.
	TArray<TObjectPtr<AEWUnit>> Snapshot = Enemies;
	int32 Hits = 0;
	for (const TObjectPtr<AEWUnit>& E : Snapshot)
	{
		if (!E || !E->IsAlive()) { continue; }
		if (FMath::Abs(E->GetActorLocation().X - CenterX) <= Radius)
		{
			E->ReceiveDamage(Damage, nullptr);
			++Hits;
		}
	}
	return Hits;
}

void AEWGameMode::HealFriendliesInRange(EEWTeam FriendTeam, float CenterX, float Radius, float HealAmount)
{
	const TArray<TObjectPtr<AEWUnit>>& Friends = (FriendTeam == EEWTeam::Cyborg) ? CyborgUnits : InsectUnits;
	for (const TObjectPtr<AEWUnit>& F : Friends)
	{
		if (!F || !F->IsAlive()) { continue; }
		if (FMath::Abs(F->GetActorLocation().X - CenterX) <= Radius)
		{
			F->Heal(HealAmount);
		}
	}
}

bool AEWGameMode::TriggerOrbital(EEWOrbital Orbital, float WorldX)
{
	if (bMatchOver) { return false; }
	EnsureOrbitalDefaults();

	const int32 Idx = (int32)Orbital;
	if (!OrbitalCdRemaining.IsValidIndex(Idx)) { return false; }
	if (OrbitalCdRemaining[Idx] > 0.f) { return false; }

	const float Cost = OrbitalCosts.IsValidIndex(Idx) ? OrbitalCosts[Idx] : 0.f;
	if (Biomass < Cost) { return false; }

	// All orbitals are fired by the player (Cyborg side) against the Insects.
	float Radius = 250.f;
	FLinearColor RingColor = FLinearColor::White;
	switch (Orbital)
	{
	case EEWOrbital::Cannon: // focused burst, small radius
		Radius = 250.f; RingColor = FLinearColor(1.0f, 0.72f, 0.18f, 1.f);
		DamageEnemiesInRange(EEWTeam::Insect, WorldX, Radius, 200.f);
		break;
	case EEWOrbital::Bomb:   // wide area blast
		Radius = 900.f; RingColor = FLinearColor(1.0f, 0.25f, 0.7f, 1.f);
		DamageEnemiesInRange(EEWTeam::Insect, WorldX, Radius, 350.f);
		break;
	case EEWOrbital::Laser:  // narrow piercing column, heavy damage
		Radius = 120.f; RingColor = FLinearColor(0.10f, 0.85f, 1.0f, 1.f);
		DamageEnemiesInRange(EEWTeam::Insect, WorldX, Radius, 600.f);
		break;
	case EEWOrbital::Shield: // heal friendlies in an area
		Radius = 700.f; RingColor = FLinearColor(0.35f, 1.0f, 0.45f, 1.f);
		HealFriendliesInRange(EEWTeam::Cyborg, WorldX, Radius, 150.f);
		break;
	default:
		return false;
	}

	SpawnOrbitalVFX(Orbital, WorldX, Radius, RingColor);

	Biomass -= Cost;
	OrbitalCdRemaining[Idx] = OrbitalCooldowns.IsValidIndex(Idx) ? OrbitalCooldowns[Idx] : 5.f;

	UE_LOG(LogTemp, Log, TEXT("[EvolutionWar] Orbital %d fired at X=%.0f (cost=%.0f)"), Idx, WorldX, Cost);
	return true;
}

void AEWGameMode::SpawnOrbitalVFX(EEWOrbital Orbital, float WorldX, float Radius, const FLinearColor& Color)
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	// Helper to register an expanding HUD ring.
	auto AddRing = [&](float R, float Life, const FLinearColor& C)
	{
		FEWStrike S;
		S.X = WorldX; S.Radius = R; S.Color = C; S.Life = Life;
		RecentStrikes.Add(S);
	};

	switch (Orbital)
	{
	case EEWOrbital::Cannon:
	{
		// A bright golden lance from the sky: a stacked column of sparks slamming
		// down onto a tight ground ring, with a hot core flash at the point of impact.
		AddRing(Radius, 0.65f, Color);
		AddRing(Radius * 0.45f, 0.4f, FLinearColor(1.f, 0.95f, 0.6f, 1.f));
		for (int32 i = 0; i < 5; ++i)
		{
			const float Z = 120.f + i * 220.f;
			AEWParticleBurst::Spawn(World, FVector(WorldX, LaneY, Z), Color, 9, 900.f, 0.45f, 26.f);
		}
		AEWParticleBurst::Spawn(World, FVector(WorldX, LaneY, 70.f),
			FLinearColor(1.f, 0.95f, 0.7f, 1.f), 18, 1500.f, 0.55f, 34.f);
		break;
	}
	case EEWOrbital::Bomb:
	{
		// A huge magenta detonation: nested shockwave rings plus a cluster of
		// secondary blasts marching outward across the whole blast footprint.
		AddRing(Radius, 0.8f, Color);
		AddRing(Radius * 0.66f, 0.6f, FLinearColor(1.f, 0.5f, 0.85f, 1.f));
		AddRing(Radius * 0.33f, 0.45f, FLinearColor(1.f, 0.8f, 0.95f, 1.f));
		AEWParticleBurst::Spawn(World, FVector(WorldX, LaneY, 90.f), Color, 34, 2600.f, 0.7f, 40.f);
		for (int32 i = 0; i < 7; ++i)
		{
			const float Off = Rng.FRandRange(-Radius, Radius);
			const float Z = Rng.FRandRange(60.f, 320.f);
			AEWParticleBurst::Spawn(World, FVector(WorldX + Off, LaneY, Z), Color, 12, 1400.f, 0.6f, 30.f);
		}
		break;
	}
	case EEWOrbital::Laser:
	{
		// A narrow piercing cyan column: a tall, tight pillar of fast vertical sparks.
		AddRing(Radius, 0.4f, Color);
		for (int32 i = 0; i < 8; ++i)
		{
			const float Z = 60.f + i * 260.f;
			AEWParticleBurst::Spawn(World, FVector(WorldX, LaneY, Z), Color, 7, 700.f, 0.35f, 22.f);
		}
		AEWParticleBurst::Spawn(World, FVector(WorldX, LaneY, 60.f),
			FLinearColor(0.6f, 0.97f, 1.f, 1.f), 14, 1200.f, 0.4f, 26.f);
		break;
	}
	case EEWOrbital::Shield:
	{
		// A restorative green aura: a soft dome ring plus slow rising motes
		// sprinkled across the whole protected area.
		AddRing(Radius, 0.9f, Color);
		AddRing(Radius * 0.6f, 0.7f, FLinearColor(0.6f, 1.f, 0.7f, 1.f));
		for (int32 i = 0; i < 10; ++i)
		{
			const float Off = Rng.FRandRange(-Radius, Radius);
			AEWParticleBurst::Spawn(World, FVector(WorldX + Off, LaneY, 50.f), Color, 5, 300.f, 1.1f, 16.f);
		}
		break;
	}
	default:
		AddRing(Radius, 0.7f, Color);
		break;
	}
}

void AEWGameMode::RegisterUnit(AEWUnit* Unit)
{
	if (!Unit) { return; }
	if (Unit->GetTeam() == EEWTeam::Cyborg)
	{
		CyborgUnits.AddUnique(Unit);
	}
	else
	{
		InsectUnits.AddUnique(Unit);
	}
}

void AEWGameMode::UnregisterUnit(AEWUnit* Unit)
{
	if (!Unit) { return; }
	CyborgUnits.Remove(Unit);
	InsectUnits.Remove(Unit);
}

void AEWGameMode::RegisterBuilding(AEWBuilding* Building)
{
	if (Building)
	{
		Buildings.AddUnique(Building);
	}
}

AActor* AEWGameMode::FindNearestEnemy(EEWTeam SeekerTeam, const FVector& Loc, float MaxRange, bool bSeekerCanHitAir) const
{
	const EEWTeam EnemyTeam = (SeekerTeam == EEWTeam::Cyborg) ? EEWTeam::Insect : EEWTeam::Cyborg;
	const TArray<TObjectPtr<AEWUnit>>& Enemies = (EnemyTeam == EEWTeam::Cyborg) ? CyborgUnits : InsectUnits;

	AActor* Best = nullptr;
	float BestDist = MaxRange;
	for (const TObjectPtr<AEWUnit>& E : Enemies)
	{
		if (!E || !E->IsAlive()) { continue; }
		// Ground-bound seekers can't reach flyers; skip aerial enemies for them.
		if (!bSeekerCanHitAir && E->IsAerial()) { continue; }
		const float D = FMath::Abs(E->GetActorLocation().X - Loc.X);
		if (D <= BestDist)
		{
			BestDist = D;
			Best = E;
		}
	}

	// Allow targeting the enemy base when it is within aggro reach.
	for (const TObjectPtr<AEWBuilding>& B : Buildings)
	{
		if (!B || !B->IsAlive() || B->GetTeam() != EnemyTeam) { continue; }
		if (B->GetBuildingType() == EEWBuildingType::Depot) { continue; }
		const float D = FMath::Abs(B->GetActorLocation().X - Loc.X);
		if (D <= BestDist)
		{
			BestDist = D;
			Best = B;
		}
	}
	return Best;
}

bool AEWGameMode::NearestFriendlyAheadX(EEWTeam Team, float MyX, float DirSign, const AEWUnit* Self, float Lookahead, float& OutX) const
{
	const TArray<TObjectPtr<AEWUnit>>& Friends = (Team == EEWTeam::Cyborg) ? CyborgUnits : InsectUnits;
	const int32 MyLane = Self ? Self->GetDepthLane() : 0;
	bool bFound = false;
	float BestAhead = Lookahead;
	for (const TObjectPtr<AEWUnit>& F : Friends)
	{
		if (!F || F == Self || !F->IsAlive()) { continue; }
		// Only queue behind friends in the SAME file, so each file holds independently
		// (=> up to GroundFiles units fight abreast). Flyers never block the queue.
		if (F->GetDepthLane() != MyLane || F->IsAerial()) { continue; }
		const float FX = F->GetActorLocation().X;
		const float Ahead = (FX - MyX) * DirSign; // > 0 means F is ahead toward the enemy.
		if (Ahead <= 0.f || Ahead > BestAhead) { continue; }
		BestAhead = Ahead;
		OutX = FX;
		bFound = true;
	}
	return bFound;
}

float AEWGameMode::GetBaseHealthFraction(EEWTeam InTeam) const
{
	for (const TObjectPtr<AEWBuilding>& B : Buildings)
	{
		if (B && B->GetTeam() == InTeam && B->GetBuildingType() != EEWBuildingType::Depot)
		{
			return B->GetHealthFraction();
		}
	}
	return 0.f;
}

float AEWGameMode::GetCombatCenterX() const
{
	// Cyborg front line = furthest-advanced cyborg (max X); Insect front = min X.
	bool bHasC = false, bHasI = false;
	float CyborgFront = -FLT_MAX;
	float InsectFront = FLT_MAX;
	for (const TObjectPtr<AEWUnit>& U : CyborgUnits)
	{
		if (U && U->IsAlive()) { CyborgFront = FMath::Max(CyborgFront, U->GetActorLocation().X); bHasC = true; }
	}
	for (const TObjectPtr<AEWUnit>& U : InsectUnits)
	{
		if (U && U->IsAlive()) { InsectFront = FMath::Min(InsectFront, U->GetActorLocation().X); bHasI = true; }
	}
	if (bHasC && bHasI) { return 0.5f * (CyborgFront + InsectFront); }
	if (bHasC) { return CyborgFront; }
	if (bHasI) { return InsectFront; }
	return 0.f;
}

AEWBuilding* AEWGameMode::GetEnemyBase(EEWTeam SeekerTeam) const
{
	const EEWTeam EnemyTeam = (SeekerTeam == EEWTeam::Cyborg) ? EEWTeam::Insect : EEWTeam::Cyborg;
	for (const TObjectPtr<AEWBuilding>& B : Buildings)
	{
		if (B && B->IsAlive() && B->GetTeam() == EnemyTeam &&
			B->GetBuildingType() != EEWBuildingType::Depot)
		{
			return B;
		}
	}
	return nullptr;
}

void AEWGameMode::NotifyUnitDied(AEWUnit* Unit, AActor* Killer)
{
	if (Unit && Unit->GetTeam() == EEWTeam::Insect)
	{
		Biomass += 5.f; // Killing enemies fuels the player's economy.
	}
}

void AEWGameMode::NotifyBuildingDestroyed(AEWBuilding* Building)
{
	if (!Building || bMatchOver) { return; }
	if (Building->GetBuildingType() == EEWBuildingType::Depot) { return; }

	bMatchOver = true;
	bPlayerWon = (Building->GetTeam() == EEWTeam::Insect);
	UE_LOG(LogTemp, Warning, TEXT("[EvolutionWar] Match over. Player %s"), bPlayerWon ? TEXT("WINS") : TEXT("LOSES"));
}
