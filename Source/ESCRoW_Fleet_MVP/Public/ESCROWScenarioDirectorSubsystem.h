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
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

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

	// Central telemetry-source guarantee: every playable scenario map should
	// have a live BP_ESCROW_ConvoyTelemetryManager (GPS's data source), without
	// relying on a human remembering to place one in each level. Idempotent -
	// does nothing if one already exists in the world (placed or previously
	// auto-spawned). Spawns one from BP_ESCROW_ConvoyTelemetryManager if none is
	// found; that class resolves CV/F1-F4 purely through UESCROWFleetSubsystem
	// and BP_ESCROW_PlayerController at its own BeginPlay/Tick, so spawning it
	// here needs no map-specific data. No-ops on menu/setup maps. Called once
	// from Initialize().
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Scenario")
	void EnsureTelemetrySource();

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	bool bTelemetrySourceAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Scenario")
	FString TelemetrySourceStatus = TEXT("Not checked");

	// --- Scenario runtime startup lifecycle (internal, not participant-facing) ---
	// NotInitialized -> WorldDetected (Initialize) -> ServicesChecking -> Ready/Failed
	// (OnWorldBeginPlay, playable scenario maps only) -> Stopped (Deinitialize).
	// Menu/setup maps stop at WorldDetected - there are no services to validate there.

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|ScenarioRuntime")
	EESCROWScenarioRuntimeState ScenarioRuntimeState = EESCROWScenarioRuntimeState::NotInitialized;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|ScenarioRuntime")
	FString RuntimeWorldName;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|ScenarioRuntime")
	bool bRuntimeIsPlayableScenario = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|ScenarioRuntime")
	FString RuntimeScenarioID;

	// Valid actor refs out of 5 (CV + F1-F4). CV checked via the resolved
	// ConvoyTelemetryManager's own VehicleRefs (the same ground truth GPS/Reports
	// already use); F1-F4 checked directly against UESCROWFleetSubsystem.
	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|ScenarioRuntime")
	int32 RuntimeFleetValidCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|ScenarioRuntime")
	bool bRuntimeEventLogAvailable = false;

	// Whether EventLog actually has an active run (real session/run metadata
	// available), not just whether the subsystem exists.
	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|ScenarioRuntime")
	bool bRuntimeSessionAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|ScenarioRuntime")
	FString RuntimeFailureReason;

	// Called once from Initialize(): detects the world/map, classifies it, and
	// transitions NotInitialized -> WorldDetected. Does not check services yet
	// (placed actors/CTM are not reliably available this early - see
	// EnsureTelemetrySource's own OnWorldBeginPlay-timing note).
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|ScenarioRuntime")
	void BeginScenarioRuntimeForWorld();

	// Called once from OnWorldBeginPlay(), after EnsureTelemetrySource(), on
	// playable scenario maps only. Logs ScenarioRuntimeServiceCheck immediately,
	// then starts a repeating deferred check (every ServiceCheckIntervalSeconds)
	// that waits for fleet refs to reach 5/5 before calling MarkScenarioRuntimeReady.
	// Telemetry/EventLog failures fail immediately, without waiting out the
	// timeout; a fleet count still below 5/5 at ServiceCheckTimeoutSeconds calls
	// MarkScenarioRuntimeFailed instead. See TickServiceCheck for the per-tick logic.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|ScenarioRuntime")
	void ValidateScenarioRuntimeServices();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|ScenarioRuntime")
	void MarkScenarioRuntimeReady();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|ScenarioRuntime")
	void MarkScenarioRuntimeFailed(const FString& Reason);

	// Compact one-line summary for debugging/logging, e.g.
	// "Ready | World=MAP_ESCROW_FLAT_PILOT_TEST | Fleet=5/5 | EventLog=Yes | Session=Yes".
	UFUNCTION(BlueprintPure, Category = "ESCRoW|ScenarioRuntime")
	FString GetScenarioRuntimeStateSummary() const;

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

	// --- RA42A: generic (non-Mission-D) scenario phase / tasking lifecycle skeleton ---
	// Additive alongside the existing Mission-D schedule above; does not replace it.

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Tasking")
	EESCROWScenarioPhase CurrentPhase = EESCROWScenarioPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Tasking")
	FESCROWTasking ActiveTasking;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Tasking")
	FString CurrentPlanValidity = TEXT("Unspecified");

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Tasking")
	int32 CurrentAffectedFollowerID = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Tasking")
	FString ScenarioCompletionReason;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Tasking")
	bool bGenericTaskingActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Tasking")
	bool bGenericScenarioComplete = false;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Tasking")
	EESCROWScenarioPhase GetCurrentPhase() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Tasking")
	FString GetCurrentPhaseName() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Tasking")
	FESCROWTasking GetActiveTasking() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Tasking")
	FString GetActiveTaskingID() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Tasking")
	FString GetActiveTaskingLabel() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Tasking")
	EESCROWTaskingState GetActiveTaskingState() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Tasking")
	FString GetActiveTaskingStateName() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Tasking")
	FString GetRequiredParticipantAction() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Tasking")
	FString GetCurrentPlanValidity() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Tasking")
	int32 GetCurrentAffectedFollowerID() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Tasking")
	bool IsGenericTaskingActive() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Tasking")
	bool IsGenericScenarioComplete() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Tasking")
	FString GetScenarioCompletionReason() const;

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Tasking")
	void StartGenericScenario();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Tasking")
	void SetScenarioPhase(EESCROWScenarioPhase NewPhase, FString Reason);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Tasking")
	void PostTasking(FString TaskingID, FString TaskingLabel, EESCROWScenarioPhase Phase, FString RequiredParticipantAction, int32 AffectedFollowerID, FString PlanValidity, FString EventID);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Tasking")
	bool AcknowledgeActiveTasking(FString Source);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Tasking")
	bool ValidateActiveTasking(FString Source);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Tasking")
	bool AuthorizeActiveTasking(FString Source);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Tasking")
	bool CompleteActiveTasking(FString CompletionReason);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Tasking")
	bool CancelActiveTasking(FString Reason);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Tasking")
	void SetPlanValidity(FString NewPlanValidity, FString Reason);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Tasking")
	void SetAffectedFollowerID(int32 NewAffectedFollowerID, FString Reason);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Tasking")
	void AdvancePhaseIfTaskingComplete();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Tasking")
	void EndGenericScenario(FString EndReason);

