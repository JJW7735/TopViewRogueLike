// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RogueLikeTopView : ModuleRules
{
	public RogueLikeTopView(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"RogueLikeTopView",
			"RogueLikeTopView/Character/Public",
			"RogueLikeTopView/Combat/Public",
			"RogueLikeTopView/Skill/Public",
			"RogueLikeTopView/Stats/Public",
			"RogueLikeTopView/AI/Public",
			"RogueLikeTopView/Core/Public",
			"RogueLikeTopView/Player/Public",
			"RogueLikeTopView/Variant_Strategy",
			"RogueLikeTopView/Variant_Strategy/UI",
			"RogueLikeTopView/Variant_TwinStick",
			"RogueLikeTopView/Variant_TwinStick/AI",
			"RogueLikeTopView/Variant_TwinStick/Gameplay",
			"RogueLikeTopView/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
