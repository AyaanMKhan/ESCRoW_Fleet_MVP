// Fill out your copyright notice in the Description page of Project Settings.

#include "ESCROWSessionSubsystem.h"

void UESCROWSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ResetSessionState();
}

void UESCROWSessionSubsystem::ResetSessionState()
{
	ParticipantID = TEXT("DEMO_001");
	SessionID = TEXT("S001");
	SelectedMissionCondition = EESCROWMissionCondition::MissionD_DriverTakeover;
	bSessionInitialized = false;
	SessionStartTimeSeconds = 0.0f;

	ApplyMissionConditionFlags();
}

void UESCROWSessionSubsystem::StartSession()
{
	bSessionInitialized = true;
	SessionStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void UESCROWSessionSubsystem::SetParticipantID(const FString& NewParticipantID)
{
	ParticipantID = NewParticipantID;
}

void UESCROWSessionSubsystem::SetSessionID(const FString& NewSessionID)
{
	SessionID = NewSessionID;
}

void UESCROWSessionSubsystem::SetMissionCondition(EESCROWMissionCondition NewMissionCondition)
{
	SelectedMissionCondition = NewMissionCondition;
	ApplyMissionConditionFlags();
}

EESCROWMissionCondition UESCROWSessionSubsystem::GetMissionCondition() const
{
	return SelectedMissionCondition;
}

FString UESCROWSessionSubsystem::GetMissionConditionName() const
{
	switch (SelectedMissionCondition)
	{
	case EESCROWMissionCondition::MissionA_PassengerMonitor:
		return TEXT("Mission A - Passenger + Monitor");
	case EESCROWMissionCondition::MissionB_PassengerTakeover:
		return TEXT("Mission B - Passenger + Takeover");
	case EESCROWMissionCondition::MissionC_DriverMonitor:
		return TEXT("Mission C - Driver + Monitor");
	case EESCROWMissionCondition::MissionD_DriverTakeover:
		return TEXT("Mission D - Driver + Takeover");
	default:
		return TEXT("Unknown Mission Condition");
	}
}

bool UESCROWSessionSubsystem::IsManualOwnVehicleControlRequired() const
{
	return bManualOwnVehicleControl;
}

bool UESCROWSessionSubsystem::IsTakeoverRequired() const
{
	return bTakeoverRequired;
}

FString UESCROWSessionSubsystem::GetParticipantID() const
{
	return ParticipantID;
}

FString UESCROWSessionSubsystem::GetSessionID() const
{
	return SessionID;
}

bool UESCROWSessionSubsystem::IsSessionInitialized() const
{
	return bSessionInitialized;
}

float UESCROWSessionSubsystem::GetSessionStartTimeSeconds() const
{
	return SessionStartTimeSeconds;
}

void UESCROWSessionSubsystem::ApplyMissionConditionFlags()
{
	switch (SelectedMissionCondition)
	{
	case EESCROWMissionCondition::MissionA_PassengerMonitor:
		bManualOwnVehicleControl = false;
		bTakeoverRequired = false;
		break;
	case EESCROWMissionCondition::MissionB_PassengerTakeover:
		bManualOwnVehicleControl = false;
		bTakeoverRequired = true;
		break;
	case EESCROWMissionCondition::MissionC_DriverMonitor:
		bManualOwnVehicleControl = true;
		bTakeoverRequired = false;
		break;
	case EESCROWMissionCondition::MissionD_DriverTakeover:
	default:
		bManualOwnVehicleControl = true;
		bTakeoverRequired = true;
		break;
	}
}
