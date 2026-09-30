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

	// Commander/system-wide HUD message not tied to a specific follower (e.g. mission start/checkpoint).
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	void ReceiveSystemMessage(const FString& MessageText, EESCROWMessageSeverity Severity = EESCROWMessageSeverity::Info);

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

	// Follow-target state/config only. Does not move or command any vehicle.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	bool SetFollowerFollowTarget(int32 FollowerID, int32 TargetID);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Fleet")
	int32 GetFollowerFollowTargetID(int32 FollowerID) const;

	// True if TargetID is a legal follow target for FollowerID: not itself, in range
	// 0-4, and does not introduce a cycle in the follow graph.
	UFUNCTION(BlueprintPure, Category = "ESCRoW|Fleet")
	bool CanSetFollowerFollowTarget(int32 FollowerID, int32 TargetID) const;

	// Restores the default convoy chain: F1->CommandVehicle, F2->F1, F3->F2, F4->F3.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Fleet")
	bool ResetDefaultFollowChain();

private:
	FESCROWFollowerStatus* FindFollower(int32 FollowerID);
	const FESCROWFollowerStatus* FindFollower(int32 FollowerID) const;

	FString GenerateMessageID();
	FString GenerateCommandID();

	void AddHUDMessage(EESCROWMessageSource Source, EESCROWMessageSeverity Severity, int32 TargetFollowerID, const FString& MessageText, bool bRequiresAcknowledgement = false);

	// Mirrors UESCROWScenarioDirectorSubsystem::GetEventLog(): looks up the
	// per-GameInstance EventLog subsystem. Forward-declared to avoid a header
	// dependency; full include is in the .cpp only.
	class UESCROWEventLogSubsystem* GetEventLog() const;

	// World time (GetWorld()->GetTimeSeconds()) at the moment AcquireTakeoverLock
	// succeeded, used to compute control duration when ReleaseTakeoverLock fires.
	// Only one takeover can be active at a time (bTakeoverActive/ActiveTakeoverFollowerID
	// are single-slot), so a single scalar is sufficient.
	double TakeoverStartWorldSeconds = 0.0;
};
