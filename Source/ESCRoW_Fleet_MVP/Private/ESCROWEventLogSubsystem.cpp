// Fill out your copyright notice in the Description page of Project Settings.

#include "ESCROWEventLogSubsystem.h"
#include "ESCROWSessionSubsystem.h"
#include "ESCROWFleetSubsystem.h"
#include "ESCROWScenarioDirectorSubsystem.h"
#include "ESCROWPlayerControlRouterComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformProcess.h"
#include "TimerManager.h"

void UESCROWEventLogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ResetLog();
}

void UESCROWEventLogSubsystem::ResetLog()
{
	EventEntries.Reset();
	EventCounter = 0;
	bLoggingEnabled = true;
}

void UESCROWEventLogSubsystem::ClearEventLog()
{
	ResetLog();
}

void UESCROWEventLogSubsystem::StartSessionLog()
{
	ResetLog();
	AddEvent(TEXT("SessionLogStarted"), TEXT("Session log started."), 0, FString(), FString());
}

void UESCROWEventLogSubsystem::EndSessionLog()
{
	AddEvent(TEXT("SessionLogEnded"), TEXT("Session log ended."), 0, FString(), FString());
}

void UESCROWEventLogSubsystem::SetLoggingEnabled(bool bEnabled)
{
	bLoggingEnabled = bEnabled;
}

bool UESCROWEventLogSubsystem::IsLoggingEnabled() const
{
	return bLoggingEnabled;
}

FESCROWEventLogEntry UESCROWEventLogSubsystem::AddEvent(const FString& EventType, const FString& EventText, int32 FollowerID, const FString& CommandID, const FString& MessageID)
{
	FESCROWEventLogEntry NewEntry;
	NewEntry.RealTimestamp = BuildTimestampString();
	NewEntry.MissionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	NewEntry.EventType = EventType;
	NewEntry.EventText = EventText;
	NewEntry.FollowerID = FollowerID;
	NewEntry.CommandID = CommandID;
	NewEntry.MessageID = MessageID;

	if (const UGameInstance* OwningGameInstance = GetGameInstance())
	{
		if (const UESCROWSessionSubsystem* SessionSubsystem = OwningGameInstance->GetSubsystem<UESCROWSessionSubsystem>())
		{
			NewEntry.ParticipantID = SessionSubsystem->GetParticipantID();
			NewEntry.SessionID = SessionSubsystem->GetSessionID();
			NewEntry.MissionCondition = SessionSubsystem->GetMissionCondition();
			NewEntry.ResearcherID = SessionSubsystem->GetResearcherID();
			NewEntry.MissionID = SessionSubsystem->GetMissionID();
			NewEntry.ConditionSummary = SessionSubsystem->GetConditionSummary();
			NewEntry.TimestampSeconds = NewEntry.MissionTime - SessionSubsystem->GetSessionStartTimeSeconds();
		}
	}

	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			const UESCROWPlayerControlRouterComponent* RouterComponent = PC->FindComponentByClass<UESCROWPlayerControlRouterComponent>();
			if (!RouterComponent && PC->GetPawn())
			{
				RouterComponent = PC->GetPawn()->FindComponentByClass<UESCROWPlayerControlRouterComponent>();
			}
			if (RouterComponent)
			{
				NewEntry.PlayerMode = RouterComponent->GetPlayerMode();
			}
		}

		if (const UESCROWFleetSubsystem* FleetSubsystem = World->GetSubsystem<UESCROWFleetSubsystem>())
		{
			FESCROWFollowerStatus FollowerStatus;
			if (FollowerID != 0 && FleetSubsystem->GetFollowerStatus(FollowerID, FollowerStatus))
			{
				NewEntry.FollowerCallsign = FollowerStatus.Callsign;
			}
		}
	}

	return AddEventFull(NewEntry);
}

FESCROWEventLogEntry UESCROWEventLogSubsystem::AddEventFull(const FESCROWEventLogEntry& Entry)
{
	if (!bLoggingEnabled)
	{
		return Entry;
	}

	++EventCounter;

	FESCROWEventLogEntry StampedEntry = Entry;
	StampedEntry.EventIndex = EventCounter;
	EventEntries.Add(StampedEntry);

	return StampedEntry;
}

TArray<FESCROWEventLogEntry> UESCROWEventLogSubsystem::GetEventEntries() const
{
	return EventEntries;
}

int32 UESCROWEventLogSubsystem::GetEventCount() const
{
	return EventEntries.Num();
}

FString UESCROWEventLogSubsystem::BuildCSVHeader() const
{
	return TEXT("EventIndex,RealTimestamp,MissionTime,TimestampSeconds,ParticipantID,ResearcherID,SessionID,MissionID,MissionCondition,ConditionSummary,PlayerMode,EventType,FollowerID,FollowerCallsign,CommandID,MessageID,EventText,AdditionalData");
}

FString UESCROWEventLogSubsystem::EntryToCSVRow(const FESCROWEventLogEntry& Entry) const
{
	const UEnum* MissionConditionEnum = StaticEnum<EESCROWMissionCondition>();
	const UEnum* PlayerModeEnum = StaticEnum<EESCROWPlayerControlMode>();

	const FString MissionConditionName = MissionConditionEnum ? MissionConditionEnum->GetNameStringByValue(static_cast<int64>(Entry.MissionCondition)) : FString();
	const FString PlayerModeName = PlayerModeEnum ? PlayerModeEnum->GetNameStringByValue(static_cast<int64>(Entry.PlayerMode)) : FString();

	return FString::Printf(TEXT("%d,%s,%.3f,%.3f,%s,%s,%s,%s,%s,%s,%s,%s,%d,%s,%s,%s,%s,%s"),
		Entry.EventIndex,
		*EscapeCSVField(Entry.RealTimestamp),
		Entry.MissionTime,
		Entry.TimestampSeconds,
		*EscapeCSVField(Entry.ParticipantID),
		*EscapeCSVField(Entry.ResearcherID),
		*EscapeCSVField(Entry.SessionID),
		*EscapeCSVField(Entry.MissionID),
		*EscapeCSVField(MissionConditionName),
		*EscapeCSVField(Entry.ConditionSummary),
		*EscapeCSVField(PlayerModeName),
		*EscapeCSVField(Entry.EventType),
		Entry.FollowerID,
		*EscapeCSVField(Entry.FollowerCallsign),
		*EscapeCSVField(Entry.CommandID),
		*EscapeCSVField(Entry.MessageID),
		*EscapeCSVField(Entry.EventText),
		*EscapeCSVField(Entry.AdditionalData));
}

