// Fill out your copyright notice in the Description page of Project Settings.

#include "ESCROWFleetSubsystem.h"
#include "ESCROWEventLogSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

UESCROWEventLogSubsystem* UESCROWFleetSubsystem::GetEventLog() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UESCROWEventLogSubsystem>() : nullptr;
}

void UESCROWFleetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ResetFleetState();
}

void UESCROWFleetSubsystem::ResetFleetState()
{
	FollowerStatuses.Reset();
	Notifications.Reset();

	for (int32 FollowerID = 1; FollowerID <= 4; ++FollowerID)
	{
		FESCROWFollowerStatus Status;
		Status.FollowerID = FollowerID;
		Status.Callsign = ESCROWFollowerCallsign(FollowerID);
		Status.CurrentState = EESCROWFollowerState::FollowingConvoy;
		Status.AutonomyMode = EESCROWAutonomyMode::Autonomous;
		Status.HealthStatus = TEXT("Nominal");
		Status.CommsStatus = TEXT("Connected");
		Status.CurrentObjective = TEXT("Awaiting tasking");
		Status.ProgressPct = 0.0f;
		Status.ETASeconds = 0.0f;
		Status.bRequiresTakeover = false;
		Status.bCompletionReportReady = false;
		Status.CurrentFollowTargetID = FollowerID - 1; // F1->0 (command vehicle), F2->1, F3->2, F4->3
		Status.FollowTargetActorRef = nullptr;

		FollowerStatuses.Add(Status);
	}

	SelectedFollowerID = 1;
	bTakeoverActive = false;
	ActiveTakeoverFollowerID = -1;
	MessageCounter = 0;
	CommandCounter = 0;

	PendingCommands.Reset();
	CompletedCommands.Reset();
	HUDMessages.Reset();
}

void UESCROWFleetSubsystem::RegisterFollower(int32 FollowerID, const FString& InCallsign, AActor* ActorRef)
{
	// The follower vehicle Blueprints register themselves with their own placeholder name
	// ("F4"); the shared callsign table is the single source of truth for F1-F4.
	const bool bKnownFollower = FollowerID >= 1 && FollowerID <= 4;
	const FString Callsign = bKnownFollower ? ESCROWFollowerCallsign(FollowerID) : InCallsign;

	FESCROWFollowerStatus* Existing = FindFollower(FollowerID);
	if (Existing)
	{
		Existing->Callsign = Callsign;
		Existing->ActorRef = ActorRef;
		return;
	}

	FESCROWFollowerStatus NewStatus;
	NewStatus.FollowerID = FollowerID;
	NewStatus.Callsign = Callsign;
	NewStatus.ActorRef = ActorRef;
	NewStatus.CurrentState = EESCROWFollowerState::FollowingConvoy;
	NewStatus.AutonomyMode = EESCROWAutonomyMode::Autonomous;
	NewStatus.HealthStatus = TEXT("Nominal");
	NewStatus.CommsStatus = TEXT("Connected");
	NewStatus.CurrentObjective = TEXT("Awaiting tasking");

	FollowerStatuses.Add(NewStatus);
}

bool UESCROWFleetSubsystem::SelectFollower(int32 FollowerID)
{
	if (!FindFollower(FollowerID))
	{
		return false;
	}

	SelectedFollowerID = FollowerID;
	return true;
}

bool UESCROWFleetSubsystem::GetFollowerStatus(int32 FollowerID, FESCROWFollowerStatus& OutStatus) const
{
	const FESCROWFollowerStatus* Found = FindFollower(FollowerID);
	if (!Found)
	{
		return false;
	}

	OutStatus = *Found;
	return true;
}

TArray<FESCROWFollowerStatus> UESCROWFleetSubsystem::GetAllFollowerStatuses() const
{
	return FollowerStatuses;
}

bool UESCROWFleetSubsystem::GetSelectedFollowerStatus(FESCROWFollowerStatus& OutStatus) const
{
	return GetFollowerStatus(SelectedFollowerID, OutStatus);
}

