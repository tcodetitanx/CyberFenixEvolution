#include "EWUnit.h"
#include "EWBuilding.h"
#include "EWGameMode.h"
#include "EWProjectile.h"
#include "EWParticleBurst.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AEWUnit::AEWUnit()
{
	PrimaryActorTick.bCanEverTick = true;

	SpriteMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpriteMesh"));
	SetRootComponent(SpriteMesh);
	SpriteMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpriteMesh->SetCastShadow(false);
	// Stand the flat plane up in the X-Z lane plane, facing the camera (-Y).
	SpriteMesh->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded())
	{
		SpriteMesh->SetStaticMesh(PlaneFinder.Object);
	}
}

void AEWUnit::InitUnit(EEWTeam InTeam, const FUnitGenome& InGenome, UTexture2D* InSprite, float InSpriteWorldHeight)
{
	Team = InTeam;
	Genome = InGenome;
	SpriteTexture = InSprite;
	SpriteWorldHeight = InSpriteWorldHeight;
	Health = Genome.MaxHealth;
	if (HasActorBegunPlay())
	{
		ApplyVisuals();
	}
}

void AEWUnit::BeginPlay()
{
	Super::BeginPlay();
	GM = GetWorld() ? GetWorld()->GetAuthGameMode<AEWGameMode>() : nullptr;
	if (GM)
	{
		GM->RegisterUnit(this);
	}
	ApplyVisuals();
}

void AEWUnit::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GM)
	{
		GM->UnregisterUnit(this);
	}
	Super::EndPlay(Reason);
}

void AEWUnit::ApplyVisuals()
{
	if (!SpriteMaterial)
	{
		SpriteMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/M_Sprite.M_Sprite"));
	}
	if (SpriteMaterial && !SpriteMID)
	{
		SpriteMID = UMaterialInstanceDynamic::Create(SpriteMaterial, this);
		SpriteMesh->SetMaterial(0, SpriteMID);
	}
	if (SpriteMID)
	{
		if (SpriteTexture)
		{
			SpriteMID->SetTextureParameterValue(TEXT("BaseTex"), SpriteTexture);
		}
		SpriteMID->SetVectorParameterValue(TEXT("Tint"), Genome.Tint);
	}

	const float Desired = FMath::Max(20.f, SpriteWorldHeight * Genome.Scale);
	const float ScaleFactor = Desired / 100.f; // engine plane is 100uu wide.
	BaseScaleFactor = ScaleFactor;
	SpriteMesh->SetRelativeScale3D(FVector(ScaleFactor));
	// SpriteMesh is the root component, so moving it moves the actor. Lift the
	// sprite so its base rests on the ground WITHOUT discarding the spawn X/Y.
	// Cache this rest height; the bob/jump/death animations ride on top of it.
	// Aerial units float a good way above the lane on a constant altitude.
	AerialAltitude = IsAerial() ? FMath::Max(360.f, Desired * 0.9f) : 0.f;
	const FVector Loc = GetActorLocation();
	BaseActorZ = Desired * 0.5f + AerialAltitude;
	SetActorLocation(FVector(Loc.X, Loc.Y, BaseActorZ));
}

float AEWUnit::DistanceAlongLane(const AActor* Other) const
{
	return Other ? FMath::Abs(GetActorLocation().X - Other->GetActorLocation().X) : TNumericLimits<float>::Max();
}

