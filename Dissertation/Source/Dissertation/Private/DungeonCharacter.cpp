#include "DungeonCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "VoxelLayoutGenerator.h" 
#include "DrawDebugHelpers.h" 
#include "GameFramework/CharacterMovementComponent.h"
#include "Interactable.h"
#include "DungeonEnemy.h"

ADungeonCharacter::ADungeonCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // Initialises First Person Camera
    FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
    FirstPersonCameraComponent->SetRelativeLocation(FVector(0, 0, 64.f));
    FirstPersonCameraComponent->bUsePawnControlRotation = true;

    // Initialises Viewmodel Weapon Mesh (Currently no physical asset has been linked to the weaponmesh)
    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
    WeaponMesh->SetupAttachment(FirstPersonCameraComponent);
    WeaponMesh->SetRelativeLocation(FVector(30, 15, -15));
    WeaponMesh->SetCastShadow(false);

    // Custom gravity scaling for smoother gameplay
    GetCharacterMovement()->GravityScale = 2.0f;
    GetCharacterMovement()->JumpZVelocity = 800.0f;
    GetCharacterMovement()->AirControl = 0.3f;
}

void ADungeonCharacter::BeginPlay()
{
    Super::BeginPlay();

    SafeSpawnLocation = GetActorLocation();
    CurrentGold = StartingGold;
    Killed_Melee = 0;
    Killed_Flying = 0;
    TotalDamageDealt = 0.0f;
    HealthPotions = 0;

    // Locates the Voxel Layout Generator in the level for dungeon generation hooks
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVoxelLayoutGenerator::StaticClass(), FoundActors);
    if (FoundActors.Num() > 0) CachedLayoutGenerator = Cast<AVoxelLayoutGenerator>(FoundActors[0]);

    RecalculateStats();
    CurrentHealth = MaxHealth;
    CurrentAmmo = MaxAmmo;
}

void ADungeonCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Handle Grappling Hook Physics Interpolation
    if (bIsGrappling)
    {
        FVector MyLoc = GetActorLocation();
        float Distance = FVector::Dist(MyLoc, GrappleTargetLocation);

        // Breaks grapple when close to the target surface and applies a slight vertical boost
        if (Distance < 150.0f)
        {
            StopGrapple();
            LaunchCharacter(FVector(0, 0, 300), false, true);
            return;
        }
        FVector Direction = (GrappleTargetLocation - MyLoc).GetSafeNormal();
        GetCharacterMovement()->Velocity = Direction * GrappleSpeed;
    }


    // Dynamic Crosshair State Check (Checks if looking at carvable voxel terrain) (Blue = Most likely will lead to an already existing/completed dungeon)
    // (Red = Will mostly not find a room but will attempt to regardless so it can be spammed till it eventually finds a room/dungeon sometimes)
    // (Green = Will guarantee a new room/dungeon for the player to explore)
    CrosshairState = 0;
    if (CachedLayoutGenerator)
    {
        FVector Start = FirstPersonCameraComponent->GetComponentLocation();
        FVector End = Start + (FirstPersonCameraComponent->GetForwardVector() * MaxCarveDistance);
        FHitResult Hit;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(this);

        if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
        {
            CrosshairState = CachedLayoutGenerator->CheckCarveStatus(Hit.ImpactPoint, -Hit.ImpactNormal);
        }
    }
}

void ADungeonCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAxis("MoveForward", this, &ADungeonCharacter::MoveForward);
    PlayerInputComponent->BindAxis("Strafe", this, &ADungeonCharacter::MoveRight);
    PlayerInputComponent->BindAxis("Turn", this, &ADungeonCharacter::CustomTurn);
    PlayerInputComponent->BindAxis("LookUp", this, &ADungeonCharacter::CustomLookUp);

    PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ADungeonCharacter::Jump);
    PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
    PlayerInputComponent->BindAction("Grapple", IE_Pressed, this, &ADungeonCharacter::FireGrapple);
    PlayerInputComponent->BindAction("Grapple", IE_Released, this, &ADungeonCharacter::StopGrapple);

    PlayerInputComponent->BindAction("Carve", IE_Pressed, this, &ADungeonCharacter::OnCarveInput);
    PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &ADungeonCharacter::Reload);
    PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ADungeonCharacter::TryInteract);

    PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ADungeonCharacter::StartPrimaryAction);
    PlayerInputComponent->BindAction("Fire", IE_Released, this, &ADungeonCharacter::StopPrimaryAction);
    PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &ADungeonCharacter::StartSecondaryAction);
    PlayerInputComponent->BindAction("Aim", IE_Released, this, &ADungeonCharacter::StopSecondaryAction);
}