bool UESCROWFleetSubsystem::SetFollowerState(int32 FollowerID, EESCROWFollowerState NewState)
{
	FESCROWFollowerStatus* Found = FindFollower(FollowerID);
	if (!Found)
	{
		return false;
	}

	const EESCROWFollowerState OldState = Found->CurrentState;
	Found->CurrentState = NewState;

	if (OldState != NewState)
	{
		if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
		{
			const UEnum* StateEnum = StaticEnum<EESCROWFollowerState>();
			const FString OldValue = StateEnum ? StateEnum->GetNameStringByValue(static_cast<int64>(OldState)) : FString();
			const FString NewValue = StateEnum ? StateEnum->GetNameStringByValue(static_cast<int64>(NewState)) : FString();
			EventLog->RecordStateTransitionSimple(FString::Printf(TEXT("F%d"), FollowerID), TEXT("Follower"), TEXT("CurrentState"),
				OldValue, NewValue, TEXT("FleetSubsystem"), FString());
		}
	}

	return true;
}

bool UESCROWFleetSubsystem::SetAutonomyMode(int32 FollowerID, EESCROWAutonomyMode NewMode)
{
	FESCROWFollowerStatus* Found = FindFollower(FollowerID);
	if (!Found)
	{
		return false;
	}

	Found->AutonomyMode = NewMode;
	return true;
}

bool UESCROWFleetSubsystem::SetFollowerObjective(int32 FollowerID, const FString& ObjectiveText)
{
	FESCROWFollowerStatus* Found = FindFollower(FollowerID);
	if (!Found)
	{
		return false;
	}

	Found->CurrentObjective = ObjectiveText;
	return true;
}

bool UESCROWFleetSubsystem::SetReportReady(int32 FollowerID, bool bReady)
{
	FESCROWFollowerStatus* Found = FindFollower(FollowerID);
	if (!Found)
	{
		return false;
	}

	Found->bCompletionReportReady = bReady;
	return true;
}

bool UESCROWFleetSubsystem::SetRequiresTakeover(int32 FollowerID, bool bRequiresTakeover)
{
	FESCROWFollowerStatus* Found = FindFollower(FollowerID);
	if (!Found)
	{
		return false;
	}

	const bool bOldValue = Found->bRequiresTakeover;
	Found->bRequiresTakeover = bRequiresTakeover;

	// The false transition is the "residual warning cleared" moment: bOldValue==true
	// means the flag was left on after whatever raised it (see RequestTakeover) had
	// already been resolved elsewhere, so this is a genuine, loggable closeout action
	// rather than a no-op re-affirmation of an already-clear flag.
	if (bOldValue != bRequiresTakeover)
	{
		if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
		{
			EventLog->RecordStateTransitionSimple(FString::Printf(TEXT("F%d"), FollowerID), TEXT("Follower"), TEXT("RequiresTakeover"),
				bOldValue ? TEXT("true") : TEXT("false"), bRequiresTakeover ? TEXT("true") : TEXT("false"), TEXT("FleetSubsystem"), FString());

			if (!bRequiresTakeover)
			{
				EventLog->RecordScenarioEventSimple(FString(), TEXT("AlertCleared"),
					FString::Printf(TEXT("%s residual alert cleared."), *Found->Callsign), FString(), FollowerID, TEXT("Unspecified"), FString());
			}
		}
	}

	return true;
}

bool UESCROWFleetSubsystem::SendLeadMessageToFollower(int32 FollowerID, const FString& MessageText)
{
	if (!FindFollower(FollowerID))
	{
		return false;
	}

	AddHUDMessage(EESCROWMessageSource::LeadVehicle, EESCROWMessageSeverity::Info, FollowerID, MessageText);
	return true;
}

bool UESCROWFleetSubsystem::ReceiveFollowerMessage(int32 FollowerID, const FString& MessageText, EESCROWMessageSeverity Severity)
{
	FESCROWFollowerStatus* Found = FindFollower(FollowerID);
	if (!Found)
	{
		return false;
	}

	Found->LastMessage = MessageText;
	AddHUDMessage(EESCROWMessageSource::Follower, Severity, FollowerID, MessageText);
	return true;
}

void UESCROWFleetSubsystem::ReceiveSystemMessage(const FString& MessageText, EESCROWMessageSeverity Severity)
{
	AddHUDMessage(EESCROWMessageSource::System, Severity, 0, MessageText);
}

