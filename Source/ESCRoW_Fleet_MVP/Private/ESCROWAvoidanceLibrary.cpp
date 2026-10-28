// Fill out your copyright notice in the Description page of Project Settings.

#include "ESCROWAvoidanceLibrary.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "CollisionQueryParams.h"

namespace
{
	constexpr float FanHalfAngleDeg = 105.0f;
	constexpr float FanStepDeg = 7.5f;
	constexpr float SweepRadius = 220.0f;
	constexpr float SweepHeight = 260.0f;
	constexpr float BlockedCost = 1000000.0f;
}

void UESCROWAvoidanceLibrary::ComputeAvoidanceHeading(UObject* WorldContextObject, AActor* Vehicle, FVector PursuitTarget, float ForwardSpeed, float PrevSelSign,
	float& OutSteerAngleDeg, float& OutClearDistance, float& OutCost, float& OutSelSign)
{
	OutSteerAngleDeg = 0.0f;
	OutClearDistance = 0.0f;
	OutCost = BlockedCost;
	OutSelSign = 0.0f;

	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || !Vehicle)
	{
		return;
	}

	const FVector Loc = Vehicle->GetActorLocation();
	const float Yaw = Vehicle->GetActorRotation().Yaw;
	const FVector ToTarget = FVector(PursuitTarget.X - Loc.X, PursuitTarget.Y - Loc.Y, 0.0f);
	const float TargetDist = ToTarget.Size();
	const float GoalRel = FMath::FindDeltaAngleDegrees(Yaw, FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X)));

	const float Speed = FMath::Max(ForwardSpeed, 0.0f);
	const float Range = FMath::Clamp(1800.0f + Speed * 3.0f, 1800.0f, 3500.0f);
	// Obstacles beyond the pursuit point do not matter; saturate the free distance there.
	const float DCap = FMath::Clamp(TargetDist + 500.0f, 1000.0f, Range);
	const float MinClear = FMath::Min(700.0f + Speed * 0.8f, DCap * 0.8f);

	const int32 NumRays = FMath::RoundToInt(2.0f * FanHalfAngleDeg / FanStepDeg) + 1;
	TArray<float> Angle;
	TArray<float> Dist;
	Angle.SetNumUninitialized(NumRays);
	Dist.SetNumUninitialized(NumRays);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ESCROWAvoidanceSweep), false, Vehicle);
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(SweepRadius);
	const FVector Start = Loc + FVector(0.0f, 0.0f, SweepHeight);

	for (int32 i = 0; i < NumRays; ++i)
	{
		const float A = -FanHalfAngleDeg + static_cast<float>(i) * FanStepDeg;
		Angle[i] = A;
		const float Rad = FMath::DegreesToRadians(Yaw + A);
		const FVector End = Start + FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.0f) * Range;
		const FVector Dir(FMath::Cos(Rad), FMath::Sin(Rad), 0.0f);
		float D = Range;
		float Traveled = 0.0f;
		FVector S = Start;
		// Ground-like hits (gentle slopes/bumps on uneven terrain) must not count as obstacles:
		// step the sweep over them and keep going, otherwise rolling desert terrain "blocks" every ray.
		for (int32 Step = 0; Step < 4; ++Step)
		{
			FHitResult Hit;
			if (!World->SweepSingleByChannel(Hit, S, S + Dir * (Range - Traveled), FQuat::Identity, ECC_Visibility, Sphere, Params))
			{
				D = Range;
				break;
			}
			const float HitDist = Traveled + (Hit.bStartPenetrating ? 0.0f : Hit.Distance);
			const bool bGroundLike = Hit.ImpactNormal.Z > 0.65f && !Cast<APawn>(Hit.GetActor());
			if (bGroundLike && Step < 3)
			{
				Traveled = HitDist + 20.0f;
				S = Start + Dir * Traveled + FVector(0.0f, 0.0f, 150.0f * static_cast<float>(Step + 1));
				continue;
			}
			D = HitDist;
			break;
		}
		Dist[i] = FMath::Min(D, DCap);
	}

	// Neighbour smoothing (min filter) so a heading hugging an obstacle edge is not trusted.
	TArray<float> Eff;
	TArray<float> Wide;
	Eff.SetNumUninitialized(NumRays);
	Wide.SetNumUninitialized(NumRays);
	for (int32 i = 0; i < NumRays; ++i)
	{
		float E = Dist[i];
		float W = Dist[i];
		for (int32 k = -3; k <= 3; ++k)
		{
			const int32 j = FMath::Clamp(i + k, 0, NumRays - 1);
			W = FMath::Min(W, Dist[j]);
			if (FMath::Abs(k) <= 1)
			{
				E = FMath::Min(E, Dist[j]);
			}
		}
		Eff[i] = E;
		Wide[i] = W;
	}

	int32 Best = INDEX_NONE;
	float BestCost = BlockedCost;
	int32 Widest = 0;
	for (int32 i = 0; i < NumRays; ++i)
	{
		if (Eff[i] > Eff[Widest])
		{
			Widest = i;
		}
		if (Eff[i] < MinClear)
		{
			continue;
		}
		float Cost = FMath::Abs(Angle[i] - GoalRel) / 90.0f;
		Cost += 1.2f * (1.0f - Eff[i] / DCap);
		Cost += 0.5f * (1.0f - Wide[i] / DCap);
		Cost += 0.3f * FMath::Abs(Angle[i]) / 180.0f;
		if (Angle[i] * PrevSelSign > 0.0f)
		{
			Cost -= 0.2f;
		}
		if (Cost < BestCost)
		{
			BestCost = Cost;
			Best = i;
		}
	}

	if (Best == INDEX_NONE)
	{
		OutSteerAngleDeg = Angle[Widest];
		OutClearDistance = Eff[Widest];
		OutCost = BlockedCost;
		OutSelSign = Angle[Widest] > 0.0f ? 1.0f : -1.0f;
		return;
	}

	float Steer = Angle[Best];
	// Goal heading is open and clear: steer exactly at it instead of the 7.5 deg grid.
	if (FMath::Abs(Steer - GoalRel) < FanStepDeg && Eff[Best] >= 1800.0f && FMath::Abs(GoalRel) <= FanHalfAngleDeg)
	{
		Steer = GoalRel;
		OutSelSign = 0.0f;
	}
	else
	{
		OutSelSign = FMath::Abs(Steer) > 10.0f ? (Steer > 0.0f ? 1.0f : -1.0f) : PrevSelSign;
	}

	OutSteerAngleDeg = Steer;
	OutClearDistance = Eff[Best];
	OutCost = BestCost;
}
