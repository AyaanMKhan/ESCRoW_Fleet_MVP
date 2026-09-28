// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ESCROWTypes.h"
#include "ESCROWEventLogSubsystem.generated.h"

/**
 * Central structured event log store, in memory for now, formatted for later
 * CSV/JSONL export. Actual file writing is a later RA pass.
 */
UCLASS(BlueprintType)
class ESCROW_FLEET_MVP_API UESCROWEventLogSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// RA41D: GameInstanceSubsystems are torn down on PIE stop and game quit, but
	// nothing else calls EndRun/FlushAllOutputs on that path — without this, a
	// live run's run_summary.json/export_status.txt stay at their initial
	// zeroed/skeleton state forever (the per-record CSV/JSONL files are fine;
	// they're appended immediately per record, not buffered).
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|EventLog")
	TArray<FESCROWEventLogEntry> EventEntries;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|EventLog")
	int32 EventCounter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|EventLog")
	bool bLoggingEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|EventLog")
	FString CurrentLogFilePath;

	// --- RA41A run-folder / data-manager state ---

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|DataManager")
	FString CurrentRunID;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|DataManager")
	FString CurrentRunRootDirectory;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|DataManager")
	FString CurrentRunFolder;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|DataManager")
	bool bRunActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|DataManager")
	int32 ScenarioEventCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|DataManager")
	int32 ParticipantActionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|DataManager")
	int32 UIExposureCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|DataManager")
	int32 HeartbeatCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|DataManager")
	int32 StateTransitionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|DataManager")
	int32 ReportRecordCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|DataManager")
	float HeartbeatIntervalSeconds = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|DataManager")
	bool bHeartbeatEnabled = false;

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|EventLog")
	void ResetLog();

	// Clears the in-memory log and resets EventCounter. Equivalent to ResetLog,
	// named to match the RA19B spec.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|EventLog")
	void ClearEventLog();

	// Clears the log and records a "Session log started" marker event.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|EventLog")
	void StartSessionLog();

	// Records a "Session log ended" marker event. Does not clear the log.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|EventLog")
	void EndSessionLog();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|EventLog")
	void SetLoggingEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|EventLog")
	bool IsLoggingEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|EventLog")
	FESCROWEventLogEntry AddEvent(const FString& EventType, const FString& EventText, int32 FollowerID, const FString& CommandID, const FString& MessageID);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|EventLog")
	FESCROWEventLogEntry AddEventFull(const FESCROWEventLogEntry& Entry);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|EventLog")
	TArray<FESCROWEventLogEntry> GetEventEntries() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|EventLog")
	int32 GetEventCount() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|EventLog")
	FString BuildCSVHeader() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|EventLog")
	FString EntryToCSVRow(const FESCROWEventLogEntry& Entry) const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|EventLog")
	FString BuildJSONLine(const FESCROWEventLogEntry& Entry) const;

	// Full CSV: header row plus one row per entry, newline-separated.
	UFUNCTION(BlueprintPure, Category = "ESCRoW|EventLog")
	FString BuildCSVString() const;

	// Full JSONL: one JSON object per line, one per entry.
	UFUNCTION(BlueprintPure, Category = "ESCRoW|EventLog")
	FString BuildJSONLString() const;

	// Writes both CSV and JSONL to Saved/ESCRoW/Logs/ESCRoW_<SessionID>_events.{csv,jsonl}.
	// Never crashes on failure; logs a warning and returns false instead.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|EventLog")
	bool ExportEventLogToFile(FString& OutResultPath, FString& OutStatusMessage);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|EventLog")
	FString GetCurrentLogFilePath() const;

	// One-line human-readable summary, e.g. "42 events logged. Last export: <path>".
	UFUNCTION(BlueprintPure, Category = "ESCRoW|EventLog")
	FString GetLogSummary() const;

	// --- RA41A data-manager run lifecycle ---

	// Ends any existing run, then starts a new one rooted at
	// Documents/ESCRoW_Data/<Config.RunID>/, creates the folder, and writes the
	// initial skeleton files (manifest, CSV headers, zeroed summary).
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool BeginRunFromConfig(const FESCROWRunConfig& Config);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	void EndRun(const FString& EndReason);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|DataManager")
	FString GetCurrentRunFolder() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|DataManager")
	bool GetRunActive() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|DataManager")
	void GetRunRecordCounts(int32& OutScenarioEvents, int32& OutParticipantActions, int32& OutUIExposures, int32& OutHeartbeats, int32& OutStateTransitions, int32& OutReportRecords) const;

	// Creates CurrentRunFolder (and root) on disk if it does not already exist.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool EnsureRunFolder();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool WriteSessionManifest(const FESCROWRunConfig& Config);

	// Creates all 10 skeleton output files (CSV headers / empty JSONL / initial
	// export_status.txt and run_summary.json). Called once from BeginRunFromConfig.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool InitializeOutputFiles();

	// Re-writes run_summary.json and export_status.txt with current counts.
	// Does not re-truncate the per-record CSV/JSONL files (those are appended to).
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool FlushAllOutputs();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	FString ExportRunSummary();

	// --- RA41A structured logging API ---
	// Each Record* function no-ops (returns false) if bRunActive is false,
	// after printing a log warning. Otherwise it stamps Timestamp/WorldSeconds/
	// RunID/RecordID if unset, increments the matching count, appends one row
	// to its dedicated file, and mirrors a raw JSONL line to event_log_raw.jsonl.

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool RecordScenarioEvent(const FESCROWScenarioEventRecord& Record);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool RecordScenarioEventSimple(const FString& Phase, const FString& EventType, const FString& EventLabel, const FString& TaskingID, int32 AffectedFollowerID, const FString& PlanValidity, const FString& PayloadJSON);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool RecordParticipantAction(const FESCROWParticipantActionRecord& Record);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool RecordParticipantActionSimple(const FString& ActionType, const FString& Source, const FString& ActivePage, int32 SelectedFollowerID, int32 TargetFollowerID, const FString& TaskingID, const FString& EventID, const FString& PayloadJSON);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool RecordUIExposure(const FESCROWUIExposureRecord& Record);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool RecordUIExposureSimple(const FString& PageName, const FString& WidgetName, int32 SelectedFollowerID, const FString& VisibleFields, const FString& RelatedEventID, bool bDiagnosticEvidenceAvailable, const FString& PayloadJSON);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool RecordHeartbeat(const FESCROWHeartbeatRecord& Record);

	// RA41B: Blueprint-callable heartbeat with primitive params, matching the
	// pattern of the other Record*Simple functions. Fleet CV/F1-F4 telemetry is
	// filled in automatically from UESCROWFleetSubsystem (see PopulateFleetTelemetry);
	// callers only need to supply UI/tasking context. The automatic 1Hz timer
	// (StartHeartbeat/HeartbeatTimerTick) calls RecordHeartbeatNow, which now also
	// populates fleet telemetry, so this function is for explicit/manual calls only.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool RecordHeartbeatSimple(const FString& Phase, const FString& ActivePage, int32 SelectedFollowerID, const FString& TaskingID, const FString& TaskingState, const FString& PlanValidity, const FString& PayloadJSON);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool RecordStateTransition(const FESCROWStateTransitionRecord& Record);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool RecordStateTransitionSimple(const FString& EntityID, const FString& EntityType, const FString& StateName, const FString& OldValue, const FString& NewValue, const FString& Source, const FString& RelatedEventID);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool RecordReportField(const FESCROWReportRecord& Record);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool RecordReportFieldSimple(const FString& ReportID, const FString& ReportType, const FString& FieldName, const FString& FieldValue, const FString& GroundTruthValue, int32 Score, const FString& PayloadJSON);

	// Appends one arbitrary raw JSONL line (caller-built) to event_log_raw.jsonl
	// only, with no typed record and no per-type count increment.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool RecordRawEvent(const FString& EventType, const FString& PayloadJSON);

	// --- RA41A heartbeat ---

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	void StartHeartbeat();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	void StopHeartbeat();

	// Records one heartbeat row immediately, independent of the timer.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|DataManager")
	bool RecordHeartbeatNow();

