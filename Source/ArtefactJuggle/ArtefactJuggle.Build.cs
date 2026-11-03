// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ArtefactJuggle : ModuleRules
{
	public ArtefactJuggle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ArtefactJuggle",
			"ArtefactJuggle/Variant_Platforming",
			"ArtefactJuggle/Variant_Platforming/Animation",
			"ArtefactJuggle/Variant_Combat",
			"ArtefactJuggle/Variant_Combat/AI",
			"ArtefactJuggle/Variant_Combat/Animation",
			"ArtefactJuggle/Variant_Combat/Gameplay",
			"ArtefactJuggle/Variant_Combat/Interfaces",
			"ArtefactJuggle/Variant_Combat/UI",
			"ArtefactJuggle/Variant_SideScrolling",
			"ArtefactJuggle/Variant_SideScrolling/AI",
			"ArtefactJuggle/Variant_SideScrolling/Gameplay",
			"ArtefactJuggle/Variant_SideScrolling/Interfaces",
			"ArtefactJuggle/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
