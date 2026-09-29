// Fill out your copyright notice in the Description page of Project Settings.

#include "ESCROWScenarioDirectorSubsystem.h"
#include "ESCROWFleetSubsystem.h"
#include "ESCROWEventLogSubsystem.h"
#include "ESCROWSessionSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

namespace
{
	const FString Milestone_F3TakeoverActive = TEXT("F3TakeoverActive");
	const FString Milestone_F3ReturnComplete = TEXT("F3ReturnComplete");
	constexpr float ScenarioTickInterval = 0.25f;

	// RA41B PART 1: menu/setup vs. pilot/scenario map detection. Substring match
	// so this is robust to the "UEDPIE_N_" prefix PIE adds to World->GetMapName().
	bool IsMenuOrSetupMap(const FString& MapName)
	{
		return MapName.Contains(TEXT("MAIN_MENU")) || MapName.Contains(TEXT("_MENU"));
	}

	bool IsPilotOrScenarioMap(const FString& MapName)
	{
		return MapName.Contains(TEXT("FLAT_PILOT_TEST")) || MapName.Contains(TEXT("DESERT_INTEGRATION_TEST"));
	}

	// Central telemetry-source guarantee (see EnsureTelemetrySource). Hardcoded
	// path rather than a UPROPERTY(TSubclassOf) so this stays a self-contained
	// startup guarantee with no new editor-facing configuration surface.
	const TCHAR* ConvoyTelemetryManagerClassPath = TEXT("/Game/ESCROW/Blueprints/Scenario/BP_ESCROW_ConvoyTelemetryManager.BP_ESCROW_ConvoyTelemetryManager_C");

	// RA42A: local JSON-string escaping for BuildTaskingPayloadJSON. Mirrors
	// UESCROWEventLogSubsystem::EscapeJSONField (private there), duplicated here
	// rather than exposing it, since ScenarioDirector must not depend on
	// EventLog internals.
	FString EscapeJSONFieldLocal(const FString& Field)
	{
		FString Escaped = Field;
		Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
		Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		return Escaped;
	}
}

void UESCROWScenarioDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	InitializeScenario();
	// Run/EventLog must start before BeginScenarioRuntimeForWorld() so its
	// ScenarioRuntimeInitializing event actually reaches the structured log
	// instead of being dropped by RecordScenarioEvent's "no active run" guard.
	InitializeRunDataForCurrentLevel();
	BeginScenarioRuntimeForWorld();
}

void UESCROWScenarioDirectorSubsystem::Deinitialize()
{
	if (bRuntimeIsPlayableScenario && ScenarioRuntimeState != EESCROWScenarioRuntimeState::NotInitialized)
	{
		ScenarioRuntimeState = EESCROWScenarioRuntimeState::Stopped;
		UE_LOG(LogTemp, Log, TEXT("ESCROWScenarioDirector: ScenarioRuntimeStopped - %s"), *GetScenarioRuntimeStateSummary());
	}

	ClearTimer();
	Super::Deinitialize();
}

void UESCROWScenarioDirectorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Placed level actors (including a hand-placed ConvoyTelemetryManager) are
	// not reliably visible to a TActorIterator scan yet during Initialize() -
	// confirmed via PIE testing: a mid-session level transition (menu -> pilot
	// map via "Start Session") still had an empty actor scan at Initialize()
	// time even though the map has one placed, while the same scan at
	// OnWorldBeginPlay (fired once the world is actually ready for actors to
	// interact) correctly finds it. This is the right hook for the guarantee.
	EnsureTelemetrySource();

	if (bRuntimeIsPlayableScenario)
	{
		ValidateScenarioRuntimeServices();
	}
}

void UESCROWScenarioDirectorSubsystem::BeginScenarioRuntimeForWorld()
{
	UWorld* World = GetWorld();
	RuntimeWorldName = World ? World->GetMapName() : FString();
	bRuntimeIsPlayableScenario = IsPilotOrScenarioMap(RuntimeWorldName);
	ScenarioRuntimeState = EESCROWScenarioRuntimeState::WorldDetected;
	RuntimeFailureReason.Empty();

	LogScenarioRuntimeEvent(TEXT("ScenarioRuntimeInitializing"), FString::Printf(
		TEXT("World=%s IsPlayableScenario=%s"), *RuntimeWorldName, bRuntimeIsPlayableScenario ? TEXT("true") : TEXT("false")));
}

void UESCROWScenarioDirectorSubsystem::ValidateScenarioRuntimeServices()
{
	// Menu/setup or unrecognized maps have no fleet/telemetry services to
	// validate; leave the runtime at WorldDetected rather than misreporting a
	// Ready/Failed verdict for a world that was never meant to have a scenario.
	if (!bRuntimeIsPlayableScenario)
	{
		return;
	}

	ScenarioRuntimeState = EESCROWScenarioRuntimeState::ServicesChecking;

	UESCROWEventLogSubsystem* EventLog = GetEventLog();
	bRuntimeEventLogAvailable = (EventLog != nullptr);
	bRuntimeSessionAvailable = (EventLog != nullptr) && EventLog->GetRunActive();
	RuntimeFleetValidCount = ComputeRuntimeFleetValidCount();
	RuntimeScenarioID = (EventLog && !EventLog->CurrentRunID.IsEmpty()) ? EventLog->CurrentRunID : ScenarioID;

	LogScenarioRuntimeEvent(TEXT("ScenarioRuntimeServiceCheck"), GetScenarioRuntimeStateSummary());

	// Fleet ref count is informational only (not gated on): individual vehicles
	// register themselves with UESCROWFleetSubsystem from their own BeginPlay,
	// which may not all have run yet at this exact point in world-begin-play -
	// a low count here can be a startup-order snapshot, not a real failure. The
	// count is still real and honestly computed, just not used as a Ready/Failed
	// gate. Telemetry source and EventLog are the two things that are load-bearing.
	if (!bTelemetrySourceAvailable)
	{
		MarkScenarioRuntimeFailed(TEXT("Telemetry source unavailable: ") + TelemetrySourceStatus);
		return;
	}

	if (!bRuntimeEventLogAvailable)
	{
		MarkScenarioRuntimeFailed(TEXT("EventLog subsystem unavailable."));
		return;
	}

	MarkScenarioRuntimeReady();
}

