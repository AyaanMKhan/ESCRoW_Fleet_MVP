// Fill out your copyright notice in the Description page of Project Settings.

#include "ESCROWScenarioDirectorSubsystem.h"
#include "ESCROWFleetSubsystem.h"
#include "ESCROWEventLogSubsystem.h"
#include "ESCROWSessionSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

namespace
{
	const FString Milestone_F3TakeoverActive = TEXT("F3TakeoverActive");
	const FString Milestone_F3ReturnComplete = TEXT("F3ReturnComplete");
	constexpr float ScenarioTickInterval = 0.25f;
}

void UESCROWScenarioDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	InitializeScenario();
	InitializeRunDataForCurrentLevel();
}

void UESCROWScenarioDirectorSubsystem::InitializeRunDataForCurrentLevel()
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		return;
	}

	UESCROWEventLogSubsystem* EventLog = GameInstance->GetSubsystem<UESCROWEventLogSubsystem>();
	UESCROWSessionSubsystem* Session = GameInstance->GetSubsystem<UESCROWSessionSubsystem>();
	if (!EventLog || !Session)
	{
		return;
	}

	FESCROWRunConfig RunConfig;
	bool bFoundPending = false;
	Session->PromotePendingRunConfigToCurrent(RunConfig, bFoundPending);

	bool bUsedFallback = false;
	if (!bFoundPending)
	{
		// PIE launched directly into the level, or the menu path was skipped:
		// build a safe placeholder dev run rather than leaving the run unstarted.
		bUsedFallback = true;

		RunConfig = FESCROWRunConfig();
		RunConfig.ParticipantID = Session->GetParticipantID().IsEmpty() ? TEXT("PILOT") : Session->GetParticipantID();
		RunConfig.ResearcherID = Session->GetResearcherID().IsEmpty() ? TEXT("UNSET") : Session->GetResearcherID();
		RunConfig.SessionID = Session->GetSessionID();
		RunConfig.ScenarioID = TEXT("DEV_NO_PENDING_CONFIG");
		RunConfig.ScenarioVersion = TEXT("dev");
		RunConfig.ConditionID = Session->GetConditionSummary();
		RunConfig.SupportMode = TEXT("ManualBaseline");
		RunConfig.DisplayMode = Session->GetDisplayModeName();
		RunConfig.VehicleControlRole = Session->GetVehicleControlRoleName();
		RunConfig.WorkloadLevel = TEXT("Unspecified");
		RunConfig.ScenarioVariant = TEXT("FlatPilot");
		RunConfig.EventSalience = TEXT("Unspecified");
		RunConfig.UncertaintyLevel = TEXT("Unspecified");
		RunConfig.MapName = World ? World->GetMapName() : TEXT("UNKNOWN");
		RunConfig.CreatedAt = FDateTime::Now();
		RunConfig.Notes = TEXT("Auto-generated fallback: no pending run config found at level load.");
		RunConfig.RunID = Session->GenerateRunID(RunConfig.ScenarioID, RunConfig.ConditionID, RunConfig.ParticipantID);

		Session->SetPendingRunConfig(RunConfig);
		Session->PromotePendingRunConfigToCurrent(RunConfig, bFoundPending);
	}

	if (RunConfig.RunID.IsEmpty())
	{
		RunConfig.RunID = Session->GenerateRunID(RunConfig.ScenarioID, RunConfig.ConditionID, RunConfig.ParticipantID);
	}
	if (RunConfig.MapName.IsEmpty() && World)
	{
		RunConfig.MapName = World->GetMapName();
	}
	if (RunConfig.CreatedAt.GetTicks() == 0)
	{
		RunConfig.CreatedAt = FDateTime::Now();
	}

	EventLog->BeginRunFromConfig(RunConfig);

	EventLog->RecordScenarioEventSimple(
		CurrentScenarioStep,
		TEXT("RunInitialized"),
		TEXT("Level loaded and run initialized"),
		FString(),
		0,
		TEXT("Unspecified"),
		bUsedFallback ? TEXT("{\"fallback\":true}") : TEXT("{\"fallback\":false}"));

	EventLog->StartHeartbeat();
}