void ADungeonCharacter::RecordMeleeKill() { Killed_Melee++; }
void ADungeonCharacter::RecordFlyingKill() { Killed_Flying++; }

void ADungeonCharacter::StartPrimaryAction()
{
    // Attack rate throttling to prevent macro spamming
    float CurrentTime = GetWorld()->GetTimeSeconds();
    float TimeSinceLastAttack = CurrentTime - LastAttackTime;
    float FirstDelay = FMath::Max(0.0f, FireRate - TimeSinceLastAttack);

    if (CurrentClass == EPlayerClass::None)
    {
        if (TimeSinceLastAttack >= FireRate) PerformMeleeAttack();
    }
    else
    {
        GetWorldTimerManager().SetTimer(TimerHandle_HandleFire, this, &ADungeonCharacter::PerformRangedAttack, FireRate, true, FirstDelay);
    }
}

void ADungeonCharacter::StopPrimaryAction() { GetWorldTimerManager().ClearTimer(TimerHandle_HandleFire); }

void ADungeonCharacter::StartSecondaryAction()
{
    // Adjusts FOV and walk speed for Sniper Crossbow ADS to allow for precision
    if (CurrentClass == EPlayerClass::Sniper)
    {
        bIsSniperAiming = true;
        FirstPersonCameraComponent->SetFieldOfView(40.0f); //Lower = Higher zoom
        GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed * 0.3f;
    }
}

void ADungeonCharacter::StopSecondaryAction()
{
    if (CurrentClass == EPlayerClass::Sniper)
    {
        bIsSniperAiming = false;
        FirstPersonCameraComponent->SetFieldOfView(90.0f);
        GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed;
    }
}

void ADungeonCharacter::PerformMeleeAttack()
{
    LastAttackTime = GetWorld()->GetTimeSeconds();
    FVector Start = FirstPersonCameraComponent->GetComponentLocation();
    FVector Forward = FirstPersonCameraComponent->GetForwardVector();

    float MeleeRange = 150.0f;
    float Radius = 60.0f;
    FVector End = Start + (Forward * MeleeRange);

    FVector HandLocation = WeaponMesh->GetComponentLocation();
    // Draws an orange line to show the quick melee swing path
    DrawDebugLine(GetWorld(), HandLocation, End, FColor::Orange, false, 0.12f, 0, 1.0f);
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    // Sphere hitbox to make a more balanced approach to melee attacks rather than wide swing or a flat cube attack
    if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Params))
    {
        ADungeonEnemy* Enemy = Cast<ADungeonEnemy>(Hit.GetActor());
        if (Enemy)
        {
            float FinalDamage = PhysicalDamage;
            bool bIsCrit = false;

            // Calculates critical hit modifiers
            if (FMath::RandRange(0.0f, 100.0f) < CritChance)
            {
                bIsCrit = true;
                FinalDamage *= CritMultiplier;
            }

            UGameplayStatics::ApplyPointDamage(Enemy, FinalDamage, Hit.ImpactNormal, Hit, GetController(), this, UDamageType::StaticClass());
            // Passives similar to other RPG Games
            // Life Steal: Heals the player for a percentage of the damage dealt
            if (LifeSteal > 0.0f)
            {
                CurrentHealth += (FinalDamage * (LifeSteal / 100.0f));
                if (CurrentHealth > MaxHealth) CurrentHealth = MaxHealth;
            }

            // Head Hunter: Instantly executes enemies if they are below 25% HP
            if (CurrentPassive == EItemPassive::Headhunter)
            {
                if (Enemy->CurrentHealth > 0.0f && Enemy->CurrentHealth <= (Enemy->MaxHealth * 0.25f))
                {
                    // Deal 9999 damage after it reaches that threshold

                    UGameplayStatics::ApplyDamage(Enemy, 9999.0f, GetController(), this, UDamageType::StaticClass());
                }
            }
            // True damage deals a bonus 30% unmitigated raw damage on every hit
            else if (CurrentPassive == EItemPassive::TrueShot)
            {
                UGameplayStatics::ApplyDamage(Enemy, FinalDamage * 0.3f, GetController(), this, UDamageType::StaticClass());
            }
            // Extra 15 damage as Burn damage
            else if (CurrentPassive == EItemPassive::Burn)
            {
                UGameplayStatics::ApplyDamage(Enemy, 15.0f, GetController(), this, UDamageType::StaticClass());
            }
            TotalDamageDealt += FinalDamage;
            ShowDamageNumber(FinalDamage, Hit.ImpactPoint, bIsCrit, false);
        }
    }
}

