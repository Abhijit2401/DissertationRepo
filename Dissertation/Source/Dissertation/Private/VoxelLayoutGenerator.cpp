#include "VoxelLayoutGenerator.h" 
#include "DungeonEnemy.h"
#include "Async/Async.h"
#include "Kismet/GameplayStatics.h"
#include "VoxelData/VoxelData.h"
#include "VoxelTools/Impl/VoxelSphereToolsImpl.inl" 
#include "Components/SceneComponent.h"
#include "VoxelTools/VoxelDataTools.h" 
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

AVoxelLayoutGenerator::AVoxelLayoutGenerator()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AVoxelLayoutGenerator::BeginPlay()
{
    Super::BeginPlay();
    FindVoxelWorld();
    SetPlayerGravityEnabled(false);
    TotalRoomsCreated = 0;
    bIsPostBossShopPending = false;
}

void AVoxelLayoutGenerator::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!VoxelWorldReference || !VoxelWorldReference->IsLoaded()) return;

    // Startup step 0: Build the first safe room right where the player spawns
    if (StartupStep == 0)
    {
        FVector StartLoc = GetActorLocation();
        CarveBoxRoom(StartLoc, RoomDimensions, 0, false);

        if (ShopContentClass)
        {
            float FloorZ = StartLoc.Z - 500.0f;
            FVector ShopLoc = StartLoc;
            ShopLoc.X += 400.0f;
            ShopLoc.Z = FloorZ + 250.0f;
            GetWorld()->SpawnActor<AActor>(ShopContentClass, ShopLoc, FRotator(0, 180, 0));
        }

        StartupStep = 1;
        return;
    }

    // Startup step 1: Turns off player gravity so you don't fall through the floor while the first room is still loading
    if (StartupStep == 1)
    {
        APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
        if (!PlayerPawn) return;

        FVector PlayerLoc = PlayerPawn->GetActorLocation();
        FVector TraceEnd = PlayerLoc - FVector(0, 0, 1000);
        FHitResult Hit;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(PlayerPawn);

        if (GetWorld()->LineTraceSingleByChannel(Hit, PlayerLoc, TraceEnd, ECC_Visibility, Params))
        {
            SetPlayerGravityEnabled(true);
            StartupStep = 2; // Generation done nmeaning the game can start normally 
        }
    }
}

void AVoxelLayoutGenerator::SetPlayerGravityEnabled(bool bEnabled)
{
    ACharacter* PlayerChar = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (PlayerChar && PlayerChar->GetCharacterMovement())
    {
        if (bEnabled)
        {
            PlayerChar->GetCharacterMovement()->GravityScale = 1.0f;
            PlayerChar->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        }
        else
        {
            PlayerChar->GetCharacterMovement()->GravityScale = 0.0f;
            PlayerChar->GetCharacterMovement()->StopMovementImmediately();
            PlayerChar->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
        }
    }
}

void AVoxelLayoutGenerator::RegisterEnemy() { ActiveEnemyCount++; }

void AVoxelLayoutGenerator::UnregisterEnemy()
{
    ActiveEnemyCount--;
    if (ActiveEnemyCount < 0) ActiveEnemyCount = 0;
}

bool AVoxelLayoutGenerator::IsRoomClear() const { return ActiveEnemyCount == 0; }

