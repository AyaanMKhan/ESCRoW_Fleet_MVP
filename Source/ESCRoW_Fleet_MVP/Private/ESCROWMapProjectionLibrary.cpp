// Fill out your copyright notice in the Description page of Project Settings.

#include "ESCROWMapProjectionLibrary.h"

#include "GameFramework/Actor.h"

FESCROWMinimapProjectionResult UESCROWMapProjectionLibrary::ProjectWorldLocationToHeadingUpMinimap(
	const FVector& ActiveLocation,
	float ActiveYawDegrees,
	const FVector& TargetLocation,
	float MinimapWorldWidth,
	float MinimapHalfPixelExtent,
	float EdgePaddingPixels)
{
	FESCROWMinimapProjectionResult Result;

	if (MinimapWorldWidth <= 0.0f || MinimapHalfPixelExtent <= 0.0f)
	{
		// Invalid capture/panel configuration; nothing sensible to return.
		return Result;
	}

	// 2D ground-plane delta only; the minimap does not represent elevation.
	const FVector2D DeltaXY(TargetLocation.X - ActiveLocation.X, TargetLocation.Y - ActiveLocation.Y);

	// Rotate the world-space delta into the active actor's local heading
	// frame. Forward/Right here match Unreal's standard FRotator convention
	// (yaw measured in the XY plane), so this stays consistent with
	// GetActorRotation().Yaw on any actor.
	const float ActiveYawRad = FMath::DegreesToRadians(ActiveYawDegrees);
	const FVector2D Forward(FMath::Cos(ActiveYawRad), FMath::Sin(ActiveYawRad));
	const FVector2D Right(-FMath::Sin(ActiveYawRad), FMath::Cos(ActiveYawRad));

	const float LocalForward = FVector2D::DotProduct(DeltaXY, Forward);
	const float LocalRight = FVector2D::DotProduct(DeltaXY, Right);

	Result.LocalForward = LocalForward;
	Result.LocalRight = LocalRight;
	Result.DistanceWorldUnits = DeltaXY.Size();
	Result.BearingDegrees = FMath::RadiansToDegrees(FMath::Atan2(LocalRight, LocalForward));

	// Normalize local-frame offsets to [-1, 1] across the minimap's world
	// footprint. Active-actor-forward maps to minimap "up", which is negative
	// Y in UMG's screen-space (Y increases downward), hence the sign flip on Y.
	const float HalfWorld = MinimapWorldWidth * 0.5f;
	float NormalizedX = LocalRight / HalfWorld;
	float NormalizedY = -LocalForward / HalfWorld;

	Result.bInRange = FMath::Abs(NormalizedX) <= 1.0f && FMath::Abs(NormalizedY) <= 1.0f;
	Result.bClamped = !Result.bInRange;

	if (Result.bClamped)
	{
		// Radial-to-square clamp: scale both axes down by whichever is
		// furthest outside [-1, 1], so the marker lands on the minimap's
		// square edge along the same ray from center, preserving bearing.
		const float MaxAbs = FMath::Max(FMath::Abs(NormalizedX), FMath::Abs(NormalizedY));
		if (MaxAbs > 1.0f)
		{
			NormalizedX /= MaxAbs;
			NormalizedY /= MaxAbs;
		}
	}

	Result.NormalizedOffset = FVector2D(NormalizedX, NormalizedY);

	const float UsablePixelExtent = FMath::Max(1.0f, MinimapHalfPixelExtent - EdgePaddingPixels);
	Result.RenderTranslation = Result.NormalizedOffset * UsablePixelExtent;

	Result.bValid = true;
	return Result;
}

FESCROWMinimapProjectionResult UESCROWMapProjectionLibrary::ProjectWorldLocationToHeadingUpMinimapSimple(
	AActor* ActiveActor,
	AActor* TargetActor,
	float MinimapWorldWidth,
	float MinimapHalfPixelExtent,
	float EdgePaddingPixels)
{
	if (!ActiveActor || !TargetActor)
	{
		return FESCROWMinimapProjectionResult();
	}

	return ProjectWorldLocationToHeadingUpMinimap(
		ActiveActor->GetActorLocation(),
		ActiveActor->GetActorRotation().Yaw,
		TargetActor->GetActorLocation(),
		MinimapWorldWidth,
		MinimapHalfPixelExtent,
		EdgePaddingPixels);
}