void AEWUnit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Death sequence runs to completion before the actor is destroyed.
	if (bDying)
	{
		UpdateDeathAnim(DeltaSeconds);
		return;
	}

	if (!IsAlive() || !GM)
	{
		return;
	}

	AttackCooldown -= DeltaSeconds;

	// Support roles continuously mend the units fighting around them.
	TickSupportAura(DeltaSeconds);

	const EEWRole MyRole = Genome.Role;
	const bool bCanHitAir = RoleCanHitAir(MyRole);

	// Validate / acquire a target (respecting whether we can reach aerial foes).
	bool bTargetValid = false;
	if (CurrentTarget)
	{
		if (const AEWUnit* U = Cast<AEWUnit>(CurrentTarget))
		{
			bTargetValid = U->IsAlive() && (bCanHitAir || !U->IsAerial());
		}
		else if (const AEWBuilding* B = Cast<AEWBuilding>(CurrentTarget))
		{
			bTargetValid = B->IsAlive();
		}
	}
	if (!bTargetValid)
	{
		CurrentTarget = GM->FindNearestEnemy(Team, GetActorLocation(), Genome.AggroRange, bCanHitAir);
	}

	// Fall back to marching at the enemy base.
	AActor* Goal = CurrentTarget;
	if (!Goal)
	{
		Goal = GM->GetEnemyBase(Team);
	}
	if (!Goal)
	{
		UpdateVisualAnim(DeltaSeconds, false);
		return;
	}

	bool bMovingNow = false;
	const float Dist = DistanceAlongLane(Goal);
	if (Dist <= Genome.AttackRange)
	{
		if (AttackCooldown <= 0.f)
		{
			AttackCooldown = Genome.AttackInterval;
			PerformAttack(Goal);
			TriggerAttackAnim(); // lunge/jump toward the foe on every swing
		}
	}
	else
	{
		const FVector Loc = GetActorLocation();
		const float GoalX = Goal->GetActorLocation().X;
		const float Dir = FMath::Sign(GoalX - Loc.X);

		// Default: advance until within attack range of the goal.
		float StopX = GoalX - Dir * Genome.AttackRange;

		// Melee/Tank/Support form ranks: hold a body-length gap behind the nearest
		// friendly ahead IN THE SAME DEPTH FILE, so up to N files can stand abreast
		// and fight simultaneously instead of everyone collapsing onto one point.
		// Flankers and flyers ignore the queue and drive straight for the enemy;
		// ranged simply stop at their (long) range, naturally sitting behind melee.
		if (RoleQueues(MyRole))
		{
			const float BodySpacing = FMath::Max(180.f, SpriteWorldHeight * Genome.Scale * 0.55f);
			float FriendX = 0.f;
			if (GM->NearestFriendlyAheadX(Team, Loc.X, Dir, this, Genome.AggroRange, FriendX))
			{
				const float QueueX = FriendX - Dir * BodySpacing;
				StopX = (Dir > 0.f) ? FMath::Min(StopX, QueueX) : FMath::Max(StopX, QueueX);
			}
		}

		const float ToStop = StopX - Loc.X; // signed distance remaining toward StopX
		float Step = FMath::Sign(ToStop) * FMath::Min(FMath::Abs(ToStop), Genome.MoveSpeed * DeltaSeconds);
		// Only ever advance toward the enemy; never back up for spacing.
		if (Dir * Step < 0.f)
		{
			Step = 0.f;
		}
		SetActorLocation(Loc + FVector(Step, 0.f, 0.f));
		bMovingNow = FMath::Abs(Step) > 0.5f;
	}

	UpdateVisualAnim(DeltaSeconds, bMovingNow);
}

void AEWUnit::UpdateVisualAnim(float DeltaSeconds, bool bMoving)
{
	if (!SpriteMesh)
	{
		return;
	}

	const float DirSign = (Team == EEWTeam::Cyborg) ? 1.f : -1.f;
	const float Desired = FMath::Max(20.f, SpriteWorldHeight * Genome.Scale);

	if (AttackAnimTime > 0.f) { AttackAnimTime = FMath::Max(0.f, AttackAnimTime - DeltaSeconds); }
	if (HitShakeTime > 0.f)   { HitShakeTime   = FMath::Max(0.f, HitShakeTime - DeltaSeconds); }
	IdlePhase += DeltaSeconds;

	float RollOff = 0.f, PitchOff = 0.f;
	float ScaleMul = 1.f;
	float ZOff = 0.f;
	float Bright = 0.f;

	// Gentle idle breathing so even waiting units feel alive.
	ScaleMul += 0.025f * FMath::Sin(IdlePhase * 2.2f);

	// Flyers bob and bank in the air instead of trudging.
	if (IsAerial())
	{
		ZOff    += FMath::Sin(IdlePhase * 2.6f) * Desired * 0.10f;
		RollOff += FMath::Sin(IdlePhase * 1.7f) * 6.f;
		PitchOff += DirSign * 3.f;
	}

	// March cycle: little hops, a side-to-side rock and a forward lean.
	if (bMoving && !IsAerial())
	{
		WalkPhase += DeltaSeconds * 11.f;
		ZOff     += FMath::Abs(FMath::Sin(WalkPhase)) * Desired * 0.06f;
		RollOff  += FMath::Sin(WalkPhase * 0.5f) * 5.f;
		PitchOff += DirSign * 4.f;
		ScaleMul += 0.03f * FMath::Sin(WalkPhase);
	}

	// Attack lunge: a quick hop + forward jab toward the enemy.
	if (AttackAnimTime > 0.f)
	{
		const float A = AttackAnimTime / 0.32f;     // 1 -> 0
		const float Arc = FMath::Sin(A * PI);       // 0 -> 1 -> 0
		ZOff     += Arc * Desired * 0.18f;
		PitchOff += DirSign * Arc * 16.f;
		ScaleMul += Arc * 0.10f;
	}

	// Damage shake: rapid jitter plus a white flash that decays out.
	if (HitShakeTime > 0.f)
	{
		const float S = HitShakeTime / 0.28f;
		const float T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		RollOff  += FMath::Sin(T * 90.f) * S * 11.f;
		PitchOff += FMath::Cos(T * 77.f) * S * 9.f;
		ScaleMul += S * 0.08f;
		Bright   += S * 0.9f;
	}

	SpriteMesh->SetRelativeRotation(FRotator(PitchOff, 0.f, 90.f + RollOff));
	SpriteMesh->SetRelativeScale3D(FVector(BaseScaleFactor * ScaleMul));

	const FVector L = GetActorLocation();
	SetActorLocation(FVector(L.X, L.Y, BaseActorZ + ZOff));

	if (SpriteMID && Bright > 0.f)
	{
		SpriteMID->SetVectorParameterValue(TEXT("Tint"), Genome.Tint + FLinearColor(Bright, Bright, Bright, 0.f));
	}
	else if (SpriteMID)
	{
		SpriteMID->SetVectorParameterValue(TEXT("Tint"), Genome.Tint);
	}
}

