// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class ESCRoW_Fleet_MVPEditorTarget : TargetRules
{
	public ESCRoW_Fleet_MVPEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;

		ExtraModuleNames.AddRange( new string[] { "ESCRoW_Fleet_MVP" } );
	}
}