FESCROWFleetCommand UESCROWFleetSubsystem::QueueFleetCommand(int32 TargetFollowerID, EESCROWCommandType CommandType, AActor* TargetWaypointActor, EESCROWActionType ActionType, const FString& CommandText)
{
	FESCROWFleetCommand NewCommand;
	NewCommand.CommandID = GenerateCommandID();
	NewCommand.CommandType = CommandType;
	NewCommand.TargetFollowerID = TargetFollowerID;
	NewCommand.TargetWaypointActor = TargetWaypointActor;
	NewCommand.ActionType = ActionType;
	NewCommand.CommandText = CommandText;
	NewCommand.TimeIssued = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	NewCommand.bAcknowledged = false;
	NewCommand.bCompleted = false;

	PendingCommands.Add(NewCommand);

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		const UEnum* CommandTypeEnum = StaticEnum<EESCROWCommandType>();
		const UEnum* ActionTypeEnum = StaticEnum<EESCROWActionType>();
		const FString CommandTypeStr = CommandTypeEnum ? CommandTypeEnum->GetNameStringByValue(static_cast<int64>(CommandType)) : FString();
		const FString ActionTypeStr = ActionTypeEnum ? ActionTypeEnum->GetNameStringByValue(static_cast<int64>(ActionType)) : FString();
		const FString PayloadJSON = FString::Printf(TEXT("{\"CommandType\":\"%s\",\"ActionType\":\"%s\"}"), *CommandTypeStr, *ActionTypeStr);
		EventLog->RecordScenarioEventSimple(FString(), TEXT("CommandQueued"), CommandText, NewCommand.CommandID, TargetFollowerID, TEXT("Unspecified"), PayloadJSON);
	}

	return NewCommand;
}

bool UESCROWFleetSubsystem::MarkCommandComplete(const FString& CommandID)
{
	const int32 FoundIndex = PendingCommands.IndexOfByPredicate([&CommandID](const FESCROWFleetCommand& Command)
	{
		return Command.CommandID == CommandID;
	});

	if (FoundIndex == INDEX_NONE)
	{
		return false;
	}

	FESCROWFleetCommand Completed = PendingCommands[FoundIndex];
	Completed.bCompleted = true;

	PendingCommands.RemoveAt(FoundIndex);
	CompletedCommands.Add(Completed);

	AddHUDMessage(EESCROWMessageSource::System, EESCROWMessageSeverity::Info, Completed.TargetFollowerID, FString::Printf(TEXT("Command %s complete."), *CommandID));

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(FString(), TEXT("CommandCompleted"), Completed.CommandText, Completed.CommandID, Completed.TargetFollowerID, TEXT("Unspecified"), FString());
	}

	return true;
}

bool UESCROWFleetSubsystem::CanTakeoverFollower(int32 FollowerID) const
{
	if (!FindFollower(FollowerID))
	{
		return false;
	}

	if (bTakeoverActive && ActiveTakeoverFollowerID != FollowerID)
	{
		return false;
	}

	return true;
}

bool UESCROWFleetSubsystem::RequestTakeover(int32 FollowerID)
{
	FESCROWFollowerStatus* Found = FindFollower(FollowerID);
	if (!Found)
	{
		return false;
	}

	Found->bRequiresTakeover = true;

	AddHUDMessage(EESCROWMessageSource::System, EESCROWMessageSeverity::Warning, FollowerID, FString::Printf(TEXT("Takeover requested for %s."), *Found->Callsign));

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(FString(), TEXT("TakeoverRequested"),
			FString::Printf(TEXT("Takeover requested for %s."), *Found->Callsign), FString(), FollowerID, TEXT("Unspecified"), FString());
	}

	return true;
}