void ADungeonCharacter::PerformRangedAttack()
{
    LastAttackTime = GetWorld()->GetTimeSeconds();

    if (CurrentAmmo <= 0) { Reload(); return; }
    CurrentAmmo--;

    FVector Start = FirstPersonCameraComponent->GetComponentLocation();
    FVector Forward = FirstPersonCameraComponent->GetForwardVector();

    // Apply weapon spread unless aiming down sights
    float Spread;
    if (bIsSniperAiming)
    {
        Spread = 0.0f;
    }
    else
    {
        Spread = WeaponSpread;
    }

    FVector ShotDir = FMath::VRandCone(Forward, FMath::DegreesToRadians(Spread));
    FVector End = Start + (ShotDir * WeaponRange);

    // Offsets to the right so that the visual tracer doesent block the screen
    FVector VisualStart = Start + (FirstPersonCameraComponent->GetRightVector() * 50.0f) - (FirstPersonCameraComponent->GetUpVector() * 20.0f) + (Forward * 40.0f);
    if (TracerEffect) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), TracerEffect, VisualStart, ShotDir.Rotation());

    // changes from yellow to purple depending on the class
    FColor LaserColor;

    if (CurrentClass == EPlayerClass::Mage)
    {
        LaserColor = FColor::Purple;
    }
    else
    {
        LaserColor = FColor::Yellow;
    }

    DrawDebugLine(GetWorld(), VisualStart, End, LaserColor, false, 0.2f, 0, 2.0f);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.bTraceComplex = true;

    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
    {
        ADungeonEnemy* Enemy = Cast<ADungeonEnemy>(Hit.GetActor());
        if (Enemy)
        {
            float FinalDamage;
            if (CurrentClass == EPlayerClass::Mage)
            {
                FinalDamage = MagicDamage;
            }
            else
            {
                FinalDamage = PhysicalDamage;
            }

            bool bIsCrit = false;
            bool bIsMagic = (CurrentClass == EPlayerClass::Mage);

            // Mages cannot conventionally crit unless I decide to add it in the future
            if (!bIsMagic)
            {
                if (FMath::RandRange(0.0f, 100.0f) < CritChance)
                {
                    bIsCrit = true;
                    FinalDamage *= CritMultiplier;
                }
            }

            UGameplayStatics::ApplyPointDamage(Enemy, FinalDamage, Hit.ImpactNormal, Hit, GetController(), this, UDamageType::StaticClass());
            if (LifeSteal > 0.0f)
            {
                CurrentHealth += (FinalDamage * (LifeSteal / 100.0f));
                if (CurrentHealth > MaxHealth) CurrentHealth = MaxHealth;
            }

            if (CurrentPassive == EItemPassive::Headhunter)
            {
                if (Enemy->CurrentHealth > 0.0f && Enemy->CurrentHealth <= (Enemy->MaxHealth * 0.25f))
                {
                    UGameplayStatics::ApplyDamage(Enemy, 9999.0f, GetController(), this, UDamageType::StaticClass());
                }
            }
            else if (CurrentPassive == EItemPassive::TrueShot)
            {
                UGameplayStatics::ApplyDamage(Enemy, FinalDamage * 0.3f, GetController(), this, UDamageType::StaticClass());
            }
            else if (CurrentPassive == EItemPassive::Burn)
            {
                UGameplayStatics::ApplyDamage(Enemy, 15.0f, GetController(), this, UDamageType::StaticClass());
            }
            TotalDamageDealt += FinalDamage;
            ShowDamageNumber(FinalDamage, Hit.ImpactPoint, bIsCrit, bIsMagic);
        }
    }
}

