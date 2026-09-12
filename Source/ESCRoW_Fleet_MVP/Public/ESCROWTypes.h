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
};