bool UESCROWFleetSubsystem::AcquireTakeoverLock(int32 FollowerID)
{
	if (!CanTakeoverFollower(FollowerID))
	{
		return false;
	}

	FESCROWFollowerStatus* Found = FindFollower(FollowerID);
	if (!Found)
	{
		return false;
	}

	const UEnum* AutonomyEnum = StaticEnum<EESCROWAutonomyMode>();
	const FString OldAutonomyStr = AutonomyEnum ? AutonomyEnum->GetNameStringByValue(static_cast<int64>(Found->AutonomyMode)) : FString();

	bTakeoverActive = true;
	ActiveTakeoverFollowerID = FollowerID;
	Found->CurrentState = EESCROWFollowerState::DirectControl;
	Found->AutonomyMode = EESCROWAutonomyMode::DirectControl;

	AddHUDMessage(EESCROWMessageSource::System, EESCROWMessageSeverity::Warning, FollowerID, FString::Printf(TEXT("Takeover lock acquired for %s."), *Found->Callsign));

	TakeoverStartWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordScenarioEventSimple(FString(), TEXT("TakeoverAcquired"),
			FString::Printf(TEXT("Takeover lock acquired for %s (direct control)."), *Found->Callsign), FString(), FollowerID, TEXT("Unspecified"), FString());
		EventLog->RecordStateTransitionSimple(FString::Printf(TEXT("F%d"), FollowerID), TEXT("Follower"), TEXT("AutonomyMode"),
			OldAutonomyStr, TEXT("DirectControl"), TEXT("FleetSubsystem"), FString());
	}

	return true;
}

bool UESCROWFleetSubsystem::ReleaseTakeoverLock(int32 FollowerID)
{
	if (!bTakeoverActive || ActiveTakeoverFollowerID != FollowerID)
	{
		return false;
	}

	FESCROWFollowerStatus* Found = FindFollower(FollowerID);

	bTakeoverActive = false;
	ActiveTakeoverFollowerID = -1;

	if (Found)
	{
		Found->CurrentState = EESCROWFollowerState::ReturningToAutonomy;
		Found->AutonomyMode = EESCROWAutonomyMode::ReturningToAutonomy;

		AddHUDMessage(EESCROWMessageSource::System, EESCROWMessageSeverity::Info, FollowerID, FString::Printf(TEXT("Takeover lock released for %s."), *Found->Callsign));

		const double DurationSeconds = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) - TakeoverStartWorldSeconds;

		if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
		{
			EventLog->RecordScenarioEventSimple(FString(), TEXT("TakeoverReleased"),
				FString::Printf(TEXT("Takeover lock released for %s; returning to autonomy."), *Found->Callsign), FString(), FollowerID, TEXT("Unspecified"),
				FString::Printf(TEXT("{\"ControlDurationSeconds\":%.2f}"), DurationSeconds));
			EventLog->RecordStateTransitionSimple(FString::Printf(TEXT("F%d"), FollowerID), TEXT("Follower"), TEXT("AutonomyMode"),
				TEXT("DirectControl"), TEXT("ReturningToAutonomy"), TEXT("FleetSubsystem"), FString());
		}
	}

	return true;
}

void UESCROWFleetSubsystem::ClearTransientAlerts()
{
	TArray<int32> ClearedFollowerIDs;
	for (FESCROWFollowerStatus& Status : FollowerStatuses)
	{
		if (Status.bRequiresTakeover)
		{
			ClearedFollowerIDs.Add(Status.FollowerID);
		}
		Status.bRequiresTakeover = false;
	}

	AddHUDMessage(EESCROWMessageSource::System, EESCROWMessageSeverity::Info, 0, TEXT("Transient alerts cleared."));

	if (ClearedFollowerIDs.Num() > 0)
	{
		if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
		{
			TArray<FString> IDStrings;
			for (int32 ID : ClearedFollowerIDs)
			{
				IDStrings.Add(FString::Printf(TEXT("F%d"), ID));
			}
			EventLog->RecordScenarioEventSimple(FString(), TEXT("AllAlertsCleared"),
				FString::Printf(TEXT("All follower alerts cleared (%s)."), *FString::Join(IDStrings, TEXT(", "))), FString(), 0, TEXT("Unspecified"), FString());
		}
	}
}