bool AVoxelLayoutGenerator::AttemptBuildAtLocation(FVector StartLocation, FVector Direction)
{
    if (!VoxelWorldReference || !VoxelWorldReference->IsLoaded()) return false;

    // Flattens the direction so the rooms snap to a grid nicely instead of going diagonal
    FVector FlatDir = GetSnappedDirection(Direction);

    FIntVector ChosenRoomSize = RoomDimensions;
    int32 ChosenRoomType = FMath::RandRange(0, 5);

    switch (ChosenRoomType)
    {
    case 0: ChosenRoomSize = FIntVector(22, 22, 14); break; // Cube
    case 1: ChosenRoomSize = FIntVector(24, 24, 20); break; // Dome
    case 2: ChosenRoomSize = FIntVector(26, 26, 14); break; // Octagon
    case 3: ChosenRoomSize = FIntVector(24, 24, 24); break; // Sphere
    case 4: ChosenRoomSize = FIntVector(40, 14, 12); break; // Long Cuboid
    case 5: ChosenRoomSize = FIntVector(30, 30, 10); break; // Wide Dungeon Room
    }

    float TotalTunnelLength = FMath::RandRange(800.0f, 3500.0f);
    float ExactRadiusVoxels;

    // Figure out how big the radius is depending on what axis we are carving on
    if (FMath::Abs(FlatDir.Z) > 0.5f) ExactRadiusVoxels = ChosenRoomSize.Z / 2.0f;
    else if (FMath::Abs(FlatDir.X) > 0.5f) ExactRadiusVoxels = ChosenRoomSize.X / 2.0f;
    else ExactRadiusVoxels = ChosenRoomSize.Y / 2.0f;

    float RoomPushRadiusUnits = ExactRadiusVoxels * 100.0f;
    FVector TunnelEnd = StartLocation + (FlatDir * TotalTunnelLength);
    FVector ProposedRoomCenter = TunnelEnd + (FlatDir * RoomPushRadiusUnits);

    // Carves a jagged tunnel so it looks like a real cave instead of a perfectly straight tube
    auto CarveJaggedTunnel = [&](FVector Start, FVector End)
        {
            int32 NumSegments = 6;
            FVector CurrentPoint = Start;
            FVector FullLine = End - Start;

            for (int32 i = 1; i <= NumSegments; i++)
            {
                FVector NextPoint;
                if (i == NumSegments) NextPoint = End + (FlatDir * 500.0f);
                else
                {
                    float Alpha = (float)i / NumSegments;
                    FVector StraightPoint = Start + (FullLine * Alpha);
                    FVector Noise = FMath::VRand() * 400.0f;
                    Noise.Z = FMath::RandRange(-50.0f, 50.0f);
                    NextPoint = StraightPoint + Noise;
                }
                CarveTunnel(CurrentPoint, NextPoint);
                CurrentPoint = NextPoint;
            }
        };

    // If there's enough space the carving works
    if (CanCarveRoom(ProposedRoomCenter, ChosenRoomSize))
    {
        CarveJaggedTunnel(StartLocation, TunnelEnd);
        CarveBoxRoom(ProposedRoomCenter, ChosenRoomSize, ChosenRoomType, true);
        return true;
    }

    // Failsafe: if there isn't space for a whole room it justs carves a tunnel into whatever cave or old tunnel or old room is blocking it
    FVector IntersectionPoint;
    FVector SonarEnd = StartLocation + (FlatDir * (TotalTunnelLength * 2.0f));

    if (ScanTunnelForAir(StartLocation, SonarEnd, IntersectionPoint))
    {
        CarveJaggedTunnel(StartLocation, IntersectionPoint);
        VoxelWorldReference->RecreateRender();
        return false; // Return false so ammo isn't wasted just for digging a tunnel
    }

    return false;
}