void UESCROWScenarioDirectorSubsystem::MarkScenarioRuntimeReady()
{
	ScenarioRuntimeState = EESCROWScenarioRuntimeState::Ready;
	RuntimeFailureReason.Empty();
	LogScenarioRuntimeEvent(TEXT("ScenarioRuntimeReady"), GetScenarioRuntimeStateSummary());
}

void UESCROWScenarioDirectorSubsystem::MarkScenarioRuntimeFailed(const FString& Reason)
{
	ScenarioRuntimeState = EESCROWScenarioRuntimeState::Failed;
	RuntimeFailureReason = Reason;
	LogScenarioRuntimeEvent(TEXT("ScenarioRuntimeFailed"), GetScenarioRuntimeStateSummary());
}

FString UESCROWScenarioDirectorSubsystem::GetScenarioRuntimeStateSummary() const
{
	FString Summary = FString::Printf(
		TEXT("%s | World=%s | ScenarioID=%s | Telemetry=%s | Fleet=%d/5 | EventLog=%s | Session=%s"),
		*ScenarioRuntimeStateToText(ScenarioRuntimeState),
		*RuntimeWorldName,
		*RuntimeScenarioID,
		*TelemetrySourceStatus,
		RuntimeFleetValidCount,
		bRuntimeEventLogAvailable ? TEXT("Yes") : TEXT("No"),
		bRuntimeSessionAvailable ? TEXT("Yes") : TEXT("No"));

	if (!RuntimeFailureReason.IsEmpty())
	{
		Summary += FString::Printf(TEXT(" | Reason=%s"), *RuntimeFailureReason);
	}

	return Summary;
}

FString UESCROWScenarioDirectorSubsystem::ScenarioRuntimeStateToText(EESCROWScenarioRuntimeState State) const
{
	switch (State)
	{
	case EESCROWScenarioRuntimeState::NotInitialized: return TEXT("NotInitialized");
	case EESCROWScenarioRuntimeState::WorldDetected: return TEXT("WorldDetected");
	case EESCROWScenarioRuntimeState::ServicesChecking: return TEXT("ServicesChecking");
	case EESCROWScenarioRuntimeState::Ready: return TEXT("Ready");
	case EESCROWScenarioRuntimeState::Failed: return TEXT("Failed");
	case EESCROWScenarioRuntimeState::Stopped: return TEXT("Stopped");
	default: return TEXT("Unknown");
	}
}

int32 UESCROWScenarioDirectorSubsystem::ComputeRuntimeFleetValidCount() const
{
	int32 ValidCount = 0;

	if (const UESCROWFleetSubsystem* Fleet = GetFleet())
	{
		for (const FESCROWFollowerStatus& Status : Fleet->FollowerStatuses)
		{
			if (Status.FollowerID >= 1 && Status.FollowerID <= 4 && IsValid(Status.ActorRef))
			{
				++ValidCount;
			}
		}
	}

	UWorld* World = GetWorld();
	if (World && bTelemetrySourceAvailable)
	{
		// CV validity is checked via the resolved ConvoyTelemetryManager's own
		// VehicleRefs[0] (the same ground truth GPS/Reports already read), called
		// through reflection since BP_ESCROW_ConvoyTelemetryManager is a
		// pure-Blueprint class with no C++ base to declare this call against.
		UClass* CTMClass = StaticLoadClass(AActor::StaticClass(), nullptr, ConvoyTelemetryManagerClassPath);
		if (CTMClass)
		{
			for (TActorIterator<AActor> It(World, CTMClass); It; ++It)
			{
				if (!IsValid(*It))
				{
					continue;
				}

				if (UFunction* Func = It->FindFunction(FName(TEXT("GetVehicleRefs"))))
				{
					struct FGetVehicleRefsParams
					{
						TArray<AActor*> ReturnValue;
					} Params;

					It->ProcessEvent(Func, &Params);
					if (Params.ReturnValue.IsValidIndex(0) && IsValid(Params.ReturnValue[0]))
					{
						++ValidCount;
					}
				}
				break;
			}
		}
	}

	return ValidCount;
}

void UESCROWScenarioDirectorSubsystem::LogScenarioRuntimeEvent(const FString& EventType, const FString& Detail) const
{
	UE_LOG(LogTemp, Log, TEXT("ESCROWScenarioDirector: %s - %s"), *EventType, *Detail);

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(ScenarioRuntimeStateToText(ScenarioRuntimeState), EventType, Detail, FString(), 0, TEXT("Unspecified"), FString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ESCROWScenarioDirector: EventLog unavailable - %s not recorded to structured log."), *EventType);
	}
}

