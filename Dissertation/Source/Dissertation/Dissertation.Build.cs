// Dissertation.Build.cs

using UnrealBuildTool;

public class Dissertation : ModuleRules
{
    public Dissertation(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
        "Core", "CoreUObject", "Engine", "InputCore",
        "Voxel", "VoxelGraph", "UMG"});

        PrivateDependencyModuleNames.AddRange(new string[] { "Voxel" });
    }
}