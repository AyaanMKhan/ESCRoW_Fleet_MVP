// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ESCROWTypes.generated.h"

UENUM(BlueprintType)
enum class EESCROWMissionCondition : uint8
{
	MissionA_PassengerMonitor,
	MissionB_PassengerTakeover,
	MissionC_DriverMonitor,
	MissionD_DriverTakeover
};

// Metadata-only complexity axis for the study. Not currently wired to any
// gameplay/scenario behavior; recorded alongside session metadata and events.
UENUM(BlueprintType)
enum class EESCROWComplexityCondition : uint8
{
	LowComplexity,
	HighComplexity
};

// RA36A-1: experimental display-locus factor. Metadata-only for this pass; does
// not yet change any runtime HUD/console behavior. HUDPrimary is the default,
// matching current build behavior.
UENUM(BlueprintType)
enum class EESCROWDisplayMode : uint8
{
	HUDPrimary,
	ConsolePrimary
};

// RA36A-1: experimental vehicle-control-role factor. Metadata-only for this
// pass; does not yet change any runtime possession/driving behavior. Driving
// is the default, matching current build behavior.
UENUM(BlueprintType)
enum class EESCROWVehicleControlRole : uint8
{
	Driving,
	NotDriving
};

UENUM(BlueprintType)
enum class EESCROWPlayerControlMode : uint8
{
	PassengerSupervisor,
	DriverSupervisor,
	CompactHUD,
	FocusedCommandHUD,
	FullMap,
	RemoteTakeover,
	ReturnToAutonomyTransition
};

UENUM(BlueprintType)
enum class EESCROWFollowerState : uint8
{
	Idle,
	FollowingConvoy,
	TaskAssigned,
	MovingToWaypoint,
	MovingToPosition,
	PositionAchieved,
	ExecutingAction,
	ReturningToConvoy,
	Rejoining,
	ReportReady,
	IssueDetected,
	AwaitingTakeover,
	DirectControl,
	ReturningToAutonomy,
	Recovered,
	// Appended last so existing serialized enum values stay stable.
	HoldingPosition
};

UENUM(BlueprintType)
enum class EESCROWAutonomyMode : uint8
{
	Autonomous,
	Supervised,
	AwaitingOperatorInput,
	AwaitingTakeover,
	DirectControl,
	ReturningToAutonomy,
	Degraded
};

UENUM(BlueprintType)
enum class EESCROWCommandType : uint8
{
	Message,
	MoveToWaypoint,
	ExecuteActionAtWaypoint,
	HoldPosition,
	ResumeAutonomy,
	RequestStatus,
	Takeover,
	ReturnToAutonomy
};

UENUM(BlueprintType)
enum class EESCROWActionType : uint8
{
	SendStatusMessage,
	SimulatedScan,
	SimulatedSecureArea,
	SimulatedObserve,
	SimulatedRelay
};

UENUM(BlueprintType)
enum class EESCROWMessageSource : uint8
{
	Commander,
	LeadVehicle,
	Follower,
	System
};

UENUM(BlueprintType)
enum class EESCROWMessageSeverity : uint8
{
	Info,
	Warning,
	Critical
};

// Operator-facing alert toast stack (top-right). Owned by UESCROWFleetSubsystem.
UENUM(BlueprintType)
enum class EESCROWNotificationKind : uint8
{
	Warning,
	TakeoverRequest,
	Report,
	Info
};

UENUM(BlueprintType)
enum class EESCROWNotificationState : uint8
{
	Active,
	Acknowledged,
	Expired
};

USTRUCT(BlueprintType)
struct FESCROWNotification
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW")
	int32 NotificationID = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW")
	EESCROWNotificationKind Kind = EESCROWNotificationKind::Info;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW")
	EESCROWMessageSeverity Severity = EESCROWMessageSeverity::Info;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW")
	EESCROWNotificationState State = EESCROWNotificationState::Active;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW")
	FString Body;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW")
	int32 FollowerID = 0;

	// -1 when not tied to a mission report.
	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW")
	int32 ReportIndex = -1;

	// HUD page name that counts as "checking" this alert (e.g. "F2", "REPORTS").
	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW")
	FString RelatedPage;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW")
	float CreatedWorldSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct FESCROWFollowerStatus
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	int32 FollowerID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString Callsign;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	EESCROWFollowerState CurrentState = EESCROWFollowerState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	EESCROWAutonomyMode AutonomyMode = EESCROWAutonomyMode::Autonomous;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString CurrentObjective;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	float ProgressPct = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	float ETASeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString CommsStatus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString HealthStatus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	bool bRequiresTakeover = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	bool bCompletionReportReady = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString LastMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	float LastStateChangeTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	AActor* ActorRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	UObject* MapMarkerRef = nullptr;

	// 0 = command vehicle, 1-4 = the FollowerID this follower currently follows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	int32 CurrentFollowTargetID = 0;

	// Cached actor reference for CurrentFollowTargetID, when known. May be null if the
	// target has not registered yet; CurrentFollowTargetID remains authoritative.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	AActor* FollowTargetActorRef = nullptr;
};

