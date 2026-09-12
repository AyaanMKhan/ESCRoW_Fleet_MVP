// Fill out your copyright notice in the Description page of Project Settings.

#include "ESCROWEventLogSubsystem.h"
#include "ESCROWSessionSubsystem.h"

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
	EventEntries.Add(Entry);

	return Entry;
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
	return TEXT("RealTimestamp,MissionTime,ParticipantID,SessionID,MissionCondition,PlayerMode,EventType,FollowerID,CommandID,MessageID,EventText");
}

FString UESCROWEventLogSubsystem::EntryToCSVRow(const FESCROWEventLogEntry& Entry) const
{
	const UEnum* MissionConditionEnum = StaticEnum<EESCROWMissionCondition>();
	const UEnum* PlayerModeEnum = StaticEnum<EESCROWPlayerControlMode>();

	const FString MissionConditionName = MissionConditionEnum ? MissionConditionEnum->GetNameStringByValue(static_cast<int64>(Entry.MissionCondition)) : FString();
	const FString PlayerModeName = PlayerModeEnum ? PlayerModeEnum->GetNameStringByValue(static_cast<int64>(Entry.PlayerMode)) : FString();

	return FString::Printf(TEXT("%s,%.3f,%s,%s,%s,%s,%s,%d,%s,%s,%s"),
		*EscapeCSVField(Entry.RealTimestamp),
		Entry.MissionTime,
		*EscapeCSVField(Entry.ParticipantID),
		*EscapeCSVField(Entry.SessionID),
		*EscapeCSVField(MissionConditionName),
		*EscapeCSVField(PlayerModeName),
		*EscapeCSVField(Entry.EventType),
		Entry.FollowerID,
		*EscapeCSVField(Entry.CommandID),
		*EscapeCSVField(Entry.MessageID),
		*EscapeCSVField(Entry.EventText));
}

FString UESCROWEventLogSubsystem::BuildJSONLine(const FESCROWEventLogEntry& Entry) const
{
	const UEnum* MissionConditionEnum = StaticEnum<EESCROWMissionCondition>();
	const UEnum* PlayerModeEnum = StaticEnum<EESCROWPlayerControlMode>();

	const FString MissionConditionName = MissionConditionEnum ? MissionConditionEnum->GetNameStringByValue(static_cast<int64>(Entry.MissionCondition)) : FString();
	const FString PlayerModeName = PlayerModeEnum ? PlayerModeEnum->GetNameStringByValue(static_cast<int64>(Entry.PlayerMode)) : FString();

	return FString::Printf(TEXT("{\"RealTimestamp\":\"%s\",\"MissionTime\":%.3f,\"ParticipantID\":\"%s\",\"SessionID\":\"%s\",\"MissionCondition\":\"%s\",\"PlayerMode\":\"%s\",\"EventType\":\"%s\",\"FollowerID\":%d,\"CommandID\":\"%s\",\"MessageID\":\"%s\",\"EventText\":\"%s\"}"),
		*EscapeJSONField(Entry.RealTimestamp),
		Entry.MissionTime,
		*EscapeJSONField(Entry.ParticipantID),
		*EscapeJSONField(Entry.SessionID),
		*EscapeJSONField(MissionConditionName),
		*EscapeJSONField(PlayerModeName),
		*EscapeJSONField(Entry.EventType),
		Entry.FollowerID,
		*EscapeJSONField(Entry.CommandID),
		*EscapeJSONField(Entry.MessageID),
		*EscapeJSONField(Entry.EventText));
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
