#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelWorld.h" 
#include "VoxelIntBox.h"
#include "VoxelLayoutGenerator.generated.h" 

class ADungeonEnemy;

UCLASS()
class DISSERTATION_API AVoxelLayoutGenerator : public AActor
{
    GENERATED_BODY()

public:
    AVoxelLayoutGenerator();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    /** * @brief Tries to dig out a room where the player is looking.
     * Uses an async thread lock so the game doesn't freeze while loading the underground.
     * @param StartLocation Where the player's spell hit.
     * @param Direction The normal of the wall they hit.
     * @return True if there was enough space to build the room.
     */
    UFUNCTION(BlueprintCallable, Category = "Dungeon Gen")
    bool AttemptBuildAtLocation(FVector StartLocation, FVector Direction);

    /** * @brief Shoots raycasts to check if a room can actually fit in the stone walls.
     * @param StartLocation Where the player's spell hit.
     * @param Direction Normal of the wall.
     * @return 0 = Blocked, 1 = Safe to build, 2 = Found another tunnel to connect to.
     */
    UFUNCTION(BlueprintPure, Category = "Dungeon Gen")
    int32 CheckCarveStatus(FVector StartLocation, FVector Direction);

    /** * @brief Checks if you killed all the enemies in the current room yet.
     * @return True if the counter hit zero.
     */
    UFUNCTION(BlueprintPure, Category = "Dungeon Gen")
    bool IsRoomClear() const;

    void RegisterEnemy();
    void UnregisterEnemy();

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    AVoxelWorld* VoxelWorldReference;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Content")
    TArray<TSubclassOf<ADungeonEnemy>> EnemyPool;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Balancing|Spawns")
    int32 MinEnemiesPerRoom = 5;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Balancing|Spawns")
    int32 MaxEnemiesPerRoom = 15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Content")
    TSubclassOf<AActor> BossContentClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Content")
    TSubclassOf<AActor> ShopContentClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Balancing|Progression")
    int32 BossInterval = 10;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Balancing|Progression")
    float ShopSpawnChance = 0.1f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Balancing|Generation")
    FIntVector RoomDimensions = FIntVector(15, 15, 10);

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Balancing|Generation")
    float TunnelWidth = 200.0f;

    /** * @brief Checks the bounding box math to make sure rooms don't spawn inside each other.
     * @param CenterLocation Where we want to put the room.
     * @param RoomSize How big the room is.
     * @return True if the space is completely solid walls/stone.
     */
    bool CanCarveRoom(FVector CenterLocation, FIntVector RoomSize);

    /** * @brief Digs out the stone and spawns the enemies asynchronously.
     * @param CenterLocation Center of the new room.
     * @param RoomSize Size of the room.
     * @param RoomType ID for the shape as shown in the .cpp file.
     * @param bSpawnEnemies Should we drop enemies in here or is it just a safe room?
     */
    void CarveBoxRoom(FVector CenterLocation, FIntVector RoomSize, int32 RoomType, bool bSpawnEnemies = true);

    /** * @brief Checks to see if there's already air/another cave nearby to connect to.
     * @param Start Start of the trace.
     * @param End End of the trace.
     * @param HitPoint Spits out the exact spot it hit air.
     * @return True if it found a cave.
     */
    bool ScanTunnelForAir(FVector Start, FVector End, FVector& HitPoint);

    /** * @brief Carves a tunnel by cutting out spheres along a line.
     * @param Start Start of tunnel.
     * @param End End of tunnel.
     */
    void CarveTunnel(FVector Start, FVector End);

    void SetPlayerGravityEnabled(bool bEnabled);

private:
    void FindVoxelWorld();

    int32 StartupStep = 0;
    int32 ActiveEnemyCount = 0;
    bool bHasGeneratedSpawnRoom = false;
    int32 TotalRoomsCreated = 0;
    bool bIsPostBossShopPending = false;
    /** * @brief Helper function to flatten the look vector and snap it to a 3D grid axis so that bugs dont occur */
    FVector GetSnappedDirection(FVector Direction) const;
};