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
#include "UObject/UnrealType.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/StructOnScope.h"

namespace
{
	// Best-effort short commit from <ProjectDir>/.git (no process spawn); dirty state is not detected.
	FString ResolveGitCommitLabel()
	{
		const FString GitDir = FPaths::Combine(FPaths::ProjectDir(), TEXT(".git"));
		FString Head;
		if (!FFileHelper::LoadFileToString(Head, *FPaths::Combine(GitDir, TEXT("HEAD"))))
		{
			return TEXT("UNKNOWN_NO_GIT");
		}
		Head.TrimStartAndEndInline();
		FString Sha = Head;
		if (Head.StartsWith(TEXT("ref:")))
		{
			const FString RefPath = Head.Mid(4).TrimStartAndEnd();
			Sha.Reset();
			if (!FFileHelper::LoadFileToString(Sha, *FPaths::Combine(GitDir, RefPath)))
			{
				FString Packed;
				if (FFileHelper::LoadFileToString(Packed, *FPaths::Combine(GitDir, TEXT("packed-refs"))))
				{
					TArray<FString> PackedLines;
					Packed.ParseIntoArrayLines(PackedLines);
					for (const FString& Line : PackedLines)
					{
						if (Line.EndsWith(RefPath))
						{
							Sha = Line.Left(40);
							break;
						}
					}
				}
			}
			Sha.TrimStartAndEndInline();
		}
		return Sha.Len() >= 7 ? Sha.Left(7) : FString(TEXT("UNKNOWN_NO_GIT"));
	}

	FString FactorCode(const FString& Level)
	{
		return Level.Equals(TEXT("Low"), ESearchCase::IgnoreCase) ? TEXT("L") : (Level.Equals(TEXT("High"), ESearchCase::IgnoreCase) ? TEXT("H") : TEXT("U"));
	}

	const FString Milestone_F3TakeoverActive = TEXT("F3TakeoverActive");
	const FString Milestone_F3ReturnComplete = TEXT("F3ReturnComplete");
	constexpr float ScenarioTickInterval = 0.25f;

	// Scenario runtime deferred service-check timing (see TickServiceCheck).
	// 5.0s is the top of the suggested 3-5s range. Fleet was observed live
	// stuck at exactly 4/5 with zero variance across ~17 checks - not a slow
	// settle - which traced to ComputeRuntimeFleetValidCount calling
	// AActor::FindFunction(TEXT("GetVehicleRefs")) against the CV's
	// VehicleRefs array, a plain UPROPERTY with no such UFunction, so the CV
	// contribution silently never counted. Fixed via direct FProperty
	// reflection (see ComputeRuntimeFleetValidCount); 5.0s is kept as
	// reasonable PIE startup margin, not because timing was ever the cause.
	constexpr float ServiceCheckIntervalSeconds = 0.25f;
	constexpr float ServiceCheckTimeoutSeconds = 5.0f;

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
	ClearServiceCheckTimer();
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
	ServiceCheckElapsedSeconds = 0.0f;
	LastLoggedRuntimeFleetValidCount = -1;
	bServiceCheckInitialLogDone = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ServiceCheckTimerHandle);
		World->GetTimerManager().SetTimer(ServiceCheckTimerHandle, this, &UESCROWScenarioDirectorSubsystem::TickServiceCheck, ServiceCheckIntervalSeconds, true);
	}

	// Run the first check immediately rather than waiting a full interval - a
	// world where the fleet is already fully registered by this point (as can
	// happen) reaches Ready right away instead of always paying one tick's delay.
	TickServiceCheck();
}

void UESCROWScenarioDirectorSubsystem::TickServiceCheck()
{
	// Already resolved (Ready/Failed) by an earlier tick, or the runtime moved
	// on for some other reason; stop ticking rather than re-evaluating a decision
	// that's already been made.
	if (ScenarioRuntimeState != EESCROWScenarioRuntimeState::ServicesChecking)
	{
		ClearServiceCheckTimer();
		return;
	}

	UESCROWEventLogSubsystem* EventLog = GetEventLog();
	bRuntimeEventLogAvailable = (EventLog != nullptr);
	bRuntimeSessionAvailable = (EventLog != nullptr) && EventLog->GetRunActive();
	RuntimeFleetValidCount = ComputeRuntimeFleetValidCount();
	RuntimeScenarioID = (EventLog && !EventLog->CurrentRunID.IsEmpty()) ? EventLog->CurrentRunID : ScenarioID;

	if (!bServiceCheckInitialLogDone)
	{
		bServiceCheckInitialLogDone = true;
		LastLoggedRuntimeFleetValidCount = RuntimeFleetValidCount;
		LogScenarioRuntimeEvent(TEXT("ScenarioRuntimeServiceCheck"), GetScenarioRuntimeStateSummary());
	}

	// Telemetry source and EventLog are load-bearing: fail immediately rather
	// than waiting out the timeout, since retrying won't fix a missing subsystem.
	if (!bTelemetrySourceAvailable)
	{
		ClearServiceCheckTimer();
		MarkScenarioRuntimeFailed(TEXT("Telemetry source unavailable: ") + TelemetrySourceStatus);
		return;
	}

	if (!bRuntimeEventLogAvailable)
	{
		ClearServiceCheckTimer();
		MarkScenarioRuntimeFailed(TEXT("EventLog subsystem unavailable."));
		return;
	}

	// Fleet refs (CV+F1-F4) register asynchronously from each vehicle's own
	// BeginPlay, which can run after this world-subsystem's first check - keep
	// polling until they settle to 5/5, or give up after ServiceCheckTimeoutSeconds.
	if (RuntimeFleetValidCount >= 5)
	{
		ClearServiceCheckTimer();
		MarkScenarioRuntimeReady();
		return;
	}

	ServiceCheckElapsedSeconds += ServiceCheckIntervalSeconds;

	if (ServiceCheckElapsedSeconds >= ServiceCheckTimeoutSeconds)
	{
		ClearServiceCheckTimer();
		MarkScenarioRuntimeFailed(FString::Printf(
			TEXT("Fleet refs did not reach 5/5 within timeout (Fleet=%d/5 after %.1fs)."),
			RuntimeFleetValidCount, ServiceCheckElapsedSeconds));
		return;
	}

	// Avoid per-tick log spam: only log when the fleet count actually changes.
	if (RuntimeFleetValidCount != LastLoggedRuntimeFleetValidCount)
	{
		LastLoggedRuntimeFleetValidCount = RuntimeFleetValidCount;
		LogScenarioRuntimeEvent(TEXT("ScenarioRuntimeFleetPending"), GetScenarioRuntimeStateSummary());
	}
}

void UESCROWScenarioDirectorSubsystem::ClearServiceCheckTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ServiceCheckTimerHandle);
	}
}