private:
	FString BuildTimestampString() const;

	static FString EscapeCSVField(const FString& Field);
	static FString EscapeJSONField(const FString& Field);

	// RA41A helpers
	static FString EscapeJSONStringManual(const FString& Field);
	FString BuildRunConfigJSON(const FESCROWRunConfig& Config) const;
	FString BuildRunSummaryJSON() const;
	FString NextRecordID(const TCHAR* Prefix);
	bool AppendLineToFile(const FString& FileName, const FString& Line) const;
	bool WriteWholeFile(const FString& FileName, const FString& Content) const;
	FString GetCurrentPhaseFromScenarioDirector() const;
	void HeartbeatTimerTick();

	// RA41B: fills CVLocation/F1-F4 Location/SpeedCms from UESCROWFleetSubsystem and
	// merges per-follower mode/readiness/comm/alert into R.PayloadJSON as a
	// "FleetTelemetry" JSON object (preserving any caller-supplied PayloadJSON under
	// a "Caller" key). No-ops (leaves fields blank/UNSET) if FleetSubsystem or a given
	// follower's ActorRef is unavailable.
	void PopulateFleetTelemetry(FESCROWHeartbeatRecord& R) const;

	// RA41B: best-effort command-vehicle location. There is no direct CV actor
	// reference on UESCROWFleetSubsystem; this looks for a follower whose
	// CurrentFollowTargetID == 0 (follows the command vehicle) and reads its cached
	// FollowTargetActorRef. Returns false if no such reference is currently known.
	bool TryGetCommandVehicleLocation(FVector& OutLocation) const;

	static FString FormatLocation(const FVector& Location);

	int32 RunConfigCounterForRecordIDs = 0;
	FTimerHandle HeartbeatTimerHandle;
};