void UESCROWScenarioDirectorSubsystem::Deinitialize()
{
	ClearTimer();
	Super::Deinitialize();
}

UESCROWFleetSubsystem* UESCROWScenarioDirectorSubsystem::GetFleet() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UESCROWFleetSubsystem>() : nullptr;
}

void UESCROWScenarioDirectorSubsystem::InitializeScenario()
{
	if (bScenarioLoaded)
	{
		return;
	}

	BuildMissionDSchedule();
	bScenarioLoaded = true;
	RunState = EESCROWScenarioRunState::Ready;
	CurrentScenarioStep = TEXT("Not started");

	LogScenarioEvent(TEXT("Initialized"), 0, FString::Printf(TEXT("Scenario loaded: %s (%d events)."), *ScenarioName, EventSchedule.Num()));
}

void UESCROWScenarioDirectorSubsystem::BuildMissionDSchedule()
{
	EventSchedule.Reset();

	auto AddEvent = [this](const FString& EventID, const FString& Label, float Time, const FString& Type, int32 FollowerID, const FString& Message, bool bMilestone, const FString& MilestoneName)
	{
		FESCROWScenarioEvent Event;
		Event.EventIndex = EventSchedule.Num();
		Event.EventID = EventID;
		Event.EventLabel = Label;
		Event.ScheduledTimeSeconds = Time;
		Event.EventType = Type;
		Event.FollowerID = FollowerID;
		Event.MessageText = Message;
		Event.bRequiresOperatorMilestone = bMilestone;
		Event.ExpectedMilestoneName = MilestoneName;
		EventSchedule.Add(Event);
		return EventSchedule.Num() - 1;
	};

	// 0. MISSION_START
	AddEvent(TEXT("MISSION_START"), TEXT("Mission start / awaiting tasking"), 0.0f, TEXT("MissionStart"), 0,
		TEXT("CMD-001: Mission start. Maintain convoy control and prepare to task autonomous followers."), false, FString());

	// 1. CMD_002_ASSIGN_F4
	AddEvent(TEXT("CMD_002_ASSIGN_F4"), TEXT("Assign F4 waypoint"), 15.0f, TEXT("CommanderMessage"), 4,
		TEXT("CMD-002: Assign F4 to investigate the marked issue area. Maintain command-vehicle control."), false, FString());

	// 2. F4_TASK_ACCEPTED_OR_ENROUTE
	{
		const int32 Index = AddEvent(TEXT("F4_TASK_ACCEPTED_OR_ENROUTE"), TEXT("F4 task active"), 35.0f, TEXT("FollowerObjectiveUpdate"), 4,
			TEXT("F4 waypoint command accepted. Proceeding to assigned position."), false, FString());
		EventSchedule[Index].bHasObjectiveChange = true;
		EventSchedule[Index].NewObjectiveText = TEXT("Investigate marked issue area");
		EventSchedule[Index].bHasStateChange = true;
		EventSchedule[Index].NewFollowerState = EESCROWFollowerState::MovingToWaypoint;
	}

	// 3. F4_REPORT_READY
	{
		const int32 Index = AddEvent(TEXT("F4_REPORT_READY"), TEXT("F4 report ready"), 60.0f, TEXT("ReportReady"), 4,
			TEXT("F4 task status report ready."), false, FString());
		EventSchedule[Index].bHasReportReadyChange = true;
		EventSchedule[Index].NewReportReadyValue = true;
	}

	// 4. CMD_003_MONITOR_F3
	AddEvent(TEXT("CMD_003_MONITOR_F3"), TEXT("Monitor F3"), 75.0f, TEXT("CommanderMessage"), 3,
		TEXT("CMD-003: Monitor F3 low-ground segment. Possible mobility or communications issue."), false, FString());

	// 5. F3_ISSUE_DETECTED
	{
		const int32 Index = AddEvent(TEXT("F3_ISSUE_DETECTED"), TEXT("F3 issue detected"), 95.0f, TEXT("IssueDetected"), 3,
			TEXT("F3 status degraded. Operator attention requested."), false, FString());
		EventSchedule[Index].bHasObjectiveChange = true;
		EventSchedule[Index].NewObjectiveText = TEXT("Manual intervention required");
		EventSchedule[Index].bHasStateChange = true;
		EventSchedule[Index].NewFollowerState = EESCROWFollowerState::IssueDetected;
		EventSchedule[Index].bHasTakeoverChange = true;
		EventSchedule[Index].NewRequiresTakeoverValue = true;
	}

	// 6. WAIT_FOR_TAKEOVER (milestone-gated)
	AddEvent(TEXT("WAIT_FOR_TAKEOVER"), TEXT("F3 takeover pending"), -1.0f, TEXT("WaitForMilestone"), 3,
		TEXT("CMD-004: Direct control required for F3 recovery."), true, Milestone_F3TakeoverActive);

	// 7. WAIT_FOR_RETURN (milestone-gated)
	AddEvent(TEXT("WAIT_FOR_RETURN"), TEXT("F3 takeover active"), -1.0f, TEXT("WaitForMilestone"), 3,
		TEXT("Awaiting return to command vehicle."), true, Milestone_F3ReturnComplete);

	// 8. F3_RECOVERY_COMPLETE
	{
		const int32 Index = AddEvent(TEXT("F3_RECOVERY_COMPLETE"), TEXT("F3 recovered"), -1.0f, TEXT("RecoveryComplete"), 3,
			TEXT("F3 recovery complete. Returning to autonomous follow."), false, FString());
		EventSchedule[Index].bHasObjectiveChange = true;
		EventSchedule[Index].NewObjectiveText = TEXT("Recovery complete; returning to convoy");
		EventSchedule[Index].bHasStateChange = true;
		EventSchedule[Index].NewFollowerState = EESCROWFollowerState::Recovered;
		EventSchedule[Index].bHasReportReadyChange = true;
		EventSchedule[Index].NewReportReadyValue = true;
		EventSchedule[Index].bHasTakeoverChange = true;
		EventSchedule[Index].NewRequiresTakeoverValue = false;
	}

	// 9. FINAL_REPORT_REQUEST
	AddEvent(TEXT("FINAL_REPORT_REQUEST"), TEXT("Final report"), -1.0f, TEXT("CommanderMessage"), 0,
		TEXT("CMD-005: Mission checkpoint complete. Fleet status report ready for export."), false, FString());

	// 10. MISSION_COMPLETE
	AddEvent(TEXT("MISSION_COMPLETE"), TEXT("Demo complete"), -1.0f, TEXT("MissionComplete"), 0,
		TEXT("Mission D demo complete."), false, FString());
}