USTRUCT(BlueprintType)
struct FESCROWFleetCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString CommandID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	EESCROWCommandType CommandType = EESCROWCommandType::Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	int32 TargetFollowerID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	AActor* TargetWaypointActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	EESCROWActionType ActionType = EESCROWActionType::SendStatusMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString CommandText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	float TimeIssued = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	bool bAcknowledged = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	bool bCompleted = false;
};

USTRUCT(BlueprintType)
struct FESCROWWaypointTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString TaskID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	int32 TargetFollowerID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	AActor* WaypointActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString ObjectiveName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	EESCROWActionType ActionType = EESCROWActionType::SendStatusMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString ActionMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	float ProgressPct = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	bool bWaypointReached = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	bool bActionComplete = false;
};

USTRUCT(BlueprintType)
struct FESCROWHUDMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString MessageID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	EESCROWMessageSource Source = EESCROWMessageSource::System;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	EESCROWMessageSeverity Severity = EESCROWMessageSeverity::Info;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	int32 TargetFollowerID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	float MissionTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString MessageText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	bool bRequiresAcknowledgement = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	bool bAcknowledged = false;
};

UENUM(BlueprintType)
enum class EESCROWReportType : uint8
{
	Text,
	FleetTelemetry
};

// Minimal operator-facing mission report (RA42/Reports deck). Owned and
// generated by runtime/scenario logic (see UESCROWFleetSubsystem::
// GenerateInitialMissionReports, called from
// UESCROWScenarioDirectorSubsystem::MarkScenarioRuntimeReady), never by the
// HUD widget - the HUD only reads and renders these.
USTRUCT(BlueprintType)
struct FESCROWMissionReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString ReportID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	EESCROWReportType ReportType = EESCROWReportType::Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString Source;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString BodyText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	float ReceivedMissionTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	bool bRequiresAck = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	bool bAcknowledged = false;
};

USTRUCT(BlueprintType)
struct FESCROWEventLogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString RealTimestamp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	float MissionTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString ParticipantID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString SessionID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	EESCROWMissionCondition MissionCondition = EESCROWMissionCondition::MissionA_PassengerMonitor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	EESCROWPlayerControlMode PlayerMode = EESCROWPlayerControlMode::PassengerSupervisor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString EventType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	int32 FollowerID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString CommandID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString MessageID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString EventText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	int32 EventIndex = 0;

	// Seconds since UESCROWSessionSubsystem::StartSession was called (not world time).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	float TimestampSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString ResearcherID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString MissionID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString ConditionSummary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString FollowerCallsign;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString AdditionalData;
};

UENUM(BlueprintType)
enum class EESCROWScenarioPlaybackMode : uint8
{
	Manual,
	Timed,
	Hybrid
};

UENUM(BlueprintType)
enum class EESCROWScenarioRunState : uint8
{
	NotLoaded,
	Ready,
	Running,
	Paused,
	WaitingForMilestone,
	Complete
};

// Internal scenario-runtime startup lifecycle: distinct from EESCROWScenarioRunState
// (Mission-D event-playback machinery). This tracks whether the world/services a
// scenario needs (CTM/telemetry, fleet refs, EventLog, session/run metadata) are
// actually present and validated at startup - not participant-facing.
UENUM(BlueprintType)
enum class EESCROWScenarioRuntimeState : uint8
{
	NotInitialized,
	WorldDetected,
	ServicesChecking,
	Ready,
	Failed,
	Stopped
};

// RA42A: generic (non-Mission-D) scenario phase for the pilot spine (Phase A-F,
// see CLAUDE.md). Distinct from EESCROWScenarioRunState (playback machinery) and
// from the free-text CurrentScenarioStep used by the Mission-D schedule.
UENUM(BlueprintType)
enum class EESCROWScenarioPhase : uint8
{
	None,
	Preflight,
	PhaseA_RoutineMonitoring,
	PhaseB_RallyBreakout,
	PhaseC_DisruptionDetection,
	PhaseD_TakeoverIntervention,
	PhaseE_PlanRepair,
	PhaseF_Closeout,
	Complete,
	Aborted
};

