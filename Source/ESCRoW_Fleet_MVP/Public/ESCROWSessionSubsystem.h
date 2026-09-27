// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ESCROWTypes.h"
#include "ESCROWSessionSubsystem.generated.h"

/**
 * Persistent cross-map experiment/session state: selected mission condition,
 * participant/session identity, and the manual-driving/takeover-required flags
 * derived from the mission condition. Survives the main-menu-to-mission map
 * transition. Live world/fleet state belongs to UESCROWFleetSubsystem, not here.
 */
UCLASS(BlueprintType)
class ESCROW_FLEET_MVP_API UESCROWSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	FString ParticipantID;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	FString SessionID;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	EESCROWMissionCondition SelectedMissionCondition = EESCROWMissionCondition::MissionD_DriverTakeover;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	bool bManualOwnVehicleControl = true;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	bool bTakeoverRequired = true;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	bool bSessionInitialized = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	float SessionStartTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	FString ResearcherID;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	FString StudyID;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	FString MissionID;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	FString MissionDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	FString MapName;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	EESCROWComplexityCondition ComplexityCondition = EESCROWComplexityCondition::LowComplexity;

	// RA36A-1: experimental display-locus / vehicle-control-role factors.
	// Metadata/plumbing only in this pass — no runtime HUD/console/possession
	// behavior reads these yet. HUDPrimary + Driving matches current default behavior.
	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	EESCROWDisplayMode DisplayMode = EESCROWDisplayMode::HUDPrimary;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	EESCROWVehicleControlRole VehicleControlRole = EESCROWVehicleControlRole::Driving;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	FString StartTimestampLocal;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	FString StartTimestampUTC;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	FString EndTimestamp;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	FString BuildVersion;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	FString Notes;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|Session")
	bool bSessionActive = false;

	// RA41A: run config written by the experiment setup menu, promoted to
	// CurrentRunConfig on level load (see UESCROWScenarioDirectorSubsystem::Initialize).
	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|RunConfig")
	FESCROWRunConfig PendingRunConfig;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|RunConfig")
	FESCROWRunConfig CurrentRunConfig;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|RunConfig")
	bool bHasPendingRunConfig = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|RunConfig")
	bool bHasCurrentRunConfig = false;

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void ResetSessionState();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void StartSession();

	// If a session was already initialized (via the setup screen or Quick Start),
	// does nothing. Otherwise applies the given fallback identity/mission and
	// starts the session, so a mission map opened directly never has empty
	// session metadata. Safe to call unconditionally from mission map BeginPlay.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void EnsureSessionFallback(const FString& FallbackResearcherID, const FString& FallbackParticipantID, const FString& FallbackMissionID, const FString& FallbackMissionDisplayName);

	// Sets identity/mission/condition fields and marks the session initialized,
	// without starting the clock. Call before loading the mission map.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void InitializeSession(const FString& NewResearcherID, const FString& NewParticipantID, const FString& NewMissionID, const FString& NewMissionDisplayName, EESCROWMissionCondition NewMissionCondition, EESCROWComplexityCondition NewComplexityCondition);

	// Marks the session ended and stamps EndTimestamp. Does not clear metadata.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void EndSession();

	// Alias for ResetSessionState, named to match the RA19B spec.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void ResetSession();

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	FESCROWSessionMetadata GetSessionMetadata() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	FString GetResearcherID() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	FString GetMissionID() const;

	// Human-readable summary combining mission condition and complexity,
	// e.g. "Mission D - Driver + Takeover / High Complexity".
	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	FString GetConditionSummary() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	bool IsSessionActive() const;

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void SetResearcherID(const FString& NewResearcherID);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void SetMissionID(const FString& NewMissionID, const FString& NewMissionDisplayName);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void SetComplexityCondition(EESCROWComplexityCondition NewComplexityCondition);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	FString GenerateSessionID();

	// UI convenience setters so widgets can pass a ComboBoxString selected index
	// directly, without building enum literal pins in the graph. Index is clamped.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void SetMissionConditionByIndex(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void SetComplexityConditionByIndex(int32 Index);

	// RA36A-1: experimental display-locus / vehicle-control-role factors.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void SetDisplayMode(EESCROWDisplayMode NewDisplayMode);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void SetDisplayModeByIndex(int32 Index);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	EESCROWDisplayMode GetDisplayMode() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	FString GetDisplayModeName() const;

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void SetVehicleControlRole(EESCROWVehicleControlRole NewVehicleControlRole);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void SetVehicleControlRoleByIndex(int32 Index);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	EESCROWVehicleControlRole GetVehicleControlRole() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	FString GetVehicleControlRoleName() const;

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void SetParticipantID(const FString& NewParticipantID);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void SetSessionID(const FString& NewSessionID);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Session")
	void SetMissionCondition(EESCROWMissionCondition NewMissionCondition);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	EESCROWMissionCondition GetMissionCondition() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	FString GetMissionConditionName() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	bool IsManualOwnVehicleControlRequired() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	bool IsTakeoverRequired() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	FString GetParticipantID() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	FString GetSessionID() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	bool IsSessionInitialized() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|Session")
	float GetSessionStartTimeSeconds() const;

	// --- RA41A run config API ---

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|RunConfig")
	void SetPendingRunConfig(const FESCROWRunConfig& Config);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|RunConfig")
	void GetPendingRunConfig(FESCROWRunConfig& OutConfig, bool& bFound) const;

	// Copies PendingRunConfig into CurrentRunConfig (if one is pending) and
	// clears the pending flag. Safe to call with no pending config: OutConfig
	// is left as the last CurrentRunConfig (possibly default) and bFound is false.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|RunConfig")
	void PromotePendingRunConfigToCurrent(FESCROWRunConfig& OutConfig, bool& bFound);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|RunConfig")
	void GetCurrentRunConfig(FESCROWRunConfig& OutConfig, bool& bFound) const;

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|RunConfig")
	void ClearPendingRunConfig();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|RunConfig")
	void ClearCurrentRunConfig();

	// Filesystem-safe run ID: YYYYMMDD_HHMMSS_<ParticipantID>_<ScenarioID>_<ConditionID>.
	// Sanitizes each component (spaces/slashes/etc. become underscores).
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|RunConfig")
	FString GenerateRunID(const FString& InScenarioID, const FString& InConditionID, const FString& InParticipantID);

private:
	void ApplyMissionConditionFlags();
	static FString SanitizeForFilesystem(const FString& In);
};