void UESCROWScenarioDirectorSubsystem::EnsureTelemetrySource()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FString RawMapName = World->GetMapName();
	if (!IsPilotOrScenarioMap(RawMapName))
	{
		// Menu/setup or unrecognized maps have no fleet to instrument; leave
		// telemetry unresolved rather than spawning a manager with nothing to find.
		bTelemetrySourceAvailable = false;
		TelemetrySourceStatus = TEXT("Not applicable (non-scenario map).");
		return;
	}

	UClass* CTMClass = StaticLoadClass(AActor::StaticClass(), nullptr, ConvoyTelemetryManagerClassPath);
	if (!CTMClass)
	{
		bTelemetrySourceAvailable = false;
		TelemetrySourceStatus = TEXT("Telemetry unavailable: ConvoyTelemetryManager class failed to load.");
		UE_LOG(LogTemp, Error, TEXT("ESCROWScenarioDirector: could not load class %s to guarantee a telemetry source."), ConvoyTelemetryManagerClassPath);
		if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
		{
			EventLog->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("TelemetrySourceMissing"), TelemetrySourceStatus, FString(), 0, TEXT("Unspecified"), FString());
		}
		return;
	}

	for (TActorIterator<AActor> It(World, CTMClass); It; ++It)
	{
		if (IsValid(*It))
		{
			bTelemetrySourceAvailable = true;
			TelemetrySourceStatus = TEXT("Telemetry source present (placed in level).");
			return;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Spawned = World->SpawnActor<AActor>(CTMClass, FTransform::Identity, SpawnParams);

	bTelemetrySourceAvailable = (Spawned != nullptr);
	TelemetrySourceStatus = Spawned
		? TEXT("Telemetry source spawned at scenario startup (no ConvoyTelemetryManager was placed in this map).")
		: TEXT("Telemetry unavailable: failed to spawn ConvoyTelemetryManager.");

	if (Spawned)
	{
		UE_LOG(LogTemp, Log, TEXT("ESCROWScenarioDirector: %s"), *TelemetrySourceStatus);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ESCROWScenarioDirector: %s"), *TelemetrySourceStatus);
	}

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(CurrentScenarioStep, Spawned ? TEXT("TelemetrySourceSpawned") : TEXT("TelemetrySourceMissing"), TelemetrySourceStatus, FString(), 0, TEXT("Unspecified"), FString());
	}
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

	const FString RawMapName = World ? World->GetMapName() : FString();
	const bool bIsMenuMap = IsMenuOrSetupMap(RawMapName);
	const bool bIsScenarioMap = IsPilotOrScenarioMap(RawMapName);

	// RA41B PART 1 guardrail. See CLAUDE.md / RA41B pass notes for the four cases:
	// (1) pending config -> promote & start normally, fallback=false.
	// (2) no pending config, on a pilot/scenario map, editor/PIE only -> dev fallback.
	// (3) no pending config, on the menu/setup map -> do nothing, EventLog stays inactive.
	// (4) no pending config outside editor/development -> fail closed, no run created.
	FESCROWRunConfig RunConfig;
	bool bFoundPending = false;
	Session->PromotePendingRunConfigToCurrent(RunConfig, bFoundPending);

	if (bFoundPending)
	{
		if (RunConfig.RunID.IsEmpty())
		{
			RunConfig.RunID = Session->GenerateRunID(RunConfig.ScenarioID, RunConfig.ConditionID, RunConfig.ParticipantID);
		}
		if (RunConfig.MapName.IsEmpty())
		{
			RunConfig.MapName = RawMapName;
		}
		if (RunConfig.CreatedAt.GetTicks() == 0)
		{
			RunConfig.CreatedAt = FDateTime::Now();
		}
		if (RunConfig.RunValidity.IsEmpty())
		{
			RunConfig.RunValidity = TEXT("Valid");
		}

		EventLog->BeginRunFromConfig(RunConfig);
		EventLog->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("RunInitialized"), TEXT("Level loaded and run initialized from menu-provided config"), FString(), 0, TEXT("Unspecified"), TEXT("{\"fallback\":false}"));
		EventLog->StartHeartbeat();
		return;
	}

	if (bIsScenarioMap)
	{
		if (!GIsEditor)
		{
			// Case 4: outside editor/development. Fail closed rather than silently
			// producing pilot-valid-looking data with no real run configuration.
			UE_LOG(LogTemp, Error, TEXT("ESCROWScenarioDirector: scenario map %s loaded with no pending run config outside the editor. Refusing to start an unattributed run; EventLog left inactive."), *RawMapName);
			return;
		}

		// Case 2: editor/PIE development fallback, clearly marked as such.
		RunConfig = FESCROWRunConfig();
		RunConfig.ParticipantID = TEXT("DEV");
		RunConfig.ResearcherID = Session->GetResearcherID().IsEmpty() ? TEXT("UNSET") : Session->GetResearcherID();
		RunConfig.SessionID = Session->GetSessionID();
		RunConfig.ScenarioID = TEXT("DEV_NO_PENDING_CONFIG");
		RunConfig.ScenarioVersion = TEXT("dev");
		RunConfig.ConditionID = TEXT("DEV_FALLBACK");
		RunConfig.SupportMode = TEXT("ManualBaseline");
		RunConfig.DisplayMode = Session->GetDisplayModeName();
		RunConfig.VehicleControlRole = Session->GetVehicleControlRoleName();
		RunConfig.WorkloadLevel = TEXT("Unspecified");
		RunConfig.ScenarioVariant = RawMapName.Contains(TEXT("FLAT_PILOT")) ? TEXT("FlatPilot") : TEXT("DesertIntegration");
		RunConfig.EventSalience = TEXT("Unspecified");
		RunConfig.UncertaintyLevel = TEXT("Unspecified");
		RunConfig.MapName = RawMapName;
		RunConfig.CreatedAt = FDateTime::Now();
		RunConfig.RunValidity = TEXT("DevelopmentOnly");
		RunConfig.Notes = TEXT("Fallback run: scenario map launched directly without going through the setup menu (no pending run config was found).");
		RunConfig.RunID = Session->GenerateRunID(RunConfig.ScenarioID, RunConfig.ConditionID, RunConfig.ParticipantID);

		EventLog->BeginRunFromConfig(RunConfig);
		EventLog->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("FallbackRunConfigUsed"), TEXT("Scenario map launched without the setup menu; development-only fallback run config generated"), FString(), 0, TEXT("DevelopmentOnly"), TEXT("{\"fallback\":true}"));
		EventLog->StartHeartbeat();
		return;
	}

	if (bIsMenuMap)
	{
		// Case 3: sitting at the main menu with no pending config is the normal
		// pre-session state. Do not create a run folder or touch EventLog.
		UE_LOG(LogTemp, Log, TEXT("ESCROWScenarioDirector: menu map %s loaded with no pending run config; EventLog left inactive (normal pre-session state)."), *RawMapName);
		return;
	}

	// Unrecognized map with no pending config: be conservative rather than guess.
	UE_LOG(LogTemp, Warning, TEXT("ESCROWScenarioDirector: unrecognized map %s loaded with no pending run config; EventLog left inactive."), *RawMapName);
}

