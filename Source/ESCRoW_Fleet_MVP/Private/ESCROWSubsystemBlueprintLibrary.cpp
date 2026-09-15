// Fill out your copyright notice in the Description page of Project Settings.

#include "ESCROWSubsystemBlueprintLibrary.h"

#include "ESCROWFleetSubsystem.h"
#include "ESCROWSessionSubsystem.h"
#include "ESCROWEventLogSubsystem.h"
#include "ESCROWScenarioDirectorSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UESCROWFleetSubsystem* UESCROWSubsystemBlueprintLibrary::GetESCROWFleetSubsystem(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UESCROWFleetSubsystem>() : nullptr;
}

UESCROWSessionSubsystem* UESCROWSubsystemBlueprintLibrary::GetESCROWSessionSubsystem(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UESCROWSessionSubsystem>() : nullptr;
}

UESCROWEventLogSubsystem* UESCROWSubsystemBlueprintLibrary::GetESCROWEventLogSubsystem(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UESCROWEventLogSubsystem>() : nullptr;
}

UESCROWScenarioDirectorSubsystem* UESCROWSubsystemBlueprintLibrary::GetESCROWScenarioDirectorSubsystem(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UESCROWScenarioDirectorSubsystem>() : nullptr;
}

bool UESCROWSubsystemBlueprintLibrary::LogESCROWEvent(const UObject* WorldContextObject, const FString& EventType, const FString& EventText, int32 FollowerID)
{
	UESCROWEventLogSubsystem* EventLogSubsystem = GetESCROWEventLogSubsystem(WorldContextObject);
	if (!EventLogSubsystem)
	{
		return false;
	}

	EventLogSubsystem->AddEvent(EventType, EventText, FollowerID, FString(), FString());
	return true;
}

bool UESCROWSubsystemBlueprintLibrary::SelectESCROWFollower(const UObject* WorldContextObject, int32 FollowerID)
{
	UESCROWFleetSubsystem* FleetSubsystem = GetESCROWFleetSubsystem(WorldContextObject);
	return FleetSubsystem ? FleetSubsystem->SelectFollower(FollowerID) : false;
}

bool UESCROWSubsystemBlueprintLibrary::SendESCROWFollowerMessage(const UObject* WorldContextObject, int32 FollowerID, const FString& MessageText)
{
	UESCROWFleetSubsystem* FleetSubsystem = GetESCROWFleetSubsystem(WorldContextObject);
	return FleetSubsystem ? FleetSubsystem->SendLeadMessageToFollower(FollowerID, MessageText) : false;
}
