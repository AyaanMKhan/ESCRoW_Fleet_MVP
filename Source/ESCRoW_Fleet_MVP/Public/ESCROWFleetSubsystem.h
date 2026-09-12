// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ESCROWTypes.h"
#include "ESCROWFleetSubsystem.generated.h"

/**
 * Authoritative world-specific fleet state, command, message, report, and
 * one-follower takeover-lock bus for F1-F4. HUD widgets, map markers, physical
 * vehicles, scenario logic, and logging should read from or write through this
 * subsystem rather than maintaining independent truths.
 */
UCLASS(BlueprintType)
class ESCROW_FLEET_MVP_API UESCROWFleetSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Fleet")
	TArray<FESCROWFollowerStatus> FollowerStatuses;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Fleet")
	int32 SelectedFollowerID = 1;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Fleet")
	bool bTakeoverActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Fleet")
	int32 ActiveTakeoverFollowerID = -1;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Fleet")
	TArray<FESCROWFleetCommand> PendingCommands;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Fleet")
	TArray<FESCROWFleetCommand> CompletedCommands;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Fleet")
	TArray<FESCROWHUDMessage> HUDMessages;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Fleet")
	int32 MessageCounter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Fleet")
	int32 CommandCounter = 0;

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	void RegisterFollower(int32 FollowerID, const FString& Callsign, AActor* ActorRef);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	bool SelectFollower(int32 FollowerID);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Fleet")
	bool GetFollowerStatus(int32 FollowerID, FESCROWFollowerStatus& OutStatus) const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Fleet")
	TArray<FESCROWFollowerStatus> GetAllFollowerStatuses() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Fleet")
	bool GetSelectedFollowerStatus(FESCROWFollowerStatus& OutStatus) const;

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	bool SetFollowerState(int32 FollowerID, EESCROWFollowerState NewState);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	bool SetAutonomyMode(int32 FollowerID, EESCROWAutonomyMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	bool SetFollowerObjective(int32 FollowerID, const FString& ObjectiveText);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	bool SetReportReady(int32 FollowerID, bool bReady);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	bool SetRequiresTakeover(int32 FollowerID, bool bRequiresTakeover);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	bool SendLeadMessageToFollower(int32 FollowerID, const FString& MessageText);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	bool ReceiveFollowerMessage(int32 FollowerID, const FString& MessageText, EESCROWMessageSeverity Severity);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	FESCROWFleetCommand QueueFleetCommand(int32 TargetFollowerID, EESCROWCommandType CommandType, AActor* TargetWaypointActor, EESCROWActionType ActionType, const FString& CommandText);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	bool MarkCommandComplete(const FString& CommandID);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Fleet")
	bool CanTakeoverFollower(int32 FollowerID) const;

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	bool RequestTakeover(int32 FollowerID);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	bool AcquireTakeoverLock(int32 FollowerID);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	bool ReleaseTakeoverLock(int32 FollowerID);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	void ClearTransientAlerts();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	void ResetFleetState();

private:
	FESCROWFollowerStatus* FindFollower(int32 FollowerID);
	const FESCROWFollowerStatus* FindFollower(int32 FollowerID) const;

	FString GenerateMessageID();
	FString GenerateCommandID();

	void AddHUDMessage(EESCROWMessageSource Source, EESCROWMessageSeverity Severity, int32 TargetFollowerID, const FString& MessageText, bool bRequiresAcknowledgement = false);
};