void UESCROWScenarioDirectorSubsystem::MarkScenarioRuntimeReady()
{
	ScenarioRuntimeState = EESCROWScenarioRuntimeState::Ready;
	RuntimeFailureReason.Empty();
	LogScenarioRuntimeEvent(TEXT("ScenarioRuntimeReady"), GetScenarioRuntimeStateSummary());

	if (UESCROWFleetSubsystem* Fleet = GetFleet())
	{
		Fleet->GenerateInitialMissionReports(ScenarioName, CurrentMissionTime);
	}

	StartPilotPhaseFlow();
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
		// VehicleRefs[0] (the same ground truth GPS/Reports already read).
		// VehicleRefs is a plain TArray<AActor*> UPROPERTY, not a UFunction -
		// confirmed via ObjectTools.list_properties after an earlier version of
		// this code called AActor::FindFunction(TEXT("GetVehicleRefs")), which
		// silently returned nullptr every time (no such UFunction exists; the
		// DSL's "GetVehicleRefs" is just how it names an auto-generated variable
		// getter node, not a real reflected function), permanently undercounting
		// CV by exactly 1 - read the array property directly via FProperty
		// reflection instead, since BP_ESCROW_ConvoyTelemetryManager is a
		// pure-Blueprint class with no C++ base to declare a typed accessor against.
		UClass* CTMClass = StaticLoadClass(AActor::StaticClass(), nullptr, ConvoyTelemetryManagerClassPath);
		if (CTMClass)
		{
			for (TActorIterator<AActor> It(World, CTMClass); It; ++It)
			{
				if (!IsValid(*It))
				{
					continue;
				}

				if (FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(CTMClass, TEXT("VehicleRefs")))
				{
					FObjectProperty* InnerObjectProp = CastField<FObjectProperty>(ArrayProp->Inner);
					if (InnerObjectProp)
					{
						FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(*It));
						if (ArrayHelper.Num() > 0)
						{
							UObject* CVRef = InnerObjectProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(0));
							if (IsValid(CVRef))
							{
								++ValidCount;
							}
						}
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
		// Normalize pilot-condition provenance first so ConditionID/RunID reflect it.
		if (RunConfig.UncertaintyLevel.IsEmpty())
		{
			RunConfig.UncertaintyLevel = TEXT("Unspecified");
		}
		if (RunConfig.ProjectionSupport.IsEmpty())
		{
			RunConfig.ProjectionSupport = TEXT("Unspecified");
		}
		if (RunConfig.GitCommitOrBuildLabel.IsEmpty())
		{
			RunConfig.GitCommitOrBuildLabel = ResolveGitCommitLabel();
		}
		{
			const bool bFactorSpecified = !RunConfig.UncertaintyLevel.Equals(TEXT("Unspecified"), ESearchCase::IgnoreCase)
				|| !RunConfig.ProjectionSupport.Equals(TEXT("Unspecified"), ESearchCase::IgnoreCase);
			if (RunConfig.ConditionID.IsEmpty() || RunConfig.ConditionID == TEXT("UNSET") || bFactorSpecified)
			{
				const FString DerivedConditionID = FString::Printf(TEXT("U%s_P%s"), *FactorCode(RunConfig.UncertaintyLevel), *FactorCode(RunConfig.ProjectionSupport));
				if (RunConfig.ConditionID != DerivedConditionID)
				{
					if (!RunConfig.ConditionID.IsEmpty() && RunConfig.ConditionID != TEXT("UNSET"))
					{
						RunConfig.Notes += FString::Printf(TEXT("%sMenuConditionSummary=%s"), RunConfig.Notes.IsEmpty() ? TEXT("") : TEXT(" | "), *RunConfig.ConditionID);
					}
					RunConfig.ConditionID = DerivedConditionID;
					RunConfig.RunID.Reset(); // regenerate so the run folder carries the 2x2 cell
				}
			}
		}
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
		RunConfig.ProjectionSupport = TEXT("Unspecified");
		RunConfig.GitCommitOrBuildLabel = ResolveGitCommitLabel();
		RunConfig.MapName = RawMapName;
		RunConfig.CreatedAt = FDateTime::Now();
		RunConfig.RunValidity = TEXT("DevelopmentOnly");
		RunConfig.Notes = TEXT("Fallback run: scenario map launched directly without going through the setup menu (no pending run config was found).");
		RunConfig.RunID = Session->GenerateRunID(RunConfig.ScenarioID, RunConfig.ConditionID, RunConfig.ParticipantID);

		EventLog->BeginRunFromConfig(RunConfig);
		UE_LOG(LogTemp, Error, TEXT("ESCROWScenarioDirector: INVALID RUN - DEVELOPMENT ONLY. Launched without Experiment Setup; data is NOT valid participant data (RunValidity=DevelopmentOnly)."));
		if (UWorld* BannerWorld = GetWorld())
		{
			TWeakObjectPtr<UESCROWScenarioDirectorSubsystem> WeakThis(this);
			BannerWorld->GetTimerManager().SetTimer(InvalidRunBannerTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
			{
				if (WeakThis.IsValid() && GEngine)
				{
					GEngine->AddOnScreenDebugMessage(static_cast<uint64>(0xE5C40001), 2.0f, FColor::Red, TEXT("INVALID RUN - DEVELOPMENT ONLY: launched without Experiment Setup. Data is NOT valid participant data."));
				}
			}), 1.0f, true, 0.0f);
		}
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

// =============================== Pilot phase flow (A-D) ===============================
// Flat pilot map only. Zones are level actors carrying one of these tags (so lookup works
// in packaged builds, where actor labels do not exist).

namespace
{
	const FName ZoneTag_RallyA(TEXT("ESCROW_Zone_RallyA"));
	const FName ZoneTag_RallyB(TEXT("ESCROW_Zone_RallyB"));
	constexpr float PilotPhaseTickSeconds = 0.25f;
	// The disruption is time-based, not zone-based: free-driving participants cannot be
	// relied on to cross a particular patch of ground, so it fires this long after the
	// command vehicle reaches Rally B.
	constexpr double PilotDisruptionDelayAfterRallyBSeconds = 15.0;
}

AActor* UESCROWScenarioDirectorSubsystem::FindZoneActorByTag(const FName& Tag) const
{
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsValid(*It) && It->ActorHasTag(Tag))
			{
				return *It;
			}
		}
	}
	return nullptr;
}

bool UESCROWScenarioDirectorSubsystem::IsLocationInZone(const AActor* Zone, const FVector& Location) const
{
	if (!IsValid(Zone))
	{
		return false;
	}
	const FBox Box = Zone->GetComponentsBoundingBox(true);
	return Box.IsValid && Location.X >= Box.Min.X && Location.X <= Box.Max.X && Location.Y >= Box.Min.Y && Location.Y <= Box.Max.Y;
}

AActor* UESCROWScenarioDirectorSubsystem::FindCommandVehicleForPhaseFlow() const
{
	if (PilotCommandVehicle.IsValid())
	{
		return PilotCommandVehicle.Get();
	}
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetClass()->GetName().Contains(TEXT("CommandVehicle")))
			{
				return *It;
			}
		}
	}
	return nullptr;
}

void UESCROWScenarioDirectorSubsystem::StartPilotPhaseFlow()
{
	UWorld* World = GetWorld();
	if (!World || bPilotPhaseFlowActive || !World->GetMapName().Contains(TEXT("FLAT_PILOT")))
	{
		return;
	}

	// Without the Rally B zone there is no deterministic disruption; stay on the legacy path.
	if (!FindZoneActorByTag(ZoneTag_RallyB))
	{
		UE_LOG(LogTemp, Warning, TEXT("ESCROWScenarioDirector: pilot phase flow not started; Rally B zone tag missing on this map."));
		LogScenarioEvent(TEXT("PilotPhaseFlowSkipped"), 0, TEXT("Zone tag missing (ESCROW_Zone_RallyB)."));
		return;
	}

	StartGenericScenario();
	SetScenarioPhase(EESCROWScenarioPhase::PhaseA_RoutineMonitoring, TEXT("ScenarioRuntimeReady"));
	bPilotPhaseFlowActive = true;
	bPilotRallyAArrivalLogged = false;
	bPilotTakeoverSeenOnAffected = false;
	PilotDisruptionOnsetWorldSeconds = -1.0;
	PilotWrongFollowersLogged.Reset();
	bPilotFaultHeld = false;
	bPilotFaultResolved = false;
	bPilotRecoveryReached = false;
	PilotLastFaultReassertSeconds = -1.0;
	PilotClearRecoveryPoint();
	PilotClearPlanMarkers();
	bPilotPlanRepairPosted = false;
	bPilotCloseoutPosted = false;
	PilotPlanRepairOnsetSeconds = -1.0;
	PilotCloseoutOnsetSeconds = -1.0;
	PilotValidateAttempt = 0;
	bPilotValidatePassed = false;
	bPilotFirstAssignmentLogged = false;
	PilotLastLoggedAssignment.Reset();
	PilotValidateReportID.Reset();
	PilotAuthorizeReportID.Reset();
	PilotCloseoutReportID.Reset();
	for (int32 i = 0; i < PilotSlotCount; ++i) { PilotSlotBlocked[i] = false; PilotSlotLocations[i] = FVector::ZeroVector; }
	World->GetTimerManager().SetTimer(PilotPhaseTimerHandle, this, &UESCROWScenarioDirectorSubsystem::PilotPhaseTick, PilotPhaseTickSeconds, true);
}