bool UESCROWFleetSubsystem::CanSetFollowerFollowTarget(int32 FollowerID, int32 TargetID) const
{
	if (FollowerID < 1 || FollowerID > 4)
	{
		return false;
	}

	if (TargetID < 0 || TargetID > 4)
	{
		return false;
	}

	if (TargetID == FollowerID)
	{
		return false;
	}

	if (TargetID == 0)
	{
		return true;
	}

	// Walk the prospective chain from TargetID back toward the command vehicle. If we
	// reach FollowerID before reaching 0, assigning this target would create a cycle.
	int32 CurrentID = TargetID;
	int32 SafetyCounter = 0;
	while (CurrentID != 0 && SafetyCounter < 8)
	{
		if (CurrentID == FollowerID)
		{
			return false;
		}

		const FESCROWFollowerStatus* Current = FindFollower(CurrentID);
		if (!Current)
		{
			break;
		}

		CurrentID = Current->CurrentFollowTargetID;
		++SafetyCounter;
	}

	return true;
}

bool UESCROWFleetSubsystem::SetFollowerFollowTarget(int32 FollowerID, int32 TargetID)
{
	if (!CanSetFollowerFollowTarget(FollowerID, TargetID))
	{
		return false;
	}

	FESCROWFollowerStatus* Follower = FindFollower(FollowerID);
	if (!Follower)
	{
		return false;
	}

	Follower->CurrentFollowTargetID = TargetID;
	Follower->FollowTargetActorRef = nullptr;

	if (TargetID != 0)
	{
		const FESCROWFollowerStatus* Target = FindFollower(TargetID);
		if (Target)
		{
			Follower->FollowTargetActorRef = Target->ActorRef;
		}
	}

	return true;
}

int32 UESCROWFleetSubsystem::GetFollowerFollowTargetID(int32 FollowerID) const
{
	const FESCROWFollowerStatus* Follower = FindFollower(FollowerID);
	return Follower ? Follower->CurrentFollowTargetID : 0;
}

bool UESCROWFleetSubsystem::ResetDefaultFollowChain()
{
	bool bAllSucceeded = true;
	bAllSucceeded = SetFollowerFollowTarget(1, 0) && bAllSucceeded;
	bAllSucceeded = SetFollowerFollowTarget(2, 1) && bAllSucceeded;
	bAllSucceeded = SetFollowerFollowTarget(3, 2) && bAllSucceeded;
	bAllSucceeded = SetFollowerFollowTarget(4, 3) && bAllSucceeded;
	return bAllSucceeded;
}

FESCROWFollowerStatus* UESCROWFleetSubsystem::FindFollower(int32 FollowerID)
{
	return FollowerStatuses.FindByPredicate([FollowerID](const FESCROWFollowerStatus& Status)
	{
		return Status.FollowerID == FollowerID;
	});
}

const FESCROWFollowerStatus* UESCROWFleetSubsystem::FindFollower(int32 FollowerID) const
{
	return FollowerStatuses.FindByPredicate([FollowerID](const FESCROWFollowerStatus& Status)
	{
		return Status.FollowerID == FollowerID;
	});
}

FString UESCROWFleetSubsystem::GenerateMessageID()
{
	++MessageCounter;
	return FString::Printf(TEXT("MSG_%03d"), MessageCounter);
}

FString UESCROWFleetSubsystem::GenerateCommandID()
{
	++CommandCounter;
	return FString::Printf(TEXT("CMD_%03d"), CommandCounter);
}