void AEWUnit::UpdateDeathAnim(float DeltaSeconds)
{
	DeathTime += DeltaSeconds;
	const float T = FMath::Clamp(DeathTime / 0.7f, 0.f, 1.f);

	if (SpriteMesh)
	{
		const float DirSign = (Team == EEWTeam::Cyborg) ? 1.f : -1.f;
		// Topple from standing (roll 90) flat to the ground, leaning along the lane.
		const float Roll = FMath::Lerp(90.f, 0.f, FMath::Sin(T * (PI * 0.5f)));
		const float Pitch = DirSign * T * 20.f;
		SpriteMesh->SetRelativeRotation(FRotator(Pitch, 0.f, Roll));
		SpriteMesh->SetRelativeScale3D(FVector(BaseScaleFactor * FMath::Lerp(1.f, 0.15f, T)));
	}

	const FVector L = GetActorLocation();
	SetActorLocation(FVector(L.X, L.Y, BaseActorZ + FMath::Lerp(0.f, -BaseActorZ * 0.7f, T)));

	if (SpriteMID)
	{
		SpriteMID->SetVectorParameterValue(TEXT("Tint"), Genome.Tint * FMath::Lerp(1.f, 0.15f, T));
	}

	if (T >= 1.f)
	{
		Destroy();
	}
}

void AEWUnit::PerformAttack(AActor* Goal)
{
	if (!Goal) { return; }

	// Apply the damage instantly (deterministic sim); the VFX is cosmetic.
	if (AEWUnit* U = Cast<AEWUnit>(Goal))
	{
		U->ReceiveDamage(Genome.Damage, this);
	}
	else if (AEWBuilding* B = Cast<AEWBuilding>(Goal))
	{
		B->ReceiveDamage(Genome.Damage, this);
	}
	FireAttackVFX(Goal);
}

void AEWUnit::FireAttackVFX(AActor* Goal)
{
	UWorld* World = GetWorld();
	if (!World || !Goal) { return; }

	const FVector MyLoc = GetActorLocation();
	const float DirX = FMath::Sign(Goal->GetActorLocation().X - MyLoc.X);

	// Team-flavoured glow: cool cyan for cyborgs, hot organic green/amber for bugs,
	// each nudged by the unit's evolved lineage tint so strains read distinctly.
	const FLinearColor Bolt = (Team == EEWTeam::Cyborg)
		? FLinearColor(0.30f + Genome.Tint.R * 0.25f, 1.70f + Genome.Tint.G * 0.40f, 2.50f + Genome.Tint.B * 0.40f, 1.f)
		: FLinearColor(1.40f + Genome.Tint.R * 0.40f, 1.65f + Genome.Tint.G * 0.30f, 0.35f + Genome.Tint.B * 0.20f, 1.f);

	if (RoleIsShooter(Genome.Role))
	{
		// Ranged + aerial actually launch a travelling bolt at the target.
		const FVector Muzzle = MyLoc + FVector(DirX * 55.f, -15.f, IsAerial() ? -10.f : 25.f);
		const FVector Fallback = Goal->GetActorLocation() + FVector(0.f, -15.f, 60.f);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = this;
		if (AEWProjectile* Proj = World->SpawnActor<AEWProjectile>(AEWProjectile::StaticClass(), FTransform(Muzzle), Params))
		{
			const float BoltSpeed = FMath::Max(4200.f, Genome.AttackRange * 16.f);
			Proj->Launch(Muzzle, Goal, Fallback, Bolt, BoltSpeed);
			AEWParticleBurst::Spawn(World, Muzzle, Bolt, 4, 420.f, 0.22f, 14.f);
		}
		return;
	}

	// Melee / Tank / Flanker / Support: a slash/impact spark right on the target,
	// heavier for tanks. Sells the clash without a projectile.
	const FVector Hit = Goal->GetActorLocation() + FVector(-DirX * 30.f, -15.f, 40.f);
	const bool bHeavy = (Genome.Role == EEWRole::Tank);
	const int32 Sparks = bHeavy ? 10 : 6;
	const float SparkSpeed = bHeavy ? 760.f : 520.f;
	const float SparkSize = bHeavy ? 26.f : 18.f;
	AEWParticleBurst::Spawn(World, Hit, Bolt, Sparks, SparkSpeed, 0.30f, SparkSize);
}