FString UESCROWEventLogSubsystem::BuildJSONLine(const FESCROWEventLogEntry& Entry) const
{
	const UEnum* MissionConditionEnum = StaticEnum<EESCROWMissionCondition>();
	const UEnum* PlayerModeEnum = StaticEnum<EESCROWPlayerControlMode>();

	const FString MissionConditionName = MissionConditionEnum ? MissionConditionEnum->GetNameStringByValue(static_cast<int64>(Entry.MissionCondition)) : FString();
	const FString PlayerModeName = PlayerModeEnum ? PlayerModeEnum->GetNameStringByValue(static_cast<int64>(Entry.PlayerMode)) : FString();

	return FString::Printf(TEXT("{\"EventIndex\":%d,\"RealTimestamp\":\"%s\",\"MissionTime\":%.3f,\"TimestampSeconds\":%.3f,\"ParticipantID\":\"%s\",\"ResearcherID\":\"%s\",\"SessionID\":\"%s\",\"MissionID\":\"%s\",\"MissionCondition\":\"%s\",\"ConditionSummary\":\"%s\",\"PlayerMode\":\"%s\",\"EventType\":\"%s\",\"FollowerID\":%d,\"FollowerCallsign\":\"%s\",\"CommandID\":\"%s\",\"MessageID\":\"%s\",\"EventText\":\"%s\",\"AdditionalData\":\"%s\"}"),
		Entry.EventIndex,
		*EscapeJSONField(Entry.RealTimestamp),
		Entry.MissionTime,
		Entry.TimestampSeconds,
		*EscapeJSONField(Entry.ParticipantID),
		*EscapeJSONField(Entry.ResearcherID),
		*EscapeJSONField(Entry.SessionID),
		*EscapeJSONField(Entry.MissionID),
		*EscapeJSONField(MissionConditionName),
		*EscapeJSONField(Entry.ConditionSummary),
		*EscapeJSONField(PlayerModeName),
		*EscapeJSONField(Entry.EventType),
		Entry.FollowerID,
		*EscapeJSONField(Entry.FollowerCallsign),
		*EscapeJSONField(Entry.CommandID),
		*EscapeJSONField(Entry.MessageID),
		*EscapeJSONField(Entry.EventText),
		*EscapeJSONField(Entry.AdditionalData));
}

FString UESCROWEventLogSubsystem::BuildCSVString() const
{
	TArray<FString> Lines;
	Lines.Add(BuildCSVHeader());

	for (const FESCROWEventLogEntry& Entry : EventEntries)
	{
		Lines.Add(EntryToCSVRow(Entry));
	}

	return FString::Join(Lines, TEXT("\n"));
}

FString UESCROWEventLogSubsystem::BuildJSONLString() const
{
	TArray<FString> Lines;
	for (const FESCROWEventLogEntry& Entry : EventEntries)
	{
		Lines.Add(BuildJSONLine(Entry));
	}

	return FString::Join(Lines, TEXT("\n"));
}

bool UESCROWEventLogSubsystem::ExportEventLogToFile(FString& OutResultPath, FString& OutStatusMessage)
{
	FString SessionIDForFile = TEXT("UNKNOWN_SESSION");
	if (const UGameInstance* OwningGameInstance = GetGameInstance())
	{
		if (const UESCROWSessionSubsystem* SessionSubsystem = OwningGameInstance->GetSubsystem<UESCROWSessionSubsystem>())
		{
			const FString SessionID = SessionSubsystem->GetSessionID();
			if (!SessionID.IsEmpty())
			{
				SessionIDForFile = SessionID;
			}
		}
	}

	const FString LogDir = FPaths::ProjectSavedDir() / TEXT("ESCRoW") / TEXT("Logs");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*LogDir) && !PlatformFile.CreateDirectoryTree(*LogDir))
	{
		OutResultPath.Empty();
		OutStatusMessage = FString::Printf(TEXT("Export failed: could not create log directory %s"), *LogDir);
		UE_LOG(LogTemp, Warning, TEXT("ESCROWEventLogSubsystem: %s"), *OutStatusMessage);
		return false;
	}

	const FString CSVPath = LogDir / FString::Printf(TEXT("ESCRoW_%s_events.csv"), *SessionIDForFile);
	const FString JSONLPath = LogDir / FString::Printf(TEXT("ESCRoW_%s_events.jsonl"), *SessionIDForFile);

	const bool bCSVWritten = FFileHelper::SaveStringToFile(BuildCSVString(), *CSVPath);
	const bool bJSONLWritten = FFileHelper::SaveStringToFile(BuildJSONLString(), *JSONLPath);

	if (!bCSVWritten || !bJSONLWritten)
	{
		OutResultPath.Empty();
		OutStatusMessage = FString::Printf(TEXT("Export failed: could not write log files to %s"), *LogDir);
		UE_LOG(LogTemp, Warning, TEXT("ESCROWEventLogSubsystem: %s"), *OutStatusMessage);
		return false;
	}

	CurrentLogFilePath = CSVPath;
	OutResultPath = CSVPath;
	OutStatusMessage = FString::Printf(TEXT("Export complete: %s"), *CSVPath);
	return true;
}

FString UESCROWEventLogSubsystem::GetCurrentLogFilePath() const
{
	return CurrentLogFilePath;
}

FString UESCROWEventLogSubsystem::GetLogSummary() const
{
	if (CurrentLogFilePath.IsEmpty())
	{
		return FString::Printf(TEXT("%d events logged. Not yet exported."), EventEntries.Num());
	}

	return FString::Printf(TEXT("%d events logged. Last export: %s"), EventEntries.Num(), *CurrentLogFilePath);
}

FString UESCROWEventLogSubsystem::BuildTimestampString() const
{
	return FDateTime::Now().ToIso8601();
}

FString UESCROWEventLogSubsystem::EscapeCSVField(const FString& Field)
{
	FString Escaped = Field;
	Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""));

	if (Escaped.Contains(TEXT(",")) || Escaped.Contains(TEXT("\"")) || Escaped.Contains(TEXT("\n")))
	{
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}

	return Escaped;
}

FString UESCROWEventLogSubsystem::EscapeJSONField(const FString& Field)
{
	FString Escaped = Field;
	Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
	Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
	return Escaped;
}

FString UESCROWEventLogSubsystem::EscapeJSONStringManual(const FString& Field)
{
	return EscapeJSONField(Field);
}

// =============================== RA41A: data-manager run lifecycle ===============================

bool UESCROWEventLogSubsystem::EnsureRunFolder()
{
	if (CurrentRunFolder.IsEmpty())
	{
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.DirectoryExists(*CurrentRunFolder))
	{
		return true;
	}

	if (!PlatformFile.CreateDirectoryTree(*CurrentRunFolder))
	{
		UE_LOG(LogTemp, Warning, TEXT("ESCROWEventLogSubsystem: could not create run folder %s"), *CurrentRunFolder);
		return false;
	}

	return true;
}

bool UESCROWEventLogSubsystem::WriteWholeFile(const FString& FileName, const FString& Content) const
{
	if (CurrentRunFolder.IsEmpty())
	{
		return false;
	}

	const FString FullPath = CurrentRunFolder / FileName;
	if (!FFileHelper::SaveStringToFile(Content, *FullPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("ESCROWEventLogSubsystem: failed to write %s"), *FullPath);
		return false;
	}

	return true;
}

bool UESCROWEventLogSubsystem::AppendLineToFile(const FString& FileName, const FString& Line) const
{
	if (CurrentRunFolder.IsEmpty())
	{
		return false;
	}

	const FString FullPath = CurrentRunFolder / FileName;
	const FString LineWithBreak = Line + TEXT("\r\n");
	if (!FFileHelper::SaveStringToFile(LineWithBreak, *FullPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append))
	{
		UE_LOG(LogTemp, Warning, TEXT("ESCROWEventLogSubsystem: failed to append to %s"), *FullPath);
		return false;
	}

	return true;
}