UESCROWFleetSubsystem* UESCROWScenarioDirectorSubsystem::GetFleet() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UESCROWFleetSubsystem>() : nullptr;
}

UESCROWEventLogSubsystem* UESCROWScenarioDirectorSubsystem::GetEventLog() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UESCROWEventLogSubsystem>() : nullptr;
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
	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("ScenarioStarted"), FString::Printf(TEXT("Scenario started in %s mode."), *GetPlaybackModeText()), FString(), 0, TEXT("Unspecified"), FString());
	}

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
	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("ScenarioPaused"), TEXT("Scenario paused."), FString(), 0, TEXT("Unspecified"), FString());
	}
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
	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("ScenarioResumed"), TEXT("Scenario resumed."), FString(), 0, TEXT("Unspecified"), FString());
	}
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
	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("ScenarioEnded"), TEXT("Scenario stopped by operator."), FString(), 0, TEXT("Unspecified"), FString());
	}
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
	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("ScenarioReset"), TEXT("Scenario reset."), FString(), 0, TEXT("Unspecified"), FString());
	}
	OutStatusMessage = TEXT("Scenario reset.");
	return true;
}

void UESCROWScenarioDirectorSubsystem::EnterEventState(const FESCROWScenarioEvent& Event, bool bManualOverrideOfWait)
{
	// RA41B PART 4: captured before mutation so PhaseChanged/AffectedFollowerChanged
	// can report accurate old->new values. Purely additive - does not change any
	// existing control flow below.
	UESCROWEventLogSubsystem* EventLogForHooks = GetEventLog();
	const FString OldPhase = CurrentScenarioStep;
	const int32 OldSelectedFollowerID = GetFleet() ? GetFleet()->SelectedFollowerID : -1;

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

	// RA41B PART 4: structured scenario-truth records for this event, additive
	// only - none of the above gameplay/state logic is affected by this block.
	if (EventLogForHooks)
	{
		if (OldPhase != CurrentScenarioStep)
		{
			EventLogForHooks->RecordStateTransitionSimple(TEXT("Scenario"), TEXT("Phase"), TEXT("Phase"), OldPhase, CurrentScenarioStep, TEXT("ScenarioDirector"), Event.EventID);
			EventLogForHooks->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("PhaseChanged"), FString::Printf(TEXT("%s -> %s"), *OldPhase, *CurrentScenarioStep), FString(), Event.FollowerID, TEXT("Unspecified"), FString());
		}

		const int32 NewSelectedFollowerID = Fleet ? Fleet->SelectedFollowerID : -1;
		if (NewSelectedFollowerID != OldSelectedFollowerID)
		{
			EventLogForHooks->RecordStateTransitionSimple(TEXT("Fleet"), TEXT("SelectedFollower"), TEXT("AffectedFollowerID"), FString::FromInt(OldSelectedFollowerID), FString::FromInt(NewSelectedFollowerID), TEXT("ScenarioDirector"), Event.EventID);
			EventLogForHooks->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("AffectedFollowerChanged"), FString::Printf(TEXT("Selected follower %d -> %d"), OldSelectedFollowerID, NewSelectedFollowerID), FString(), NewSelectedFollowerID, TEXT("Unspecified"), FString());
		}

		if (Event.EventType == TEXT("CommanderMessage") && Event.FollowerID >= 1)
		{
			EventLogForHooks->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("TaskingPosted"), Event.MessageText, Event.EventID, Event.FollowerID, TEXT("Unspecified"), FString());
		}
		else if (Event.EventType == TEXT("FollowerObjectiveUpdate"))
		{
			// No distinct "acknowledge" gameplay step exists yet; this fires when the
			// follower's objective/state is updated to reflect it acting on the
			// tasking, which is the closest existing lifecycle hook.
			EventLogForHooks->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("TaskingAcknowledged"), Event.MessageText, Event.EventID, Event.FollowerID, TEXT("Unspecified"), FString());
		}
		else if (Event.EventType == TEXT("ReportReady"))
		{
			// No distinct "validated"/"authorized" operator step exists yet either;
			// ReportReady is the closest existing signal that a tasking's result is done.
			EventLogForHooks->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("TaskingCompleted"), Event.MessageText, Event.EventID, Event.FollowerID, TEXT("Unspecified"), FString());
		}

		if (!bWaitingForMilestone)
		{
			EventLogForHooks->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("EventTriggered"), Event.MessageText, Event.EventID, Event.FollowerID, TEXT("Unspecified"), FString());
		}

		if (RunState == EESCROWScenarioRunState::Complete)
		{
			EventLogForHooks->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("ScenarioEnded"), TEXT("Scenario reached its final scripted event."), FString(), 0, TEXT("Unspecified"), FString());
		}
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
	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(CurrentScenarioStep, TEXT("EventResolved"), FString::Printf(TEXT("Milestone satisfied: %s"), *WaitingMilestoneName), FString(), 3, TEXT("Unspecified"), FString());
	}
	bWaitingForMilestone = false;
	WaitingMilestoneName.Empty();

	return AdvanceToNextEvent();
}