void AEWUnit::TickSupportAura(float DeltaSeconds)
{
	if (Genome.Role != EEWRole::Support || !GM) { return; }

	SupportPulseTime -= DeltaSeconds;
	if (SupportPulseTime > 0.f) { return; }
	SupportPulseTime = 1.6f; // seconds between heal pulses

	const float Radius = FMath::Max(420.f, Genome.AggroRange * 0.7f);
	const float Heal = FMath::Max(8.f, Genome.MaxHealth * 0.04f);
	GM->HealFriendliesInRange(Team, GetActorLocation().X, Radius, Heal);

	// Soft green restorative sparkle rising from the support unit.
	if (UWorld* World = GetWorld())
	{
		const FLinearColor HealCol(0.35f, 1.25f, 0.55f, 1.f);
		AEWParticleBurst::Spawn(World, GetActorLocation() + FVector(0.f, -10.f, 40.f), HealCol, 7, 320.f, 0.6f, 16.f);
	}
}

void AEWUnit::ReceiveDamage(float Amount, AActor* DamageInstigator)
{
	if (!IsAlive())
	{
		return;
	}
	const float Applied = FMath::Max(1.f, Amount - Genome.Armor);
	Health -= Applied;

	// Flinch: recoil shake + bright flash on every hit taken.
	HitShakeTime = 0.28f;

	// Credit the attacker's fitness: damage dealt now, a bonus if this kill lands.
	// Natural selection rewards genomes that actually win fights.
	if (AEWUnit* Attacker = Cast<AEWUnit>(DamageInstigator))
	{
		Attacker->Fitness += Applied;
		if (Health <= 0.f)
		{
			Attacker->Fitness += 60.f; // kill bonus
			Attacker->Kills += 1;
			// Crown dominant survivors too, not just the fallen: a killer offers its
			// running fitness to the gene pool the instant it scores.
			if (GM)
			{
				GM->ReportFitness(Attacker->Team, Attacker->Genome, Attacker->SpriteTexture, Attacker->SpriteWorldHeight, Attacker->Fitness);
			}
		}
	}

	if (Health <= 0.f)
	{
		Health = 0.f;
		Die();
	}
}

void AEWUnit::Heal(float Amount)
{
	if (!IsAlive())
	{
		return;
	}
	Health = FMath::Min(Genome.MaxHealth, Health + Amount);
}

void AEWUnit::Die()
{
	if (bDying)
	{
		return;
	}

	if (GM)
	{
		// Offer this unit's final performance to the gene pool before it dies, so a
		// hard-fighting genome can be crowned champion and seed the next generation.
		GM->ReportFitness(Team, Genome, SpriteTexture, SpriteWorldHeight, Fitness);
		GM->NotifyUnitDied(this, CurrentTarget);
	}

	// Begin the death animation instead of vanishing instantly. IsAlive() is already
	// false (Health == 0), so combat/targeting ignores us while we topple over.
	bDying = true;
	DeathTime = 0.f;

	const float Desired = FMath::Max(20.f, SpriteWorldHeight * Genome.Scale);
	const FLinearColor BurstCol = (Team == EEWTeam::Cyborg)
		? FLinearColor(0.4f, 0.9f, 1.2f, 1.f)
		: FLinearColor(1.2f, 0.55f, 0.18f, 1.f);
	AEWParticleBurst::Spawn(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, Desired * 0.3f),
		BurstCol, 8, 700.f, 0.5f, 18.f);
}
