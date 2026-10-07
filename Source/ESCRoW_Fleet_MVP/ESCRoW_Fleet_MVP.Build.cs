// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class ESCRoW_Fleet_MVP : ModuleRules
{
	public ESCRoW_Fleet_MVP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });

		// ChaosVehicles: reads the standard engine UChaosVehicleMovementComponent's
		// commanded throttle/steering/brake for driving-input telemetry (see
		// UESCROWEventLogSubsystem::PopulateFleetTelemetry). This is Epic's own
		// component API, not vendor (DD_Vehicles_Basic) graph logic.
		PrivateDependencyModuleNames.AddRange(new string[] { "ChaosVehicles" });

		// NavigationSystem: the Director's startup nav self-check reads the live nav mesh
		// agent size and tests real follower paths (see PilotRunNavDiagnostics).
		PrivateDependencyModuleNames.AddRange(new string[] { "NavigationSystem" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