FString UESCROWEventLogSubsystem::NextRecordID(const TCHAR* Prefix)
{
	++RunConfigCounterForRecordIDs;
	return FString::Printf(TEXT("%s_%s_%05d"), Prefix, *CurrentRunID, RunConfigCounterForRecordIDs);
}

FString UESCROWEventLogSubsystem::GetCurrentPhaseFromScenarioDirector() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const UESCROWScenarioDirectorSubsystem* Director = World->GetSubsystem<UESCROWScenarioDirectorSubsystem>())
		{
			return Director->CurrentScenarioStep;
		}
	}

	return TEXT("UNSET");
}

FString UESCROWEventLogSubsystem::BuildRunConfigJSON(const FESCROWRunConfig& Config) const
{
	TArray<FString> Lines;
	Lines.Add(TEXT("{"));
	Lines.Add(FString::Printf(TEXT("  \"RunID\": \"%s\","), *EscapeJSONField(Config.RunID)));
	Lines.Add(FString::Printf(TEXT("  \"SessionID\": \"%s\","), *EscapeJSONField(Config.SessionID)));
	Lines.Add(FString::Printf(TEXT("  \"ParticipantID\": \"%s\","), *EscapeJSONField(Config.ParticipantID)));
	Lines.Add(FString::Printf(TEXT("  \"ResearcherID\": \"%s\","), *EscapeJSONField(Config.ResearcherID)));
	Lines.Add(FString::Printf(TEXT("  \"ScenarioID\": \"%s\","), *EscapeJSONField(Config.ScenarioID)));
	Lines.Add(FString::Printf(TEXT("  \"ScenarioVersion\": \"%s\","), *EscapeJSONField(Config.ScenarioVersion)));
	Lines.Add(FString::Printf(TEXT("  \"ConditionID\": \"%s\","), *EscapeJSONField(Config.ConditionID)));
	Lines.Add(FString::Printf(TEXT("  \"SupportMode\": \"%s\","), *EscapeJSONField(Config.SupportMode)));
	Lines.Add(FString::Printf(TEXT("  \"DisplayMode\": \"%s\","), *EscapeJSONField(Config.DisplayMode)));
	Lines.Add(FString::Printf(TEXT("  \"VehicleControlRole\": \"%s\","), *EscapeJSONField(Config.VehicleControlRole)));
	Lines.Add(FString::Printf(TEXT("  \"WorkloadLevel\": \"%s\","), *EscapeJSONField(Config.WorkloadLevel)));
	Lines.Add(FString::Printf(TEXT("  \"ScenarioVariant\": \"%s\","), *EscapeJSONField(Config.ScenarioVariant)));
	Lines.Add(FString::Printf(TEXT("  \"EventSalience\": \"%s\","), *EscapeJSONField(Config.EventSalience)));
	Lines.Add(FString::Printf(TEXT("  \"UncertaintyLevel\": \"%s\","), *EscapeJSONField(Config.UncertaintyLevel)));
	Lines.Add(FString::Printf(TEXT("  \"RunSeed\": %d,"), Config.RunSeed));
	Lines.Add(FString::Printf(TEXT("  \"MapName\": \"%s\","), *EscapeJSONField(Config.MapName)));
	Lines.Add(FString::Printf(TEXT("  \"AppVersionOrBuild\": \"%s\","), *EscapeJSONField(Config.AppVersionOrBuild)));
	Lines.Add(FString::Printf(TEXT("  \"GitCommitOrBuildLabel\": \"%s\","), *EscapeJSONField(Config.GitCommitOrBuildLabel)));
	Lines.Add(FString::Printf(TEXT("  \"CreatedAt\": \"%s\","), *Config.CreatedAt.ToIso8601()));
	Lines.Add(FString::Printf(TEXT("  \"Notes\": \"%s\","), *EscapeJSONField(Config.Notes)));
	Lines.Add(FString::Printf(TEXT("  \"RunValidity\": \"%s\""), *EscapeJSONField(Config.RunValidity.IsEmpty() ? TEXT("Valid") : *Config.RunValidity)));
	Lines.Add(TEXT("}"));

	return FString::Join(Lines, TEXT("\n"));
}

FString UESCROWEventLogSubsystem::BuildRunSummaryJSON() const
{
	TArray<FString> Lines;
	Lines.Add(TEXT("{"));
	Lines.Add(FString::Printf(TEXT("  \"RunID\": \"%s\","), *EscapeJSONField(CurrentRunID)));
	Lines.Add(FString::Printf(TEXT("  \"RunFolder\": \"%s\","), *EscapeJSONField(CurrentRunFolder)));
	Lines.Add(FString::Printf(TEXT("  \"bRunActive\": %s,"), bRunActive ? TEXT("true") : TEXT("false")));
	Lines.Add(FString::Printf(TEXT("  \"GeneratedAt\": \"%s\","), *FDateTime::Now().ToIso8601()));
	Lines.Add(FString::Printf(TEXT("  \"ScenarioEventCount\": %d,"), ScenarioEventCount));
	Lines.Add(FString::Printf(TEXT("  \"ParticipantActionCount\": %d,"), ParticipantActionCount));
	Lines.Add(FString::Printf(TEXT("  \"UIExposureCount\": %d,"), UIExposureCount));
	Lines.Add(FString::Printf(TEXT("  \"HeartbeatCount\": %d,"), HeartbeatCount));
	Lines.Add(FString::Printf(TEXT("  \"StateTransitionCount\": %d,"), StateTransitionCount));
	Lines.Add(FString::Printf(TEXT("  \"ReportRecordCount\": %d"), ReportRecordCount));
	Lines.Add(TEXT("}"));

	return FString::Join(Lines, TEXT("\n"));
}

bool UESCROWEventLogSubsystem::BeginRunFromConfig(const FESCROWRunConfig& Config)
{
	if (bRunActive)
	{
		EndRun(TEXT("SupersededByNewRun"));
	}

	CurrentRunID = Config.RunID.IsEmpty() ? FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) : Config.RunID;
	CurrentRunRootDirectory = FPaths::Combine(FString(FPlatformProcess::UserDir()), TEXT("ESCRoW_Data"));
	CurrentRunFolder = CurrentRunRootDirectory / CurrentRunID;

	ScenarioEventCount = 0;
	ParticipantActionCount = 0;
	UIExposureCount = 0;
	HeartbeatCount = 0;
	StateTransitionCount = 0;
	ReportRecordCount = 0;
	RunConfigCounterForRecordIDs = 0;

	if (!EnsureRunFolder())
	{
		bRunActive = false;
		return false;
	}

	bRunActive = true;

	WriteSessionManifest(Config);
	InitializeOutputFiles();

	UE_LOG(LogTemp, Log, TEXT("ESCROWEventLogSubsystem: run %s started at %s"), *CurrentRunID, *CurrentRunFolder);
	return true;
}

void UESCROWEventLogSubsystem::EndRun(const FString& EndReason)
{
	if (!bRunActive)
	{
		return;
	}

	StopHeartbeat();
	FlushAllOutputs();

	const FString StatusLine = FString::Printf(TEXT("Run ended at %s. Reason: %s"), *FDateTime::Now().ToIso8601(), *EndReason);
	AppendLineToFile(TEXT("export_status.txt"), StatusLine);

	bRunActive = false;
}

