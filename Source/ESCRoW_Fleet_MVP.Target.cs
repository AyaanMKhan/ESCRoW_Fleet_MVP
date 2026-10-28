// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class ESCRoW_Fleet_MVPTarget : TargetRules
{
	public ESCRoW_Fleet_MVPTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;

		ExtraModuleNames.AddRange( new string[] { "ESCRoW_Fleet_MVP" } );
	}
}