void UESCROWScenarioDirectorSubsystem::PilotPhaseTick()
{
	UWorld* World = GetWorld();
	UESCROWFleetSubsystem* Fleet = GetFleet();
	if (!World || !Fleet || !bPilotPhaseFlowActive)
	{
		return;
	}

	AActor* CV = FindCommandVehicleForPhaseFlow();
	if (!CV)
	{
		return;
	}
	PilotCommandVehicle = CV;
	const FVector CVLocation = CV->GetActorLocation();
	UESCROWEventLogSubsystem* EventLog = GetEventLog();

	switch (CurrentPhase)
	{
	case EESCROWScenarioPhase::PhaseA_RoutineMonitoring:
		if (!bPilotRallyAArrivalLogged && IsLocationInZone(FindZoneActorByTag(ZoneTag_RallyA), CVLocation))
		{
			bPilotRallyAArrivalLogged = true;
			LogScenarioEvent(TEXT("RallyAReached"), 0, TEXT("Command vehicle entered Rally A zone."));
			if (EventLog) { EventLog->RecordScenarioEventSimple(GetCurrentPhaseName(), TEXT("RallyAReached"), TEXT("Command vehicle entered Rally A zone."), FString(), 0, CurrentPlanValidity, FString()); }
		}
		if (IsLocationInZone(FindZoneActorByTag(ZoneTag_RallyB), CVLocation))
		{
			SetScenarioPhase(EESCROWScenarioPhase::PhaseB_RallyBreakout, TEXT("CommandVehicleEnteredRallyB"));
			// Until the disruption fires, PilotDisruptionOnsetWorldSeconds holds the time Rally B
			// was reached (it is overwritten with the real onset time below).
			PilotDisruptionOnsetWorldSeconds = World->GetTimeSeconds();
			LogScenarioEvent(TEXT("RallyBReached"), 0, TEXT("Command vehicle entered Rally B zone."));
			if (EventLog) { EventLog->RecordScenarioEventSimple(GetCurrentPhaseName(), TEXT("RallyBReached"), TEXT("Command vehicle entered Rally B zone."), FString(), 0, CurrentPlanValidity, FString()); }
		}
		break;

	case EESCROWScenarioPhase::PhaseB_RallyBreakout:
		if (PilotDisruptionOnsetWorldSeconds >= 0.0 && (World->GetTimeSeconds() - PilotDisruptionOnsetWorldSeconds) >= PilotDisruptionDelayAfterRallyBSeconds)
		{
			PilotDisruptionOnsetWorldSeconds = World->GetTimeSeconds();
			SetScenarioPhase(EESCROWScenarioPhase::PhaseC_DisruptionDetection, TEXT("RallyBReachedPlusDelay"));
			SetAffectedFollowerID(PilotAffectedFollowerID, TEXT("DisruptionOnset"));
			const FString OnsetEventID = FString::Printf(TEXT("DISRUPTION_ONSET_F%d"), PilotAffectedFollowerID);
			PostTasking(TEXT("TASK_C_DISRUPTION"), TEXT("Detect disruption and take over affected follower"), EESCROWScenarioPhase::PhaseC_DisruptionDetection,
				TEXT("TakeOverAffectedFollower"), PilotAffectedFollowerID, TEXT("Valid"), OnsetEventID);
			if (EventLog)
			{
				EventLog->RecordScenarioEventSimple(GetCurrentPhaseName(), TEXT("DisruptionOnset"),
					FString::Printf(TEXT("Disruption onset: F%d requires takeover."), PilotAffectedFollowerID), ActiveTasking.TaskingID, PilotAffectedFollowerID, CurrentPlanValidity,
					FString::Printf(TEXT("{\"AffectedFollowerID\":%d,\"OnsetWorldSeconds\":%.3f}"), PilotAffectedFollowerID, PilotDisruptionOnsetWorldSeconds));
			}
			Fleet->RequestTakeover(PilotAffectedFollowerID);
			Fleet->PostNotification(EESCROWNotificationKind::TakeoverRequest, EESCROWMessageSeverity::Warning, PilotAffectedFollowerID,
				FString::Printf(TEXT("TAKEOVER REQUESTED: %s"), *ESCROWFollowerCallsign(PilotAffectedFollowerID)),
				FString::Printf(TEXT("%s has stopped and cannot continue. Take control and drive it to the recovery point."), *ESCROWFollowerCallsign(PilotAffectedFollowerID)),
				FString::Printf(TEXT("F%d"), PilotAffectedFollowerID));
			Fleet->AddMissionReport(FString::Printf(TEXT("RPT-0003-DISRUPTION-F%d"), PilotAffectedFollowerID), TEXT("Convoy Alert"), TEXT("ScenarioDirector"),
				FString::Printf(TEXT("%s has reported a fault and has stopped. Take control of %s, drive it to the marked recovery point, then release control to return it to autonomy."), *ESCROWFollowerCallsign(PilotAffectedFollowerID), *ESCROWFollowerCallsign(PilotAffectedFollowerID)), true);

			// Make the fault physical: hold the affected follower and mark a recovery point ahead of it.
			PilotPriorOrderType = PilotReadConvoyIntArray(TEXT("PathModes"), PilotAffectedFollowerID, 5);
			PilotPriorTargetVehicleID = PilotReadConvoyIntArray(TEXT("ReferenceVehicleIDs"), PilotAffectedFollowerID, -1);
			PilotSetFollowerOrder(PilotAffectedFollowerID, 5, -1, TEXT("DisruptionFault"));
			bPilotFaultHeld = true;
			bPilotFaultResolved = false;
			bPilotRecoveryReached = false;
			PilotLastFaultReassertSeconds = World->GetTimeSeconds();
			const AActor* FaultFollower = PilotGetFollowerActor(PilotAffectedFollowerID);
			const FVector FaultFrom = FaultFollower ? FaultFollower->GetActorLocation() : CVLocation;
			PilotRecoveryLocation = PilotComputeRecoveryLocation(FaultFrom);
			PilotSpawnRecoveryPoint(PilotRecoveryLocation);
			if (EventLog)
			{
				EventLog->RecordScenarioEventSimple(GetCurrentPhaseName(), TEXT("FaultHoldApplied"),
					FString::Printf(TEXT("F%d held (fault)."), PilotAffectedFollowerID), ActiveTasking.TaskingID, PilotAffectedFollowerID, CurrentPlanValidity,
					FString::Printf(TEXT("{\"FollowerLocation\":\"%s\"}"), *FaultFrom.ToCompactString()));
				EventLog->RecordScenarioEventSimple(GetCurrentPhaseName(), TEXT("RecoveryPointSpawned"),
					TEXT("Recovery point marked ahead of the affected follower."), ActiveTasking.TaskingID, PilotAffectedFollowerID, CurrentPlanValidity,
					FString::Printf(TEXT("{\"X\":%.0f,\"Y\":%.0f,\"DistanceFromFollowerCm\":%.0f}"), PilotRecoveryLocation.X, PilotRecoveryLocation.Y, FVector::Dist2D(FaultFrom, PilotRecoveryLocation)));
			}
		}
		break;

	case EESCROWScenarioPhase::PhaseC_DisruptionDetection:
		PilotReassertFault(Fleet, World);
		if (Fleet->bTakeoverActive)
		{
			const int32 TakenOver = Fleet->ActiveTakeoverFollowerID;
			if (TakenOver == PilotAffectedFollowerID)
			{
				bPilotTakeoverSeenOnAffected = true;
				const double Latency = World->GetTimeSeconds() - PilotDisruptionOnsetWorldSeconds;
				if (EventLog)
				{
					EventLog->RecordScenarioEventSimple(GetCurrentPhaseName(), TEXT("TakeoverOnAffectedFollower"),
						FString::Printf(TEXT("Operator took over affected follower F%d."), TakenOver), ActiveTasking.TaskingID, TakenOver, CurrentPlanValidity,
						FString::Printf(TEXT("{\"TakeoverLatencySeconds\":%.3f}"), Latency));
				}
				AcknowledgeActiveTasking(TEXT("OperatorTakeover"));
				SetScenarioPhase(EESCROWScenarioPhase::PhaseD_TakeoverIntervention, TEXT("TakeoverOnAffectedFollower"));
			}
			else if (!PilotWrongFollowersLogged.Contains(TakenOver))
			{
				PilotWrongFollowersLogged.Add(TakenOver);
				if (EventLog)
				{
					EventLog->RecordScenarioEventSimple(GetCurrentPhaseName(), TEXT("WrongFollowerTakeover"),
						FString::Printf(TEXT("Operator took over F%d; affected follower is F%d."), TakenOver, PilotAffectedFollowerID), ActiveTasking.TaskingID, TakenOver, CurrentPlanValidity,
						FString::Printf(TEXT("{\"TakenOverFollowerID\":%d,\"AffectedFollowerID\":%d}"), TakenOver, PilotAffectedFollowerID));
				}
			}
		}
		break;

	case EESCROWScenarioPhase::PhaseD_TakeoverIntervention:
		PilotTickFault(Fleet, EventLog, World);
		break;

	case EESCROWScenarioPhase::PhaseE_PlanRepair:
		PilotTickPlanRepair(Fleet, EventLog, World, CVLocation);
		break;

	case EESCROWScenarioPhase::PhaseF_Closeout:
		PilotTickCloseout(Fleet, EventLog, World);
		break;

	default:
		break;
	}
}

