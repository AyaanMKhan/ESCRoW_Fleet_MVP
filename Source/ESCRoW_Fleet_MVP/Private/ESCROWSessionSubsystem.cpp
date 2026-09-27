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

	ResearcherID.Empty();
	StudyID.Empty();
	MissionID.Empty();
	MissionDisplayName.Empty();
	MapName.Empty();
	ComplexityCondition = EESCROWComplexityCondition::LowComplexity;
	DisplayMode = EESCROWDisplayMode::HUDPrimary;
	VehicleControlRole = EESCROWVehicleControlRole::Driving;
	StartTimestampLocal.Empty();
	StartTimestampUTC.Empty();
	EndTimestamp.Empty();
	Notes.Empty();
	bSessionActive = false;

	ApplyMissionConditionFlags();
}

void UESCROWSessionSubsystem::StartSession()
{
	bSessionInitialized = true;
	bSessionActive = true;
	SessionStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	StartTimestampLocal = FDateTime::Now().ToString();
	StartTimestampUTC = FDateTime::UtcNow().ToIso8601();
}

void UESCROWSessionSubsystem::EnsureSessionFallback(const FString& FallbackResearcherID, const FString& FallbackParticipantID, const FString& FallbackMissionID, const FString& FallbackMissionDisplayName)
{
	if (bSessionInitialized)
	{
		return;
	}

	ResearcherID = FallbackResearcherID;
	ParticipantID = FallbackParticipantID;
	MissionID = FallbackMissionID;
	MissionDisplayName = FallbackMissionDisplayName;
	StartSession();
}

void UESCROWSessionSubsystem::InitializeSession(const FString& NewResearcherID, const FString& NewParticipantID, const FString& NewMissionID, const FString& NewMissionDisplayName, EESCROWMissionCondition NewMissionCondition, EESCROWComplexityCondition NewComplexityCondition)
{
	ResearcherID = NewResearcherID;
	ParticipantID = NewParticipantID;
	MissionID = NewMissionID;
	MissionDisplayName = NewMissionDisplayName;
	SetMissionCondition(NewMissionCondition);
	ComplexityCondition = NewComplexityCondition;

	if (SessionID.IsEmpty())
	{
		GenerateSessionID();
	}

	bSessionInitialized = true;
}

void UESCROWSessionSubsystem::EndSession()
{
	bSessionActive = false;
	EndTimestamp = FDateTime::Now().ToString();
}

