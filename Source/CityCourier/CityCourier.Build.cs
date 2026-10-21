using UnrealBuildTool;

public class CityCourier : ModuleRules
{
    public CityCourier(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicIncludePaths.Add(ModuleDirectory);
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore"
        });
        if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AnimGraph", "BlueprintGraph", "KismetCompiler", "Json", "JsonUtilities" });
        PublicDependencyModuleNames.Add("AnimGraphRuntime");
        PrivateDependencyModuleNames.Add("RHI");
        PrivateDependencyModuleNames.Add("RenderCore");
        PrivateDependencyModuleNames.Add("AudioMixer");
        PrivateDependencyModuleNames.Add("NavigationSystem");
    }
}