// =============================== Pilot fault (hold + recovery point) ===============================

AActor* UESCROWScenarioDirectorSubsystem::PilotFindConvoyManager()
{
	if (PilotConvoyManager.IsValid())
	{
		return PilotConvoyManager.Get();
	}
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetClass()->GetName().Contains(TEXT("ConvoyTelemetryManager")))
			{
				PilotConvoyManager = *It;
				return *It;
			}
		}
	}
	return nullptr;
}

AActor* UESCROWScenarioDirectorSubsystem::PilotGetFollowerActor(int32 FollowerID)
{
	AActor* CTM = PilotFindConvoyManager();
	if (!CTM)
	{
		return nullptr;
	}
	const FObjectProperty* Prop = CastField<FObjectProperty>(CTM->GetClass()->FindPropertyByName(*FString::Printf(TEXT("F%dRef"), FollowerID)));
	return Prop ? Cast<AActor>(Prop->GetObjectPropertyValue_InContainer(CTM)) : nullptr;
}

bool UESCROWScenarioDirectorSubsystem::PilotSetFollowerOrder(int32 FollowerID, int32 OrderType, int32 TargetVehicleID, const FString& Reason)
{
	AActor* CTM = PilotFindConvoyManager();
	UFunction* Fn = CTM ? CTM->FindFunction(TEXT("Escrow_ApplyNavOrder")) : nullptr;
	if (!Fn)
	{
		UE_LOG(LogTemp, Warning, TEXT("ESCROWScenarioDirector: Escrow_ApplyNavOrder not found on ConvoyTelemetryManager."));
		return false;
	}
	FStructOnScope Params(Fn);
	uint8* Mem = Params.GetStructMemory();
	auto SetInt = [&](const TCHAR* Name, int32 Value)
	{
		if (const FIntProperty* P = CastField<FIntProperty>(Fn->FindPropertyByName(Name))) { P->SetPropertyValue_InContainer(Mem, Value); }
	};
	SetInt(TEXT("FollowerID"), FollowerID);
	SetInt(TEXT("OrderType"), OrderType);
	SetInt(TEXT("TargetVehicleID"), TargetVehicleID);
	SetInt(TEXT("BreadcrumbIndex"), -1);
	if (const FStrProperty* P = CastField<FStrProperty>(Fn->FindPropertyByName(TEXT("Reason")))) { P->SetPropertyValue_InContainer(Mem, Reason); }
	CTM->ProcessEvent(Fn, Mem);
	return true;
}

bool UESCROWScenarioDirectorSubsystem::PilotSetFollowerHold(int32 FollowerID, bool bHold)
{
	AActor* CTM = PilotFindConvoyManager();
	UFunction* Fn = CTM ? CTM->FindFunction(TEXT("Escrow_SetFollowerHold")) : nullptr;
	if (!Fn)
	{
		return false;
	}
	FStructOnScope Params(Fn);
	uint8* Mem = Params.GetStructMemory();
	if (const FIntProperty* P = CastField<FIntProperty>(Fn->FindPropertyByName(TEXT("FollowerID")))) { P->SetPropertyValue_InContainer(Mem, FollowerID); }
	if (const FBoolProperty* P = CastField<FBoolProperty>(Fn->FindPropertyByName(TEXT("bHold")))) { P->SetPropertyValue_InContainer(Mem, bHold); }
	CTM->ProcessEvent(Fn, Mem);
	return true;
}

FVector UESCROWScenarioDirectorSubsystem::PilotComputeRecoveryLocation(const FVector& From) const
{
	// Walk 300 m along the route (start -> Rally B -> Rally C) from the follower's position.
	constexpr double WalkDistanceCm = 30000.0;
	const AActor* BZone = FindZoneActorByTag(ZoneTag_RallyB);
	const AActor* CMarker = FindZoneActorByTag(FName(TEXT("ESCROW_RallyMarker_C")));
	FVector Pos(From.X, From.Y, 120.0);
	double Remain = WalkDistanceCm;
	if (BZone)
	{
		const FVector Bp(BZone->GetActorLocation().X, BZone->GetActorLocation().Y, 120.0);
		const FVector ToB = Bp - Pos;
		if (ToB.Y > 0.0)
		{
			const double DistToB = ToB.Size2D();
			if (DistToB >= Remain)
			{
				return Pos + ToB.GetSafeNormal2D() * Remain;
			}
			Pos = Bp;
			Remain -= DistToB;
		}
	}
	if (CMarker)
	{
		const FVector Cp(CMarker->GetActorLocation().X, CMarker->GetActorLocation().Y, 120.0);
		const FVector ToC = Cp - Pos;
		const double Usable = FMath::Max(0.0, ToC.Size2D() - 15000.0);
		return Pos + ToC.GetSafeNormal2D() * FMath::Min(Remain, Usable);
	}
	return Pos + FVector(0.0, Remain, 0.0);
}

