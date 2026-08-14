#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DungeonEnemy.generated.h"

class AVoxelLayoutGenerator;
class UBoxComponent;

UENUM(BlueprintType)
enum class EEnemyType : uint8
{
	MeleeEnemy   UMETA(DisplayName = "Melee Enemy"),
	FlyingEnemy  UMETA(DisplayName = "Flying Enemy"),
	Boss         UMETA(DisplayName = "Boss Enemy")
};

UCLASS()
class DISSERTATION_API ADungeonEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	ADungeonEnemy();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	EEnemyType EnemyBehavior = EEnemyType::MeleeEnemy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Balancing|Combat")
	float MaxHealth = 50.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	float CurrentHealth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UBoxComponent* WeaponHitbox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UStaticMeshComponent* TelegraphIndicator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FName ItemToDrop = "CoagulatedBlood";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	int32 DropChancePercentage = 25;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Balancing|Loot")
	int32 GoldDropAmount = 10;

	UPROPERTY(EditDefaultsOnly, Category = "Balancing|Combat")
	float AttackRange = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Balancing|Combat")
	float AttackDamage = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Balancing|Combat")
	float TelegraphDelay = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Balancing|Combat")
	float DashSpeed = 2500.0f;

	bool bIsAttacking = false;
	bool bIsSwinging = false;
	FVector SavedDashDirection;

	/** * @brief Starts the windup for the attack depending on what enemy it is.
	 * Stops them moving and shows the red warning mesh to show attack range.
	 */
	void StartAttackSequence();

	/** * @brief Actually does the attack after the delay finishes.
	 * Turns on the weapon hitbox and launches flying enemies forward.
	 */
	void ExecuteStrike();

	/** * @brief Resets the AI back to normal chasing mode.
	 * Turns off hitboxes and hides the red warning mesh.
	 */
	void ResetAttack();

	/** * @brief Custom slam attack just for the Boss.
	 * Does AOE radial damage on the floor and knocks the player back if they get hit.
	 */
	void ExecuteBossSlam();

	/** * @brief Turns on the weapon collision so it can actually hit the player.
	 * Sent through to bueprints so I can trigger it exactly when the animation goes through.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void EnableWeaponHitbox();

	/** * @brief Turns off the weapon collision so it doesn't hit the player twice in one swing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void DisableWeaponHitbox();

	UFUNCTION()
	void OnWeaponOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

private:
	UPROPERTY()
	AActor* TargetActor;

	UPROPERTY()
	AVoxelLayoutGenerator* Director;

	FTimerHandle TimerHandle_AttackDelay;
	FTimerHandle TimerHandle_AttackReset;

	void Die();
};