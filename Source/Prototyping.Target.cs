// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class PrototypingTarget : TargetRules
{
	public PrototypingTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("Prototyping");

        bOverrideBuildEnvironment = true;
    }
}
