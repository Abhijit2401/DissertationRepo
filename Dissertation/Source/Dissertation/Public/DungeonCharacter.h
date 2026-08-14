#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/DataTable.h"
#include "DungeonCharacter.generated.h"

UENUM(BlueprintType)
enum class EItemStat : uint8
{
    None                UMETA(DisplayName = "None"),
    Health              UMETA(DisplayName = "Max Health"),
    PhysicalDamage      UMETA(DisplayName = "Physical Damage"),
    MagicDamage         UMETA(DisplayName = "Magic Damage"),
    Armour              UMETA(DisplayName = "Armour"),
    MagicSuppression    UMETA(DisplayName = "Magic Suppression"),
    MoveSpeed           UMETA(DisplayName = "Move Speed"),
    CritChance          UMETA(DisplayName = "Crit Chance"),
    CooldownReduc       UMETA(DisplayName = "Cooldown Reduction"),
    ArmourShred         UMETA(DisplayName = "Armour Shred"),
    MagicShred          UMETA(DisplayName = "Magic Shred")
};

UENUM(BlueprintType)
enum class EPlayerClass : uint8
{
    None        UMETA(DisplayName = "No Class"),
    Sniper      UMETA(DisplayName = "Sniper"),
    Mage        UMETA(DisplayName = "Mage")
};

UENUM(BlueprintType)
enum class EItemPassive : uint8
{
    None            UMETA(DisplayName = "None"),
    TrueShot        UMETA(DisplayName = "True Damage"),
    Burn            UMETA(DisplayName = "Burn"),
    LifeSteal       UMETA(DisplayName = "Life Steal"),
    Headhunter      UMETA(DisplayName = "Execute Low HP")
};

UENUM(BlueprintType)
enum class EItemCategory : uint8
{
    Equipment       UMETA(DisplayName = "Equipment / Passive"),
    LootMaterial    UMETA(DisplayName = "Loot / Material")
};

USTRUCT(BlueprintType)
struct FDungeonItemInfo : public FTableRowBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    EItemCategory ItemCategory = EItemCategory::Equipment;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    FText Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    UTexture2D* Icon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data", meta = (MultiLine = true))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    int32 GoldCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
    TArray<FName> RequiredMaterials;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    bool bIsLegendary = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float HealthBonus = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float PhysicalDamageBonus = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float MagicDamageBonus = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float ArmourBonus = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float MagicSuppressionBonus = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float CritChanceBonus = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float MoveSpeedBonus = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float LifeStealBonus = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class System")
    EPlayerClass ClassType = EPlayerClass::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class System")
    EItemPassive PassiveEffect = EItemPassive::None;
};

class UCameraComponent;
class AVoxelLayoutGenerator;

UCLASS()
class DISSERTATION_API ADungeonCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ADungeonCharacter();

protected:
    virtual void BeginPlay() override;
    void CustomTurn(float Value);
    void CustomLookUp(float Value);

public:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
    UCameraComponent* FirstPersonCameraComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Combat)
    UStaticMeshComponent* WeaponMesh;

    /**
     * @brief Pushes damage text data to the HUD widget.
     * @param DamageAmount The numerical damage calculated post-mitigation.
     * @param HitLocation World-space location of the impact for projection.
     * @param bIsCrit True if the attack bypassed standard damage ranges.
     * @param bIsMagic Determines the color (Purple vs Yellow).
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void ShowDamageNumber(float DamageAmount, FVector HitLocation, bool bIsCrit, bool bIsMagic);

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats Tracker")
    int32 Killed_Melee = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats Tracker")
    int32 Killed_Flying = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats Tracker")
    float TotalDamageDealt = 0.0f;

    /** * @brief Adds internal counter for melee enemies defeated.
     */
    UFUNCTION(BlueprintCallable, Category = "Stats Tracker")
    void RecordMeleeKill();

    /** * @brief Adds internal counter for flying enemies defeated (The bats).
     */
    UFUNCTION(BlueprintCallable, Category = "Stats Tracker")
    void RecordFlyingKill();

    UPROPERTY(BlueprintReadWrite, Category = "Stats")
    int32 HealthPotions = 0;

    /** * @brief Consumes a health potion from inventory, applying flat healing up to MaxHealth.
     */
    UFUNCTION(BlueprintCallable, Category = "Stats")
    void UseHealthPotion();

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Class System")
    EPlayerClass CurrentClass = EPlayerClass::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Class System")
    EItemPassive CurrentPassive = EItemPassive::None;

    UFUNCTION(BlueprintImplementableEvent, Category = "Grapple")
    void PlayGrappleVisual(FVector TargetLocation);

    UFUNCTION(BlueprintImplementableEvent, Category = "Grapple")
    void StopGrappleVisual();

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Balancing|Base Stats")
    float BaseMaxHealth = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Balancing|Base Stats")
    float BasePhysicalDamage = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Balancing|Base Stats")
    float BaseMoveSpeed = 600.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Balancing|Economy")
    int32 StartingGold = 100;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Balancing|Combat")
    float MaxCarveDistance = 5000.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats")
    float MaxHealth;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats")
    float PhysicalDamage;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats")
    float MagicDamage = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats")
    float Armour = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats")
    float MagicSuppression = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats")
    float ArmourShred = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats")
    float MagicShred = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats")
    float CritChance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float CritMultiplier = 2.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats")
    float LifeSteal = 0.0f;

    UFUNCTION(BlueprintPure, Category = "HUD")
    int32 GetCurrentAmmo() const;

    UFUNCTION(BlueprintPure, Category = "HUD")
    int32 GetMaxAmmo() const;

    UFUNCTION(BlueprintPure, Category = "HUD")
    FString GetRoomStatus() const;

    UFUNCTION(BlueprintPure, Category = "HUD")
    float GetHealthPercent() const;

    UFUNCTION(BlueprintPure, Category = "HUD")
    float GetCurrentHealth() const;

    UFUNCTION(BlueprintPure, Category = "HUD")
    FString GetPlayerStatsString() const;

    UPROPERTY(BlueprintReadOnly, Category = "UI")
    int32 CrosshairState = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Inventory")
    TArray<FName> EquipmentList;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Inventory")
    TArray<FName> LootBag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balancing|Inventory")
    int32 MaxEquipmentSlots = 8;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balancing|Inventory")
    int32 MaxLootSlots = 20;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    FString GetEquipmentString() const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    FString GetMaterialsString() const;

    /** * @brief Safely adds an item to the player's inventory if class and slot constraints allow.
     * @param ItemID The Row namne of the item located in DT_ShopItems bluueprint.
     * @return True if successfully added to inventory, false if inventory full or class restricted.
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool GrantItemToPlayer(FName ItemID);

    /** * @brief Processes crafting transaction logic and handles material consumption and granting equipment.
     * @param ItemID The Row Name of the item to craft.
     * @return True if the transaction was successful and the item was created.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool CraftItem(FName ItemID);

    /** * @brief Removes an item from the player's inventory and refunds a percentage of its gold value.
     * @param ItemID The Row Name of the item to sell.
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SellItem(FName ItemID);

    /** * @brief Iterates through current equipment to dynamically recalculate all combat statistics.
     * Should be called anytime inventory contents change.
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void RecalculateStats();

    void MoveForward(float Value);
    void MoveRight(float Value);
    virtual void Jump() override;
    void FireGrapple();
    void StopGrapple();
    void TryInteract();
    void OnCarveInput();

    void StartPrimaryAction();
    void StopPrimaryAction();
    void StartSecondaryAction();
    void StopSecondaryAction();

    void PerformMeleeAttack();
    void PerformRangedAttack();

    void Reload();

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Economy")
    int32 CurrentGold = 0;

    /** * @brief Adds the specified amount of currency to the player.
     * @param Amount Numerical value of gold to inject.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    void AddGold(int32 Amount);

    /** * @brief Decouples the character's movement logic to safely hand cursor control to Widgets.
     * @param bOpen True unlocks the mouse, False re-locks the mouse to the viewport.
     */
    UFUNCTION(BlueprintCallable, Category = "UI")
    void SetMenuState(bool bOpen);

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "UI")
    bool bIsMenuOpen = false;

protected:
    virtual void FellOutOfWorld(const class UDamageType& dmgType) override;
    FVector SafeSpawnLocation;
    void Die();

    UFUNCTION(BlueprintImplementableEvent, Category = "Game Flow")
    void OnPlayerDied();

    bool bIsGrappling = false;
    FTimerHandle TimerHandle_GrappleDelay;
    void ExecuteGrapplePull();

    UPROPERTY(EditDefaultsOnly, Category = "Balancing|Movement")
    float GrappleDelay = 0.15f;
    FVector GrappleTargetLocation;
    FTimerHandle TimerHandle_HandleFire;
    bool bIsSniperAiming = false;

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    int32 CurrentAmmo;

    UPROPERTY(EditDefaultsOnly, Category = "Balancing|Combat")
    int32 MaxAmmo = 30;

    UPROPERTY(EditDefaultsOnly, Category = "Balancing|Combat")
    float FireRate = 0.4f;

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    float LastAttackTime = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Balancing|Combat")
    float WeaponRange = 5000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Balancing|Combat")
    float WeaponSpread = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Balancing|Combat")
    UParticleSystem* TracerEffect;

    UPROPERTY(VisibleAnywhere, Category = "Dungeon")
    AVoxelLayoutGenerator* CachedLayoutGenerator;

    UPROPERTY(EditDefaultsOnly, Category = "Balancing|Movement")
    float GrappleSpeed = 1500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Balancing|Movement")
    float GrappleRange = 3000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Balancing|Interaction")
    float InteractionRange = 300.0f;

    UPROPERTY(VisibleAnywhere, Category = "Health")
    float CurrentHealth;
};