// RA42A: lifecycle state of a single generic participant tasking.
UENUM(BlueprintType)
enum class EESCROWTaskingState : uint8
{
	None,
	Posted,
	Acknowledged,
	Validated,
	Authorized,
	Completed,
	Cancelled,
	Failed
};

// RA42A: a single generic scenario tasking, owned by UESCROWScenarioDirectorSubsystem.
// Distinct from FESCROWScenarioEvent (the Mission-D scripted-event schedule) -
// this is the new phase-A-F tasking lifecycle skeleton.
USTRUCT(BlueprintType)
struct FESCROWTasking
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Tasking")
	FString TaskingID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Tasking")
	FString TaskingLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Tasking")
	EESCROWScenarioPhase Phase = EESCROWScenarioPhase::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Tasking")
	EESCROWTaskingState State = EESCROWTaskingState::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Tasking")
	FString RequiredParticipantAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Tasking")
	int32 AffectedFollowerID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Tasking")
	FString PlanValidity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Tasking")
	FString EventID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Tasking")
	FString CompletionReason;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Tasking")
	FDateTime PostedAt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Tasking")
	FDateTime AcknowledgedAt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Tasking")
	FDateTime ValidatedAt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Tasking")
	FDateTime AuthorizedAt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Tasking")
	FDateTime CompletedAt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Tasking")
	bool bIsActive = false;
};

// A single scripted step in a scenario's event schedule. ExecuteEventByIndex
// interprets these fields to drive UESCROWFleetSubsystem state; the optional
// bHas*Change flags let an event leave a field untouched instead of forcing
// a value, since physical follower progress may not exactly track the script.
USTRUCT(BlueprintType)
struct FESCROWScenarioEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	int32 EventIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	FString EventID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	FString EventLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	float ScheduledTimeSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	FString EventType;

	// 0 = not follower-specific (broadcast/system), 1-4 = a specific follower.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	int32 FollowerID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	FString MessageText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	bool bRequiresOperatorMilestone = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	FString ExpectedMilestoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	bool bHasObjectiveChange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	FString NewObjectiveText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	bool bHasStateChange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	EESCROWFollowerState NewFollowerState = EESCROWFollowerState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	bool bHasReportReadyChange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	bool NewReportReadyValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	bool bHasTakeoverChange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Scenario")
	bool NewRequiresTakeoverValue = false;
};

// Snapshot of all session/study metadata, returned by
// UESCROWSessionSubsystem::GetSessionMetadata for display/export purposes.
USTRUCT(BlueprintType)
struct FESCROWSessionMetadata
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString ResearcherID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString ParticipantID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString SessionID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString StudyID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString MissionID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString MissionDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString MapName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	EESCROWComplexityCondition ComplexityCondition = EESCROWComplexityCondition::LowComplexity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	EESCROWMissionCondition MissionCondition = EESCROWMissionCondition::MissionD_DriverTakeover;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString ConditionSummary;

	// RA36A-1: experimental display-locus / vehicle-control-role factors, metadata only.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	EESCROWDisplayMode DisplayMode = EESCROWDisplayMode::HUDPrimary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	EESCROWVehicleControlRole VehicleControlRole = EESCROWVehicleControlRole::Driving;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString StartTimestampLocal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString StartTimestampUTC;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString EndTimestamp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString BuildVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	FString Notes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	bool bSessionInitialized = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW")
	bool bSessionActive = false;
};

// RA41A: formal per-run configuration, written by the experiment setup menu as
// a pending config and promoted to current on level load. Distinct from
// FESCROWSessionMetadata (which is the session subsystem's own live field
// snapshot) - this is a self-contained, exportable record of exactly what a
// single data-collection run was configured with.
USTRUCT(BlueprintType)
struct FESCROWRunConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString RunID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString SessionID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString ParticipantID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString ResearcherID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString ScenarioID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString ScenarioVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString ConditionID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString SupportMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString DisplayMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString VehicleControlRole;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString WorkloadLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString ScenarioVariant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString EventSalience;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString UncertaintyLevel;

	// Pilot 2x2 factor B: "Low" / "High" / "Unspecified".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString ProjectionSupport;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	int32 RunSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString MapName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString AppVersionOrBuild;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString GitCommitOrBuildLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FDateTime CreatedAt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString Notes;

	// RA41B: "Valid" for a real menu-derived run, "DevelopmentOnly" for the
	// editor/PIE direct-scenario-map fallback. Empty/unset for older configs;
	// treated as "Valid" by the promotion path in that case.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|RunConfig")
	FString RunValidity;
};