void UESCROWScenarioDirectorSubsystem::LogScenarioEvent(const FString& ScenarioLogType, int32 FollowerID, const FString& Detail)
{
	UESCROWEventLogSubsystem* EventLog = GetEventLog();
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

// =============================== RA42A: generic scenario phase / tasking lifecycle ===============================
// Additive skeleton alongside the Mission-D schedule above. Does not touch or replace it.

FString UESCROWScenarioDirectorSubsystem::ScenarioPhaseToText(EESCROWScenarioPhase Phase) const
{
	switch (Phase)
	{
	case EESCROWScenarioPhase::None: return TEXT("None");
	case EESCROWScenarioPhase::Preflight: return TEXT("Preflight");
	case EESCROWScenarioPhase::PhaseA_RoutineMonitoring: return TEXT("PhaseA_RoutineMonitoring");
	case EESCROWScenarioPhase::PhaseB_RallyBreakout: return TEXT("PhaseB_RallyBreakout");
	case EESCROWScenarioPhase::PhaseC_DisruptionDetection: return TEXT("PhaseC_DisruptionDetection");
	case EESCROWScenarioPhase::PhaseD_TakeoverIntervention: return TEXT("PhaseD_TakeoverIntervention");
	case EESCROWScenarioPhase::PhaseE_PlanRepair: return TEXT("PhaseE_PlanRepair");
	case EESCROWScenarioPhase::PhaseF_Closeout: return TEXT("PhaseF_Closeout");
	case EESCROWScenarioPhase::Complete: return TEXT("Complete");
	case EESCROWScenarioPhase::Aborted: return TEXT("Aborted");
	default: return TEXT("Unknown");
	}
}

FString UESCROWScenarioDirectorSubsystem::TaskingStateToText(EESCROWTaskingState State) const
{
	switch (State)
	{
	case EESCROWTaskingState::None: return TEXT("None");
	case EESCROWTaskingState::Posted: return TEXT("Posted");
	case EESCROWTaskingState::Acknowledged: return TEXT("Acknowledged");
	case EESCROWTaskingState::Validated: return TEXT("Validated");
	case EESCROWTaskingState::Authorized: return TEXT("Authorized");
	case EESCROWTaskingState::Completed: return TEXT("Completed");
	case EESCROWTaskingState::Cancelled: return TEXT("Cancelled");
	case EESCROWTaskingState::Failed: return TEXT("Failed");
	default: return TEXT("Unknown");
	}
}

FString UESCROWScenarioDirectorSubsystem::BuildTaskingPayloadJSON(const FString& ReasonOrSource) const
{
	return FString::Printf(
		TEXT("{\"TaskingID\":\"%s\",\"TaskingLabel\":\"%s\",\"Phase\":\"%s\",\"TaskingState\":\"%s\",\"RequiredParticipantAction\":\"%s\",\"AffectedFollowerID\":%d,\"PlanValidity\":\"%s\",\"EventID\":\"%s\",\"Reason\":\"%s\"}"),
		*EscapeJSONFieldLocal(ActiveTasking.TaskingID),
		*EscapeJSONFieldLocal(ActiveTasking.TaskingLabel),
		*EscapeJSONFieldLocal(ScenarioPhaseToText(ActiveTasking.Phase)),
		*EscapeJSONFieldLocal(TaskingStateToText(ActiveTasking.State)),
		*EscapeJSONFieldLocal(ActiveTasking.RequiredParticipantAction),
		ActiveTasking.AffectedFollowerID,
		*EscapeJSONFieldLocal(ActiveTasking.PlanValidity),
		*EscapeJSONFieldLocal(ActiveTasking.EventID),
		*EscapeJSONFieldLocal(ReasonOrSource));
}

EESCROWScenarioPhase UESCROWScenarioDirectorSubsystem::GetCurrentPhase() const
{
	return CurrentPhase;
}

FString UESCROWScenarioDirectorSubsystem::GetCurrentPhaseName() const
{
	return ScenarioPhaseToText(CurrentPhase);
}

FESCROWTasking UESCROWScenarioDirectorSubsystem::GetActiveTasking() const
{
	return ActiveTasking;
}

FString UESCROWScenarioDirectorSubsystem::GetActiveTaskingID() const
{
	return ActiveTasking.TaskingID;
}

FString UESCROWScenarioDirectorSubsystem::GetActiveTaskingLabel() const
{
	return ActiveTasking.TaskingLabel;
}

EESCROWTaskingState UESCROWScenarioDirectorSubsystem::GetActiveTaskingState() const
{
	return ActiveTasking.State;
}

FString UESCROWScenarioDirectorSubsystem::GetActiveTaskingStateName() const
{
	return TaskingStateToText(ActiveTasking.State);
}

FString UESCROWScenarioDirectorSubsystem::GetRequiredParticipantAction() const
{
	return ActiveTasking.RequiredParticipantAction;
}

FString UESCROWScenarioDirectorSubsystem::GetCurrentPlanValidity() const
{
	return CurrentPlanValidity;
}

int32 UESCROWScenarioDirectorSubsystem::GetCurrentAffectedFollowerID() const
{
	return CurrentAffectedFollowerID;
}

bool UESCROWScenarioDirectorSubsystem::IsGenericTaskingActive() const
{
	return bGenericTaskingActive;
}

bool UESCROWScenarioDirectorSubsystem::IsGenericScenarioComplete() const
{
	return bGenericScenarioComplete;
}

FString UESCROWScenarioDirectorSubsystem::GetScenarioCompletionReason() const
{
	return ScenarioCompletionReason;
}

void UESCROWScenarioDirectorSubsystem::StartGenericScenario()
{
	CurrentPhase = EESCROWScenarioPhase::Preflight;
	bGenericScenarioComplete = false;
	ScenarioCompletionReason.Empty();
	CurrentPlanValidity = TEXT("Valid");
	CurrentAffectedFollowerID = 0;
	ActiveTasking = FESCROWTasking();
	bGenericTaskingActive = false;

	LogScenarioEvent(TEXT("GenericScenarioStarted"), 0, TEXT("Generic scenario tasking lifecycle started."));
	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(ScenarioPhaseToText(CurrentPhase), TEXT("GenericScenarioStarted"), TEXT("Generic scenario tasking lifecycle started."), FString(), 0, CurrentPlanValidity, BuildTaskingPayloadJSON(TEXT("StartGenericScenario")));
	}
}

