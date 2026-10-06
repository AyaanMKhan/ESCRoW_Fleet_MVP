// Fill out your copyright notice in the Description page of Project Settings.

#include "ESCROWPlayerControlRouterComponent.h"
#include "ESCROWEventLogSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UESCROWPlayerControlRouterComponent::UESCROWPlayerControlRouterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UESCROWEventLogSubsystem* UESCROWPlayerControlRouterComponent::GetEventLog() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UESCROWEventLogSubsystem>() : nullptr;
}

void UESCROWPlayerControlRouterComponent::SetPlayerMode(EESCROWPlayerControlMode NewMode)
{
	if (NewMode == CurrentPlayerMode)
	{
		return;
	}

	PreviousPlayerMode = CurrentPlayerMode;
	CurrentPlayerMode = NewMode;

	// This is the sole place player control mode changes (every public API on
	// this component routes through here), so this is the one spot that needs
	// to log it to make takeover/supervisory-mode transitions reconstructable
	// after the fact - previously this left zero trace in the durable record.
	if (UESCROWEventLogSubsystem* EventLog = GetEventLog())
	{
		const UEnum* ModeEnum = StaticEnum<EESCROWPlayerControlMode>();

		FESCROWStateTransitionRecord Record;
		Record.EntityID = TEXT("Player");
		Record.EntityType = TEXT("ControlMode");
		Record.StateName = TEXT("CurrentPlayerMode");
		Record.OldValue = ModeEnum ? ModeEnum->GetNameStringByValue(static_cast<int64>(PreviousPlayerMode)) : FString();
		Record.NewValue = ModeEnum ? ModeEnum->GetNameStringByValue(static_cast<int64>(CurrentPlayerMode)) : FString();
		Record.Source = TEXT("PlayerControlRouter");
		Record.PayloadJSON = FString::Printf(TEXT("{\"ActiveRemoteFollowerID\":%d}"), ActiveRemoteFollowerID);
		EventLog->RecordStateTransition(Record);
	}

	OnPlayerModeChanged.Broadcast(CurrentPlayerMode, PreviousPlayerMode);
}

EESCROWPlayerControlMode UESCROWPlayerControlRouterComponent::GetPlayerMode() const
{
	return CurrentPlayerMode;
}

EESCROWPlayerControlMode UESCROWPlayerControlRouterComponent::GetPreviousPlayerMode() const
{
	return PreviousPlayerMode;
}

void UESCROWPlayerControlRouterComponent::FocusCommandHUD()
{
	// A remote takeover owns the player mode until EndRemoteTakeoverShell; HUD toggles (Tab/M/etc.)
	// must not pull the mode out of RemoteTakeover or B/V can no longer request return.
	if (bRemoteTakeoverActive)
	{
		return;
	}

	bConsoleFocused = true;
	SetPlayerMode(EESCROWPlayerControlMode::FocusedCommandHUD);
}

void UESCROWPlayerControlRouterComponent::ReturnToCompactHUD()
{
	// A remote takeover owns the player mode until EndRemoteTakeoverShell; HUD toggles (Tab/M/etc.)
	// must not pull the mode out of RemoteTakeover or B/V can no longer request return.
	if (bRemoteTakeoverActive)
	{
		return;
	}

	bConsoleFocused = false;
	SetPlayerMode(EESCROWPlayerControlMode::CompactHUD);
}

void UESCROWPlayerControlRouterComponent::OpenFullMap()
{
	// A remote takeover owns the player mode until EndRemoteTakeoverShell; HUD toggles (Tab/M/etc.)
	// must not pull the mode out of RemoteTakeover or B/V can no longer request return.
	if (bRemoteTakeoverActive)
	{
		return;
	}

	bFullMapOpen = true;
	SetPlayerMode(EESCROWPlayerControlMode::FullMap);
}

void UESCROWPlayerControlRouterComponent::CloseFullMap()
{
	// A remote takeover owns the player mode until EndRemoteTakeoverShell; HUD toggles (Tab/M/etc.)
	// must not pull the mode out of RemoteTakeover or B/V can no longer request return.
	if (bRemoteTakeoverActive)
	{
		return;
	}

	bFullMapOpen = false;
	SetPlayerMode(PreviousPlayerMode);
}

void UESCROWPlayerControlRouterComponent::BeginRemoteTakeoverShell(int32 FollowerID)
{
	bRemoteTakeoverActive = true;
	ActiveRemoteFollowerID = FollowerID;
	SetPlayerMode(EESCROWPlayerControlMode::RemoteTakeover);

	OnRemoteTakeoverShellStarted.Broadcast(FollowerID);
}

void UESCROWPlayerControlRouterComponent::EndRemoteTakeoverShell()
{
	const int32 EndedFollowerID = ActiveRemoteFollowerID;

	bRemoteTakeoverActive = false;
	ActiveRemoteFollowerID = -1;
	SetPlayerMode(EESCROWPlayerControlMode::ReturnToAutonomyTransition);

	OnRemoteTakeoverShellEnded.Broadcast(EndedFollowerID);
}

bool UESCROWPlayerControlRouterComponent::IsRemoteTakeoverActive() const
{
	return bRemoteTakeoverActive;
}

int32 UESCROWPlayerControlRouterComponent::GetActiveRemoteFollowerID() const
{
	return ActiveRemoteFollowerID;
}
