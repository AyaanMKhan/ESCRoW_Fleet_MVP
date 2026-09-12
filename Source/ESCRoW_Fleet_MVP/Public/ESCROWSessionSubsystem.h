// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ESCROWTypes.h"
#include "ESCROWSessionSubsystem.generated.h"

/**
 * Persistent cross-map experiment/session state: selected mission condition,
 * participant/session identity, and the manual-driving/takeover-required flags
 * derived from the mission condition. Survives the main-menu-to-mission map
 * transition. Live world/fleet state belongs to UESCROWFleetSubsystem, not here.
 */
UCLASS(BlueprintType)
class ESCROW_FLEET_MVP_API UESCROWSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	FString ParticipantID;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	FString SessionID;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	EESCROWMissionCondition SelectedMissionCondition = EESCROWMissionCondition::MissionD_DriverTakeover;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	bool bManualOwnVehicleControl = true;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	bool bTakeoverRequired = true;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	bool bSessionInitialized = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	float SessionStartTimeSeconds = 0.0f;

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void ResetSessionState();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void StartSession();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void SetParticipantID(const FString& NewParticipantID);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void SetSessionID(const FString& NewSessionID);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void SetMissionCondition(EESCROWMissionCondition NewMissionCondition);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	EESCROWMissionCondition GetMissionCondition() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	FString GetMissionConditionName() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	bool IsManualOwnVehicleControlRequired() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	bool IsTakeoverRequired() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	FString GetParticipantID() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	FString GetSessionID() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	bool IsSessionInitialized() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	float GetSessionStartTimeSeconds() const;

private:
	void ApplyMissionConditionFlags();
};
