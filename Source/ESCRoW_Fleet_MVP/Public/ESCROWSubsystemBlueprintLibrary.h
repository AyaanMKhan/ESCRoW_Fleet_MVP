// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ESCROWSubsystemBlueprintLibrary.generated.h"

class UESCROWFleetSubsystem;
class UESCROWSessionSubsystem;
class UESCROWEventLogSubsystem;

/**
 * Clean Blueprint/UMG access point for the ESCRoW subsystems, so widgets and
 * wrapper Blueprints do not each duplicate their own subsystem lookup logic.
 */
UCLASS()
class ESCROW_FLEET_MVP_API UESCROWSubsystemBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "ESCRoW|Subsystems", meta = (WorldContext = "WorldContextObject"))
	static UESCROWFleetSubsystem* GetESCROWFleetSubsystem(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Subsystems", meta = (WorldContext = "WorldContextObject"))
	static UESCROWSessionSubsystem* GetESCROWSessionSubsystem(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Subsystems", meta = (WorldContext = "WorldContextObject"))
	static UESCROWEventLogSubsystem* GetESCROWEventLogSubsystem(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Subsystems", meta = (WorldContext = "WorldContextObject"))
	static bool LogESCROWEvent(const UObject* WorldContextObject, const FString& EventType, const FString& EventText, int32 FollowerID);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Subsystems", meta = (WorldContext = "WorldContextObject"))
	static bool SelectESCROWFollower(const UObject* WorldContextObject, int32 FollowerID);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Subsystems", meta = (WorldContext = "WorldContextObject"))
	static bool SendESCROWFollowerMessage(const UObject* WorldContextObject, int32 FollowerID, const FString& MessageText);
};
