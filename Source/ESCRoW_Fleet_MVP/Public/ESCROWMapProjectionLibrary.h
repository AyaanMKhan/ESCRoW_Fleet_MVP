// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ESCROWMapProjectionLibrary.generated.h"

class AActor;

// Result of projecting a world-space target location onto a heading-up local
// minimap, relative to an active/controlled actor. See
// UESCROWMapProjectionLibrary::ProjectWorldLocationToHeadingUpMinimap for the
// coordinate convention and why this math lives in C++ rather than a UMG
// Blueprint graph.
USTRUCT(BlueprintType)
struct FESCROWMinimapProjectionResult
{
	GENERATED_BODY()

	// False only when the inputs were invalid (e.g. MinimapWorldWidth <= 0) or,
	// for the actor-based helper, when an actor pointer was null. All other
	// fields are meaningless when this is false.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Minimap")
	bool bValid = false;

	// True when the target falls within the minimap's visible radius, i.e. its
	// normalized offset did not need clamping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Minimap")
	bool bInRange = false;

	// True when the target was outside the visible radius and RenderTranslation
	// was clamped to the minimap edge (bearing preserved, distance is not).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Minimap")
	bool bClamped = false;

	// Ready-to-use UMG RenderTranslation (pixels), already accounting for the
	// UMG screen Y-down inversion. Apply directly to a Center/Center-anchored
	// marker widget so the active actor sits at (0, 0).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Minimap")
	FVector2D RenderTranslation = FVector2D::ZeroVector;

	// The same offset as RenderTranslation but normalized to the [-1, 1] range
	// (post-clamp), before scaling to pixels. Useful for edge-indicator glyphs
	// that need a direction vector rather than a pixel position.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Minimap")
	FVector2D NormalizedOffset = FVector2D::ZeroVector;

	// True 2D (XY) world distance from the active actor to the target, in
	// world units, ignoring Z and ignoring any clamping applied to the marker.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Minimap")
	float DistanceWorldUnits = 0.0f;

	// Bearing of the target relative to the active actor's heading, in
	// degrees: 0 = ahead, 90 = right, +/-180 = behind, -90 = left.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Minimap")
	float BearingDegrees = 0.0f;

	// Raw (unscaled, unclamped) world-unit offset along the active actor's
	// forward axis. Positive = ahead of the active actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Minimap")
	float LocalForward = 0.0f;

	// Raw (unscaled, unclamped) world-unit offset along the active actor's
	// right axis. Positive = to the right of the active actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESCRoW|Minimap")
	float LocalRight = 0.0f;
};

/**
 * Heading-up local minimap projection math, kept in C++ so it can be edited
 * reliably rather than through fragile UMG Blueprint graph surgery.
 *
 * Background: the ESCRoW minimap is a local, active-vehicle-centered,
 * heading-up navigation aid (distinct from the full MAP page, which stays
 * global/north-up). Earlier attempts to compute follower/marker bearing for
 * this minimap directly in WBP_ESCROW_TestHUD's Blueprint graphs ran into two
 * problems: (1) the north-up world-delta math used there does not carry over
 * to a heading-up display without a rotation transform, and (2) the Unreal
 * MCP Blueprint-editing tooling used on this project cannot reliably create
 * new call-nodes to custom Blueprint-local functions and does not roll back
 * cleanly on failure, so attempting the fix as raw UMG graph edits corrupted
 * the widget blueprint with orphaned nodes. Doing the math here instead means
 * Blueprints only need to wire in a single BlueprintPure node.
 */
UCLASS()
class ESCROW_FLEET_MVP_API UESCROWMapProjectionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Projects TargetLocation into heading-up minimap marker coordinates
	 * relative to ActiveLocation/ActiveYawDegrees.
	 *
	 * Coordinate convention (matches UMG's screen space, Y increasing downward):
	 *   - The active actor's forward direction is "up" on the minimap.
	 *   - A target ahead of the active actor  => negative Y render translation.
	 *   - A target behind the active actor    => positive Y render translation.
	 *   - A target to the right               => positive X render translation.
	 *   - A target to the left                => negative X render translation.
	 *
	 * Z is ignored; this is a 2D (ground-plane) projection only.
	 *
	 * @param ActiveLocation          World location of the active/controlled actor (map center).
	 * @param ActiveYawDegrees        World yaw of the active/controlled actor, in degrees.
	 * @param TargetLocation          World location of the marker being placed.
	 * @param MinimapWorldWidth       Full width, in world units, that the minimap capture covers
	 *                                (e.g. the minimap SceneCapture2D's OrthoWidth).
	 * @param MinimapHalfPixelExtent  Half-width of the minimap's usable area, in pixels (e.g.
	 *                                half the minimap render target/panel size).
	 * @param EdgePaddingPixels       Pixels to keep clear inside the minimap edge so clamped
	 *                                markers/labels do not touch the border.
	 */
	UFUNCTION(BlueprintPure, Category = "ESCRoW|Minimap")
	static FESCROWMinimapProjectionResult ProjectWorldLocationToHeadingUpMinimap(
		const FVector& ActiveLocation,
		float ActiveYawDegrees,
		const FVector& TargetLocation,
		float MinimapWorldWidth,
		float MinimapHalfPixelExtent,
		float EdgePaddingPixels);

	/**
	 * Convenience wrapper around ProjectWorldLocationToHeadingUpMinimap that
	 * reads location/yaw directly from actors. Returns an invalid (bValid =
	 * false) result if either actor is null.
	 */
	UFUNCTION(BlueprintPure, Category = "ESCRoW|Minimap")
	static FESCROWMinimapProjectionResult ProjectWorldLocationToHeadingUpMinimapSimple(
		AActor* ActiveActor,
		AActor* TargetActor,
		float MinimapWorldWidth,
		float MinimapHalfPixelExtent,
		float EdgePaddingPixels);
};