FString UESCROWEventLogSubsystem::GetCurrentRunFolder() const
{
	return CurrentRunFolder;
}

bool UESCROWEventLogSubsystem::GetRunActive() const
{
	return bRunActive;
}

void UESCROWEventLogSubsystem::GetRunRecordCounts(int32& OutScenarioEvents, int32& OutParticipantActions, int32& OutUIExposures, int32& OutHeartbeats, int32& OutStateTransitions, int32& OutReportRecords) const
{
	OutScenarioEvents = ScenarioEventCount;
	OutParticipantActions = ParticipantActionCount;
	OutUIExposures = UIExposureCount;
	OutHeartbeats = HeartbeatCount;
	OutStateTransitions = StateTransitionCount;
	OutReportRecords = ReportRecordCount;
}

bool UESCROWEventLogSubsystem::WriteSessionManifest(const FESCROWRunConfig& Config)
{
	if (!EnsureRunFolder())
	{
		return false;
	}

	return WriteWholeFile(TEXT("session_manifest.json"), BuildRunConfigJSON(Config));
}

bool UESCROWEventLogSubsystem::InitializeOutputFiles()
{
	if (!EnsureRunFolder())
	{
		return false;
	}

	bool bAllOk = true;

	bAllOk &= WriteWholeFile(TEXT("scenario_events.jsonl"), TEXT(""));
	bAllOk &= WriteWholeFile(TEXT("participant_actions.jsonl"), TEXT(""));
	bAllOk &= WriteWholeFile(TEXT("ui_exposure.jsonl"), TEXT(""));
	bAllOk &= WriteWholeFile(TEXT("event_log_raw.jsonl"), TEXT(""));

	bAllOk &= WriteWholeFile(TEXT("fleet_heartbeat.csv"), TEXT("RecordID,RunID,TimestampISO,WorldSeconds,ScenarioTimeSeconds,Phase,ActivePage,SelectedFollowerID,TaskingID,TaskingState,PlanValidity,CVLocation,F1Location,F1SpeedCms,F2Location,F2SpeedCms,F3Location,F3SpeedCms,F4Location,F4SpeedCms,PayloadJSON\r\n"));
	bAllOk &= WriteWholeFile(TEXT("state_transitions.csv"), TEXT("RecordID,RunID,TimestampISO,WorldSeconds,ScenarioTimeSeconds,EntityID,EntityType,StateName,OldValue,NewValue,Source,RelatedEventID,SourceObject,EventID,PayloadJSON\r\n"));
	bAllOk &= WriteWholeFile(TEXT("reports.csv"), TEXT("RecordID,RunID,TimestampISO,WorldSeconds,ScenarioTimeSeconds,ReportID,ReportType,FieldName,FieldValue,GroundTruthValue,Score,SourceSystem,Phase,EventID,PayloadJSON\r\n"));

	bAllOk &= WriteWholeFile(TEXT("run_summary.json"), BuildRunSummaryJSON());

	const FString StatusText = FString::Printf(TEXT("Run created at %s\r\nFolder: %s\r\n"), *FDateTime::Now().ToIso8601(), *CurrentRunFolder);
	bAllOk &= WriteWholeFile(TEXT("export_status.txt"), StatusText);

	return bAllOk;
}

bool UESCROWEventLogSubsystem::FlushAllOutputs()
{
	if (CurrentRunFolder.IsEmpty())
	{
		return false;
	}

	bool bAllOk = true;
	bAllOk &= WriteWholeFile(TEXT("run_summary.json"), BuildRunSummaryJSON());

	const FString StatusText = FString::Printf(TEXT("Last flushed at %s\r\nFolder: %s\r\nActive: %s\r\n"), *FDateTime::Now().ToIso8601(), *CurrentRunFolder, bRunActive ? TEXT("true") : TEXT("false"));
	bAllOk &= WriteWholeFile(TEXT("export_status.txt"), StatusText);

	return bAllOk;
}

FString UESCROWEventLogSubsystem::ExportRunSummary()
{
	FlushAllOutputs();

	return FString::Printf(TEXT("Run %s: %d scenario events, %d actions, %d UI exposures, %d heartbeats, %d transitions, %d report fields | Folder: %s"),
		*CurrentRunID, ScenarioEventCount, ParticipantActionCount, UIExposureCount, HeartbeatCount, StateTransitionCount, ReportRecordCount, *CurrentRunFolder);
}

// =============================== RA41A: structured logging API ===============================

bool UESCROWEventLogSubsystem::RecordScenarioEvent(const FESCROWScenarioEventRecord& Record)
{
	if (!bRunActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("ESCROWEventLogSubsystem::RecordScenarioEvent called with no active run; dropped."));
		return false;
	}

	FESCROWScenarioEventRecord R = Record;
	if (R.RecordID.IsEmpty()) { R.RecordID = NextRecordID(TEXT("SE")); }
	if (R.RunID.IsEmpty()) { R.RunID = CurrentRunID; }
	if (R.Timestamp.GetTicks() == 0) { R.Timestamp = FDateTime::Now(); }
	if (R.WorldSeconds <= 0.0f && GetWorld()) { R.WorldSeconds = GetWorld()->GetTimeSeconds(); }
	if (R.ScenarioTimeSeconds < 0.0f) { R.ScenarioTimeSeconds = R.WorldSeconds; }
	if (R.SourceSystem.IsEmpty()) { R.SourceSystem = TEXT("ScenarioDirector"); }
	if (R.EventID.IsEmpty()) { R.EventID = R.RecordID; }

	++ScenarioEventCount;

	// Split into two Printf calls: UE5.8's compile-time format-string sanitizer
	// hits an internal compiler limitation on a single Printf with this many
	// mixed-type args (confirmed via a real build failure on a similar 21-arg
	// call elsewhere in this file - see RecordHeartbeat).
	const FString JsonLine = FString::Printf(TEXT("{\"RecordFamily\":\"ScenarioEvent\",\"RecordID\":\"%s\",\"RunID\":\"%s\",\"TimestampISO\":\"%s\",\"WorldSeconds\":%.3f,\"ScenarioTimeSeconds\":%.3f,\"Phase\":\"%s\",\"EventType\":\"%s\",\"EventLabel\":\"%s\","),
		*EscapeJSONField(R.RecordID), *EscapeJSONField(R.RunID), *R.Timestamp.ToIso8601(), R.WorldSeconds, R.ScenarioTimeSeconds,
		*EscapeJSONField(R.Phase), *EscapeJSONField(R.EventType), *EscapeJSONField(R.EventLabel))
		+ FString::Printf(TEXT("\"TaskingID\":\"%s\",\"AffectedFollowerID\":%d,\"PlanValidity\":\"%s\",\"SourceSystem\":\"%s\",\"SourceObject\":\"%s\",\"EventID\":\"%s\",\"RelatedEventID\":\"%s\",\"PayloadJSON\":\"%s\"}"),
		*EscapeJSONField(R.TaskingID), R.AffectedFollowerID, *EscapeJSONField(R.PlanValidity), *EscapeJSONField(R.SourceSystem),
		*EscapeJSONField(R.SourceObject), *EscapeJSONField(R.EventID), *EscapeJSONField(R.RelatedEventID), *EscapeJSONField(R.PayloadJSON));

	AppendLineToFile(TEXT("scenario_events.jsonl"), JsonLine);
	AppendLineToFile(TEXT("event_log_raw.jsonl"), JsonLine);
	return true;
}