void ADungeonCharacter::MoveForward(float Value) { if (Value != 0.0f) AddMovementInput(GetActorForwardVector(), Value); }
void ADungeonCharacter::MoveRight(float Value) { if (Value != 0.0f) AddMovementInput(GetActorRightVector(), Value); }

bool ADungeonCharacter::GrantItemToPlayer(FName ItemID)
{
    // Retrieve Item config from DataTable
    UDataTable* ItemTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TEXT("/Game/DT_ShopItems")));
    if (!ItemTable) return false;

    FDungeonItemInfo* NewItem = ItemTable->FindRow<FDungeonItemInfo>(ItemID, "");
    if (!NewItem) return false;

    if (NewItem->ItemCategory == EItemCategory::Equipment)
    {
        // Implements inventory capacity and class restriction logic
        if (EquipmentList.Num() >= MaxEquipmentSlots) return false;
        if (NewItem->bIsLegendary && EquipmentList.Contains(ItemID)) return false;
        if (CurrentClass != EPlayerClass::None && NewItem->ClassType != EPlayerClass::None && NewItem->ClassType != CurrentClass) return false;

        // Prevents player from equipping multiple legendary items
        if (NewItem->bIsLegendary)
        {
            for (FName ExistingID : EquipmentList)
            {
                FDungeonItemInfo* OldItem = ItemTable->FindRow<FDungeonItemInfo>(ExistingID, "");
                if (OldItem && OldItem->bIsLegendary) return false;
            }
        }

        EquipmentList.Add(ItemID);
        RecalculateStats(); // Refreshes player stats instnatly
        return true;
    }
    else if (NewItem->ItemCategory == EItemCategory::LootMaterial)
    {
        if (LootBag.Num() >= MaxLootSlots) return false;
        LootBag.Add(ItemID);
        return true;
    }

    return false;
}

bool ADungeonCharacter::CraftItem(FName ItemID)
{
    UDataTable* ItemTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TEXT("/Game/DT_ShopItems")));
    if (!ItemTable) return false;

    FDungeonItemInfo* ItemInfo = ItemTable->FindRow<FDungeonItemInfo>(ItemID, "");
    if (!ItemInfo) return false;
    if (ItemInfo->ItemCategory != EItemCategory::Equipment) return false;

    // Preliminary economy and constraint checks
    if (EquipmentList.Num() >= MaxEquipmentSlots) return false;
    if (ItemInfo->bIsLegendary && EquipmentList.Contains(ItemID)) return false;
    if (CurrentGold < ItemInfo->GoldCost) return false;
    if (CurrentClass != EPlayerClass::None && ItemInfo->ClassType != EPlayerClass::None && ItemInfo->ClassType != CurrentClass) return false;

    // Creates temporary arrays to handle material consumption
    TArray<FName> TempLootBag = LootBag;
    TArray<FName> TempEquipmentList = EquipmentList;

    for (FName MatID : ItemInfo->RequiredMaterials)
    {
        int32 FoundInLoot = TempLootBag.Find(MatID);
        if (FoundInLoot != INDEX_NONE)
        {
            TempLootBag.RemoveAt(FoundInLoot);
        }
        else
        {
            int32 FoundInEquip = TempEquipmentList.Find(MatID);
            if (FoundInEquip != INDEX_NONE) TempEquipmentList.RemoveAt(FoundInEquip);
            else return false; // Missing required materials therefore the crafting fials
        }
    }

    // Apply permanent changes to the inventory and gold only after passing all checks
    CurrentGold -= ItemInfo->GoldCost;
    LootBag = TempLootBag;
    EquipmentList = TempEquipmentList;

    EquipmentList.Add(ItemID);
    RecalculateStats();

    return true;
}