void UESCROWSessionSubsystem::ResetSession()
{
	ResetSessionState();
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

FESCROWSessionMetadata UESCROWSessionSubsystem::GetSessionMetadata() const
{
	FESCROWSessionMetadata Metadata;
	Metadata.ResearcherID = ResearcherID;
	Metadata.ParticipantID = ParticipantID;
	Metadata.SessionID = SessionID;
	Metadata.StudyID = StudyID;
	Metadata.MissionID = MissionID;
	Metadata.MissionDisplayName = MissionDisplayName;
	Metadata.MapName = MapName;
	Metadata.ComplexityCondition = ComplexityCondition;
	Metadata.MissionCondition = SelectedMissionCondition;
	Metadata.DisplayMode = DisplayMode;
	Metadata.VehicleControlRole = VehicleControlRole;
	Metadata.ConditionSummary = GetConditionSummary();
	Metadata.StartTimestampLocal = StartTimestampLocal;
	Metadata.StartTimestampUTC = StartTimestampUTC;
	Metadata.EndTimestamp = EndTimestamp;
	Metadata.BuildVersion = BuildVersion;
	Metadata.Notes = Notes;
	Metadata.bSessionInitialized = bSessionInitialized;
	Metadata.bSessionActive = bSessionActive;
	return Metadata;
}

FString UESCROWSessionSubsystem::GetResearcherID() const
{
	return ResearcherID;
}

FString UESCROWSessionSubsystem::GetMissionID() const
{
	return MissionID;
}

FString UESCROWSessionSubsystem::GetConditionSummary() const
{
	const FString ComplexityName = (ComplexityCondition == EESCROWComplexityCondition::HighComplexity) ? TEXT("High Complexity") : TEXT("Low Complexity");
	return FString::Printf(TEXT("%s / %s / %s / %s"), *GetMissionConditionName(), *ComplexityName, *GetDisplayModeName(), *GetVehicleControlRoleName());
}

void UESCROWSessionSubsystem::SetDisplayMode(EESCROWDisplayMode NewDisplayMode)
{
	DisplayMode = NewDisplayMode;
}

void UESCROWSessionSubsystem::SetDisplayModeByIndex(int32 Index)
{
	const int32 ClampedIndex = FMath::Clamp(Index, 0, 1);
	DisplayMode = static_cast<EESCROWDisplayMode>(ClampedIndex);
}

EESCROWDisplayMode UESCROWSessionSubsystem::GetDisplayMode() const
{
	return DisplayMode;
}

FString UESCROWSessionSubsystem::GetDisplayModeName() const
{
	return (DisplayMode == EESCROWDisplayMode::ConsolePrimary) ? TEXT("Console Primary") : TEXT("HUD Primary");
}

void UESCROWSessionSubsystem::SetVehicleControlRole(EESCROWVehicleControlRole NewVehicleControlRole)
{
	VehicleControlRole = NewVehicleControlRole;
}

void UESCROWSessionSubsystem::SetVehicleControlRoleByIndex(int32 Index)
{
	const int32 ClampedIndex = FMath::Clamp(Index, 0, 1);
	VehicleControlRole = static_cast<EESCROWVehicleControlRole>(ClampedIndex);
}

EESCROWVehicleControlRole UESCROWSessionSubsystem::GetVehicleControlRole() const
{
	return VehicleControlRole;
}

FString UESCROWSessionSubsystem::GetVehicleControlRoleName() const
{
	return (VehicleControlRole == EESCROWVehicleControlRole::NotDriving) ? TEXT("Not Driving") : TEXT("Driving");
}

bool UESCROWSessionSubsystem::IsSessionActive() const
{
	return bSessionActive;
}

void UESCROWSessionSubsystem::SetResearcherID(const FString& NewResearcherID)
{
	ResearcherID = NewResearcherID;
}

void UESCROWSessionSubsystem::SetMissionID(const FString& NewMissionID, const FString& NewMissionDisplayName)
{
	MissionID = NewMissionID;
	MissionDisplayName = NewMissionDisplayName;
}

void UESCROWSessionSubsystem::SetComplexityCondition(EESCROWComplexityCondition NewComplexityCondition)
{
	ComplexityCondition = NewComplexityCondition;
}

FString UESCROWSessionSubsystem::GenerateSessionID()
{
	SessionID = FString::Printf(TEXT("S_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	return SessionID;
}

void UESCROWSessionSubsystem::SetMissionConditionByIndex(int32 Index)
{
	const int32 ClampedIndex = FMath::Clamp(Index, 0, 3);
	SetMissionCondition(static_cast<EESCROWMissionCondition>(ClampedIndex));
}

void UESCROWSessionSubsystem::SetComplexityConditionByIndex(int32 Index)
{
	const int32 ClampedIndex = FMath::Clamp(Index, 0, 1);
	ComplexityCondition = static_cast<EESCROWComplexityCondition>(ClampedIndex);
}

void UESCROWSessionSubsystem::SetPendingRunConfig(const FESCROWRunConfig& Config)
{
	PendingRunConfig = Config;
	bHasPendingRunConfig = true;
}

void UESCROWSessionSubsystem::GetPendingRunConfig(FESCROWRunConfig& OutConfig, bool& bFound) const
{
	OutConfig = PendingRunConfig;
	bFound = bHasPendingRunConfig;
}

void UESCROWSessionSubsystem::PromotePendingRunConfigToCurrent(FESCROWRunConfig& OutConfig, bool& bFound)
{
	if (bHasPendingRunConfig)
	{
		CurrentRunConfig = PendingRunConfig;
		bHasCurrentRunConfig = true;
		bHasPendingRunConfig = false;
	}

	OutConfig = CurrentRunConfig;
	bFound = bHasCurrentRunConfig;
}

void UESCROWSessionSubsystem::GetCurrentRunConfig(FESCROWRunConfig& OutConfig, bool& bFound) const
{
	OutConfig = CurrentRunConfig;
	bFound = bHasCurrentRunConfig;
}

void UESCROWSessionSubsystem::ClearPendingRunConfig()
{
	PendingRunConfig = FESCROWRunConfig();
	bHasPendingRunConfig = false;
}

void UESCROWSessionSubsystem::ClearCurrentRunConfig()
{
	CurrentRunConfig = FESCROWRunConfig();
	bHasCurrentRunConfig = false;
}

FString UESCROWSessionSubsystem::GenerateRunID(const FString& InScenarioID, const FString& InConditionID, const FString& InParticipantID)
{
	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString SafeParticipant = SanitizeForFilesystem(InParticipantID.IsEmpty() ? TEXT("PILOT") : InParticipantID);
	const FString SafeScenario = SanitizeForFilesystem(InScenarioID.IsEmpty() ? TEXT("UNSET") : InScenarioID);
	const FString SafeCondition = SanitizeForFilesystem(InConditionID.IsEmpty() ? TEXT("UNSET") : InConditionID);

	return FString::Printf(TEXT("%s_%s_%s_%s"), *Timestamp, *SafeParticipant, *SafeScenario, *SafeCondition);
}

FString UESCROWSessionSubsystem::SanitizeForFilesystem(const FString& In)
{
	FString Out = In;
	Out.ReplaceInline(TEXT(" "), TEXT("_"));

	static const TCHAR* Illegal = TEXT("\\/:*?\"<>|.,");
	for (const TCHAR* Ch = Illegal; *Ch; ++Ch)
	{
		Out.ReplaceCharInline(*Ch, TEXT('_'));
	}

	return Out;
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