void UESCROWScenarioDirectorSubsystem::PilotSpawnRecoveryPoint(const FVector& Location)
{
	PilotClearRecoveryPoint();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Marker = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FRotator::ZeroRotator, Location), Params);
	if (!Marker)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Marker, TEXT("RecoveryRoot"));
	Marker->SetRootComponent(Root);
	Root->SetWorldLocation(Location);
	Root->RegisterComponent();

	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(Marker, TEXT("RecoveryLabel"));
	Label->SetupAttachment(Root);
	Label->SetText(FText::FromString(TEXT("RECOVERY POINT")));
	Label->SetTextRenderColor(FColor(255, 190, 0));
	Label->SetWorldSize(3000.0f);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 3000.0));
	Label->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
	Label->bNeverDistanceCull = true;
	Label->RegisterComponent();

	// Same tag family the GPS rally markers use, so the GPS shows it.
	Marker->Tags.Add(FName(TEXT("ESCROW_RallyMarker_R")));
	PilotRecoveryActor = Marker;
}

void UESCROWScenarioDirectorSubsystem::PilotClearRecoveryPoint()
{
	if (AActor* Marker = PilotRecoveryActor.Get())
	{
		Marker->Destroy();
	}
	PilotRecoveryActor.Reset();
}

void UESCROWScenarioDirectorSubsystem::PilotReassertFault(UESCROWFleetSubsystem* Fleet, UWorld* World)
{
	if (!bPilotFaultHeld || !Fleet || Fleet->bTakeoverActive)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (Now - PilotLastFaultReassertSeconds < 1.0)
	{
		return;
	}
	PilotLastFaultReassertSeconds = Now;
	FESCROWFollowerStatus Status;
	if (Fleet->GetFollowerStatus(PilotAffectedFollowerID, Status) && Status.CurrentState != EESCROWFollowerState::HoldingPosition)
	{
		// The operator (or the convoy logic) tried to move the faulted follower; hold it again.
		PilotSetFollowerOrder(PilotAffectedFollowerID, 5, -1, TEXT("FaultReasserted"));
		LogScenarioEvent(TEXT("FaultReasserted"), PilotAffectedFollowerID, TEXT("Faulted follower order overridden; hold re-applied."));
	}
}

void UESCROWScenarioDirectorSubsystem::PilotTickFault(UESCROWFleetSubsystem* Fleet, UESCROWEventLogSubsystem* EventLog, UWorld* World)
{
	const int32 ID = PilotAffectedFollowerID;
	const double Now = World->GetTimeSeconds();
	AActor* Follower = PilotGetFollowerActor(ID);
	const bool bUnderTakeover = Fleet->bTakeoverActive && Fleet->ActiveTakeoverFollowerID == ID;

	auto LogEv = [&](const TCHAR* Type, const FString& Text, const FString& Payload)
	{
		LogScenarioEvent(Type, ID, Text);
		if (EventLog)
		{
			EventLog->RecordScenarioEventSimple(GetCurrentPhaseName(), Type, Text, ActiveTasking.TaskingID, ID, CurrentPlanValidity, Payload);
		}
	};

	if (bUnderTakeover)
	{
		if (bPilotFaultHeld)
		{
			// Lift the hold so the operator can actually drive the follower.
			PilotSetFollowerHold(ID, false);
			bPilotFaultHeld = false;
			LogEv(TEXT("FaultHoldReleasedForTakeover"), FString::Printf(TEXT("F%d hold lifted for operator control."), ID), FString());
		}
		if (!bPilotRecoveryReached && Follower && FVector::Dist2D(Follower->GetActorLocation(), PilotRecoveryLocation) <= 2500.0)
		{
			bPilotRecoveryReached = true;
			LogEv(TEXT("RecoveryPointReached"), FString::Printf(TEXT("F%d reached the recovery point."), ID),
				FString::Printf(TEXT("{\"SecondsSinceOnset\":%.2f}"), Now - PilotDisruptionOnsetWorldSeconds));
			Fleet->PostNotification(EESCROWNotificationKind::Info, EESCROWMessageSeverity::Info, ID, TEXT("RECOVERY POINT REACHED"),
				FString::Printf(TEXT("%s is at the recovery point. Release control to return it to autonomy."), *ESCROWFollowerCallsign(ID)), FString::Printf(TEXT("F%d"), ID));
		}
		return;
	}

	if (bPilotFaultResolved)
	{
		return;
	}

	if (bPilotRecoveryReached)
	{
		bPilotFaultResolved = true;
		Fleet->SetRequiresTakeover(ID, false);
		// Put the follower back to what it was doing before the fault. Orders that need a target
		// location (move-to-waypoint types) cannot be rebuilt here, so those fall back to hold.
		const bool bRestorable = (PilotPriorOrderType == 1 || PilotPriorOrderType == 5);
		const int32 RestoreOrder = bRestorable ? PilotPriorOrderType : 5;
		PilotSetFollowerOrder(ID, RestoreOrder, RestoreOrder == 1 ? PilotPriorTargetVehicleID : -1, TEXT("FaultResolved"));
		PilotClearRecoveryPoint();
		LogEv(TEXT("FaultResolved"), FString::Printf(TEXT("F%d fault resolved; returned to convoy."), ID),
			FString::Printf(TEXT("{\"SecondsSinceOnset\":%.2f}"), Now - PilotDisruptionOnsetWorldSeconds));
		CompleteActiveTasking(TEXT("FaultResolved"));
		SetScenarioPhase(EESCROWScenarioPhase::PhaseE_PlanRepair, TEXT("FaultResolved"));
		return;
	}

	if (!bPilotFaultHeld)
	{
		// Control was released before the recovery point: stop it again.
		PilotSetFollowerOrder(ID, 5, -1, TEXT("ReleasedBeforeRecovery"));
		bPilotFaultHeld = true;
		PilotLastFaultReassertSeconds = Now;
		LogEv(TEXT("ReleasedBeforeRecovery"), FString::Printf(TEXT("F%d released before the recovery point; held again."), ID), FString());
		// The release path says "returned to autonomy" and clears the takeover flag; the fault is
		// still present, so raise the request again (new HUD directive + flag back on).
		Fleet->RequestTakeover(ID);
		Fleet->PostNotification(EESCROWNotificationKind::Warning, EESCROWMessageSeverity::Warning, ID,
			FString::Printf(TEXT("%s STILL FAULTED"), *ESCROWFollowerCallsign(ID)),
			FString::Printf(TEXT("Control was released before the recovery point. Retake control and drive %s there."), *ESCROWFollowerCallsign(ID)), FString::Printf(TEXT("F%d"), ID));
	}
}

int32 UESCROWScenarioDirectorSubsystem::PilotReadConvoyIntArray(const TCHAR* ArrayName, int32 Index, int32 Default)
{
	AActor* CTM = PilotFindConvoyManager();
	if (!CTM)
	{
		return Default;
	}
	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(CTM->GetClass()->FindPropertyByName(ArrayName));
	const FIntProperty* Inner = ArrayProp ? CastField<FIntProperty>(ArrayProp->Inner) : nullptr;
	if (!ArrayProp || !Inner)
	{
		return Default;
	}
	FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(CTM));
	return Helper.IsValidIndex(Index) ? Inner->GetPropertyValue(Helper.GetRawPtr(Index)) : Default;
}

// =============================== Pilot Phase E (plan repair) / Phase F (closeout) ===============================

namespace
{
	const FName RallyMarkerTagC(TEXT("ESCROW_RallyMarker_C"));
	constexpr double PlanTriggerRadiusCm = 25000.0;     // command vehicle within 250 m of Rally C
	constexpr double PlanAltMatchRadiusCm = 2200.0;     // a waypoint this close to an ALT position counts as assigned there
	constexpr double PlanBlockedClearRadiusCm = 3000.0; // ... and must stay this far from every blocked slot
}

