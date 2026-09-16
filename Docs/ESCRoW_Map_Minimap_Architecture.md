# ESCRoW Map / Minimap Architecture (RA30A, RA30B, RA30C-REPAIR)

## Full MAP page

Purpose: **global, north-up, fleet/tactical overview** for mission planning and fleet command.

Implementation: `BP_ESCROW_MapCaptureManager` owns a top-down orthographic `SceneCaptureComponent2D`
(`MapSceneCapture`, OrthoWidth 25000, `SCS_FinalColorLDR`) that renders into a 1024x1024
`LiveMapRT` render target once at `BeginPlay`. `WBP_ESCROW_CommandHUD`'s MAP page
(`MAP_LiveMapImage`) displays that render target inside a fixed 900x500 frame
(`MAP_WireframeSizeBox` / `MAP_WireframeCanvas`). CV/F1-F4 icons, route/waypoint overlays, and the
`MAP_NorthIndicator` remain valid here — this page is always north-up and unaffected by the minimap
changes below.

**Command-feed inset (RA30C-REPAIR):** `MAP_FeedImage` is a small bottom-right inset on the MAP page
(resized from a full-map-covering overlay back in RA29A). Its widget-tree default `Visibility` is
`Collapsed`. Previously, `Escrow_SetActivePage` automatically called `Escrow_EnsureCommandFeed` every
time the MAP page (page 0) was opened, which set the image `Visible` unconditionally — this is the
clutter the user reported. RA30C-REPAIR removed that automatic call (deleted the single
`Escrow_EnsureCommandFeed` call node inside `Escrow_SetActivePage`'s "if NewPage==0" branch). The MAP
page is now map-first by default: `MAP_FeedImage` stays `Collapsed` until something explicitly shows it.
**No toggle button was wired up in this pass** — `Escrow_EnsureCommandFeed` / `Escrow_OpenCommandFeed` /
`Escrow_OpenCommandFeed_Direct` still exist and still work exactly as before, they are just no longer
called automatically. Wiring a "Toggle Command Feed" button was deferred as higher-risk (it would
require adding a new call to a custom Blueprint function from a button's existing event graph, in the
same fragile-DSL-rewrite territory that caused the RA30C incident below) — see Known Limitations.

## Minimap

Purpose: **local, active-vehicle-centered, heading-up navigation aid** — never a miniature copy of the
tactical map.

Implementation: `BP_ESCROW_MapCaptureManager` also owns a second orthographic capture
(`MinimapSceneCapture`, OrthoWidth 6000, its own 512x512 `MinimapRT`). Every 0.3s (existing
`WBP_ESCROW_TestHUD::Escrow_UpdateMinimapMarker` timer), the HUD resolves the current active
controlled actor (`BP_ESCROW_PlayerController::Escrow_GetActiveControlActor` — the possessed pawn:
command vehicle during normal driving, the possessed follower during remote takeover) and calls
`BP_ESCROW_MapCaptureManager::Escrow_UpdateMinimapCapture(ActiveLocation, ActiveYaw)`.

**Heading-up (RA30C-REPAIR):** `Escrow_UpdateMinimapCapture` now sets `MinimapSceneCapture`'s world
rotation to `(Pitch: -90, Yaw: ActiveYaw, Roll: 0)` in addition to snapping its location to the active
actor's XY, so the captured terrain/road imagery rotates with the active vehicle's heading instead of
staying north-up. `ActiveYaw` is read via a new `GetActorRotation` → `BreakRotator` node pair added
directly to `Escrow_UpdateMinimapMarker`, reusing the active-actor reference already resolved there.
**Yaw formula used: `SceneCapture.Yaw = ActiveActor.Yaw` (Candidate A, the simplest of the candidates
considered).** This has not been empirically verified in PIE against a live turning vehicle (input
automation in this environment cannot reliably drive sustained heading changes) — if the rotation
direction turns out inverted or offset, the fix is a one-line change to the Yaw expression (e.g. negate
it, or add/subtract 90/180) inside `Escrow_UpdateMinimapCapture`.

The minimap Image (`Minimap_LiveMapImage`) always displays the full `MinimapRT` (no UV cropping)
inside the rounded amber-bordered `ESCROW_MinimapPanel`. The `"▲ ME"` marker (`ESCROW_MinimapCV`)
stays dead-center (render translation zeroed).

**Heading indicator:** the old static `"N"` label (`ESCROW_MinimapNorth`, a design-time-only
`TextBlock` with no runtime graph logic) was misleading on a heading-up display, since it implied a
fixed north-up compass that does not actually rotate with the map. RA30C-REPAIR changed its static
text to `"HDG"` (a neutral heading-up label) rather than hiding it outright, since it still usefully
marks "top of minimap = forward." It is not a rotating compass — it does not track true world north.

**Follower markers are currently suppressed (RA30C-REPAIR).** See below.

## Why follower/selected minimap markers are suppressed, not fixed

RA30B added `ESCROW_MinimapF1`-`F4` and reused `ESCROW_MinimapSelected`, computing bearing via
`WorldToMapUV(follower) − WorldToMapUV(activeActor)` — a **world-frame (north-up) delta**. That was
correct for a north-up minimap, but became wrong the moment the minimap turned heading-up in RA30C:
a follower's on-screen position needs to be rotated into the active actor's local forward/right frame,
not left in world X/Y space, or its displayed bearing no longer matches its real bearing relative to
where the vehicle is pointing.

An attempt to fix this properly (RA30C, before this repair pass) tried to rewrite
`Escrow_UpdateOneMinimapFollowerMarker`'s and `Escrow_UpdateMinimapSelectedMarker`'s full function
bodies via the Blueprint DSL tool to add the rotation math. This failed: the DSL/graph-creation tool
used by this project's Unreal MCP integration **cannot create new call-nodes referencing custom
Blueprint-local functions** (confirmed specifically for `Escrow_CheckValid`, called from inside those
functions) — it can read/decompile existing calls to such functions, but not recreate them. Worse,
each failed rewrite attempt left large orphaned/disconnected node trees behind instead of rolling back
cleanly (one function graph went from ~30 nodes to over 150).

RA30C-REPAIR recovered from this by rerouting `Escrow_UpdateMinimapMarker`'s exec chain to skip the
SEL and F1-F4 update calls entirely, deleting the now-unused caller nodes, and deleting the entire
contaminated `Escrow_UpdateOneMinimapFollowerMarker` function graph in one operation (`remove_function_graph`) rather than
deleting ~150 orphan nodes individually. This left the blueprint compiling clean with **no follower or
selected-follower markers on the minimap at all** — better than showing spatially wrong markers, per
the explicit fallback this pass was scoped to allow. `Escrow_UpdateMinimapSelectedMarker` itself was
not deleted (it has no orphan contamination), just no longer called; it remains available, unused,
for a future pass.

**The robust fix path for a future pass:** implement the active-local heading-relative coordinate
transform (world delta → rotate by `-ActiveYaw` → local forward/right → minimap pixels) in a small
**C++ `BlueprintFunctionLibrary` helper** exposed as a single callable Blueprint node, rather than
attempting further raw Blueprint-graph surgery through this DSL tool. A C++ function is one node to
wire in, sidestepping the custom-local-function-creation limitation entirely.

**RA30D-1 update: this helper now exists.** `UESCROWMapProjectionLibrary::ProjectWorldLocationToHeadingUpMinimap`
(`Source/ESCRoW_Fleet_MVP/Public/ESCROWMapProjectionLibrary.h` /
`Private/ESCROWMapProjectionLibrary.cpp`) is a `BlueprintPure` node that takes an active actor's
location/yaw and a target world location and returns an `FESCROWMinimapProjectionResult` — a ready-to-use
`RenderTranslation` (already sign-corrected for UMG's Y-down screen space and clamped to the minimap
edge when out of range), plus `bInRange`/`bClamped`/`BearingDegrees` for future edge-indicator work. A
second overload, `ProjectWorldLocationToHeadingUpMinimapSimple`, takes two `AActor*` directly. Neither
touches any Blueprint graph, widget, or `.uasset` — this pass only added the C++ helper and compiled it;
CV/F1-F4/objective minimap markers are still suppressed and will be wired up in RA30D-2 using this node.

## Why the vendor minimap is unused

The vendor `MapSystem` marketplace plugin remains enabled in the project (`.uproject`) but no `MapZone`
or other required vendor actor is placed in `MAP_ESCROW_DESERT_INTEGRATION_TEST`. The vendor
`WBP_Minimap` widget instance that used to sit (Collapsed, but still ticking and generating
`CaptureScene ... major inefficiency` warnings) inside `WBP_ESCROW_TestHUD` was removed in RA30A — an
ESCRoW-owned widget-tree edit, not a vendor-source edit. Restoring the vendor minimap was not
attempted: it would require placing/configuring vendor `MapZone` actors and is not guaranteed to
support active-vehicle-centered, heading-up behavior without touching vendor internals, which is
disallowed. The ESCRoW-owned dual-capture approach on `BP_ESCROW_MapCaptureManager` remains the chosen
path.

## Marker semantics

- **Full MAP page:** `CV` = command vehicle, `F1`-`F4` = followers, selected-follower highlight =
  selected asset, route/waypoint/issue/rejoin = mission overlays, `MAP_NorthIndicator` = true north
  (page is always north-up).
- **Minimap:** `ME` = whichever actor is currently possessed (command vehicle or, during takeover, the
  possessed follower) — never labeled `CV`. `F1`-`F4` and `SEL` are **currently suppressed** (see
  above) rather than shown with incorrect bearing. `HDG` = "top of minimap is the active vehicle's
  forward direction," not a compass.

## Known limitations

- **Yaw rotation direction is unverified.** `Escrow_UpdateMinimapCapture` sets
  `SceneCapture.Yaw = ActiveActor.Yaw`; this has not been confirmed correct against a live turning
  vehicle in PIE. Manually verify by driving in a circle and checking the minimap terrain rotates the
  expected way.
- **F1-F4 and SEL minimap markers are suppressed, not implemented.** See "Why follower/selected
  minimap markers are suppressed" above. The full MAP page's CV/F1-F4 markers are unaffected.
- **No command-feed toggle button.** `MAP_FeedImage` is hidden by default and stays hidden; there is
  currently no in-HUD control to show it on the MAP page. The feed setup functions themselves
  (`Escrow_EnsureCommandFeed` etc.) are untouched and could be wired to a button in a future pass.
- **`HDG` label is static text, not a live indicator.** It does not rotate or otherwise reflect
  anything dynamic; it is a fixed reminder that the minimap is heading-up.
- MAP page frame is a fixed 900x500 rectangle; the live map render target is cropped (not resized) to
  that aspect via a UV region on `MAP_LiveMapImage`'s brush, so the visible tactical-map ground
  footprint is a vertical center-crop of the full 25000-unit capture, not the full square.
- Minimap OrthoWidth (6000 units) is a literal constant on `BP_ESCROW_MapCaptureManager`'s component
  default; no shared Blueprint variable was added for it, to keep changes narrow.
- The vendor `MapSystem` subsystem still logs one harmless `Failed to retrieve MapHelper` warning per
  session at startup; this is pre-existing vendor behavior and out of scope to silence without editing
  vendor internals.
- Both scene captures use `bCaptureEveryFrame=false` and are only re-captured on their respective
  timers (0.3s minimap follow, once at BeginPlay for the main map), so terrain/lighting changes after
  BeginPlay are not reflected in the main map image.
- **Blueprint DSL tooling limitation (important for future passes):** the Unreal MCP Blueprint-editing
  tool cannot create new call-nodes to custom Blueprint-local functions, and failed full-function
  `write_graph_dsl` rewrites do not roll back cleanly (they leave orphaned node trees). Future changes
  to functions that call other custom Blueprint functions should use small, surgical node-level edits
  (`create_node`/`connect_pins`/`delete_node` with engine-level node types only) rather than full-body
  DSL rewrites, or move the logic into C++.
