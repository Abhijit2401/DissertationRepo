# DissertationRepo

# Destructible Voxel Dungeon (UE5, C++)

My final-year dissertation at City St George's (86.2%). Its a dungeon crawler in Unreal Engine 5 where the player casts a spell to carve new rooms out of solid rock at runtime. If a room won't fit, it carves a connecting tunnel instead.

PDF can be found on my portfolio: https://abhijit2401.github.io/

## What I built
I wrote about 1,450 lines of C++ across these classes:

| Class | What it does |
|---|---|
| `VoxelLayoutGenerator` | Checks for space, carves rooms and tunnels, and runs the carving off the game thread |
| `DungeonCharacter` | The player: Sniper and Mage classes, stats and equipment |
| `DungeonEnemy` | Enemy AI as a state machine: pursue, telegraph, attack, dead |
| `Shopkeeper` / `Interactable` | The shop and anything the player can interact with |

The voxel meshing and data structures come from the [Voxel Plugin](https://voxelplugin.com) (Free Legacy version, MIT licence), which uses Marching Cubes. Everything above is mine.

## How it works
**Checking for space:** before carving, `CanCarveRoom` builds a slightly padded bounding box around the new room and asks the voxel octree if there's any air inside it. If there is, the room would break into an existing cave, so it carves a tunnel instead. `ScanTunnelForAir` steps along the aim direction to find where that tunnel should end.

**Threading:** carving ~108,000 voxels on the game thread caused big frame drops. I moved the carving loop onto worker threads with Unreal's `AsyncTask` and a voxel write lock. Spawning actors off the game thread crashes, so they're handed back to the game thread through a callback.

## Results
Measured on an i7-12700H / RTX 3060 laptop:

| Test | Result |
|---|---|
| 1% low frame time while generating a room | **45.2 ms → 17.1 ms** after threading |
| Total carve time (108,000 voxels) | 9.06 ms on the game thread vs 9.37 ms threaded |
| 100 casts in open space | 96 rooms, 4 tunnels, 0 blocked |
| 100 casts in a dense cave network | 18 rooms, 75 tunnels, 7 blocked |
| Memory over 10 to 50 rooms | Stayed around 2.6 GB with no sign of leaks |

The threaded version is slightly slower in total, but that wasn't the goal. 9 ms is over half of a 60 fps frame, so moving it off the game thread is what stops the stutter.

## What I'd improve
- Enemies use direct pursuit, because Unreal's runtime NavMesh didn't work on freshly carved rooms. Melee enemies can get stuck on the voxel "stairs" in round rooms.

## Running it
- Unreal Engine 5.4
- Voxel Plugin (Free Legacy) installed
- Open `Dissertation/Dissertation.uproject`, let it build, then press Play

More of my work: [abhijit2401.github.io](https://abhijit2401.github.io)
