// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ESCROWAvoidanceLibrary.generated.h"

class AActor;

// Obstacle-avoidance heading selection for the follower own-driver
// (BP_ESCROW_FollowerVehicle::Escrow_SelectHeading). Lives in C++ because it is a dense
// per-tick sweep + scoring loop that is unreadable and slow as a Blueprint graph.
UCLASS()
class ESCROW_FLEET_MVP_API UESCROWAvoidanceLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Sweeps a fan of vehicle-width spheres ahead of Vehicle (+/-105 deg, 7.5 deg steps,
	// look-ahead scaled with speed and capped just past the pursuit target), smooths the
	// free-distance profile across neighbouring rays, then scores feasible headings by goal
	// error, clearance, how wide the surrounding gap is, steering effort and left/right
	// hysteresis. If nothing is feasible the widest-open heading is returned with
	// OutCost = 1,000,000 (caller treats that as "blocked").
	//
	// OutSteerAngleDeg is relative to the vehicle heading (positive = right/clockwise, same
	// convention as the existing own-driver SelAng). OutSelSign is the new left/right memory.
	UFUNCTION(BlueprintCallable, Category = "ESCRoW|Avoidance", meta = (WorldContext = "WorldContextObject"))
	static void ComputeAvoidanceHeading(UObject* WorldContextObject, AActor* Vehicle, FVector PursuitTarget, float ForwardSpeed, float PrevSelSign,
		float& OutSteerAngleDeg, float& OutClearDistance, float& OutCost, float& OutSelSign);
};