void UESCROWScenarioDirectorSubsystem::SetScenarioPhase(EESCROWScenarioPhase NewPhase, FString Reason)
{
	const EESCROWScenarioPhase OldPhase = CurrentPhase;
	if (OldPhase == NewPhase)
	{
		return;
	}

	CurrentPhase = NewPhase;

	const FString Detail = FString::Printf(TEXT("%s -> %s (%s)"), *ScenarioPhaseToText(OldPhase), *ScenarioPhaseToText(NewPhase), *Reason);

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordStateTransitionSimple(TEXT("Scenario"), TEXT("Phase"), TEXT("Phase"), ScenarioPhaseToText(OldPhase), ScenarioPhaseToText(NewPhase), TEXT("ScenarioDirector"), ActiveTasking.EventID);
		EventLog->RecordScenarioEventSimple(ScenarioPhaseToText(NewPhase), TEXT("PhaseChanged"), Detail, ActiveTasking.TaskingID, CurrentAffectedFollowerID, CurrentPlanValidity, BuildTaskingPayloadJSON(Reason));
	}
	LogScenarioEvent(TEXT("PhaseChanged"), CurrentAffectedFollowerID, Detail);
}

void UESCROWScenarioDirectorSubsystem::PostTasking(FString TaskingID, FString TaskingLabel, EESCROWScenarioPhase Phase, FString RequiredParticipantAction, int32 AffectedFollowerID, FString PlanValidity, FString EventID)
{
	ActiveTasking = FESCROWTasking();
	ActiveTasking.TaskingID = TaskingID;
	ActiveTasking.TaskingLabel = TaskingLabel;
	ActiveTasking.Phase = Phase;
	ActiveTasking.State = EESCROWTaskingState::Posted;
	ActiveTasking.RequiredParticipantAction = RequiredParticipantAction;
	ActiveTasking.AffectedFollowerID = AffectedFollowerID;
	ActiveTasking.PlanValidity = PlanValidity;
	ActiveTasking.EventID = EventID;
	ActiveTasking.PostedAt = FDateTime::Now();
	ActiveTasking.bIsActive = true;

	bGenericTaskingActive = true;
	CurrentPhase = Phase;
	CurrentPlanValidity = PlanValidity;
	CurrentAffectedFollowerID = AffectedFollowerID;

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(ScenarioPhaseToText(CurrentPhase), TEXT("TaskingPosted"), TaskingLabel, TaskingID, AffectedFollowerID, PlanValidity, BuildTaskingPayloadJSON(TEXT("PostTasking")));
		EventLog->RecordStateTransitionSimple(TaskingID, TEXT("Tasking"), TEXT("TaskingState"), TaskingStateToText(EESCROWTaskingState::None), TaskingStateToText(EESCROWTaskingState::Posted), TEXT("ScenarioDirector"), EventID);
	}
	LogScenarioEvent(TEXT("TaskingPosted"), AffectedFollowerID, FString::Printf(TEXT("%s: %s"), *TaskingID, *TaskingLabel));
}

