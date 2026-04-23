// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Prototyping : ModuleRules
{
	public Prototyping(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(new string[] { "Prototyping" });



		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Networking", "Sockets", "Json", "JsonUtilities", "UMG", "Slate", "SlateCore", "Niagara", "ApplicationCore", "AIModule", "NavigationSystem" });

		PrivateDependencyModuleNames.AddRange(new string[] { "NiagaraCore", "NiagaraShader" });

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