bool UESCROWEventLogSubsystem::RecordScenarioEventSimple(const FString& Phase, const FString& EventType, const FString& EventLabel, const FString& TaskingID, int32 AffectedFollowerID, const FString& PlanValidity, const FString& PayloadJSON)
{
	FESCROWScenarioEventRecord R;
	R.Phase = Phase;
	R.EventType = EventType;
	R.EventLabel = EventLabel;
	R.TaskingID = TaskingID;
	R.AffectedFollowerID = AffectedFollowerID;
	R.PlanValidity = PlanValidity;
	R.PayloadJSON = PayloadJSON;
	return RecordScenarioEvent(R);
}

bool UESCROWEventLogSubsystem::RecordParticipantAction(const FESCROWParticipantActionRecord& Record)
{
	if (!bRunActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("ESCROWEventLogSubsystem::RecordParticipantAction called with no active run; dropped."));
		return false;
	}

	FESCROWParticipantActionRecord R = Record;
	if (R.RecordID.IsEmpty()) { R.RecordID = NextRecordID(TEXT("PA")); }
	if (R.RunID.IsEmpty()) { R.RunID = CurrentRunID; }
	if (R.Timestamp.GetTicks() == 0) { R.Timestamp = FDateTime::Now(); }
	if (R.WorldSeconds <= 0.0f && GetWorld()) { R.WorldSeconds = GetWorld()->GetTimeSeconds(); }
	if (R.ScenarioTimeSeconds < 0.0f) { R.ScenarioTimeSeconds = R.WorldSeconds; }
	if (R.Source.IsEmpty()) { R.Source = TEXT("CommandHUD"); }
	if (R.EventID.IsEmpty()) { R.EventID = R.RecordID; }

	++ParticipantActionCount;

	const FString JsonLine = FString::Printf(TEXT("{\"RecordFamily\":\"ParticipantAction\",\"RecordID\":\"%s\",\"RunID\":\"%s\",\"TimestampISO\":\"%s\",\"WorldSeconds\":%.3f,\"ScenarioTimeSeconds\":%.3f,\"ActionType\":\"%s\",\"SourceSystem\":\"%s\",\"SourceObject\":\"%s\","),
		*EscapeJSONField(R.RecordID), *EscapeJSONField(R.RunID), *R.Timestamp.ToIso8601(), R.WorldSeconds, R.ScenarioTimeSeconds,
		*EscapeJSONField(R.ActionType), *EscapeJSONField(R.Source), *EscapeJSONField(R.SourceObject))
		+ FString::Printf(TEXT("\"Phase\":\"%s\",\"ActivePage\":\"%s\",\"SelectedFollowerID\":%d,\"TargetFollowerID\":%d,\"TaskingID\":\"%s\",\"EventID\":\"%s\",\"RelatedEventID\":\"%s\",\"PayloadJSON\":\"%s\"}"),
		*EscapeJSONField(R.Phase), *EscapeJSONField(R.ActivePage), R.SelectedFollowerID, R.TargetFollowerID,
		*EscapeJSONField(R.TaskingID), *EscapeJSONField(R.EventID), *EscapeJSONField(R.RelatedEventID), *EscapeJSONField(R.PayloadJSON));

	AppendLineToFile(TEXT("participant_actions.jsonl"), JsonLine);
	AppendLineToFile(TEXT("event_log_raw.jsonl"), JsonLine);
	return true;
}

bool UESCROWEventLogSubsystem::RecordParticipantActionSimple(const FString& ActionType, const FString& Source, const FString& ActivePage, int32 SelectedFollowerID, int32 TargetFollowerID, const FString& TaskingID, const FString& EventID, const FString& PayloadJSON)
{
	FESCROWParticipantActionRecord R;
	R.ActionType = ActionType;
	R.Source = Source;
	R.ActivePage = ActivePage;
	R.SelectedFollowerID = SelectedFollowerID;
	R.TargetFollowerID = TargetFollowerID;
	R.TaskingID = TaskingID;
	R.EventID = EventID;
	R.PayloadJSON = PayloadJSON;
	return RecordParticipantAction(R);
}

bool UESCROWEventLogSubsystem::RecordUIExposure(const FESCROWUIExposureRecord& Record)
{
	if (!bRunActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("ESCROWEventLogSubsystem::RecordUIExposure called with no active run; dropped."));
		return false;
	}

	FESCROWUIExposureRecord R = Record;
	if (R.RecordID.IsEmpty()) { R.RecordID = NextRecordID(TEXT("UX")); }
	if (R.RunID.IsEmpty()) { R.RunID = CurrentRunID; }
	if (R.Timestamp.GetTicks() == 0) { R.Timestamp = FDateTime::Now(); }
	if (R.WorldSeconds <= 0.0f && GetWorld()) { R.WorldSeconds = GetWorld()->GetTimeSeconds(); }
	if (R.ScenarioTimeSeconds < 0.0f) { R.ScenarioTimeSeconds = R.WorldSeconds; }
	if (R.SourceSystem.IsEmpty()) { R.SourceSystem = TEXT("CommandHUD"); }
	if (R.EventID.IsEmpty()) { R.EventID = R.RecordID; }
	if (R.Phase.IsEmpty()) { R.Phase = GetCurrentPhaseFromScenarioDirector(); }

	++UIExposureCount;

	const FString JsonLine = FString::Printf(TEXT("{\"RecordFamily\":\"UIExposure\",\"RecordID\":\"%s\",\"RunID\":\"%s\",\"TimestampISO\":\"%s\",\"WorldSeconds\":%.3f,\"ScenarioTimeSeconds\":%.3f,\"Phase\":\"%s\",\"PageName\":\"%s\",\"WidgetName\":\"%s\","),
		*EscapeJSONField(R.RecordID), *EscapeJSONField(R.RunID), *R.Timestamp.ToIso8601(), R.WorldSeconds, R.ScenarioTimeSeconds, *EscapeJSONField(R.Phase),
		*EscapeJSONField(R.PageName), *EscapeJSONField(R.WidgetName))
		+ FString::Printf(TEXT("\"SelectedFollowerID\":%d,\"VisibleFields\":\"%s\",\"TaskingID\":\"%s\",\"SourceSystem\":\"%s\",\"SourceObject\":\"%s\",\"EventID\":\"%s\",\"RelatedEventID\":\"%s\",\"bDiagnosticEvidenceAvailable\":%s,\"PayloadJSON\":\"%s\"}"),
		R.SelectedFollowerID, *EscapeJSONField(R.VisibleFields), *EscapeJSONField(R.TaskingID),
		*EscapeJSONField(R.SourceSystem), *EscapeJSONField(R.SourceObject), *EscapeJSONField(R.EventID),
		*EscapeJSONField(R.RelatedEventID), R.bDiagnosticEvidenceAvailable ? TEXT("true") : TEXT("false"), *EscapeJSONField(R.PayloadJSON));

	AppendLineToFile(TEXT("ui_exposure.jsonl"), JsonLine);
	AppendLineToFile(TEXT("event_log_raw.jsonl"), JsonLine);
	return true;
}