bool UESCROWScenarioDirectorSubsystem::AcknowledgeActiveTasking(FString Source)
{
	if (!ActiveTasking.bIsActive || ActiveTasking.State != EESCROWTaskingState::Posted)
	{
		return false;
	}

	const EESCROWTaskingState OldState = ActiveTasking.State;
	ActiveTasking.State = EESCROWTaskingState::Acknowledged;
	ActiveTasking.AcknowledgedAt = FDateTime::Now();

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(ScenarioPhaseToText(CurrentPhase), TEXT("TaskingAcknowledged"), ActiveTasking.TaskingLabel, ActiveTasking.TaskingID, ActiveTasking.AffectedFollowerID, CurrentPlanValidity, BuildTaskingPayloadJSON(Source));
		EventLog->RecordStateTransitionSimple(ActiveTasking.TaskingID, TEXT("Tasking"), TEXT("TaskingState"), TaskingStateToText(OldState), TaskingStateToText(ActiveTasking.State), Source, ActiveTasking.EventID);
	}
	LogScenarioEvent(TEXT("TaskingAcknowledged"), ActiveTasking.AffectedFollowerID, ActiveTasking.TaskingID);
	return true;
}

bool UESCROWScenarioDirectorSubsystem::ValidateActiveTasking(FString Source)
{
	if (!ActiveTasking.bIsActive || !(ActiveTasking.State == EESCROWTaskingState::Posted || ActiveTasking.State == EESCROWTaskingState::Acknowledged))
	{
		return false;
	}

	const EESCROWTaskingState OldState = ActiveTasking.State;
	ActiveTasking.State = EESCROWTaskingState::Validated;
	ActiveTasking.ValidatedAt = FDateTime::Now();

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(ScenarioPhaseToText(CurrentPhase), TEXT("TaskingValidated"), ActiveTasking.TaskingLabel, ActiveTasking.TaskingID, ActiveTasking.AffectedFollowerID, CurrentPlanValidity, BuildTaskingPayloadJSON(Source));
		EventLog->RecordStateTransitionSimple(ActiveTasking.TaskingID, TEXT("Tasking"), TEXT("TaskingState"), TaskingStateToText(OldState), TaskingStateToText(ActiveTasking.State), Source, ActiveTasking.EventID);
	}
	LogScenarioEvent(TEXT("TaskingValidated"), ActiveTasking.AffectedFollowerID, ActiveTasking.TaskingID);
	return true;
}

bool UESCROWScenarioDirectorSubsystem::AuthorizeActiveTasking(FString Source)
{
	if (!ActiveTasking.bIsActive || !(ActiveTasking.State == EESCROWTaskingState::Validated || ActiveTasking.State == EESCROWTaskingState::Acknowledged))
	{
		return false;
	}

	const EESCROWTaskingState OldState = ActiveTasking.State;
	ActiveTasking.State = EESCROWTaskingState::Authorized;
	ActiveTasking.AuthorizedAt = FDateTime::Now();

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(ScenarioPhaseToText(CurrentPhase), TEXT("TaskingAuthorized"), ActiveTasking.TaskingLabel, ActiveTasking.TaskingID, ActiveTasking.AffectedFollowerID, CurrentPlanValidity, BuildTaskingPayloadJSON(Source));
		EventLog->RecordStateTransitionSimple(ActiveTasking.TaskingID, TEXT("Tasking"), TEXT("TaskingState"), TaskingStateToText(OldState), TaskingStateToText(ActiveTasking.State), Source, ActiveTasking.EventID);
	}
	LogScenarioEvent(TEXT("TaskingAuthorized"), ActiveTasking.AffectedFollowerID, ActiveTasking.TaskingID);
	return true;
}

bool UESCROWScenarioDirectorSubsystem::CompleteActiveTasking(FString CompletionReason)
{
	if (!ActiveTasking.bIsActive
		|| ActiveTasking.State == EESCROWTaskingState::Completed
		|| ActiveTasking.State == EESCROWTaskingState::Cancelled
		|| ActiveTasking.State == EESCROWTaskingState::Failed)
	{
		return false;
	}

	const EESCROWTaskingState OldState = ActiveTasking.State;
	ActiveTasking.State = EESCROWTaskingState::Completed;
	ActiveTasking.CompletedAt = FDateTime::Now();
	ActiveTasking.CompletionReason = CompletionReason;
	bGenericTaskingActive = false;

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(ScenarioPhaseToText(CurrentPhase), TEXT("TaskingCompleted"), CompletionReason, ActiveTasking.TaskingID, ActiveTasking.AffectedFollowerID, CurrentPlanValidity, BuildTaskingPayloadJSON(CompletionReason));
		EventLog->RecordStateTransitionSimple(ActiveTasking.TaskingID, TEXT("Tasking"), TEXT("TaskingState"), TaskingStateToText(OldState), TaskingStateToText(ActiveTasking.State), TEXT("ScenarioDirector"), ActiveTasking.EventID);
	}
	LogScenarioEvent(TEXT("TaskingCompleted"), ActiveTasking.AffectedFollowerID, FString::Printf(TEXT("%s: %s"), *ActiveTasking.TaskingID, *CompletionReason));

	AdvancePhaseIfTaskingComplete();
	return true;
}

bool UESCROWScenarioDirectorSubsystem::CancelActiveTasking(FString Reason)
{
	if (!ActiveTasking.bIsActive
		|| ActiveTasking.State == EESCROWTaskingState::Completed
		|| ActiveTasking.State == EESCROWTaskingState::Cancelled
		|| ActiveTasking.State == EESCROWTaskingState::Failed)
	{
		return false;
	}

	const EESCROWTaskingState OldState = ActiveTasking.State;
	ActiveTasking.State = EESCROWTaskingState::Cancelled;
	ActiveTasking.CompletionReason = Reason;
	bGenericTaskingActive = false;

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(ScenarioPhaseToText(CurrentPhase), TEXT("TaskingCancelled"), Reason, ActiveTasking.TaskingID, ActiveTasking.AffectedFollowerID, CurrentPlanValidity, BuildTaskingPayloadJSON(Reason));
		EventLog->RecordStateTransitionSimple(ActiveTasking.TaskingID, TEXT("Tasking"), TEXT("TaskingState"), TaskingStateToText(OldState), TaskingStateToText(ActiveTasking.State), TEXT("ScenarioDirector"), ActiveTasking.EventID);
	}
	LogScenarioEvent(TEXT("TaskingCancelled"), ActiveTasking.AffectedFollowerID, FString::Printf(TEXT("%s: %s"), *ActiveTasking.TaskingID, *Reason));
	return true;
}