void ADungeonCharacter::SellItem(FName ItemID)
{
    if (ItemID == FName("HealthPotion"))
    {
        if (HealthPotions > 0)
        {
            HealthPotions--;
            UDataTable* ItemTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TEXT("/Game/DT_ShopItems")));
            if (ItemTable)
            {
                FDungeonItemInfo* ItemInfo = ItemTable->FindRow<FDungeonItemInfo>(ItemID, "");
                if (ItemInfo) AddGold(ItemInfo->GoldCost * 0.7f); // 70% return rate on sell
            }
            return;
        }
    }

    bool bFound = false;
    if (EquipmentList.Contains(ItemID))
    {
        EquipmentList.RemoveSingle(ItemID);
        bFound = true;
    }
    else if (LootBag.Contains(ItemID))
    {
        LootBag.RemoveSingle(ItemID);
        bFound = true;
    }

    if (!bFound) return;

    UDataTable* ItemTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TEXT("/Game/DT_ShopItems")));
    if (ItemTable)
    {
        FDungeonItemInfo* ItemInfo = ItemTable->FindRow<FDungeonItemInfo>(ItemID, "");
        if (ItemInfo) AddGold(ItemInfo->GoldCost * 0.7f);
    }

    RecalculateStats();
}

void ADungeonCharacter::RecalculateStats()
{
    // Resets to baseline stats before applying modifiers
    float NewMaxHealth = BaseMaxHealth;
    PhysicalDamage = BasePhysicalDamage;
    MagicDamage = 0.0f;
    Armour = 0.0f;
    MagicSuppression = 0.0f;
    BaseMoveSpeed = 600.0f;
    CritChance = 0.0f;
    ArmourShred = 0.0f;
    MagicShred = 0.0f;
    LifeSteal = 0.0f;

    CurrentClass = EPlayerClass::None;
    CurrentPassive = EItemPassive::None;

    UDataTable* ItemTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TEXT("/Game/DT_ShopItems")));
    if (ItemTable && EquipmentList.Num() > 0)
    {
        // Collects all stat bonuses dynamically
        for (FName ItemID : EquipmentList)
        {
            FDungeonItemInfo* ItemInfo = ItemTable->FindRow<FDungeonItemInfo>(ItemID, "");
            if (ItemInfo)
            {
                NewMaxHealth += ItemInfo->HealthBonus;
                PhysicalDamage += ItemInfo->PhysicalDamageBonus;
                MagicDamage += ItemInfo->MagicDamageBonus;
                Armour += ItemInfo->ArmourBonus;
                MagicSuppression += ItemInfo->MagicSuppressionBonus;
                BaseMoveSpeed += ItemInfo->MoveSpeedBonus;
                CritChance += ItemInfo->CritChanceBonus;
                LifeSteal += ItemInfo->LifeStealBonus;

                if (ItemInfo->ClassType != EPlayerClass::None) CurrentClass = ItemInfo->ClassType;
                if (ItemInfo->PassiveEffect != EItemPassive::None) CurrentPassive = ItemInfo->PassiveEffect;
            }
        }
    }

    // Applies final stats to gameplay components
    MaxHealth = NewMaxHealth;
    GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed;
    if (CurrentHealth > MaxHealth) CurrentHealth = MaxHealth;
}

void ADungeonCharacter::AddGold(int32 Amount) { CurrentGold += Amount; }
int32 ADungeonCharacter::GetCurrentAmmo() const { return CurrentAmmo; }
int32 ADungeonCharacter::GetMaxAmmo() const { return MaxAmmo; }

float ADungeonCharacter::GetHealthPercent() const
{
    if (MaxHealth > 0)
    {
        return CurrentHealth / MaxHealth;
    }
    else
    {
        return 0.0f;
    }
}

