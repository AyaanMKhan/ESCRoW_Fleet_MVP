// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ESCROWTypes.h"
#include "ESCROWScenarioDirectorSubsystem.generated.h"

class UESCROWFleetSubsystem;

/**
 * Lightweight Mission D scenario/mission-playback director (RA21A). Owns a
 * hardcoded event schedule and a small state machine (manual/timed/hybrid
 * playback), and drives mission state purely by calling into
 * UESCROWFleetSubsystem and UESCROWEventLogSubsystem. Holds no duplicate
 * fleet/session truth of its own beyond scenario-local playback bookkeeping,
 * and never touches actor possession/control routing - takeover and return
 * remain entirely operator/PlayerControlRouter driven. Physical follower
 * movement is not required for playback to progress: events set state,
 * objective, report-ready, and takeover-required flags directly.
 */
UCLASS(BlueprintType)
class ESCROW_FLEET_MVP_API UESCROWScenarioDirectorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	FString ScenarioID = TEXT("MISSION_D_V1");

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	FString ScenarioName = TEXT("Mission D - Desert Demo");

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	bool bScenarioLoaded = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	bool bScenarioRunning = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	bool bScenarioPaused = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	bool bAutoPlaybackEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	bool bHybridMilestoneMode = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	EESCROWScenarioPlaybackMode PlaybackMode = EESCROWScenarioPlaybackMode::Manual;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	float ScenarioStartTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	float CurrentMissionTime = 0.0f;

	// -1 = no event executed yet. After executing event N, this equals N.
	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	int32 CurrentEventIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	FString CurrentScenarioStep = TEXT("Not started");

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	FString CurrentEventMessageText;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	float ScenarioSpeedMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	TArray<FESCROWScenarioEvent> EventSchedule;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	bool bWaitingForMilestone = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	FString WaitingMilestoneName;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	EESCROWScenarioRunState RunState = EESCROWScenarioRunState::NotLoaded;

	// --- Core functions (Feature A) ---

	// Idempotent: safe to call repeatedly (e.g. from GameMode BeginPlay).
	// Builds the Mission D schedule if not already loaded. Does not start playback.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Scenario")
	void InitializeScenario();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Scenario")
	bool StartScenario(EESCROWScenarioPlaybackMode Mode);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Scenario")
	bool PauseScenario();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Scenario")
	bool ResumeScenario();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Scenario")
	bool StopScenario();

	// Refuses (returns false with OutStatusMessage explaining why) if a
	// takeover is currently active, rather than forcing an unsafe possession
	// change. Resets scenario-local state only; fleet-state cleanup is left
	// to the caller (see WBP_ESCROW_CommandHUD::Escrow_ResetScenario, which
	// also calls the existing Escrow_ResetDemoState) because
	// UESCROWFleetSubsystem::ResetFleetState() rebuilds FollowerStatuses from
	// scratch and would null out each follower's ActorRef.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Scenario")
	bool ResetScenario(FString& OutStatusMessage);

	// Advances exactly one step (CurrentEventIndex + 1) and executes it,
	// regardless of whether the scenario is running/paused or which mode is
	// active. If the target event requires an operator milestone, playback
	// enters a waiting state instead of auto-continuing further. Calling this
	// while already waiting on a milestone is a manual override past the gate
	// (logged as such) - this is the facilitator's escape hatch per Feature E.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Scenario")
	bool AdvanceToNextEvent();

	// Re-applies the event at CurrentEventIndex without advancing.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Scenario")
	void ExecuteCurrentEvent();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Scenario")
	bool ExecuteEventByIndex(int32 Index);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Scenario")
	FString GetCurrentScenarioStatus() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Scenario")
	FString GetCurrentMissionTimeText() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Scenario")
	FString GetCurrentEventLabel() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Scenario")
	FString GetNextEventLabel() const;

	// If waiting for a milestone, checks whether it is now satisfied; if so,
	// clears the wait and advances to the next event. Returns true if it
	// advanced. Safe to call any time, including when not waiting (no-op).
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Scenario")
	bool CheckMilestoneAdvance();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Scenario")
	void LogScenarioEvent(const FString& ScenarioLogType, int32 FollowerID, const FString& Detail);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Scenario")
	FString GetPlaybackModeText() const;

	// Compact multi-line status block for the Commander page readout.
	UFUNCTION(BlueprintPure, Category = "ESCRoW|Scenario")
	FString GetScenarioReadoutText() const;

private:
	FTimerHandle TickTimerHandle;

	void BuildMissionDSchedule();
	void TickScenario();
	void StartTimerIfNeeded();
	void ClearTimer();
	void EnterEventState(const FESCROWScenarioEvent& Event, bool bManualOverrideOfWait);
	UESCROWFleetSubsystem* GetFleet() const;
	static FString FormatMissionTime(float Seconds);
	FString RunStateToText() const;
};