bool UESCROWScenarioDirectorSubsystem::StartScenario(EESCROWScenarioPlaybackMode Mode)
{
	if (!bScenarioLoaded)
	{
		InitializeScenario();
	}

	if (bScenarioRunning)
	{
		return false;
	}

	PlaybackMode = Mode;
	bAutoPlaybackEnabled = (Mode != EESCROWScenarioPlaybackMode::Manual);
	bHybridMilestoneMode = (Mode == EESCROWScenarioPlaybackMode::Hybrid);
	bScenarioRunning = true;
	bScenarioPaused = false;
	bWaitingForMilestone = false;
	WaitingMilestoneName.Empty();
	CurrentMissionTime = 0.0f;
	CurrentEventIndex = -1;
	CurrentScenarioStep = TEXT("Not started");
	CurrentEventMessageText.Empty();
	ScenarioStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	RunState = EESCROWScenarioRunState::Running;

	LogScenarioEvent(TEXT("Started"), 0, FString::Printf(TEXT("Scenario started in %s mode."), *GetPlaybackModeText()));

	StartTimerIfNeeded();

	// Fire MISSION_START immediately so the operator gets instant feedback.
	AdvanceToNextEvent();

	return true;
}

bool UESCROWScenarioDirectorSubsystem::PauseScenario()
{
	if (!bScenarioRunning || bScenarioPaused)
	{
		return false;
	}

	bScenarioPaused = true;
	ClearTimer();

	LogScenarioEvent(TEXT("Paused"), 0, TEXT("Scenario paused."));
	return true;
}