float ADungeonCharacter::GetCurrentHealth() const { return CurrentHealth; }

FString ADungeonCharacter::GetRoomStatus() const
{
    if (CachedLayoutGenerator && CachedLayoutGenerator->IsRoomClear()) return "Tunnelling Spell Ready";
    return "LOCKED SPELL: Eliminate all Enemies";
}

void ADungeonCharacter::SetMenuState(bool bOpen)
{
    bIsMenuOpen = bOpen;
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC)
    {
        PC->bShowMouseCursor = bOpen;
        if (bOpen)
        {
            // Locks mouse to viewport when UI is active to prevent clicking outside the window
            FInputModeGameAndUI InputMode;
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
            InputMode.SetHideCursorDuringCapture(false);
            PC->SetInputMode(InputMode);
        }
        else
        {
            // Restores standard gameplay input
            FInputModeGameOnly InputMode;
            PC->SetInputMode(InputMode);
        }
    }
}

void ADungeonCharacter::CustomTurn(float Value) { if (!bIsMenuOpen) AddControllerYawInput(Value); }
void ADungeonCharacter::CustomLookUp(float Value) { if (!bIsMenuOpen) AddControllerPitchInput(Value); }
void ADungeonCharacter::Jump() { Super::Jump(); }
void ADungeonCharacter::Reload() { CurrentAmmo = MaxAmmo; }

void ADungeonCharacter::FireGrapple()
{
    if (bIsGrappling || GetWorldTimerManager().IsTimerActive(TimerHandle_GrappleDelay)) return;

    FVector Start = FirstPersonCameraComponent->GetComponentLocation();
    FVector End = Start + (FirstPersonCameraComponent->GetForwardVector() * GrappleRange);
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
    {
        GrappleTargetLocation = Hit.ImpactPoint;

        FVector Right = FirstPersonCameraComponent->GetRightVector();
        FVector Up = FirstPersonCameraComponent->GetUpVector();
        FVector Forward = FirstPersonCameraComponent->GetForwardVector();

        // Fake hand offset so the cable doesn't block the center of the screen
        FVector HandLocation = Start + (Right * 40.0f) - (Up * 30.0f) + (Forward * 50.0f);

        // Draws the black grappling hook starting from the offset
        DrawDebugLine(GetWorld(), HandLocation, GrappleTargetLocation, FColor::Black, false, 0.5f, 0, 2.0f);

        GetWorldTimerManager().SetTimer(TimerHandle_GrappleDelay, this, &ADungeonCharacter::ExecuteGrapplePull, GrappleDelay, false);
    }
}

void ADungeonCharacter::ExecuteGrapplePull()
{
    bIsGrappling = true;
    GetCharacterMovement()->SetMovementMode(MOVE_Flying); // Prevent Gravity from dragging the player down
    LaunchCharacter(FVector(0, 0, 250.0f), false, false);
}

void ADungeonCharacter::StopGrapple()
{
    if (bIsGrappling)
    {
        bIsGrappling = false;
        GetCharacterMovement()->SetMovementMode(MOVE_Falling); // Switches back to Falling movement mode
        StopGrappleVisual();
    }
}

void ADungeonCharacter::TryInteract()
{
    if (bIsMenuOpen) return;

    FVector Start = FirstPersonCameraComponent->GetComponentLocation();
    FVector End = Start + (FirstPersonCameraComponent->GetForwardVector() * InteractionRange);
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    // Checks if the raycast hits an object implementing the IInteractable interface
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
    {
        if (Hit.GetActor() && Hit.GetActor()->Implements<UInteractable>())
        {
            IInteractable::Execute_Interact(Hit.GetActor(), this);
        }
    }
}