void AVoxelLayoutGenerator::CarveBoxRoom(FVector CenterLocation, FIntVector RoomSize, int32 RoomType, bool bSpawnEnemies)
{
    if (!VoxelWorldReference) return;

    // Sends the voxel carving to a background thread so the whole game doesn't lag/freeze
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, CenterLocation, RoomSize, RoomType, bSpawnEnemies]()
        {
            FVoxelData& Data = VoxelWorldReference->GetData();
            FIntVector VoxelCenter = VoxelWorldReference->GlobalToLocal(CenterLocation);
            FIntVector HalfSize = RoomSize / 2;
            FVoxelIntBox Bounds(VoxelCenter - HalfSize, VoxelCenter + HalfSize);

            {
                // Locks sthe voxel thread so it doesn't crash while editing the stone

                FVoxelWriteScopeLock Lock(Data, Bounds, "CarveRoom_Async");

                for (int32 Z = Bounds.Min.Z; Z < Bounds.Max.Z; Z++)
                {
                    for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; Y++)
                    {
                        for (int32 X = Bounds.Min.X; X < Bounds.Max.X; X++)
                        {
                            bool bCarveVoxel = false;

                            // Calculate distances to figure out what shape to carve
                            int32 DistX = FMath::Abs(X - VoxelCenter.X);
                            int32 DistY = FMath::Abs(Y - VoxelCenter.Y);
                            int32 DistZ = FMath::Abs(Z - VoxelCenter.Z);

                            if (RoomType == 0 || RoomType == 4 || RoomType == 5) bCarveVoxel = true; // Cuboids
                            else if (RoomType == 1) // Dome shape
                            {
                                if (Z < VoxelCenter.Z) bCarveVoxel = true;
                                else
                                {
                                    float Dist3D = FVector(DistX, DistY, DistZ).Size();
                                    if (Dist3D <= HalfSize.X) bCarveVoxel = true;
                                }
                            }
                            else if (RoomType == 2) // Octagon
                            {
                                if (DistX < HalfSize.X * 0.4f || DistY < HalfSize.Y * 0.4f || (DistX + DistY) < HalfSize.X * 1.2f)
                                {
                                    bCarveVoxel = true;
                                }
                            }
                            else if (RoomType == 3) // Sphere
                            {
                                float Dist3D = FVector(DistX, DistY, DistZ).Size();
                                if (Dist3D <= HalfSize.X) bCarveVoxel = true;
                            }

                            if (bCarveVoxel) Data.SetValue(X, Y, Z, FVoxelValue(1.0f)); // 1.0f deletes the stone
                        }
                    }
                }
            }

            // Jump back to the main game thread to spawn the enemies (Unreal crashes if you spawn things on background threads)
            AsyncTask(ENamedThreads::GameThread, [this, CenterLocation, RoomSize, RoomType, bSpawnEnemies]()
                {
                    if (bSpawnEnemies)
                    {
                        TotalRoomsCreated++;

                        float RoomHeight = RoomSize.Z * 100.0f;
                        float FloorZ = CenterLocation.Z - (RoomHeight / 2.0f);
                        FVector SpawnLoc = CenterLocation;

                        // Move the spawn point up a bit so the player doesn't spawn stuck in the floor
                        SpawnLoc.Z = FloorZ + 400.0f;

                        if (TotalRoomsCreated % BossInterval == 0 && BossContentClass) {
                            AActor* Boss = GetWorld()->SpawnActor<AActor>(BossContentClass, SpawnLoc, FRotator::ZeroRotator);
                            if (APawn* P = Cast<APawn>(Boss)) P->SpawnDefaultController();
                            RegisterEnemy();
                            bIsPostBossShopPending = true;
                        }
                        else if (bIsPostBossShopPending && ShopContentClass)
                        {
                            GetWorld()->SpawnActor<AActor>(ShopContentClass, SpawnLoc, FRotator::ZeroRotator);
                            bIsPostBossShopPending = false;
                        }
                        else if (FMath::FRand() <= ShopSpawnChance && ShopContentClass)
                        {
                            GetWorld()->SpawnActor<AActor>(ShopContentClass, SpawnLoc, FRotator::ZeroRotator);
                        }
                        else if (EnemyPool.Num() > 0)
                        {
                            int32 NumEnemies = FMath::RandRange(MinEnemiesPerRoom, MaxEnemiesPerRoom);

                            // Calculates a safe circle in the middle of the room so enemies don't spawn stuck in the walls too
                            float SafeRadius = FMath::Min(RoomSize.X, RoomSize.Y) * 100.0f * 0.30f;

                            for (int i = 0; i < NumEnemies; i++)
                            {
                                int32 RandomIdx = FMath::RandRange(0, EnemyPool.Num() - 1);
                                TSubclassOf<ADungeonEnemy> SelectedClass = EnemyPool[RandomIdx];

                                if (SelectedClass)
                                {
                                    FVector2D RandomPoint = FMath::RandPointInCircle(SafeRadius);
                                    FVector Offset(RandomPoint.X, RandomPoint.Y, 0.0f);

                                    // Force them to spawn even if the NavMesh hasn't finished loading yet
                                    FActorSpawnParameters SpawnParams;
                                    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                                    AActor* Enemy = GetWorld()->SpawnActor<AActor>(SelectedClass, SpawnLoc + Offset, FRotator::ZeroRotator, SpawnParams);
                                    if (APawn* P = Cast<APawn>(Enemy)) P->SpawnDefaultController();
                                    RegisterEnemy();
                                }
                            }
                        }
                    }
                    VoxelWorldReference->RecreateRender();
                });
        });
}

bool AVoxelLayoutGenerator::CanCarveRoom(FVector CenterLocation, FIntVector RoomSize)
{
    if (!VoxelWorldReference) return false;
    FVoxelData& Data = VoxelWorldReference->GetData();
    FIntVector VoxelCenter = VoxelWorldReference->GlobalToLocal(CenterLocation);
    FIntVector HalfSize = RoomSize / 2;
    FIntVector Buffer(2, 2, 2);

    // Makes the box bigger just to be safe
    FVoxelIntBox Bounds(VoxelCenter - HalfSize - Buffer, VoxelCenter + HalfSize + Buffer);

    FVoxelReadScopeLock Lock(Data, Bounds, "SpaceCheck");
    TVoxelRange<FVoxelValue> ValueRange = Data.GetValueRange(Bounds, 0);

    // If the box is completely full of stone, it's safe to carve here
    return !ValueRange.Max.IsEmpty();
}

