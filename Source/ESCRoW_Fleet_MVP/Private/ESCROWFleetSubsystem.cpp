// Fill out your copyright notice in the Description page of Project Settings.

#include "ESCROWFleetSubsystem.h"

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

	Found->bRequiresTakeover = bRequiresTakeover;
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

	bTakeoverActive = true;
	ActiveTakeoverFollowerID = FollowerID;
	Found->CurrentState = EESCROWFollowerState::DirectControl;
	Found->AutonomyMode = EESCROWAutonomyMode::DirectControl;

	AddHUDMessage(EESCROWMessageSource::System, EESCROWMessageSeverity::Warning, FollowerID, FString::Printf(TEXT("Takeover lock acquired for %s."), *Found->Callsign));

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
	}

	return true;
}

void UESCROWFleetSubsystem::ClearTransientAlerts()
{
	for (FESCROWFollowerStatus& Status : FollowerStatuses)
	{
		Status.bRequiresTakeover = false;
	}

	AddHUDMessage(EESCROWMessageSource::System, EESCROWMessageSeverity::Info, 0, TEXT("Transient alerts cleared."));
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
