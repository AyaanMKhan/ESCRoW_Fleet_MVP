// Fill out your copyright notice in the Description page of Project Settings.

#include "ESCROWEventLogSubsystem.h"
#include "ESCROWSessionSubsystem.h"
#include "ESCROWFleetSubsystem.h"
#include "ESCROWPlayerControlRouterComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"

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