bool UESCROWScenarioDirectorSubsystem::PilotReadConvoyVectorArray(const TCHAR* ArrayName, int32 Index, FVector& Out)
{
	AActor* CTM = PilotFindConvoyManager();
	if (!CTM)
	{
		return false;
	}
	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(CTM->GetClass()->FindPropertyByName(ArrayName));
	const FStructProperty* Inner = ArrayProp ? CastField<FStructProperty>(ArrayProp->Inner) : nullptr;
	if (!ArrayProp || !Inner || Inner->Struct != TBaseStructure<FVector>::Get())
	{
		return false;
	}
	FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(CTM));
	if (!Helper.IsValidIndex(Index))
	{
		return false;
	}
	Out = *reinterpret_cast<const FVector*>(Helper.GetRawPtr(Index));
	return true;
}

bool UESCROWScenarioDirectorSubsystem::PilotReportAcknowledged(UESCROWFleetSubsystem* Fleet, const FString& ReportID) const
{
	if (!Fleet || ReportID.IsEmpty())
	{
		return false;
	}
	for (const FESCROWMissionReport& Report : Fleet->MissionReports)
	{
		if (Report.ReportID == ReportID)
		{
			return Report.bAcknowledged;
		}
	}
	return false;
}

double UESCROWScenarioDirectorSubsystem::PilotFindGroundZ(const FVector& Location, double Fallback) const
{
	if (const UWorld* World = GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(PilotGroundZ), false);
		if (World->LineTraceSingleByChannel(Hit, Location + FVector(0.0, 0.0, 20000.0), Location - FVector(0.0, 0.0, 20000.0), ECC_Visibility, Query))
		{
			return Hit.ImpactPoint.Z;
		}
	}
	return Fallback;
}

AActor* UESCROWScenarioDirectorSubsystem::PilotSpawnPlanMarker(const FVector& Location, const FString& Label, const FColor& Color, bool bBlocker, const FName& Tag, double LabelYaw)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Marker = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FRotator::ZeroRotator, Location), Params);
	if (!Marker)
	{
		return nullptr;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Marker, TEXT("PlanRoot"));
	Marker->SetRootComponent(Root);
	Root->SetWorldLocation(Location);
	Root->RegisterComponent();

	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Marker, TEXT("PlanMesh"));
		Mesh->SetStaticMesh(Cube);
		Mesh->SetupAttachment(Root);
		// A blocked slot is a physical barrier the followers cannot drive through; an open slot or
		// alternate is a thin flat pad.
		Mesh->SetRelativeScale3D(bBlocker ? FVector(10.0, 10.0, 4.0) : FVector(7.0, 7.0, 0.05));
		Mesh->SetRelativeLocation(FVector(0.0, 0.0, bBlocker ? 200.0 : 8.0));
		if (bBlocker)
		{
			Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		}
		else
		{
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Mesh->RegisterComponent();
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Mesh))
			{
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(Color));
				Mesh->SetMaterial(0, MID);
			}
		}
	}

	UTextRenderComponent* Text = NewObject<UTextRenderComponent>(Marker, TEXT("PlanLabel"));
	Text->SetupAttachment(Root);
	Text->SetText(FText::FromString(Label));
	Text->SetTextRenderColor(Color);
	Text->SetWorldSize(450.0f);
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetVerticalAlignment(EVRTA_TextCenter);
	Text->SetRelativeLocation(FVector(0.0, 0.0, bBlocker ? 750.0 : 400.0));
	Text->SetRelativeRotation(FRotator(0.0, LabelYaw, 0.0));
	Text->bNeverDistanceCull = true;
	Text->RegisterComponent();

	if (!Tag.IsNone())
	{
		Marker->Tags.Add(Tag);
	}
	PilotPlanMarkers.Add(Marker);
	return Marker;
}

void UESCROWScenarioDirectorSubsystem::PilotClearPlanMarkers()
{
	for (TWeakObjectPtr<AActor>& Marker : PilotPlanMarkers)
	{
		if (AActor* Actor = Marker.Get())
		{
			Actor->Destroy();
		}
	}
	PilotPlanMarkers.Reset();
}

void UESCROWScenarioDirectorSubsystem::PilotPostValidateReport(UESCROWFleetSubsystem* Fleet, bool bNotify)
{
	++PilotValidateAttempt;
	PilotValidateReportID = FString::Printf(TEXT("RPT-0005-PLAN-VALIDATE-%d"), PilotValidateAttempt);
	Fleet->AddMissionReport(PilotValidateReportID, TEXT("Validate Plan"), TEXT("ScenarioDirector"),
		TEXT("Confirm this report once WOLF and LYNX each have a waypoint on a different ALT position. The plan is then checked and you are told if anything is still wrong."), true, bNotify);
}