// --- RA41A structured data-manager record types ---
// One row per record type, appended to its own file under the run folder and
// mirrored as a raw JSONL line in event_log_raw.jsonl. Every record carries
// RunID + Timestamp + WorldSeconds so rows are self-describing even if a file
// is examined in isolation from session_manifest.json.

USTRUCT(BlueprintType)
struct FESCROWScenarioEventRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RecordID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RunID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FDateTime Timestamp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	float WorldSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString Phase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString EventType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString EventLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString TaskingID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	int32 AffectedFollowerID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString PlanValidity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString PayloadJSON;

	// RA41B: standardized common fields (see ESCROWEventLogSubsystem RA41B section).
	// -1 = not available; consumers should fall back to WorldSeconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	float ScenarioTimeSeconds = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString SourceSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString SourceObject;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString EventID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RelatedEventID;
};

USTRUCT(BlueprintType)
struct FESCROWParticipantActionRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RecordID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RunID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FDateTime Timestamp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	float WorldSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString ActionType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString Source;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString ActivePage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	int32 SelectedFollowerID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	int32 TargetFollowerID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString TaskingID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString EventID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString PayloadJSON;

	// RA41B: standardized common fields. Source (above) serves as SourceSystem.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	float ScenarioTimeSeconds = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString SourceObject;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString Phase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RelatedEventID;
};

USTRUCT(BlueprintType)
struct FESCROWUIExposureRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RecordID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RunID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FDateTime Timestamp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	float WorldSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString PageName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString WidgetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	int32 SelectedFollowerID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString VisibleFields;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RelatedEventID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	bool bDiagnosticEvidenceAvailable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString PayloadJSON;

	// RA41B: standardized common fields.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	float ScenarioTimeSeconds = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString SourceSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString SourceObject;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString Phase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString TaskingID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString EventID;
};

USTRUCT(BlueprintType)
struct FESCROWHeartbeatRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RecordID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RunID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FDateTime Timestamp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	float WorldSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString Phase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString ActivePage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	int32 SelectedFollowerID = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString TaskingID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString TaskingState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString PlanValidity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString PayloadJSON;

	// RA41B: standardized common fields.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	float ScenarioTimeSeconds = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString SourceSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString SourceObject;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString EventID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RelatedEventID;

	// RA41B: "CV" plus one entry per follower (1-4), formatted "X,Y,Z" in cm,
	// or empty if the actor could not be resolved. See
	// UESCROWEventLogSubsystem::TryGetCommandVehicleLocation for how CV location
	// is derived (there is no direct command-vehicle actor reference on
	// UESCROWFleetSubsystem).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager|Heartbeat")
	FString CVLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager|Heartbeat")
	FString F1Location;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager|Heartbeat")
	FString F2Location;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager|Heartbeat")
	FString F3Location;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager|Heartbeat")
	FString F4Location;

	// Speed in cm/s, derived from AActor::GetVelocity().Size(). -1 = unavailable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager|Heartbeat")
	float F1SpeedCms = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager|Heartbeat")
	float F2SpeedCms = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager|Heartbeat")
	float F3SpeedCms = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager|Heartbeat")
	float F4SpeedCms = -1.0f;
};

USTRUCT(BlueprintType)
struct FESCROWStateTransitionRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RecordID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RunID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FDateTime Timestamp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	float WorldSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString EntityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString EntityType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString StateName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString OldValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString NewValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString Source;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RelatedEventID;

	// RA41B: standardized common fields. Source (above) serves as SourceSystem.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString PayloadJSON;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	float ScenarioTimeSeconds = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString SourceObject;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString Phase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString EventID;
};

USTRUCT(BlueprintType)
struct FESCROWReportRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RecordID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RunID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FDateTime Timestamp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	float WorldSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString ReportID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString ReportType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString FieldName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString FieldValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString GroundTruthValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	int32 Score = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString PayloadJSON;

	// RA41B: standardized common fields.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	float ScenarioTimeSeconds = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString SourceSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString SourceObject;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString Phase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString EventID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|DataManager")
	FString RelatedEventID;
};
