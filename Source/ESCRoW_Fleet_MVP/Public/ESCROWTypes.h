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
	Recovered
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
