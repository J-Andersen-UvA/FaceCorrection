using UnrealBuildTool;
using System.IO;

public class FaceCorrectionEditor : ModuleRules
{
    public FaceCorrectionEditor(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "FaceCorrection"
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "CoreUObject",
                "ContentBrowser",
                "AssetRegistry",
                "AnimationBlueprintLibrary",
                "AnimationDataController",
                "Engine",
                "InputCore",
                "PropertyEditor",
                "Slate",
                "SlateCore",
                "UnrealEd"
            });

        string pluginsDirectory = Path.GetFullPath(Path.Combine(PluginDirectory, ".."));
        string handposeTimelineDirectory = Path.Combine(
            pluginsDirectory,
            "HandposeTimeline",
            "Source",
            "HandposeTimeline");
        if (Directory.Exists(handposeTimelineDirectory))
        {
            PrivateDependencyModuleNames.Add("HandposeTimeline");
            PublicDefinitions.Add("WITH_HANDPOSE_TIMELINE=1");
        }
        else
        {
            PublicDefinitions.Add("WITH_HANDPOSE_TIMELINE=0");
        }

        string piperDirectory = Path.Combine(
            pluginsDirectory,
            "PiperTTS",
            "Source",
            "PiperTTS");
        if (Directory.Exists(piperDirectory))
        {
            PrivateDependencyModuleNames.Add("PiperTTS");
            PublicDefinitions.Add("WITH_PIPER_TTS=1");
        }
        else
        {
            PublicDefinitions.Add("WITH_PIPER_TTS=0");
        }
    }
}