void UESCROWScenarioDirectorSubsystem::PilotPostPlanRepair(UESCROWFleetSubsystem* Fleet, UESCROWEventLogSubsystem* EventLog, UWorld* World, const FVector& CVLocation)
{
	bPilotPlanRepairPosted = true;
	PilotPlanRepairOnsetSeconds = World->GetTimeSeconds();
	bPilotValidatePassed = false;
	bPilotFirstAssignmentLogged = false;
	PilotLastLoggedAssignment.Reset();

	const AActor* CMarker = FindZoneActorByTag(RallyMarkerTagC);
	const AActor* BZone = FindZoneActorByTag(ZoneTag_RallyB);
	const FVector C = CMarker ? CMarker->GetActorLocation() : CVLocation;
	const FVector B = BZone ? BZone->GetActorLocation() : C - FVector(0.0, 30000.0, 0.0);
	FVector Dir(C.X - B.X, C.Y - B.Y, 0.0);
	Dir = Dir.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = FVector(0.0, 1.0, 0.0);
	}
	// Facing Dir, right is (-Dir.Y, Dir.X): with Dir = +Y the right-hand side is -X.
	const FVector Right(-Dir.Y, Dir.X, 0.0);
	const double LabelYaw = Dir.Rotation().Yaw - 180.0;
	const double GroundZ = PilotFindGroundZ(FVector(C.X, C.Y, CVLocation.Z), CVLocation.Z - 100.0);

	// 60 m between slots so the labels and barriers read as separate positions.
	const double SlotOffsetsCm[PilotSlotCount] = { -9000.0, -3000.0, 3000.0, 9000.0 };
	int32 BlockedFollowers[2] = { 2, 3 };
	for (int32 i = 0; i < PilotSlotCount; ++i)
	{
		FVector Loc = C + Right * SlotOffsetsCm[i];
		Loc.Z = GroundZ;
		PilotSlotLocations[i] = Loc;
		PilotSlotBlocked[i] = (i + 1 == BlockedFollowers[0] || i + 1 == BlockedFollowers[1]);
	}
	for (int32 k = 0; k < PilotAltCount; ++k)
	{
		FVector Loc = C + Dir * 14000.0 + Right * (k == 0 ? -5000.0 : 5000.0);
		Loc.Z = GroundZ;
		PilotAltLocations[k] = Loc;
	}

	int32 BlockedSeen = 0;
	for (int32 i = 0; i < PilotSlotCount; ++i)
	{
		const FString Name = ESCROWFollowerCallsign(i + 1);
		if (PilotSlotBlocked[i])
		{
			++BlockedSeen;
			PilotSpawnPlanMarker(PilotSlotLocations[i], FString::Printf(TEXT("P%d %s\nBLOCKED"), i + 1, *Name), FColor(235, 45, 45), true,
				FName(*FString::Printf(TEXT("ESCROW_RallyMarker_X%d"), BlockedSeen)), LabelYaw);
		}
		else
		{
			PilotSpawnPlanMarker(PilotSlotLocations[i], FString::Printf(TEXT("P%d %s"), i + 1, *Name), FColor(90, 225, 100), false, NAME_None, LabelYaw);
		}
	}
	for (int32 k = 0; k < PilotAltCount; ++k)
	{
		PilotSpawnPlanMarker(PilotAltLocations[k], FString::Printf(TEXT("ALT %d"), k + 1), FColor(70, 205, 255), false,
			FName(*FString::Printf(TEXT("ESCROW_RallyMarker_ALT%d"), k + 1)), LabelYaw);
	}

	PostTasking(TEXT("TASK_E_PLAN_REPAIR"), TEXT("Reassign blocked Rally C positions and validate the plan"), EESCROWScenarioPhase::PhaseE_PlanRepair,
		TEXT("ReassignBlockedPositionsAndValidate"), BlockedFollowers[0], TEXT("Invalid"), TEXT("PLAN_REPAIR_ONSET"));

	const FString BlockedNames = FString::Printf(TEXT("%s and %s"), *ESCROWFollowerCallsign(BlockedFollowers[0]), *ESCROWFollowerCallsign(BlockedFollowers[1]));
	LogScenarioEvent(TEXT("PlanRepairOnset"), BlockedFollowers[0], FString::Printf(TEXT("Rally C positions blocked for %s."), *BlockedNames));
	if (EventLog)
	{
		EventLog->RecordScenarioEventSimple(GetCurrentPhaseName(), TEXT("PlanRepairOnset"),
			FString::Printf(TEXT("Rally C positions P%d and P%d blocked."), BlockedFollowers[0], BlockedFollowers[1]), ActiveTasking.TaskingID, BlockedFollowers[0], CurrentPlanValidity,
			FString::Printf(TEXT("{\"BlockedSlots\":[%d,%d],\"Slots\":[\"%s\",\"%s\",\"%s\",\"%s\"],\"Alts\":[\"%s\",\"%s\"]}"), BlockedFollowers[0], BlockedFollowers[1],
				*PilotSlotLocations[0].ToCompactString(), *PilotSlotLocations[1].ToCompactString(), *PilotSlotLocations[2].ToCompactString(), *PilotSlotLocations[3].ToCompactString(),
				*PilotAltLocations[0].ToCompactString(), *PilotAltLocations[1].ToCompactString()));
	}

	Fleet->PostNotification(EESCROWNotificationKind::Warning, EESCROWMessageSeverity::Warning, BlockedFollowers[0], TEXT("RALLY C PLAN BLOCKED"),
		FString::Printf(TEXT("The planned positions for %s are blocked. Reassign them to the ALT positions, then confirm the Validate Plan report."), *BlockedNames), TEXT("REPORTS"));
	Fleet->AddMissionReport(TEXT("RPT-0004-PLAN-REPAIR"), TEXT("Rally C Plan Repair"), TEXT("ScenarioDirector"),
		FString::Printf(TEXT("The convoy has reached Rally C. The plan was for %s to take positions P1, P2, P3 and P4 in a line. Positions P2 and P3 are blocked (red), so %s cannot use them.\n\nAssign %s a new position each: use the two cyan ALT positions beyond Rally C. On the GPS page, select the follower, place a waypoint on its ALT position and confirm it.\n\nWhen both are reassigned, confirm the Validate Plan report."),
			*FString::Printf(TEXT("%s, %s, %s and %s"), *ESCROWFollowerCallsign(1), *ESCROWFollowerCallsign(2), *ESCROWFollowerCallsign(3), *ESCROWFollowerCallsign(4)), *BlockedNames, *BlockedNames), false, false);
	PilotPostValidateReport(Fleet, false);
}

bool UESCROWScenarioDirectorSubsystem::PilotEvaluatePlan(FString& OutDetail)
{
	bool bAllOk = true;
	TArray<int32> AltUsed;
	OutDetail.Reset();
	for (int32 f = 1; f <= PilotSlotCount; ++f)
	{
		if (!PilotSlotBlocked[f - 1])
		{
			continue;
		}
		const FString Name = ESCROWFollowerCallsign(f);
		const int32 Mode = PilotReadConvoyIntArray(TEXT("PathModes"), f, -1);
		FVector Target = FVector::ZeroVector;
		FString Problem;
		if (Mode != 4 || !PilotReadConvoyVectorArray(TEXT("ActiveTargetLocations"), f, Target))
		{
			Problem = TEXT("no new waypoint assigned");
		}
		else
		{
			int32 Alt = -1;
			double AltDist = TNumericLimits<double>::Max();
			for (int32 k = 0; k < PilotAltCount; ++k)
			{
				const double D = FVector::Dist2D(Target, PilotAltLocations[k]);
				if (D < AltDist) { AltDist = D; Alt = k; }
			}
			bool bNearBlocked = false;
			for (int32 s = 0; s < PilotSlotCount; ++s)
			{
				if (PilotSlotBlocked[s] && FVector::Dist2D(Target, PilotSlotLocations[s]) < PlanBlockedClearRadiusCm)
				{
					bNearBlocked = true;
				}
			}
			if (bNearBlocked)
			{
				Problem = TEXT("waypoint is still on a blocked position");
			}
			else if (Alt < 0 || AltDist > PlanAltMatchRadiusCm)
			{
				Problem = TEXT("waypoint is not on an ALT position");
			}
			else if (AltUsed.Contains(Alt))
			{
				Problem = FString::Printf(TEXT("ALT %d is already used by another follower"), Alt + 1);
			}
			else
			{
				AltUsed.Add(Alt);
			}
		}
		if (!Problem.IsEmpty())
		{
			bAllOk = false;
			OutDetail += (OutDetail.IsEmpty() ? TEXT("") : TEXT("; ")) + FString::Printf(TEXT("%s: %s"), *Name, *Problem);
		}
	}
	if (bAllOk)
	{
		OutDetail = TEXT("All blocked positions reassigned to distinct ALT positions.");
	}
	return bAllOk;
}