private:
	// Pilot phase flow (flat pilot map only): zone actors are found by tag, the
	// command vehicle's position drives A->B->C, and the system-raised disruption
	// is deterministic (fires on entering the onset zone). Operator takeover on the
	// affected follower advances C->D; release advances D->E.
	void StartPilotPhaseFlow();
	void PilotPhaseTick();
	AActor* FindZoneActorByTag(const FName& Tag) const;
	bool IsLocationInZone(const AActor* Zone, const FVector& Location) const;
	AActor* FindCommandVehicleForPhaseFlow() const;

	FTimerHandle PilotPhaseTimerHandle;
	TWeakObjectPtr<AActor> PilotCommandVehicle;
	bool bPilotPhaseFlowActive = false;
	bool bPilotRallyAArrivalLogged = false;
	double PilotDisruptionOnsetWorldSeconds = -1.0; // time Rally B was reached / formation was held (not a fault onset)

	// Orders. The participant is told what to do by a numbered report at each step (Mission Control):
	//   1 gather everyone at Rally A, 2 confirm the gather, 3 arrange the convoy for the move to Rally B,
	//   4 arrange the Rally B formation, 5 proceed to Rally C.
	// Progress is driven by the real state of the fleet, never by the confirmation alone: the sim checks
	// whether all five vehicles are inside Rally A and whether the follow chain is as ordered, and logs
	// whether the participant's confirmation was correct.
	double PilotScenarioStartSeconds = -1.0;
	double PilotRallyAArrivalSeconds = -1.0;
	double PilotGatherHeldSince = -1.0;
	double PilotGatherCompleteSeconds = -1.0;
	double PilotOrderGatherPostedSeconds = -1.0;
	double PilotOrderConvoyDueSeconds = -1.0;
	double PilotOrderConvoyPostedSeconds = -1.0;
	double PilotConvoyHeldSince = -1.0;
	bool bPilotOrderGatherPosted = false;
	bool bPilotOrderConfirmPosted = false;
	bool bPilotOrderConvoyPosted = false;
	bool bPilotGatherComplete = false;
	bool bPilotGatherConfirmHandled = false;
	bool bPilotGatherReminderPosted = false;
	bool bPilotConvoyArranged = false;
	static constexpr int32 PilotConvoyOrder[4] = { 1, 2, 3, 4 }; // HAWK, WOLF, LYNX, BEAR (BEAR at the rear)
	void PilotTickOpeningOrders(UESCROWFleetSubsystem* Fleet, class UESCROWEventLogSubsystem* EventLog, UWorld* World);
	bool PilotEvaluateGather(FString& OutDetail, TArray<int32>& OutOutside);
	bool PilotEvaluateConvoy(FString& OutDetail);
	void PilotPostOrder(UESCROWFleetSubsystem* Fleet, class UESCROWEventLogSubsystem* EventLog, int32 OrderIndex, const FString& ReportID, const FString& Title, const FString& Body, bool bRequiresAck, bool bNotify);

	// Pilot faults: a follower stops (nav order "hold") until the operator takes over, drives it to
	// the recovery point, and releases control there. Two faults run in the pilot: WOLF (F2) fails
	// early on the route once it has passed Rally A; BEAR (F4) fails later, after the formation at
	// Rally B is held and it has started toward Rally C. Both are independent of each other.
	struct FPilotFault
	{
		int32 FollowerID = 0;
		FString Key;                 // "EarlyRoute" / "LateConvoy"
		bool bDrivesPhases = false;  // true: takeover -> Phase D, resolution -> Phase E
		bool bTriggered = false;
		bool bHeld = false;
		bool bResolved = false;
		bool bRecoveryReached = false;
		bool bMarkerSpawned = false;
		bool bSelectionLogged = false;
		bool bTakeoverSeen = false;
		double OnsetSeconds = -1.0;
		double LastReassertSeconds = -1.0;
		int32 PriorOrderType = 5;           // restored when the fault is resolved
		int32 PriorTargetVehicleID = -1;
		int32 LastSeenSelected = -1;
		FVector RecoveryLocation = FVector::ZeroVector;
		TWeakObjectPtr<AActor> RecoveryActor;
		TSet<int32> WrongTakeoversLogged;
	};
	TArray<FPilotFault> PilotFaults;
	TWeakObjectPtr<AActor> PilotConvoyManager;
	int32 PilotReadConvoyIntArray(const TCHAR* ArrayName, int32 Index, int32 Default);
	AActor* PilotFindConvoyManager();
	AActor* PilotGetFollowerActor(int32 FollowerID);
	bool PilotSetFollowerOrder(int32 FollowerID, int32 OrderType, int32 TargetVehicleID, const FString& Reason);
	bool PilotSetFollowerHold(int32 FollowerID, bool bHold);
	// Runs the follower's own Escrow_RestoreAfterTakeover (zero inputs, forward gear, clear reversing).
	// Needed after a fault hold is re-applied right after a takeover release, which otherwise leaves the
	// follower crawling in a stale reverse state.
	void PilotResetFollowerDriveState(int32 FollowerID, const TCHAR* Why);
	FVector PilotComputeRecoveryLocation(const FVector& From) const;
	void PilotSpawnRecoveryPoint(FPilotFault& Fault);
	void PilotClearRecoveryPoint(FPilotFault& Fault);
	// Spawns the fault's recovery marker once and logs when/why (Trigger: "DisruptionOnset" or "ParticipantTakeover").
	void PilotSpawnRecoveryMarkerLogged(FPilotFault& Fault, class UESCROWEventLogSubsystem* EventLog, const TCHAR* Trigger);
	class UESCROWSessionSubsystem* PilotGetSession() const;
	void PilotTickFaults(UESCROWFleetSubsystem* Fleet, class UESCROWEventLogSubsystem* EventLog, UWorld* World, const FVector& CVLocation);
	void PilotTriggerFault(FPilotFault& Fault, UESCROWFleetSubsystem* Fleet, class UESCROWEventLogSubsystem* EventLog, UWorld* World, const FVector& CVLocation, const FString& Reason);
	void PilotTrackDetection(FPilotFault& Fault, UESCROWFleetSubsystem* Fleet, class UESCROWEventLogSubsystem* EventLog, UWorld* World);
	void PilotHandleFaultTakeover(FPilotFault& Fault, UESCROWFleetSubsystem* Fleet, class UESCROWEventLogSubsystem* EventLog, UWorld* World);
	void PilotReassertFault(FPilotFault& Fault, UESCROWFleetSubsystem* Fleet, UWorld* World);
	void PilotTickFault(FPilotFault& Fault, UESCROWFleetSubsystem* Fleet, class UESCROWEventLogSubsystem* EventLog, UWorld* World);

	// Pilot Phase E (Rally C plan repair) and Phase F (closeout). On reaching Rally C the
	// planned formation slots appear, two of them blocked; the operator reassigns the
	// affected followers to alternate positions (GPS waypoints), then confirms the
	// "validate" and "authorize" reports. Validity is judged here from real waypoint
	// positions, never from a UI button.
	static constexpr int32 PilotSlotCount = 4;
	static constexpr int32 PilotAltCount = 2;
	bool bPilotPlanRepairPosted = false;
	bool bPilotCloseoutPosted = false;
	double PilotPlanRepairOnsetSeconds = -1.0;
	double PilotCloseoutOnsetSeconds = -1.0;
	int32 PilotValidateAttempt = 0;
	bool bPilotValidatePassed = false;
	bool bPilotFirstAssignmentLogged = false;
	FVector PilotSlotLocations[PilotSlotCount];
	bool PilotSlotBlocked[PilotSlotCount];
	FVector PilotAltLocations[PilotAltCount];
	TArray<TWeakObjectPtr<AActor>> PilotPlanMarkers;
	TMap<int32, FString> PilotLastLoggedAssignment;
	FString PilotValidateReportID;
	FString PilotAuthorizeReportID;
	FString PilotCloseoutReportID;

	void PilotTickPlanRepair(UESCROWFleetSubsystem* Fleet, class UESCROWEventLogSubsystem* EventLog, UWorld* World, const FVector& CVLocation);
	void PilotTickCloseout(UESCROWFleetSubsystem* Fleet, class UESCROWEventLogSubsystem* EventLog, UWorld* World);
	void PilotPostPlanRepair(UESCROWFleetSubsystem* Fleet, class UESCROWEventLogSubsystem* EventLog, UWorld* World, const FVector& CVLocation);
	void PilotPostValidateReport(UESCROWFleetSubsystem* Fleet, bool bNotify);
	bool PilotEvaluatePlan(FString& OutDetail);
	bool PilotReportAcknowledged(UESCROWFleetSubsystem* Fleet, const FString& ReportID) const;
	bool PilotReadConvoyVectorArray(const TCHAR* ArrayName, int32 Index, FVector& Out);
	AActor* PilotSpawnPlanMarker(const FVector& Location, const FString& Label, const FColor& Color, bool bBlocker, const FName& Tag, double LabelYaw);
	void PilotClearPlanMarkers();
	double PilotFindGroundZ(const FVector& Location, double Fallback) const;

	// Pilot Phase B formation: on reaching Rally B the operator must arrange the fleet into a
	// line abreast. The slot order is deliberately different from the callsign order
	// (HAWK, BEAR, LYNX, WOLF) so rearranging takes thought. The disruption only starts once
	// the formation has been held (or after a long fallback timeout).
	bool bPilotFormationPosted = false;
	bool bPilotFormationSatisfied = false;
	double PilotFormationPostedSeconds = -1.0;
	double PilotFormationHeldSince = -1.0;
	double PilotFormationLastReminderSeconds = -1.0;
	FVector PilotFormLocations[PilotSlotCount];
	int32 PilotFormFollowers[PilotSlotCount] = { 1, 4, 3, 2 };
	bool bPilotValidatePrompted = false;

	void PilotTickFormation(UESCROWFleetSubsystem* Fleet, class UESCROWEventLogSubsystem* EventLog, UWorld* World);
	void PilotPostFormation(UESCROWFleetSubsystem* Fleet, class UESCROWEventLogSubsystem* EventLog, UWorld* World);
	bool PilotEvaluateFormation(FString& OutDetail);

	// Periodic / phase-change fleet status reports (new report each time, with a snapshot).
	double PilotLastFleetReportSeconds = -1.0;
	int32 PilotFleetReportCount = 0;
	EESCROWScenarioPhase PilotLastReportedPhase = EESCROWScenarioPhase::None;
	void PilotMaybePostFleetReport(UESCROWFleetSubsystem* Fleet, UWorld* World);

	// Startup nav self-check: logs the live nav agent size and whether each follower can
	// path to the command vehicle (waits for the dynamic nav mesh to finish building).
	void PilotRunNavDiagnostics();
	FTimerHandle PilotNavDiagTimerHandle;
	int32 PilotNavDiagAttempts = 0;
	int32 PilotNavDiagRepeats = 0;

	// RA42A helpers
	FString ScenarioPhaseToText(EESCROWScenarioPhase Phase) const;
	FString TaskingStateToText(EESCROWTaskingState State) const;
	FString BuildTaskingPayloadJSON(const FString& ReasonOrSource) const;

	FTimerHandle TickTimerHandle;

	// RA41A: promotes SessionSubsystem's pending run config (or a generated dev
	// fallback if none is pending) to current, then starts the EventLog run and
	// heartbeat. Called once from Initialize(), after InitializeScenario().
	void InitializeRunDataForCurrentLevel();

	void BuildMissionDSchedule();
	void TickScenario();
	void StartTimerIfNeeded();
	void ClearTimer();
	void EnterEventState(const FESCROWScenarioEvent& Event, bool bManualOverrideOfWait);
	UESCROWFleetSubsystem* GetFleet() const;

	// RA41B PART 4: shared lookup for the scenario-truth EventLog hooks below.
	class UESCROWEventLogSubsystem* GetEventLog() const;
	static FString FormatMissionTime(float Seconds);
	FString RunStateToText() const;

	// Scenario runtime lifecycle helpers
	FString ScenarioRuntimeStateToText(EESCROWScenarioRuntimeState State) const;
	// Counts valid F1-F4 refs from UESCROWFleetSubsystem plus CV (via the resolved
	// ConvoyTelemetryManager's own VehicleRefs[0], read through reflection since
	// BP_ESCROW_ConvoyTelemetryManager is a pure-Blueprint class with no C++ base).
	int32 ComputeRuntimeFleetValidCount() const;
	void LogScenarioRuntimeEvent(const FString& EventType, const FString& Detail) const;

	// Repeating timer callback started by ValidateScenarioRuntimeServices. Re-checks
	// telemetry/EventLog/fleet count every ServiceCheckIntervalSeconds; resolves to
	// Ready (fleet=5/5), Failed (telemetry/EventLog missing, or timeout reached), or
	// logs ScenarioRuntimeFleetPending (only when the fleet count changes, to avoid
	// per-tick log spam) and keeps waiting.
	void TickServiceCheck();
	void ClearServiceCheckTimer();

	FTimerHandle ServiceCheckTimerHandle;

	// Re-posts the "INVALID RUN" on-screen banner while a DevelopmentOnly fallback run is active.
	FTimerHandle InvalidRunBannerTimerHandle;
	float ServiceCheckElapsedSeconds = 0.0f;
	int32 LastLoggedRuntimeFleetValidCount = -1;
	bool bServiceCheckInitialLogDone = false;
};
