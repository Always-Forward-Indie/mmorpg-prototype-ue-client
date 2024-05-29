// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class PrototypingEditorTarget : TargetRules
{
	public PrototypingEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("Prototyping");

        //BuildEnvironment = TargetBuildEnvironment.Unique;
        bOverrideBuildEnvironment = true;
    }
}
