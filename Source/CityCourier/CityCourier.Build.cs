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
        if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AnimGraph", "BlueprintGraph", "KismetCompiler", "Json", "JsonUtilities", "IKRigEditor", "AssetTools", "AssetRegistry", "PoseSearchEditor", "ControlRigDeveloper" });
        PublicDependencyModuleNames.AddRange(new string[] { "PoseSearch", "Chooser", "MotionWarping", "IKRig", "ControlRig" });
        PublicDependencyModuleNames.Add("AnimGraphRuntime");
        PublicDependencyModuleNames.Add("RigVM");
        PrivateDependencyModuleNames.Add("AnimationCore");
        PrivateDependencyModuleNames.Add("PhysicsCore");
        if(Target.bBuildEditor) PrivateDependencyModuleNames.Add("PhysicsUtilities");
        PrivateDependencyModuleNames.Add("RHI");
        PrivateDependencyModuleNames.Add("RenderCore");
        PrivateDependencyModuleNames.Add("AudioMixer");
        PrivateDependencyModuleNames.Add("NavigationSystem");
    }
}
