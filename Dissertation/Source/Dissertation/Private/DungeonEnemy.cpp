#include "DungeonEnemy.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "VoxelLayoutGenerator.h" 
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h" 
#include "Engine/DamageEvents.h" 
#include "DungeonCharacter.h" 

ADungeonEnemy::ADungeonEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	// Makes sure the AI actually looks in the direction it's moving
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->MaxWalkSpeed = 450.0f;

	// Sets up the melee weapon hitbox (starts turned off so it doesn't randomly hit things)
	WeaponHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("WeaponHitbox"));
	WeaponHitbox->SetupAttachment(GetRootComponent());
	WeaponHitbox->SetRelativeLocation(FVector(80.0f, 0.0f, 0.0f));
	WeaponHitbox->SetBoxExtent(FVector(60.0f, 60.0f, 30.0f));

	// Hitbox collision stuff: ignores everything except the player when attacking so it doesnt glitch
	WeaponHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponHitbox->SetCollisionObjectType(ECC_WorldDynamic);
	WeaponHitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	WeaponHitbox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	WeaponHitbox->SetGenerateOverlapEvents(true);

	// The red indicator mesh that warns the player about the attack
	TelegraphIndicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TelegraphIndicator"));
	TelegraphIndicator->SetupAttachment(GetRootComponent());
	TelegraphIndicator->SetRelativeLocation(FVector(100.0f, 0.0f, -90.0f));
	TelegraphIndicator->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.1f));
	TelegraphIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TelegraphIndicator->SetVisibility(false);
}

void ADungeonEnemy::BeginPlay()
{
	Super::BeginPlay();

	// Save the player reference here so we aren't constantly searching for them every single tick
	TargetActor = UGameplayStatics::GetPlayerPawn(this, 0);
	SpawnDefaultController();
	CurrentHealth = MaxHealth;

	// Set up the enemy behaviors and what loot they drop depending on what I set in the editor
	if (EnemyBehavior == EEnemyType::FlyingEnemy)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		ItemToDrop = FName("CoagulatedBlood");
	}
	else if (EnemyBehavior == EEnemyType::MeleeEnemy)
	{
		ItemToDrop = FName("TornCloth");
	}
	else if (EnemyBehavior == EEnemyType::Boss)
	{
		ItemToDrop = FName("BossSoul");
		DropChancePercentage = 100; // Bosses always drop their item no matter what
	}

	WeaponHitbox->OnComponentBeginOverlap.AddDynamic(this, &ADungeonEnemy::OnWeaponOverlap);

	// Find the layout generator so the enemy can tell it when it dies (for clearing rooms)
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVoxelLayoutGenerator::StaticClass(), FoundActors);
	if (FoundActors.Num() > 0) Director = Cast<AVoxelLayoutGenerator>(FoundActors[0]);
}

void ADungeonEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Spin the enemy model around when attacking so it actually looks like a melee swing
	if (bIsSwinging)
	{
		if (EnemyBehavior == EEnemyType::MeleeEnemy)
		{
			AddActorLocalRotation(FRotator(0, 700.0f * DeltaTime, 0));
		}
	}

	// Basic AI logic: enemy runs at player, pauses in front of player to wind up attack and then spins to hit
	if (TargetActor && !bIsAttacking && CurrentHealth > 0)
	{
		float Distance = FVector::Dist(GetActorLocation(), TargetActor->GetActorLocation());

		if (EnemyBehavior == EEnemyType::MeleeEnemy || EnemyBehavior == EEnemyType::Boss)
		{
			if (Distance <= AttackRange)
			{
				StartAttackSequence();
			}
			else
			{
				// Walk towards the player (ignoring Z so they don't try to fly up)
				FVector Direction = (TargetActor->GetActorLocation() - GetActorLocation());
				Direction.Z = 0.0f;
				Direction.Normalize();
				AddMovementInput(Direction, 1.0f);
			}
		}
		else if (EnemyBehavior == EEnemyType::FlyingEnemy)
		{
			// Bats start their attack dash from further away
			if (Distance <= AttackRange * 4.0f)
			{
				StartAttackSequence();
			}
			else
			{
				// Hovering logic: tries to stay a bit above the player's head while chasing
				FVector PlayerLoc = TargetActor->GetActorLocation();
				FVector HoverGoal = FVector(PlayerLoc.X, PlayerLoc.Y, PlayerLoc.Z + 300.0f);
				FVector Direction = (HoverGoal - GetActorLocation()).GetSafeNormal();
				AddMovementInput(Direction, 0.4f);

				// Smoothly look at the player
				FRotator LookAtRot = FRotationMatrix::MakeFromX(PlayerLoc - GetActorLocation()).Rotator();
				LookAtRot.Pitch = 0.0f;
				LookAtRot.Roll = 0.0f;
				SetActorRotation(FMath::RInterpTo(GetActorRotation(), LookAtRot, DeltaTime, 5.0f));
			}
		}
	}
}