bool UESCROWEventLogSubsystem::RecordUIExposureSimple(const FString& PageName, const FString& WidgetName, int32 SelectedFollowerID, const FString& VisibleFields, const FString& RelatedEventID, bool bDiagnosticEvidenceAvailable, const FString& PayloadJSON)
{
	FESCROWUIExposureRecord R;
	R.PageName = PageName;
	R.WidgetName = WidgetName;
	R.SelectedFollowerID = SelectedFollowerID;
	R.VisibleFields = VisibleFields;
	R.RelatedEventID = RelatedEventID;
	R.bDiagnosticEvidenceAvailable = bDiagnosticEvidenceAvailable;
	R.PayloadJSON = PayloadJSON;
	return RecordUIExposure(R);
}

bool UESCROWEventLogSubsystem::RecordHeartbeat(const FESCROWHeartbeatRecord& Record)
{
	if (!bRunActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("ESCROWEventLogSubsystem::RecordHeartbeat called with no active run; dropped."));
		return false;
	}

	FESCROWHeartbeatRecord R = Record;
	if (R.RecordID.IsEmpty()) { R.RecordID = NextRecordID(TEXT("HB")); }
	if (R.RunID.IsEmpty()) { R.RunID = CurrentRunID; }
	if (R.Timestamp.GetTicks() == 0) { R.Timestamp = FDateTime::Now(); }
	if (R.WorldSeconds <= 0.0f && GetWorld()) { R.WorldSeconds = GetWorld()->GetTimeSeconds(); }
	if (R.ScenarioTimeSeconds < 0.0f) { R.ScenarioTimeSeconds = R.WorldSeconds; }
	if (R.SourceSystem.IsEmpty()) { R.SourceSystem = TEXT("EventLog"); }
	if (R.EventID.IsEmpty()) { R.EventID = R.RecordID; }

	PopulateFleetTelemetry(R);

	++HeartbeatCount;

	// Split into several Printf calls: UE5.8's compile-time format-string
	// sanitizer hits an internal compiler error ("call to immediate function is
	// not a constant expression") on a single Printf with ~20+ mixed-type args -
	// confirmed via a real build failure on this exact line before splitting.
	const FString CsvRow = FString::Printf(TEXT("%s,%s,%s,%.3f,%.3f,%s,%s,"),
		*EscapeCSVField(R.RecordID), *EscapeCSVField(R.RunID), *EscapeCSVField(R.Timestamp.ToIso8601()), R.WorldSeconds, R.ScenarioTimeSeconds,
		*EscapeCSVField(R.Phase), *EscapeCSVField(R.ActivePage))
		+ FString::Printf(TEXT("%d,%s,%s,%s,%s,%s,%.1f,"),
		R.SelectedFollowerID, *EscapeCSVField(R.TaskingID), *EscapeCSVField(R.TaskingState), *EscapeCSVField(R.PlanValidity),
		*EscapeCSVField(R.CVLocation), *EscapeCSVField(R.F1Location), R.F1SpeedCms)
		+ FString::Printf(TEXT("%s,%.1f,%s,%.1f,%s,%.1f,%s"),
		*EscapeCSVField(R.F2Location), R.F2SpeedCms, *EscapeCSVField(R.F3Location), R.F3SpeedCms,
		*EscapeCSVField(R.F4Location), R.F4SpeedCms, *EscapeCSVField(R.PayloadJSON));
	AppendLineToFile(TEXT("fleet_heartbeat.csv"), CsvRow);

	const FString JsonLine = FString::Printf(TEXT("{\"RecordFamily\":\"Heartbeat\",\"RecordID\":\"%s\",\"RunID\":\"%s\",\"TimestampISO\":\"%s\",\"WorldSeconds\":%.3f,\"ScenarioTimeSeconds\":%.3f,\"Phase\":\"%s\",\"ActivePage\":\"%s\","),
		*EscapeJSONField(R.RecordID), *EscapeJSONField(R.RunID), *R.Timestamp.ToIso8601(), R.WorldSeconds, R.ScenarioTimeSeconds,
		*EscapeJSONField(R.Phase), *EscapeJSONField(R.ActivePage))
		+ FString::Printf(TEXT("\"SelectedFollowerID\":%d,\"TaskingID\":\"%s\",\"TaskingState\":\"%s\",\"PlanValidity\":\"%s\",\"SourceSystem\":\"%s\",\"EventID\":\"%s\",\"CVLocation\":\"%s\","),
		R.SelectedFollowerID, *EscapeJSONField(R.TaskingID), *EscapeJSONField(R.TaskingState), *EscapeJSONField(R.PlanValidity),
		*EscapeJSONField(R.SourceSystem), *EscapeJSONField(R.EventID), *EscapeJSONField(R.CVLocation))
		+ FString::Printf(TEXT("\"F1Location\":\"%s\",\"F1SpeedCms\":%.1f,\"F2Location\":\"%s\",\"F2SpeedCms\":%.1f,"),
		*EscapeJSONField(R.F1Location), R.F1SpeedCms, *EscapeJSONField(R.F2Location), R.F2SpeedCms)
		+ FString::Printf(TEXT("\"F3Location\":\"%s\",\"F3SpeedCms\":%.1f,\"F4Location\":\"%s\",\"F4SpeedCms\":%.1f,\"PayloadJSON\":\"%s\"}"),
		*EscapeJSONField(R.F3Location), R.F3SpeedCms, *EscapeJSONField(R.F4Location), R.F4SpeedCms, *EscapeJSONField(R.PayloadJSON));
	AppendLineToFile(TEXT("event_log_raw.jsonl"), JsonLine);

	return true;
}

bool UESCROWEventLogSubsystem::RecordHeartbeatSimple(const FString& Phase, const FString& ActivePage, int32 SelectedFollowerID, const FString& TaskingID, const FString& TaskingState, const FString& PlanValidity, const FString& PayloadJSON)
{
	FESCROWHeartbeatRecord R;
	R.Phase = Phase;
	R.ActivePage = ActivePage;
	R.SelectedFollowerID = SelectedFollowerID;
	R.TaskingID = TaskingID;
	R.TaskingState = TaskingState;
	R.PlanValidity = PlanValidity;
	R.PayloadJSON = PayloadJSON;
	return RecordHeartbeat(R);
}