void UESCROWScenarioDirectorSubsystem::PilotTickPlanRepair(UESCROWFleetSubsystem* Fleet, UESCROWEventLogSubsystem* EventLog, UWorld* World, const FVector& CVLocation)
{
	if (!bPilotPlanRepairPosted)
	{
		const AActor* CMarker = FindZoneActorByTag(RallyMarkerTagC);
		if (CMarker && FVector::Dist2D(CMarker->GetActorLocation(), CVLocation) <= PlanTriggerRadiusCm)
		{
			PilotPostPlanRepair(Fleet, EventLog, World, CVLocation);
		}
		return;
	}

	const double Now = World->GetTimeSeconds();
	auto LogEv = [&](const TCHAR* Type, int32 FollowerID, const FString& Text, const FString& Payload)
	{
		LogScenarioEvent(Type, FollowerID, Text);
		if (EventLog)
		{
			EventLog->RecordScenarioEventSimple(GetCurrentPhaseName(), Type, Text, ActiveTasking.TaskingID, FollowerID, CurrentPlanValidity, Payload);
		}
	};
	auto Clean = [](FString S) { S.ReplaceInline(TEXT("\""), TEXT("'")); return S; };

	// Record every change to a blocked follower's assignment (what the operator did, and when).
	for (int32 f = 1; f <= PilotSlotCount; ++f)
	{
		if (!PilotSlotBlocked[f - 1])
		{
			continue;
		}
		const int32 Mode = PilotReadConvoyIntArray(TEXT("PathModes"), f, -1);
		FVector Target = FVector::ZeroVector;
		const bool bHasTarget = (Mode == 4) && PilotReadConvoyVectorArray(TEXT("ActiveTargetLocations"), f, Target);
		const FString Key = bHasTarget ? FString::Printf(TEXT("%d:%d:%d"), Mode, FMath::RoundToInt(Target.X / 100.0), FMath::RoundToInt(Target.Y / 100.0)) : FString::Printf(TEXT("%d"), Mode);
		const FString* Last = PilotLastLoggedAssignment.Find(f);
		if (Last && *Last == Key)
		{
			continue;
		}
		const bool bFirstSight = (Last == nullptr);
		PilotLastLoggedAssignment.Add(f, Key);
		if (bFirstSight || !bHasTarget)
		{
			continue;
		}
		int32 Alt = -1;
		double AltDist = TNumericLimits<double>::Max();
		for (int32 k = 0; k < PilotAltCount; ++k)
		{
			const double D = FVector::Dist2D(Target, PilotAltLocations[k]);
			if (D < AltDist) { AltDist = D; Alt = k; }
		}
		LogEv(TEXT("PlanRepairAssignmentChanged"), f, FString::Printf(TEXT("%s given a waypoint."), *ESCROWFollowerCallsign(f)),
			FString::Printf(TEXT("{\"Target\":\"%s\",\"NearestAlt\":%d,\"DistanceToNearestAltCm\":%.0f,\"SecondsSinceOnset\":%.2f}"), *Target.ToCompactString(), Alt + 1, AltDist, Now - PilotPlanRepairOnsetSeconds));
		if (!bPilotFirstAssignmentLogged)
		{
			bPilotFirstAssignmentLogged = true;
			AcknowledgeActiveTasking(TEXT("OperatorAssignedWaypoint"));
			LogEv(TEXT("PlanRepairFirstAssignment"), f, TEXT("First reassignment."), FString::Printf(TEXT("{\"LatencySeconds\":%.2f}"), Now - PilotPlanRepairOnsetSeconds));
		}
	}

	if (!bPilotValidatePassed)
	{
		if (PilotReportAcknowledged(Fleet, PilotValidateReportID))
		{
			FString Detail;
			const bool bOk = PilotEvaluatePlan(Detail);
			LogEv(TEXT("PlanValidationResult"), 0, bOk ? TEXT("Plan validation passed.") : TEXT("Plan validation failed."),
				FString::Printf(TEXT("{\"Attempt\":%d,\"Valid\":%s,\"Detail\":\"%s\",\"SecondsSinceOnset\":%.2f}"), PilotValidateAttempt, bOk ? TEXT("true") : TEXT("false"), *Clean(Detail), Now - PilotPlanRepairOnsetSeconds));
			if (bOk)
			{
				bPilotValidatePassed = true;
				SetPlanValidity(TEXT("Valid"), TEXT("PilotTruthValid"));
				ValidateActiveTasking(TEXT("PilotValidateReport"));
				PilotAuthorizeReportID = TEXT("RPT-0006-PLAN-AUTHORIZE");
				Fleet->AddMissionReport(PilotAuthorizeReportID, TEXT("Authorize Plan"), TEXT("ScenarioDirector"),
					TEXT("The checked plan is valid. Confirm this report to authorize it and send the convoy to its final positions."), true, false);
				Fleet->PostNotification(EESCROWNotificationKind::Info, EESCROWMessageSeverity::Info, 0, TEXT("PLAN VALID"),
					TEXT("Confirm the Authorize Plan report to execute it."), TEXT("REPORTS"));
			}
			else
			{
				Fleet->PostNotification(EESCROWNotificationKind::Warning, EESCROWMessageSeverity::Warning, 0, TEXT("PLAN NOT VALID"), Detail, TEXT("GPS"));
				PilotPostValidateReport(Fleet, false);
			}
		}
		return;
	}

	if (PilotReportAcknowledged(Fleet, PilotAuthorizeReportID))
	{
		FString Detail;
		const bool bOk = PilotEvaluatePlan(Detail);
		if (bOk)
		{
			AuthorizeActiveTasking(TEXT("PilotAuthorizeReport"));
			LogEv(TEXT("PlanAuthorized"), 0, TEXT("Plan authorized."), FString::Printf(TEXT("{\"SecondsSinceOnset\":%.2f,\"ValidateAttempts\":%d}"), Now - PilotPlanRepairOnsetSeconds, PilotValidateAttempt));
			Fleet->PostNotification(EESCROWNotificationKind::Info, EESCROWMessageSeverity::Info, 0, TEXT("PLAN AUTHORIZED"), TEXT("The convoy is taking its final positions."), TEXT("GPS"));
			CompleteActiveTasking(TEXT("PlanRepairAuthorized"));
		}
		else
		{
			// Changed after validation: the plan is no longer valid, so it must be checked again.
			bPilotValidatePassed = false;
			SetPlanValidity(TEXT("Invalid"), TEXT("PilotTruthInvalid"));
			LogEv(TEXT("PlanAuthorizedWhileInvalid"), 0, TEXT("Authorize confirmed but plan no longer valid."), FString::Printf(TEXT("{\"Detail\":\"%s\"}"), *Clean(Detail)));
			PilotAuthorizeReportID.Reset();
			Fleet->PostNotification(EESCROWNotificationKind::Warning, EESCROWMessageSeverity::Warning, 0, TEXT("PLAN NOT VALID"), Detail, TEXT("GPS"));
			PilotPostValidateReport(Fleet, false);
		}
	}
}

void UESCROWScenarioDirectorSubsystem::PilotTickCloseout(UESCROWFleetSubsystem* Fleet, UESCROWEventLogSubsystem* EventLog, UWorld* World)
{
	const double Now = World->GetTimeSeconds();
	if (!bPilotCloseoutPosted)
	{
		bPilotCloseoutPosted = true;
		PilotCloseoutOnsetSeconds = Now;
		const int32 Residual = Fleet->GetActiveNotificationCount();
		PostTasking(TEXT("TASK_F_CLOSEOUT"), TEXT("Confirm readiness and close out"), EESCROWScenarioPhase::PhaseF_Closeout, TEXT("ConfirmCloseout"), 0, TEXT("Valid"), TEXT("CLOSEOUT_ONSET"));
		PilotCloseoutReportID = TEXT("RPT-0007-CLOSEOUT");
		Fleet->AddMissionReport(PilotCloseoutReportID, TEXT("Closeout"), TEXT("ScenarioDirector"),
			TEXT("The plan is authorized and the convoy is in its final positions. Check the Fleet Status report and clear any remaining alerts (press Y). Then confirm this report to close out the run."), true, true);
		LogScenarioEvent(TEXT("CloseoutOnset"), 0, TEXT("Closeout posted."));
		if (EventLog)
		{
			EventLog->RecordScenarioEventSimple(GetCurrentPhaseName(), TEXT("CloseoutOnset"), TEXT("Closeout report posted."), ActiveTasking.TaskingID, 0, CurrentPlanValidity,
				FString::Printf(TEXT("{\"ActiveAlerts\":%d}"), Residual));
		}
		return;
	}

	if (PilotReportAcknowledged(Fleet, PilotCloseoutReportID))
	{
		const int32 Residual = Fleet->GetActiveNotificationCount();
		LogScenarioEvent(TEXT("CloseoutConfirmed"), 0, FString::Printf(TEXT("Closeout confirmed with %d active alerts."), Residual));
		if (EventLog)
		{
			EventLog->RecordScenarioEventSimple(GetCurrentPhaseName(), TEXT("CloseoutConfirmed"), TEXT("Closeout confirmed."), ActiveTasking.TaskingID, 0, CurrentPlanValidity,
				FString::Printf(TEXT("{\"ResidualWarnings\":%d,\"SecondsSinceOnset\":%.2f}"), Residual, Now - PilotCloseoutOnsetSeconds));
		}
		bPilotPhaseFlowActive = false;
		World->GetTimerManager().ClearTimer(PilotPhaseTimerHandle);
		CompleteActiveTasking(TEXT("CloseoutConfirmed"));
	}
}
