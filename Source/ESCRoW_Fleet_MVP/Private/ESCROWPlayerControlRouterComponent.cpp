// Fill out your copyright notice in the Description page of Project Settings.

#include "ESCROWPlayerControlRouterComponent.h"

UESCROWPlayerControlRouterComponent::UESCROWPlayerControlRouterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UESCROWPlayerControlRouterComponent::SetPlayerMode(EESCROWPlayerControlMode NewMode)
{
	if (NewMode == CurrentPlayerMode)
	{
		return;
	}

	PreviousPlayerMode = CurrentPlayerMode;
	CurrentPlayerMode = NewMode;

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
	bConsoleFocused = true;
	SetPlayerMode(EESCROWPlayerControlMode::FocusedCommandHUD);
}

void UESCROWPlayerControlRouterComponent::ReturnToCompactHUD()
{
	bConsoleFocused = false;
	SetPlayerMode(EESCROWPlayerControlMode::CompactHUD);
}

void UESCROWPlayerControlRouterComponent::OpenFullMap()
{
	bFullMapOpen = true;
	SetPlayerMode(EESCROWPlayerControlMode::FullMap);
}

void UESCROWPlayerControlRouterComponent::CloseFullMap()
{
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