void UESCROWFleetSubsystem::AddHUDMessage(EESCROWMessageSource Source, EESCROWMessageSeverity Severity, int32 TargetFollowerID, const FString& MessageText, bool bRequiresAcknowledgement)
{
	FESCROWHUDMessage NewMessage;
	NewMessage.MessageID = GenerateMessageID();
	NewMessage.Source = Source;
	NewMessage.Severity = Severity;
	NewMessage.TargetFollowerID = TargetFollowerID;
	NewMessage.MissionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	NewMessage.MessageText = MessageText;
	NewMessage.bRequiresAcknowledgement = bRequiresAcknowledgement;
	NewMessage.bAcknowledged = false;

	HUDMessages.Add(NewMessage);

	while (HUDMessages.Num() > 100)
	{
		HUDMessages.RemoveAt(0);
	}

	// HUDMessages above is the ephemeral in-session feed (capped, not persisted).
	// Mirror every message to the durable structured log too, so the operator
	// message feed is reconstructable after the fact even though the in-memory
	// array itself is not saved to disk.
	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		const UEnum* SourceEnum = StaticEnum<EESCROWMessageSource>();
		const UEnum* SeverityEnum = StaticEnum<EESCROWMessageSeverity>();
		const FString SourceStr = SourceEnum ? SourceEnum->GetNameStringByValue(static_cast<int64>(Source)) : FString();
		const FString SeverityStr = SeverityEnum ? SeverityEnum->GetNameStringByValue(static_cast<int64>(Severity)) : FString();
		const FString PayloadJSON = FString::Printf(TEXT("{\"Source\":\"%s\",\"Severity\":\"%s\",\"RequiresAck\":%s,\"MessageID\":\"%s\"}"),
			*SourceStr, *SeverityStr, bRequiresAcknowledgement ? TEXT("true") : TEXT("false"), *NewMessage.MessageID);
		EventLog->RecordScenarioEventSimple(FString(), TEXT("HUDMessage"), MessageText, FString(), TargetFollowerID, TEXT("Unspecified"), PayloadJSON);
	}

	if (Severity == EESCROWMessageSeverity::Critical)
	{
		PostNotification(EESCROWNotificationKind::Warning, EESCROWMessageSeverity::Critical, TargetFollowerID,
			TEXT("CRITICAL ALERT"), MessageText,
			TargetFollowerID > 0 ? FString::Printf(TEXT("F%d"), TargetFollowerID) : FString());
	}
}

void UESCROWFleetSubsystem::GenerateInitialMissionReports(const FString& ScenarioName, float MissionTime)
{
	if (MissionReports.Num() > 0)
	{
		// Already seeded - idempotent no-op.
		return;
	}

	FESCROWMissionReport Brief;
	Brief.ReportID = TEXT("RPT-0001-BRIEF");
	Brief.ReportType = EESCROWReportType::Text;
	Brief.Title = TEXT("Mission Brief");
	Brief.Source = TEXT("ScenarioDirector");
	// Written for the flat pilot scenario; ScenarioName is kept in the signature for callers.
	Brief.BodyText = TEXT("You are supervising a command vehicle and four autonomous followers (F1-F4) travelling north from the start: Rally A, then Rally B, then Rally C to the north-east. "
		"Monitor follower status, spacing and route progress on the HUD and GPS. If a follower reports a fault, take control of it, resolve the problem, and return it to autonomy. "
		"At Rally C, check the planned follower positions, repair any that are not feasible, and validate the plan before it is executed. "
		"Finish by confirming readiness, clearing outstanding warnings, and submitting the closeout report.");
	Brief.ReceivedMissionTime = MissionTime;
	Brief.bRequiresAck = false;
	Brief.bAcknowledged = false;
	MissionReports.Add(Brief);

	FESCROWMissionReport FleetStatus;
	FleetStatus.ReportID = TEXT("RPT-0002-FLEET");
	FleetStatus.ReportType = EESCROWReportType::FleetTelemetry;
	FleetStatus.Title = TEXT("Fleet Status");
	FleetStatus.Source = TEXT("ConvoyTelemetryManager");
	FleetStatus.BodyText = TEXT("Live CV/F1-F4 position and spacing data. See telemetry table below.");
	FleetStatus.ReceivedMissionTime = MissionTime;
	FleetStatus.bRequiresAck = false;
	FleetStatus.bAcknowledged = false;
	MissionReports.Add(FleetStatus);

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordReportFieldSimple(Brief.ReportID, TEXT("Text"), TEXT("Generated"), Brief.Title, FString(), -1, FString());
		EventLog->RecordReportFieldSimple(FleetStatus.ReportID, TEXT("FleetTelemetry"), TEXT("Generated"), FleetStatus.Title, FString(), -1, FString());
	}
}

FString UESCROWFleetSubsystem::GetFollowerCallsign(int32 FollowerID) const
{
	return ESCROWFollowerCallsign(FollowerID);
}

int32 UESCROWFleetSubsystem::GetMissionReportCount() const
{
	return MissionReports.Num();
}