bool UESCROWEventLogSubsystem::RecordStateTransition(const FESCROWStateTransitionRecord& Record)
{
	if (!bRunActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("ESCROWEventLogSubsystem::RecordStateTransition called with no active run; dropped."));
		return false;
	}

	FESCROWStateTransitionRecord R = Record;
	if (R.RecordID.IsEmpty()) { R.RecordID = NextRecordID(TEXT("ST")); }
	if (R.RunID.IsEmpty()) { R.RunID = CurrentRunID; }
	if (R.Timestamp.GetTicks() == 0) { R.Timestamp = FDateTime::Now(); }
	if (R.WorldSeconds <= 0.0f && GetWorld()) { R.WorldSeconds = GetWorld()->GetTimeSeconds(); }
	if (R.ScenarioTimeSeconds < 0.0f) { R.ScenarioTimeSeconds = R.WorldSeconds; }
	if (R.Source.IsEmpty()) { R.Source = TEXT("ScenarioDirector"); }
	if (R.EventID.IsEmpty()) { R.EventID = R.RecordID; }

	++StateTransitionCount;

	const FString CsvRow = FString::Printf(TEXT("%s,%s,%s,%.3f,%.3f,%s,%s,%s,"),
		*EscapeCSVField(R.RecordID), *EscapeCSVField(R.RunID), *EscapeCSVField(R.Timestamp.ToIso8601()), R.WorldSeconds, R.ScenarioTimeSeconds,
		*EscapeCSVField(R.EntityID), *EscapeCSVField(R.EntityType), *EscapeCSVField(R.StateName))
		+ FString::Printf(TEXT("%s,%s,%s,%s,%s,%s,%s"),
		*EscapeCSVField(R.OldValue), *EscapeCSVField(R.NewValue), *EscapeCSVField(R.Source), *EscapeCSVField(R.RelatedEventID),
		*EscapeCSVField(R.SourceObject), *EscapeCSVField(R.EventID), *EscapeCSVField(R.PayloadJSON));
	AppendLineToFile(TEXT("state_transitions.csv"), CsvRow);

	const FString JsonLine = FString::Printf(TEXT("{\"RecordFamily\":\"StateTransition\",\"RecordID\":\"%s\",\"RunID\":\"%s\",\"TimestampISO\":\"%s\",\"WorldSeconds\":%.3f,\"ScenarioTimeSeconds\":%.3f,\"EntityID\":\"%s\",\"EntityType\":\"%s\",\"StateName\":\"%s\","),
		*EscapeJSONField(R.RecordID), *EscapeJSONField(R.RunID), *R.Timestamp.ToIso8601(), R.WorldSeconds, R.ScenarioTimeSeconds,
		*EscapeJSONField(R.EntityID), *EscapeJSONField(R.EntityType), *EscapeJSONField(R.StateName))
		+ FString::Printf(TEXT("\"OldValue\":\"%s\",\"NewValue\":\"%s\",\"SourceSystem\":\"%s\",\"SourceObject\":\"%s\",\"EventID\":\"%s\",\"RelatedEventID\":\"%s\",\"PayloadJSON\":\"%s\"}"),
		*EscapeJSONField(R.OldValue), *EscapeJSONField(R.NewValue), *EscapeJSONField(R.Source), *EscapeJSONField(R.SourceObject),
		*EscapeJSONField(R.EventID), *EscapeJSONField(R.RelatedEventID), *EscapeJSONField(R.PayloadJSON));
	AppendLineToFile(TEXT("event_log_raw.jsonl"), JsonLine);

	return true;
}

bool UESCROWEventLogSubsystem::RecordStateTransitionSimple(const FString& EntityID, const FString& EntityType, const FString& StateName, const FString& OldValue, const FString& NewValue, const FString& Source, const FString& RelatedEventID)
{
	FESCROWStateTransitionRecord R;
	R.EntityID = EntityID;
	R.EntityType = EntityType;
	R.StateName = StateName;
	R.OldValue = OldValue;
	R.NewValue = NewValue;
	R.Source = Source;
	R.RelatedEventID = RelatedEventID;
	return RecordStateTransition(R);
}

bool UESCROWEventLogSubsystem::RecordReportField(const FESCROWReportRecord& Record)
{
	if (!bRunActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("ESCROWEventLogSubsystem::RecordReportField called with no active run; dropped."));
		return false;
	}

	FESCROWReportRecord R = Record;
	if (R.RecordID.IsEmpty()) { R.RecordID = NextRecordID(TEXT("RP")); }
	if (R.RunID.IsEmpty()) { R.RunID = CurrentRunID; }
	if (R.Timestamp.GetTicks() == 0) { R.Timestamp = FDateTime::Now(); }
	if (R.WorldSeconds <= 0.0f && GetWorld()) { R.WorldSeconds = GetWorld()->GetTimeSeconds(); }
	if (R.ScenarioTimeSeconds < 0.0f) { R.ScenarioTimeSeconds = R.WorldSeconds; }
	if (R.SourceSystem.IsEmpty()) { R.SourceSystem = TEXT("CommandHUD"); }
	if (R.EventID.IsEmpty()) { R.EventID = R.RecordID; }
	if (R.Phase.IsEmpty()) { R.Phase = GetCurrentPhaseFromScenarioDirector(); }

	++ReportRecordCount;

	const FString CsvRow = FString::Printf(TEXT("%s,%s,%s,%.3f,%.3f,%s,%s,%s,"),
		*EscapeCSVField(R.RecordID), *EscapeCSVField(R.RunID), *EscapeCSVField(R.Timestamp.ToIso8601()), R.WorldSeconds, R.ScenarioTimeSeconds,
		*EscapeCSVField(R.ReportID), *EscapeCSVField(R.ReportType), *EscapeCSVField(R.FieldName))
		+ FString::Printf(TEXT("%s,%s,%d,%s,%s,%s,%s"),
		*EscapeCSVField(R.FieldValue), *EscapeCSVField(R.GroundTruthValue), R.Score,
		*EscapeCSVField(R.SourceSystem), *EscapeCSVField(R.Phase), *EscapeCSVField(R.EventID), *EscapeCSVField(R.PayloadJSON));
	AppendLineToFile(TEXT("reports.csv"), CsvRow);

	const FString JsonLine = FString::Printf(TEXT("{\"RecordFamily\":\"ReportField\",\"RecordID\":\"%s\",\"RunID\":\"%s\",\"TimestampISO\":\"%s\",\"WorldSeconds\":%.3f,\"ScenarioTimeSeconds\":%.3f,\"ReportID\":\"%s\",\"ReportType\":\"%s\",\"FieldName\":\"%s\","),
		*EscapeJSONField(R.RecordID), *EscapeJSONField(R.RunID), *R.Timestamp.ToIso8601(), R.WorldSeconds, R.ScenarioTimeSeconds,
		*EscapeJSONField(R.ReportID), *EscapeJSONField(R.ReportType), *EscapeJSONField(R.FieldName))
		+ FString::Printf(TEXT("\"FieldValue\":\"%s\",\"GroundTruthValue\":\"%s\",\"Score\":%d,\"SourceSystem\":\"%s\",\"SourceObject\":\"%s\","),
		*EscapeJSONField(R.FieldValue), *EscapeJSONField(R.GroundTruthValue), R.Score,
		*EscapeJSONField(R.SourceSystem), *EscapeJSONField(R.SourceObject))
		+ FString::Printf(TEXT("\"Phase\":\"%s\",\"EventID\":\"%s\",\"RelatedEventID\":\"%s\",\"PayloadJSON\":\"%s\"}"),
		*EscapeJSONField(R.Phase), *EscapeJSONField(R.EventID), *EscapeJSONField(R.RelatedEventID), *EscapeJSONField(R.PayloadJSON));
	AppendLineToFile(TEXT("event_log_raw.jsonl"), JsonLine);

	return true;
}

bool UESCROWEventLogSubsystem::RecordReportFieldSimple(const FString& ReportID, const FString& ReportType, const FString& FieldName, const FString& FieldValue, const FString& GroundTruthValue, int32 Score, const FString& PayloadJSON)
{
	FESCROWReportRecord R;
	R.ReportID = ReportID;
	R.ReportType = ReportType;
	R.FieldName = FieldName;
	R.FieldValue = FieldValue;
	R.GroundTruthValue = GroundTruthValue;
	R.Score = Score;
	R.PayloadJSON = PayloadJSON;
	return RecordReportField(R);
}