void ADungeonEnemy::StartAttackSequence()
{
	if (bIsAttacking) return;
	bIsAttacking = true;

	// Stop moving during the windup so the player has a fair chance to react
	GetCharacterMovement()->StopMovementImmediately();

	if (EnemyBehavior == EEnemyType::Boss)
	{
		// Makes the boss jump up high before slamming down
		LaunchCharacter(FVector(0, 0, 1200.0f), true, true);
		GetWorldTimerManager().SetTimer(TimerHandle_AttackDelay, this, &ADungeonEnemy::ExecuteBossSlam, 1.2f, false);
		return;
	}

	TelegraphIndicator->SetVisibility(true);

	// Lock in the direction the bat will dash in so ti flies in a straight line towards the locked in position
	if (TargetActor && EnemyBehavior == EEnemyType::FlyingEnemy)
	{
		SavedDashDirection = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	}

	GetWorldTimerManager().SetTimer(TimerHandle_AttackDelay, this, &ADungeonEnemy::ExecuteStrike, TelegraphDelay, false);
}

void ADungeonEnemy::ExecuteStrike()
{
	TelegraphIndicator->SetVisibility(false);
	bIsSwinging = true;
	EnableWeaponHitbox();

	if (EnemyBehavior == EEnemyType::FlyingEnemy)
	{
		// Launches the bat at the player based on Dashspeed which can be editted in the blueprint details tab
		LaunchCharacter(SavedDashDirection * DashSpeed, true, true);
	}

	float AttackDuration;
	if (EnemyBehavior == EEnemyType::FlyingEnemy)
	{
		AttackDuration = 0.8f;
	}
	else
	{
		AttackDuration = 0.5f;
	}

	GetWorldTimerManager().SetTimer(TimerHandle_AttackReset, this, &ADungeonEnemy::ResetAttack, AttackDuration, false);
}

void ADungeonEnemy::ExecuteBossSlam()
{
	// Shoot the boss straight down into the floor
	LaunchCharacter(FVector(0, 0, -3000.0f), true, true);

	FVector StartLoc = GetActorLocation();
	FVector EndLoc = StartLoc - FVector(0, 0, 2500.0f);
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FVector SlamLoc = StartLoc;

	// Trace down to find the exact floor coordinate so the AoE damage centers properly
	if (GetWorld()->LineTraceSingleByChannel(Hit, StartLoc, EndLoc, ECC_Visibility, Params))
	{
		SlamLoc = Hit.ImpactPoint;
	}

	float SlamRadius = 600.0f;

	// Does AOE damage in a circle
	UGameplayStatics::ApplyRadialDamage(GetWorld(), AttackDamage * 2.0f, SlamLoc, SlamRadius, UDamageType::StaticClass(), TArray<AActor*>(), this, GetController(), true);

	// Knocks the player back if they get caught in the slam blast radiuus
	if (TargetActor && FVector::Dist(TargetActor->GetActorLocation(), SlamLoc) <= SlamRadius)
	{
		ACharacter* PlayerChar = Cast<ACharacter>(TargetActor);
		if (PlayerChar)
		{
			FVector PushDir = (PlayerChar->GetActorLocation() - SlamLoc).GetSafeNormal();
			PushDir.Z = 1.0f; // Give it a upwards push too
			PlayerChar->LaunchCharacter(PushDir * 1500.0f, true, true);
		}
	}

	GetWorldTimerManager().SetTimer(TimerHandle_AttackReset, this, &ADungeonEnemy::ResetAttack, 1.5f, false);
}

void ADungeonEnemy::ResetAttack()
{
	bIsAttacking = false;
	bIsSwinging = false;
	DisableWeaponHitbox();
	TelegraphIndicator->SetVisibility(false);
}

void ADungeonEnemy::EnableWeaponHitbox()
{
	WeaponHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ADungeonEnemy::DisableWeaponHitbox()
{
	WeaponHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADungeonEnemy::OnWeaponOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this && OtherActor == TargetActor)
	{
		UGameplayStatics::ApplyDamage(OtherActor, AttackDamage, GetController(), this, UDamageType::StaticClass());

		ACharacter* PlayerChar = Cast<ACharacter>(OtherActor);
		if (PlayerChar)
		{
			// Add a little bit of knockback to the player when they get hit
			FVector PushDir = (PlayerChar->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			PlayerChar->LaunchCharacter(PushDir * 800.0f + FVector(0, 0, 200), true, true);
		}

		// Turns off the hitbox instantly so it doesn't accidentally hit the player multiple times in one frame
		DisableWeaponHitbox();
	}
}

float ADungeonEnemy::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	CurrentHealth -= ActualDamage;

	if (CurrentHealth <= 0.0f) Die();
	return ActualDamage;
}

void ADungeonEnemy::Die()
{
	// Tell the layout generator an enemy died so it knows when the room is clear
	if (Director) Director->UnregisterEnemy();

	ADungeonCharacter* Player = Cast<ADungeonCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (Player)
	{
		// Update the player's kill stats
		if (EnemyBehavior == EEnemyType::MeleeEnemy) Player->RecordMeleeKill();
		else if (EnemyBehavior == EEnemyType::FlyingEnemy) Player->RecordFlyingKill();

		Player->AddGold(GoldDropAmount);

		// RNG to see if they drop their material
		int32 DropChance = FMath::RandRange(1, 100);
		if (DropChance <= DropChancePercentage)
		{
			Player->GrantItemToPlayer(ItemToDrop);
		}
	}

	Destroy();
}