bool UESCROWFleetSubsystem::IsMissionReportTelemetry(int32 Index) const
{
	return MissionReports.IsValidIndex(Index) && MissionReports[Index].ReportType == EESCROWReportType::FleetTelemetry;
}

bool UESCROWFleetSubsystem::GetMissionReportByIndex(int32 Index, FESCROWMissionReport& OutReport) const
{
	if (!MissionReports.IsValidIndex(Index))
	{
		return false;
	}

	OutReport = MissionReports[Index];
	return true;
}

bool UESCROWFleetSubsystem::AcknowledgeMissionReport(int32 Index)
{
	if (!MissionReports.IsValidIndex(Index))
	{
		return false;
	}

	FESCROWMissionReport& Report = MissionReports[Index];
	if (!Report.bRequiresAck)
	{
		return false;
	}

	Report.bAcknowledged = true;

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordReportFieldSimple(Report.ReportID, Report.ReportType == EESCROWReportType::Text ? TEXT("Text") : TEXT("FleetTelemetry"), TEXT("Acknowledged"), Report.Title, FString(), -1, FString());
	}

	return true;
}

// =============================== Alert toast stack ===============================

int32 UESCROWFleetSubsystem::PostNotification(EESCROWNotificationKind Kind, EESCROWMessageSeverity Severity, int32 FollowerID, const FString& Title, const FString& Body, const FString& RelatedPage, int32 ReportIndex)
{
	FESCROWNotification N;
	N.NotificationID = ++NotificationCounter;
	N.Kind = Kind;
	N.Severity = Severity;
	N.State = EESCROWNotificationState::Active;
	N.Title = Title;
	N.Body = Body;
	N.FollowerID = FollowerID;
	N.ReportIndex = ReportIndex;
	N.RelatedPage = RelatedPage;
	N.CreatedWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Notifications.Add(N);

	// Keep the history bounded; only ever drop finished entries.
	while (Notifications.Num() > 100 && Notifications[0].State != EESCROWNotificationState::Active)
	{
		Notifications.RemoveAt(0);
	}

	LogNotificationEvent(TEXT("AlertPosted"), N, FString(), 0.0f);

	if (UWorld* World = GetWorld())
	{
		if (!World->GetTimerManager().IsTimerActive(NotificationTimerHandle))
		{
			World->GetTimerManager().SetTimer(NotificationTimerHandle, this, &UESCROWFleetSubsystem::NotificationTick, 1.0f, true);
		}
	}
	return N.NotificationID;
}

void UESCROWFleetSubsystem::LogNotificationEvent(const TCHAR* EventType, const FESCROWNotification& Notification, const FString& Method, float AgeSeconds) const
{
	UESCROWEventLogSubsystem* EventLog = GetEventLog();
	if (!EventLog)
	{
		return;
	}
	const UEnum* KindEnum = StaticEnum<EESCROWNotificationKind>();
	const FString KindStr = KindEnum ? KindEnum->GetNameStringByValue(static_cast<int64>(Notification.Kind)) : FString();
	const FString Payload = FString::Printf(TEXT("{\"NotificationID\":%d,\"Kind\":\"%s\",\"FollowerID\":%d,\"ReportIndex\":%d,\"RelatedPage\":\"%s\",\"Method\":\"%s\",\"AgeSeconds\":%.2f}"),
		Notification.NotificationID, *KindStr, Notification.FollowerID, Notification.ReportIndex, *Notification.RelatedPage, *Method, AgeSeconds);
	EventLog->RecordScenarioEventSimple(FString(), EventType, Notification.Title, FString(), Notification.FollowerID, TEXT("Unspecified"), Payload);
}

bool UESCROWFleetSubsystem::AcknowledgeNotification(int32 NotificationID, const FString& Method)
{
	for (FESCROWNotification& N : Notifications)
	{
		if (N.NotificationID == NotificationID && N.State == EESCROWNotificationState::Active)
		{
			N.State = EESCROWNotificationState::Acknowledged;
			const float Age = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) - N.CreatedWorldSeconds;
			LogNotificationEvent(TEXT("AlertAcknowledged"), N, Method, Age);
			return true;
		}
	}
	return false;
}

