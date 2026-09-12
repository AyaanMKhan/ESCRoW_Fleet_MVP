// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ESCROWTypes.h"
#include "ESCROWPlayerControlRouterComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FESCROWOnPlayerModeChanged, EESCROWPlayerControlMode, NewMode, EESCROWPlayerControlMode, OldMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FESCROWOnRemoteTakeoverShellStarted, int32, FollowerID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FESCROWOnRemoteTakeoverShellEnded, int32, FollowerID);

/**
 * The only ESCRoW-owned runtime object allowed to change player control mode.
 * This is a state-tracking shell: it does not possess vehicles, switch cameras,
 * or add/remove input mapping contexts. Those responsibilities are added in
 * later RA passes on top of this component.
 */
UCLASS(ClassGroup = (ESCRoW), meta = (BlueprintSpawnableComponent))
class ESCROW_FLEET_MVP_API UESCROWPlayerControlRouterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UESCROWPlayerControlRouterComponent();

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|ControlRouter")
	EESCROWPlayerControlMode CurrentPlayerMode = EESCROWPlayerControlMode::PassengerSupervisor;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|ControlRouter")
	EESCROWPlayerControlMode PreviousPlayerMode = EESCROWPlayerControlMode::PassengerSupervisor;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|ControlRouter")
	int32 ActiveRemoteFollowerID = -1;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|ControlRouter")
	bool bConsoleFocused = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|ControlRouter")
	bool bFullMapOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "ESCRoW|ControlRouter")
	bool bRemoteTakeoverActive = false;

	UPROPERTY(BlueprintAssignable, Category = "ESCRoW|ControlRouter")
	FESCROWOnPlayerModeChanged OnPlayerModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "ESCRoW|ControlRouter")
	FESCROWOnRemoteTakeoverShellStarted OnRemoteTakeoverShellStarted;

	UPROPERTY(BlueprintAssignable, Category = "ESCRoW|ControlRouter")
	FESCROWOnRemoteTakeoverShellEnded OnRemoteTakeoverShellEnded;

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|ControlRouter")
	void SetPlayerMode(EESCROWPlayerControlMode NewMode);

	UFUNCTION(BlueprintPure, Category = "ESCRoW|ControlRouter")
	EESCROWPlayerControlMode GetPlayerMode() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|ControlRouter")
	EESCROWPlayerControlMode GetPreviousPlayerMode() const;

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|ControlRouter")
	void FocusCommandHUD();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|ControlRouter")
	void ReturnToCompactHUD();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|ControlRouter")
	void OpenFullMap();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|ControlRouter")
	void CloseFullMap();

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|ControlRouter")
	void BeginRemoteTakeoverShell(int32 FollowerID);

	UFUNCTION(BlueprintCallable, Category = "ESCRoW|ControlRouter")
	void EndRemoteTakeoverShell();

	UFUNCTION(BlueprintPure, Category = "ESCRoW|ControlRouter")
	bool IsRemoteTakeoverActive() const;

	UFUNCTION(BlueprintPure, Category = "ESCRoW|ControlRouter")
	int32 GetActiveRemoteFollowerID() const;
};
