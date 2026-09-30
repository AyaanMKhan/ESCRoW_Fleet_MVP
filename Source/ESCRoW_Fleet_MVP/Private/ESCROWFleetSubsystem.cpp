// Fill out your copyright notice in the Description page of Project Settings.

#include "ESCROWFleetSubsystem.h"
#include "ESCROWEventLogSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

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

	for (int32 FollowerID = 1; FollowerID <= 4; ++FollowerID)
	{
		FESCROWFollowerStatus Status;
		Status.FollowerID = FollowerID;
		Status.Callsign = FString::Printf(TEXT("F%d"), FollowerID);
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

void UESCROWFleetSubsystem::RegisterFollower(int32 FollowerID, const FString& Callsign, AActor* ActorRef)
{
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

	Found->CurrentState = NewState;
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
}