bool UESCROWScenarioDirectorSubsystem::ResumeScenario()
{
	if (!bScenarioRunning || !bScenarioPaused)
	{
		return false;
	}

	bScenarioPaused = false;
	StartTimerIfNeeded();

	LogScenarioEvent(TEXT("Resumed"), 0, TEXT("Scenario resumed."));
	return true;
}

bool UESCROWScenarioDirectorSubsystem::StopScenario()
{
	if (!bScenarioRunning)
	{
		return false;
	}

	bScenarioRunning = false;
	bScenarioPaused = false;
	bWaitingForMilestone = false;
	ClearTimer();
	RunState = EESCROWScenarioRunState::Ready;

	LogScenarioEvent(TEXT("Stopped"), 0, TEXT("Scenario stopped."));
	return true;
}

bool UESCROWScenarioDirectorSubsystem::ResetScenario(FString& OutStatusMessage)
{
	const UESCROWFleetSubsystem* Fleet = GetFleet();
	if (Fleet && Fleet->bTakeoverActive)
	{
		OutStatusMessage = TEXT("Cannot reset scenario: F3 takeover in progress. Return to command vehicle first.");
		return false;
	}

	ClearTimer();
	bScenarioRunning = false;
	bScenarioPaused = false;
	bWaitingForMilestone = false;
	WaitingMilestoneName.Empty();
	CurrentEventIndex = -1;
	CurrentMissionTime = 0.0f;
	CurrentScenarioStep = TEXT("Not started");
	CurrentEventMessageText.Empty();
	RunState = bScenarioLoaded ? EESCROWScenarioRunState::Ready : EESCROWScenarioRunState::NotLoaded;

	LogScenarioEvent(TEXT("Reset"), 0, TEXT("Scenario reset."));
	OutStatusMessage = TEXT("Scenario reset.");
	return true;
}

void UESCROWScenarioDirectorSubsystem::EnterEventState(const FESCROWScenarioEvent& Event, bool bManualOverrideOfWait)
{
	CurrentEventIndex = Event.EventIndex;
	CurrentScenarioStep = Event.EventLabel;
	CurrentEventMessageText = Event.MessageText;

	UESCROWFleetSubsystem* Fleet = GetFleet();
	if (Fleet && Event.FollowerID >= 1 && Event.FollowerID <= 4)
	{
		if (Event.EventType == TEXT("CommanderMessage"))
		{
			Fleet->SelectFollower(Event.FollowerID);
		}
		if (Event.bHasObjectiveChange)
		{
			Fleet->SetFollowerObjective(Event.FollowerID, Event.NewObjectiveText);
		}
		if (Event.bHasStateChange)
		{
			Fleet->SetFollowerState(Event.FollowerID, Event.NewFollowerState);
		}
		if (Event.bHasReportReadyChange)
		{
			Fleet->SetReportReady(Event.FollowerID, Event.NewReportReadyValue);
		}
		if (Event.bHasTakeoverChange)
		{
			Fleet->SetRequiresTakeover(Event.FollowerID, Event.NewRequiresTakeoverValue);
		}
		if (!Event.MessageText.IsEmpty())
		{
			const EESCROWMessageSeverity Severity = (Event.EventType == TEXT("IssueDetected")) ? EESCROWMessageSeverity::Warning : EESCROWMessageSeverity::Info;
			Fleet->ReceiveFollowerMessage(Event.FollowerID, Event.MessageText, Severity);
		}
	}
	else if (Fleet && Event.FollowerID == 0 && !Event.MessageText.IsEmpty())
	{
		const EESCROWMessageSeverity Severity = (Event.EventType == TEXT("IssueDetected")) ? EESCROWMessageSeverity::Warning : EESCROWMessageSeverity::Info;
		Fleet->ReceiveSystemMessage(Event.MessageText, Severity);
	}

	if (Event.bRequiresOperatorMilestone)
	{
		bWaitingForMilestone = true;
		WaitingMilestoneName = Event.ExpectedMilestoneName;
		RunState = EESCROWScenarioRunState::WaitingForMilestone;
		LogScenarioEvent(TEXT("WaitingForMilestone"), Event.FollowerID, FString::Printf(TEXT("%s: %s (waiting on %s)"), *Event.EventID, *Event.MessageText, *Event.ExpectedMilestoneName));
	}
	else
	{
		bWaitingForMilestone = false;
		WaitingMilestoneName.Empty();
		RunState = bScenarioRunning ? EESCROWScenarioRunState::Running : EESCROWScenarioRunState::Ready;
		LogScenarioEvent(bManualOverrideOfWait ? TEXT("ManualAdvance") : TEXT("EventFired"), Event.FollowerID, FString::Printf(TEXT("%s: %s"), *Event.EventID, *Event.MessageText));
	}

	if (CurrentEventIndex == EventSchedule.Num() - 1)
	{
		RunState = EESCROWScenarioRunState::Complete;
		bScenarioRunning = false;
		ClearTimer();
		LogScenarioEvent(TEXT("Complete"), 0, TEXT("Scenario complete."));
	}
}