bool UESCROWFleetSubsystem::AcknowledgeOldestNotification(const FString& Method)
{
	for (const FESCROWNotification& N : Notifications)
	{
		if (N.State == EESCROWNotificationState::Active)
		{
			return AcknowledgeNotification(N.NotificationID, Method);
		}
	}
	return false;
}

int32 UESCROWFleetSubsystem::AcknowledgeNotificationsForPage(const FString& PageName)
{
	if (PageName.IsEmpty())
	{
		return 0;
	}
	TArray<int32> ToAck;
	for (const FESCROWNotification& N : Notifications)
	{
		if (N.State == EESCROWNotificationState::Active && N.RelatedPage.Equals(PageName, ESearchCase::IgnoreCase))
		{
			ToAck.Add(N.NotificationID);
		}
	}
	for (int32 ID : ToAck)
	{
		AcknowledgeNotification(ID, TEXT("PageOpened"));
	}
	return ToAck.Num();
}

void UESCROWFleetSubsystem::NotificationTick()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	// One alert per tick, oldest first: stacked alerts clear one at a time.
	for (FESCROWNotification& N : Notifications)
	{
		if (N.State == EESCROWNotificationState::Active && (Now - N.CreatedWorldSeconds) >= NotificationAutoClearSeconds)
		{
			N.State = EESCROWNotificationState::Expired;
			LogNotificationEvent(TEXT("AlertExpired"), N, TEXT("AutoClear"), Now - N.CreatedWorldSeconds);
			break;
		}
	}
}

int32 UESCROWFleetSubsystem::GetActiveNotificationCount() const
{
	int32 Count = 0;
	for (const FESCROWNotification& N : Notifications)
	{
		if (N.State == EESCROWNotificationState::Active) { ++Count; }
	}
	return Count;
}

const FESCROWNotification* UESCROWFleetSubsystem::GetActiveBySlot(int32 SlotIndex) const
{
	if (SlotIndex < 0)
	{
		return nullptr;
	}
	int32 Seen = 0;
	for (int32 i = Notifications.Num() - 1; i >= 0; --i)
	{
		if (Notifications[i].State == EESCROWNotificationState::Active)
		{
			if (Seen == SlotIndex)
			{
				return &Notifications[i];
			}
			++Seen;
		}
	}
	return nullptr;
}

FString UESCROWFleetSubsystem::GetNotificationSlotText(int32 SlotIndex) const
{
	const FESCROWNotification* N = GetActiveBySlot(SlotIndex);
	if (!N)
	{
		return FString();
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const int32 Remaining = FMath::Max(0, FMath::CeilToInt(NotificationAutoClearSeconds - (Now - N->CreatedWorldSeconds)));
	return FString::Printf(TEXT("%s\n%s\n[Y] acknowledge  -  clears in %ds"), *N->Title, *N->Body, Remaining);
}

int32 UESCROWFleetSubsystem::GetNotificationSlotSeverity(int32 SlotIndex) const
{
	const FESCROWNotification* N = GetActiveBySlot(SlotIndex);
	return N ? static_cast<int32>(N->Severity) : -1;
}

int32 UESCROWFleetSubsystem::AddMissionReport(const FString& ReportID, const FString& Title, const FString& Source, const FString& BodyText, bool bRequiresAck, bool bNotify)
{
	FESCROWMissionReport Report;
	Report.ReportID = ReportID;
	Report.ReportType = EESCROWReportType::Text;
	Report.Title = Title;
	Report.Source = Source;
	Report.BodyText = BodyText;
	Report.ReceivedMissionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Report.bRequiresAck = bRequiresAck;
	Report.bAcknowledged = false;
	const int32 Index = MissionReports.Add(Report);

	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		EventLog->RecordReportFieldSimple(Report.ReportID, TEXT("Text"), TEXT("Generated"), Report.Title, FString(), -1, FString());
	}

	if (bNotify)
	{
		PostNotification(EESCROWNotificationKind::Report, EESCROWMessageSeverity::Info, 0,
			FString::Printf(TEXT("NEW REPORT: %s"), *Title), BodyText, TEXT("REPORTS"), Index);
	}
	return Index;
}
