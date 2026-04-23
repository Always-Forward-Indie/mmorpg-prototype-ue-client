using UnrealBuildTool;

public class WorldMapExporter : ModuleRules
{
    public WorldMapExporter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "InputCore",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "LevelEditor",
            "Json",
            "JsonUtilities",
            "ImageWrapper",
            "DesktopPlatform",
            "RenderCore",
            "RHI"
        });
    }
}