void UESCROWScenarioDirectorSubsystem::SetPlanValidity(FString NewPlanValidity, FString Reason)
{
	const FString OldValidity = CurrentPlanValidity;
	if (OldValidity == NewPlanValidity)
	{
		return;
	}

	CurrentPlanValidity = NewPlanValidity;
	ActiveTasking.PlanValidity = NewPlanValidity;

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordStateTransitionSimple(TEXT("Scenario"), TEXT("PlanValidity"), TEXT("PlanValidity"), OldValidity, NewPlanValidity, TEXT("ScenarioDirector"), ActiveTasking.EventID);
		EventLog->RecordScenarioEventSimple(ScenarioPhaseToText(CurrentPhase), TEXT("PlanValidityChanged"), Reason, ActiveTasking.TaskingID, CurrentAffectedFollowerID, NewPlanValidity, BuildTaskingPayloadJSON(Reason));
	}
	LogScenarioEvent(TEXT("PlanValidityChanged"), CurrentAffectedFollowerID, FString::Printf(TEXT("%s -> %s (%s)"), *OldValidity, *NewPlanValidity, *Reason));
}

void UESCROWScenarioDirectorSubsystem::SetAffectedFollowerID(int32 NewAffectedFollowerID, FString Reason)
{
	const int32 OldID = CurrentAffectedFollowerID;
	if (OldID == NewAffectedFollowerID)
	{
		return;
	}

	CurrentAffectedFollowerID = NewAffectedFollowerID;
	ActiveTasking.AffectedFollowerID = NewAffectedFollowerID;

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordStateTransitionSimple(TEXT("Fleet"), TEXT("AffectedFollowerID"), TEXT("AffectedFollowerID"), FString::FromInt(OldID), FString::FromInt(NewAffectedFollowerID), TEXT("ScenarioDirector"), ActiveTasking.EventID);
		EventLog->RecordScenarioEventSimple(ScenarioPhaseToText(CurrentPhase), TEXT("AffectedFollowerChanged"), Reason, ActiveTasking.TaskingID, NewAffectedFollowerID, CurrentPlanValidity, BuildTaskingPayloadJSON(Reason));
	}
	LogScenarioEvent(TEXT("AffectedFollowerChanged"), NewAffectedFollowerID, FString::Printf(TEXT("%d -> %d (%s)"), OldID, NewAffectedFollowerID, *Reason));
}

void UESCROWScenarioDirectorSubsystem::AdvancePhaseIfTaskingComplete()
{
	EESCROWScenarioPhase NextPhase = CurrentPhase;
	switch (CurrentPhase)
	{
	case EESCROWScenarioPhase::Preflight: NextPhase = EESCROWScenarioPhase::PhaseA_RoutineMonitoring; break;
	case EESCROWScenarioPhase::PhaseA_RoutineMonitoring: NextPhase = EESCROWScenarioPhase::PhaseB_RallyBreakout; break;
	case EESCROWScenarioPhase::PhaseB_RallyBreakout: NextPhase = EESCROWScenarioPhase::PhaseC_DisruptionDetection; break;
	case EESCROWScenarioPhase::PhaseC_DisruptionDetection: NextPhase = EESCROWScenarioPhase::PhaseD_TakeoverIntervention; break;
	case EESCROWScenarioPhase::PhaseD_TakeoverIntervention: NextPhase = EESCROWScenarioPhase::PhaseE_PlanRepair; break;
	case EESCROWScenarioPhase::PhaseE_PlanRepair: NextPhase = EESCROWScenarioPhase::PhaseF_Closeout; break;
	case EESCROWScenarioPhase::PhaseF_Closeout: NextPhase = EESCROWScenarioPhase::Complete; break;
	default:
		// None/Complete/Aborted: no automatic advance.
		return;
	}

	SetScenarioPhase(NextPhase, TEXT("AdvancePhaseIfTaskingComplete"));

	if (NextPhase == EESCROWScenarioPhase::Complete)
	{
		EndGenericScenario(TEXT("Final phase (PhaseF_Closeout) tasking complete."));
	}
}

void UESCROWScenarioDirectorSubsystem::EndGenericScenario(FString EndReason)
{
	bGenericScenarioComplete = true;
	if (CurrentPhase != EESCROWScenarioPhase::Aborted)
	{
		CurrentPhase = EESCROWScenarioPhase::Complete;
	}
	ScenarioCompletionReason = EndReason;
	bGenericTaskingActive = false;

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(ScenarioPhaseToText(CurrentPhase), TEXT("GenericScenarioEnded"), EndReason, ActiveTasking.TaskingID, CurrentAffectedFollowerID, CurrentPlanValidity, BuildTaskingPayloadJSON(EndReason));
	}
	LogScenarioEvent(TEXT("GenericScenarioEnded"), CurrentAffectedFollowerID, EndReason);

	// RA42A: shutdown flush remains EventLog's own responsibility (see RA41D);
	// this deliberately does not call EventLog->EndRun().
}