bool UESCROWScenarioDirectorSubsystem::ExecuteEventByIndex(int32 Index)
{
	if (!EventSchedule.IsValidIndex(Index))
	{
		return false;
	}

	EnterEventState(EventSchedule[Index], false);
	return true;
}

void UESCROWScenarioDirectorSubsystem::ExecuteCurrentEvent()
{
	if (EventSchedule.IsValidIndex(CurrentEventIndex))
	{
		EnterEventState(EventSchedule[CurrentEventIndex], false);
	}
}

bool UESCROWScenarioDirectorSubsystem::AdvanceToNextEvent()
{
	if (!bScenarioLoaded)
	{
		InitializeScenario();
	}

	const int32 TargetIndex = CurrentEventIndex + 1;
	if (!EventSchedule.IsValidIndex(TargetIndex))
	{
		return false;
	}

	const bool bWasWaiting = bWaitingForMilestone;
	EnterEventState(EventSchedule[TargetIndex], bWasWaiting);
	return true;
}

bool UESCROWScenarioDirectorSubsystem::CheckMilestoneAdvance()
{
	if (!bWaitingForMilestone)
	{
		return false;
	}

	const UESCROWFleetSubsystem* Fleet = GetFleet();
	if (!Fleet)
	{
		return false;
	}

	bool bSatisfied = false;
	if (WaitingMilestoneName == Milestone_F3TakeoverActive)
	{
		bSatisfied = Fleet->bTakeoverActive && Fleet->ActiveTakeoverFollowerID == 3;
	}
	else if (WaitingMilestoneName == Milestone_F3ReturnComplete)
	{
		bSatisfied = !Fleet->bTakeoverActive;
	}

	if (!bSatisfied)
	{
		return false;
	}

	LogScenarioEvent(TEXT("MilestoneSatisfied"), 3, WaitingMilestoneName);
	bWaitingForMilestone = false;
	WaitingMilestoneName.Empty();

	return AdvanceToNextEvent();
}

void UESCROWScenarioDirectorSubsystem::LogScenarioEvent(const FString& ScenarioLogType, int32 FollowerID, const FString& Detail)
{
	UESCROWEventLogSubsystem* EventLog = nullptr;
	if (const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		EventLog = GameInstance->GetSubsystem<UESCROWEventLogSubsystem>();
	}

	if (!EventLog)
	{
		return;
	}

	const FString EventText = FString::Printf(TEXT("[%s] idx=%d step=\"%s\" :: %s"), *ScenarioID, CurrentEventIndex, *CurrentScenarioStep, *Detail);
	EventLog->AddEvent(FString::Printf(TEXT("Scenario_%s"), *ScenarioLogType), EventText, FollowerID, ScenarioID, FString());
}

void UESCROWScenarioDirectorSubsystem::StartTimerIfNeeded()
{
	if (PlaybackMode == EESCROWScenarioPlaybackMode::Manual)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TickTimerHandle, this, &UESCROWScenarioDirectorSubsystem::TickScenario, ScenarioTickInterval, true);
	}
}

void UESCROWScenarioDirectorSubsystem::ClearTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
	}
}