void ADungeonCharacter::OnCarveInput()
{
    if (!CachedLayoutGenerator || !CachedLayoutGenerator->IsRoomClear()) return;
    if (CurrentAmmo <= 0)
    {
        Reload();
        return;
    }

    FVector Start = FirstPersonCameraComponent->GetComponentLocation();
    FVector End = Start + (FirstPersonCameraComponent->GetForwardVector() * MaxCarveDistance);
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
    {
        // Request the Layout Generator to evaluate voxel boundaries and carve if safe
        bool bSuccessfullyBuiltRoom = CachedLayoutGenerator->AttemptBuildAtLocation(Hit.ImpactPoint, -Hit.ImpactNormal);
        if (bSuccessfullyBuiltRoom) CurrentAmmo--;
    }
}

float ADungeonCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    CurrentHealth -= ActualDamage;
    if (CurrentHealth <= 0.0f) Die();
    return ActualDamage;
}

void ADungeonCharacter::Die()
{
    GetCharacterMovement()->DisableMovement();
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        DisableInput(PC);
    }
    OnPlayerDied();
}

void ADungeonCharacter::FellOutOfWorld(const class UDamageType& dmgType)
{
    // Failsafe by preventing infinite falling if voxel generation reaches boundaries or fails somehow
    SetActorLocation(SafeSpawnLocation);
    GetCharacterMovement()->Velocity = FVector::ZeroVector;
}

void ADungeonCharacter::UseHealthPotion()
{
    if (HealthPotions > 0 && CurrentHealth < MaxHealth)
    {
        HealthPotions--;
        CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + 50.0f);
    }
}

// HUD Formatting

FString ADungeonCharacter::GetEquipmentString() const
{
    FString Result = "";
    if (HealthPotions > 0) Result += FString::Printf(TEXT("Health Potion x%d\n"), HealthPotions);
    if (EquipmentList.Num() == 0 && HealthPotions == 0) return "No Equipment\n";

    TMap<FName, int32> ItemCounts;
    for (FName Item : EquipmentList) { ItemCounts.FindOrAdd(Item)++; }

    for (const TPair<FName, int32>& Pair : ItemCounts) {
        Result += FString::Printf(TEXT("%s x%d\n"), *Pair.Key.ToString(), Pair.Value);
    }
    return Result;
}

FString ADungeonCharacter::GetMaterialsString() const
{
    FString Result = "";
    if (LootBag.Num() == 0) return "Empty\n";

    TMap<FName, int32> ItemCounts;
    for (FName Item : LootBag) { ItemCounts.FindOrAdd(Item)++; }

    for (const TPair<FName, int32>& Pair : ItemCounts) {
        Result += FString::Printf(TEXT("%s x%d\n"), *Pair.Key.ToString(), Pair.Value);
    }
    return Result;
}

FString ADungeonCharacter::GetPlayerStatsString() const
{
    FString ClassName = "Classless";
    if (CurrentClass == EPlayerClass::Sniper) ClassName = "Sniper";
    else if (CurrentClass == EPlayerClass::Mage) ClassName = "Mage";
    FString Stats = FString::Printf(TEXT("--- COMBAT STATS ---\n"));
    Stats += FString::Printf(TEXT("Class: %s\n\n"), *ClassName);

    Stats += FString::Printf(TEXT("HP: %.0f / %.0f\n"), CurrentHealth, MaxHealth);
    Stats += FString::Printf(TEXT("Physical Damage: %.0f\n"), PhysicalDamage);
    Stats += FString::Printf(TEXT("Magic Damage: %.0f\n"), MagicDamage);
    Stats += FString::Printf(TEXT("Attack Cooldown: %.2fs\n"), FireRate);

    Stats += FString::Printf(TEXT("Crit Rate: %.0f%%\n"), CritChance);
    Stats += FString::Printf(TEXT("Life Steal: %.0f%%\n\n"), LifeSteal);

    Stats += FString::Printf(TEXT("Armour: %.0f\n"), Armour);
    Stats += FString::Printf(TEXT("Magic Suppression: %.0f\n"), MagicSuppression);
    Stats += FString::Printf(TEXT("Armour Pen: %.0f\n"), ArmourShred);
    Stats += FString::Printf(TEXT("Magic Pen: %.0f\n\n"), MagicShred);

    Stats += FString::Printf(TEXT("Move Speed: %.0f\n"), BaseMoveSpeed);

    return Stats;
}