int32 AVoxelLayoutGenerator::CheckCarveStatus(FVector StartLocation, FVector Direction)
{
    if (!VoxelWorldReference || !VoxelWorldReference->IsLoaded()) return 0;

    FVector FlatDir = GetSnappedDirection(Direction);

    float TestTunnelLength = 1500.0f;
    FIntVector TestRoomSize = RoomDimensions;
    float ExactRadiusVoxels;

    if (FMath::Abs(FlatDir.Z) > 0.5f) ExactRadiusVoxels = TestRoomSize.Z / 2.0f;
    else if (FMath::Abs(FlatDir.X) > 0.5f) ExactRadiusVoxels = TestRoomSize.X / 2.0f;
    else ExactRadiusVoxels = TestRoomSize.Y / 2.0f;

    float RoomPushRadiusUnits = ExactRadiusVoxels * 100.0f;
    FVector ProposedRoomCenter = StartLocation + (FlatDir * TestTunnelLength) + (FlatDir * RoomPushRadiusUnits);

    if (CanCarveRoom(ProposedRoomCenter, TestRoomSize)) return 1;

    FVector IntersectionPoint;
    if (ScanTunnelForAir(StartLocation, StartLocation + (FlatDir * 3000.0f), IntersectionPoint)) return 2;

    return 0;
}

void AVoxelLayoutGenerator::FindVoxelWorld()
{
    if (VoxelWorldReference) return;
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("MainVoxelWorld"), FoundActors);
    if (FoundActors.Num() > 0) VoxelWorldReference = Cast<AVoxelWorld>(FoundActors[0]);
}

bool AVoxelLayoutGenerator::ScanTunnelForAir(FVector Start, FVector End, FVector& HitPoint)
{
    if (!VoxelWorldReference) return false;
    FVoxelData& Data = VoxelWorldReference->GetData();
    float Distance = FVector::Dist(Start, End);
    int32 Steps = Distance / 50.0f;
    bool bEnteredRock = false;

    // Math loop to step through the vector instead of a physics trace because it's easier to run
    for (int32 i = 0; i < Steps; i++)
    {
        FVector TestPos = FMath::Lerp(Start, End, (float)i / Steps);
        FIntVector VoxelPos = VoxelWorldReference->GlobalToLocal(TestPos);
        FVoxelReadScopeLock Lock(Data, FVoxelIntBox(VoxelPos, VoxelPos + 1), "ScanAir");

        bool bIsAir = Data.GetValue(VoxelPos, 0).IsEmpty();
        if (!bIsAir) bEnteredRock = true;
        else if (bIsAir && bEnteredRock) { HitPoint = TestPos; return true; }
    }
    return false;
}

void AVoxelLayoutGenerator::CarveTunnel(FVector Start, FVector End)
{
    if (!VoxelWorldReference) return;
    FVoxelData& Data = VoxelWorldReference->GetData();
    float Distance = FVector::Dist(Start, End);

    float SafeTunnelWidth;
    if (TunnelWidth > 0.0f)
    {
        SafeTunnelWidth = TunnelWidth;
    }
    else
    {
        SafeTunnelWidth = 400.0f;
    }

    int32 NumSteps = FMath::CeilToInt(Distance / (SafeTunnelWidth * 0.5f));
    if (NumSteps <= 0) NumSteps = 1;

    // Moves along the calculated line and carves out a bunch of spheres to make the tunnel
    for (int32 i = 0; i <= NumSteps; i++)
    {
        FVector CarvePos = FMath::Lerp(Start, End, (float)i / (float)NumSteps);
        FVoxelVector VoxelPosition = VoxelWorldReference->GlobalToLocalFloat(CarvePos);
        float VoxelRadius = SafeTunnelWidth / VoxelWorldReference->VoxelSize;
        FIntVector Center = VoxelWorldReference->GlobalToLocal(CarvePos);
        int32 Rad = FMath::CeilToInt(VoxelRadius);
        FVoxelIntBox Bounds(Center - Rad, Center + Rad + 1);

        {
            FVoxelWriteScopeLock Lock(Data, Bounds, "Tunnel");
            FVoxelSphereToolsImpl::RemoveSphere(Data, VoxelPosition, VoxelRadius);
        }
    }
}
FVector AVoxelLayoutGenerator::GetSnappedDirection(FVector Direction) const
{
    FVector FlatDir = FVector::ZeroVector;
    FVector AbsDir = Direction.GetAbs();

    if (AbsDir.Z > AbsDir.X && AbsDir.Z > AbsDir.Y) FlatDir = FVector(0.0f, 0.0f, FMath::Sign(Direction.Z));
    else if (AbsDir.X > AbsDir.Y) FlatDir = FVector(FMath::Sign(Direction.X), 0.0f, 0.0f);
    else FlatDir = FVector(0.0f, FMath::Sign(Direction.Y), 0.0f);

    return FlatDir;
}