void UESCROWScenarioDirectorSubsystem::TickScenario()
{
	if (!bScenarioRunning || bScenarioPaused)
	{
		return;
	}

	CurrentMissionTime += ScenarioTickInterval * FMath::Max(ScenarioSpeedMultiplier, 0.0f);

	if (bWaitingForMilestone)
	{
		CheckMilestoneAdvance();
		return;
	}

	const int32 NextIndex = CurrentEventIndex + 1;
	if (!EventSchedule.IsValidIndex(NextIndex))
	{
		return;
	}

	const FESCROWScenarioEvent& NextEvent = EventSchedule[NextIndex];
	if (NextEvent.ScheduledTimeSeconds >= 0.0f && CurrentMissionTime >= NextEvent.ScheduledTimeSeconds)
	{
		AdvanceToNextEvent();
	}
}

FString UESCROWScenarioDirectorSubsystem::FormatMissionTime(float Seconds)
{
	const int32 TotalSeconds = FMath::Max(FMath::FloorToInt(Seconds), 0);
	const int32 Minutes = TotalSeconds / 60;
	const int32 Secs = TotalSeconds % 60;
	return FString::Printf(TEXT("%02d:%02d"), Minutes, Secs);
}

FString UESCROWScenarioDirectorSubsystem::GetCurrentMissionTimeText() const
{
	return FormatMissionTime(CurrentMissionTime);
}

FString UESCROWScenarioDirectorSubsystem::GetCurrentEventLabel() const
{
	if (EventSchedule.IsValidIndex(CurrentEventIndex))
	{
		return EventSchedule[CurrentEventIndex].EventLabel;
	}
	return TEXT("Not started");
}

FString UESCROWScenarioDirectorSubsystem::GetNextEventLabel() const
{
	const int32 NextIndex = CurrentEventIndex + 1;
	if (EventSchedule.IsValidIndex(NextIndex))
	{
		return EventSchedule[NextIndex].EventLabel;
	}
	return TEXT("(none - scenario complete)");
}

FString UESCROWScenarioDirectorSubsystem::GetPlaybackModeText() const
{
	switch (PlaybackMode)
	{
	case EESCROWScenarioPlaybackMode::Manual: return TEXT("Manual");
	case EESCROWScenarioPlaybackMode::Timed: return TEXT("Timed");
	case EESCROWScenarioPlaybackMode::Hybrid: return TEXT("Hybrid");
	default: return TEXT("Unknown");
	}
}

FString UESCROWScenarioDirectorSubsystem::RunStateToText() const
{
	switch (RunState)
	{
	case EESCROWScenarioRunState::NotLoaded: return TEXT("Not Loaded");
	case EESCROWScenarioRunState::Ready: return TEXT("Ready");
	case EESCROWScenarioRunState::Running: return TEXT("Running");
	case EESCROWScenarioRunState::Paused: return TEXT("Paused");
	case EESCROWScenarioRunState::WaitingForMilestone: return TEXT("Waiting");
	case EESCROWScenarioRunState::Complete: return TEXT("Complete");
	default: return TEXT("Unknown");
	}
}

FString UESCROWScenarioDirectorSubsystem::GetCurrentScenarioStatus() const
{
	if (bScenarioPaused)
	{
		return TEXT("Paused");
	}

	if (RunState == EESCROWScenarioRunState::WaitingForMilestone)
	{
		if (WaitingMilestoneName == Milestone_F3TakeoverActive)
		{
			return TEXT("Waiting for takeover");
		}
		if (WaitingMilestoneName == Milestone_F3ReturnComplete)
		{
			return TEXT("Waiting for return");
		}
		return TEXT("Waiting for milestone");
	}

	return RunStateToText();
}

FString UESCROWScenarioDirectorSubsystem::GetScenarioReadoutText() const
{
	return FString::Printf(
		TEXT("Scenario: %s\nMode: %s\nCurrent: %s\nNext: %s\nTime: %s\nStatus: %s\nLast: %s"),
		*ScenarioName,
		*GetPlaybackModeText(),
		*GetCurrentEventLabel(),
		*GetNextEventLabel(),
		*GetCurrentMissionTimeText(),
		*GetCurrentScenarioStatus(),
		CurrentEventMessageText.IsEmpty() ? TEXT("-") : *CurrentEventMessageText
	);
}
