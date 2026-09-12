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

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|EventLog")
	TArray<FESCROWEventLogEntry> EventEntries;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|EventLog")
	int32 EventCounter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|EventLog")
	bool bLoggingEnabled = true;

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|EventLog")
	void ResetLog();

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

private:
	FString BuildTimestampString() const;

	static FString EscapeCSVField(const FString& Field);
	static FString EscapeJSONField(const FString& Field);
};