bool UESCROWEventLogSubsystem::RecordRawEvent(const FString& EventType, const FString& PayloadJSON)
{
	if (!bRunActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("ESCROWEventLogSubsystem::RecordRawEvent called with no active run; dropped."));
		return false;
	}

	const FString JsonLine = FString::Printf(TEXT("{\"RecordFamily\":\"Raw\",\"RunID\":\"%s\",\"TimestampISO\":\"%s\",\"WorldSeconds\":%.3f,\"EventType\":\"%s\",\"PayloadJSON\":\"%s\"}"),
		*EscapeJSONField(CurrentRunID), *FDateTime::Now().ToIso8601(), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f,
		*EscapeJSONField(EventType), *EscapeJSONField(PayloadJSON));

	return AppendLineToFile(TEXT("event_log_raw.jsonl"), JsonLine);
}

// =============================== RA41A: heartbeat ===============================

void UESCROWEventLogSubsystem::StartHeartbeat()
{
	bHeartbeatEnabled = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("ESCROWEventLogSubsystem::StartHeartbeat: no World available; heartbeat timer not started (RecordHeartbeatNow can still be called manually)."));
		return;
	}

	World->GetTimerManager().ClearTimer(HeartbeatTimerHandle);
	World->GetTimerManager().SetTimer(HeartbeatTimerHandle, this, &UESCROWEventLogSubsystem::HeartbeatTimerTick, FMath::Max(HeartbeatIntervalSeconds, 0.1f), true);
}

void UESCROWEventLogSubsystem::StopHeartbeat()
{
	bHeartbeatEnabled = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HeartbeatTimerHandle);
	}
}

bool UESCROWEventLogSubsystem::RecordHeartbeatNow()
{
	FESCROWHeartbeatRecord R;
	R.Phase = GetCurrentPhaseFromScenarioDirector();
	R.ActivePage = TEXT("UNSET");
	R.SelectedFollowerID = -1;
	R.TaskingID = TEXT("UNSET");
	R.TaskingState = TEXT("UNSET");
	R.PlanValidity = TEXT("UNSET");
	R.PayloadJSON = TEXT("{}");

	return RecordHeartbeat(R);
}

void UESCROWEventLogSubsystem::HeartbeatTimerTick()
{
	RecordHeartbeatNow();
}

// =============================== RA41B: fleet telemetry for heartbeat ===============================

FString UESCROWEventLogSubsystem::FormatLocation(const FVector& Location)
{
	return FString::Printf(TEXT("%.1f,%.1f,%.1f"), Location.X, Location.Y, Location.Z);
}

bool UESCROWEventLogSubsystem::TryGetCommandVehicleLocation(FVector& OutLocation) const
{
	const UWorld* World = GetWorld();
	const UESCROWFleetSubsystem* Fleet = World ? World->GetSubsystem<UESCROWFleetSubsystem>() : nullptr;
	if (!Fleet)
	{
		return false;
	}

	// There is no direct command-vehicle actor reference on UESCROWFleetSubsystem.
	// 0 = command vehicle in the follow-target convention (see FESCROWFollowerStatus),
	// so the first follower whose CurrentFollowTargetID == 0 with a resolved
	// FollowTargetActorRef gives us the CV's actor.
	for (const FESCROWFollowerStatus& Status : Fleet->FollowerStatuses)
	{
		if (Status.CurrentFollowTargetID == 0 && IsValid(Status.FollowTargetActorRef))
		{
			OutLocation = Status.FollowTargetActorRef->GetActorLocation();
			return true;
		}
	}

	return false;
}

void UESCROWEventLogSubsystem::PopulateFleetTelemetry(FESCROWHeartbeatRecord& R) const
{
	const UWorld* World = GetWorld();
	const UESCROWFleetSubsystem* Fleet = World ? World->GetSubsystem<UESCROWFleetSubsystem>() : nullptr;

	FVector CVLoc;
	if (TryGetCommandVehicleLocation(CVLoc))
	{
		R.CVLocation = FormatLocation(CVLoc);
	}

	if (!Fleet)
	{
		return;
	}

	// Per-follower mode/readiness/comm/alert go into PayloadJSON as a structured
	// "FleetTelemetry" object (any caller-supplied PayloadJSON is preserved under
	// a "Caller" key) rather than as individual CSV columns, to keep the CSV
	// schema stable if the fleet size ever changes.
	TArray<FString> FollowerJsonEntries;

	for (int32 FollowerID = 1; FollowerID <= 4; ++FollowerID)
	{
		FESCROWFollowerStatus Status;
		const bool bFound = Fleet->GetFollowerStatus(FollowerID, Status);

		FString LocationStr;
		float SpeedCms = -1.0f;
		if (bFound && IsValid(Status.ActorRef))
		{
			LocationStr = FormatLocation(Status.ActorRef->GetActorLocation());
			SpeedCms = Status.ActorRef->GetVelocity().Size();
		}

		switch (FollowerID)
		{
		case 1: R.F1Location = LocationStr; R.F1SpeedCms = SpeedCms; break;
		case 2: R.F2Location = LocationStr; R.F2SpeedCms = SpeedCms; break;
		case 3: R.F3Location = LocationStr; R.F3SpeedCms = SpeedCms; break;
		case 4: R.F4Location = LocationStr; R.F4SpeedCms = SpeedCms; break;
		default: break;
		}

		if (bFound)
		{
			const UEnum* StateEnum = StaticEnum<EESCROWFollowerState>();
			const UEnum* AutonomyEnum = StaticEnum<EESCROWAutonomyMode>();
			const FString ModeStr = StateEnum ? StateEnum->GetNameStringByValue(static_cast<int64>(Status.CurrentState)) : FString();
			const FString AutonomyStr = AutonomyEnum ? AutonomyEnum->GetNameStringByValue(static_cast<int64>(Status.AutonomyMode)) : FString();

			FollowerJsonEntries.Add(FString::Printf(
				TEXT("\"F%d\":{\"Mode\":\"%s\",\"AutonomyMode\":\"%s\",\"Health\":\"%s\",\"Comm\":\"%s\",\"RequiresTakeover\":%s}"),
				FollowerID, *EscapeJSONField(ModeStr), *EscapeJSONField(AutonomyStr), *EscapeJSONField(Status.HealthStatus),
				*EscapeJSONField(Status.CommsStatus), Status.bRequiresTakeover ? TEXT("true") : TEXT("false")));
		}
		else
		{
			FollowerJsonEntries.Add(FString::Printf(TEXT("\"F%d\":\"UNSET\""), FollowerID));
		}
	}

	const FString FleetTelemetryJson = FString::Printf(TEXT("{%s}"), *FString::Join(FollowerJsonEntries, TEXT(",")));

	if (R.PayloadJSON.IsEmpty())
	{
		R.PayloadJSON = FString::Printf(TEXT("{\"FleetTelemetry\":%s}"), *FleetTelemetryJson);
	}
	else
	{
		R.PayloadJSON = FString::Printf(TEXT("{\"Caller\":%s,\"FleetTelemetry\":%s}"), *R.PayloadJSON, *FleetTelemetryJson);
	}
}
