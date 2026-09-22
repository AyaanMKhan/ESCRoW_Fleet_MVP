# ESCRoW Fleet MVP — Claude Working Notes

This file is the working orientation for Claude when modifying the ESCRoW Unreal project. It should stay concise, operational, and current. Do not spend tokens restating this file back to the user unless asked; use it to guide scoped implementation work.

## Project identity

- Repository: `rhyse-bendell/ESCRoW_Fleet_MVP`
- Local project root: `C:\UnrealProjects\ESCRoW_Fleet_MVP`
- Unreal project: `ESCRoW_Fleet_MVP.uproject`
- Unreal version: UE 5.8
- Main ESCRoW content root: `/Game/ESCROW`
- Work on `main`. Do not create branches unless explicitly told.

## Purpose of the testbed

ESCRoW is a convoy / command-vehicle supervisory-control research testbed. It is not meant to be an operationally validated military simulator. Its purpose is to create a controllable, lightweight environment where we can study and demonstrate:

- human supervision of multiple semi-autonomous follower vehicles;
- transitions from monitoring to intervention or takeover;
- function allocation between autonomy, interface, and human operator;
- detection of route, vehicle, spacing, communication, and autonomy problems;
- plan repair when a previously valid waypoint or formation plan becomes infeasible;
- manual baseline performance before adding AI decision-support scaffolds.

The design target is functional fidelity: preserve the cognitive work of supervisory control without building an unnecessarily heavy or over-realistic world.

## Current strategic direction

The old desert integration map remains useful as a visual/demo reference, but the experimental pilot should move to a new lightweight map:

- Legacy/demo map: `/Game/ESCROW/Maps/MAP_ESCROW_DESERT_INTEGRATION_TEST`
- New pilot target map: `/Game/ESCROW/Maps/MAP_ESCROW_FLAT_PILOT_TEST`

The flat pilot map should be driveable, small enough for fast iteration, and visually sufficient: arid test terrain, dirt route, sparse rocks/barriers, rally areas, and clear experimental zones. Do not use heavy environment packs when simple ESCRoW-owned geometry or lightweight existing meshes will work.

## Core pilot scenario spine

The pilot scenario should support these phases:

1. **Phase A — Normal convoy travel and monitoring**
   - Command vehicle and followers travel in convoy.
   - Operator monitors follower identity, spacing, route progress, readiness, autonomy/communication state, and map position.

2. **Phase B — Rally point B breakout / formation**
   - Convoy reaches Rally B.
   - Followers move into planned formation or breakout positions.
   - Operator verifies the phase transition and formation readiness.

3. **Phase C — Movement from B toward C with disruption**
   - Convoy attempts to proceed toward Rally C.
   - Route/spacing/autonomy problem appears: obstacle, barricade, narrow passage, irregular spacing, communication degradation, or autonomy failure.
   - Operator must detect the disruption and identify the affected follower or convoy segment.

4. **Phase D — Follower takeover or guided intervention**
   - Operator selects the affected follower.
   - Operator takes over, guides, redirects, or otherwise intervenes.
   - Operator returns the asset/fleet to supervisory mode after the disruption.

5. **Phase E — Rally C plan repair**
   - Planned positions at Rally C are discovered to be infeasible.
   - Operator repairs waypoints or selects alternate positions.
   - Operator validates before execution.

6. **Phase F — Closeout / report / readiness reset**
   - Operator confirms readiness, clears residual warnings, and completes report/closure.

Manual baseline comes first. AI-supported supervision comes later.

## Manual baseline versus AI-supported condition

### Manual baseline

The operator has the standard HUD / console / map / follower / report interfaces, but no proactive AI assistant. The operator must detect, diagnose, select, take over, repair, validate, and report without AI summarization, prioritization, diagnosis, or recommendation.

### Later AI-supported condition

Use the same scenario spine, but add support such as:

- convoy-state summary;
- affected-follower highlight;
- takeover-needed cue;
- what-changed message;
- replan suggestion;
- validation checklist;
- report draft.

The AI layer should never be the source of experimental truth. Scenario state, event timing, ground truth, and scoring must remain deterministic and logged by ESCRoW systems.

## Immediate implementation priorities

1. **Fix command UI presentation across surfaces.**
   - HUD currently works best.
   - Physical console rendering/layout still needs to work reliably.
   - Virtual tablet fallback should be available if the physical console remains fragile.
   - The same supervisory state should drive HUD, console, and tablet variants.

2. **Create the lightweight flat pilot map.**
   - New map: `/Game/ESCROW/Maps/MAP_ESCROW_FLAT_PILOT_TEST`
   - Must support Rally A/start, Rally B formation, B-to-C disruption corridor, Rally C planned positions, Rally C alternate positions, and closeout/report area.
   - Use dynamic lighting / no baked lighting.
   - Do not create or track generated `*_BuiltData` assets.

3. **Implement scenario phase management.**
   - Deterministic phase flow A-F.
   - Named zones/triggers for Rally B, disruption onset, takeover requirement, Rally C plan repair, and closeout/report.

4. **Implement manual baseline logging.**
   - Event onset time, selected follower, page/view state, detection latency, takeover initiation, wrong-follower selection, control duration, waypoint repair, validation, report closure, and residual-warning handling.

5. **Add AI support only after manual baseline is stable.**

## Current important ESCRoW-owned assets

Core:

- `/Game/ESCROW/Blueprints/Core/BP_ESCROW_GameMode`
- `/Game/ESCROW/Blueprints/Core/BP_ESCROW_PlayerController`
- `/Game/ESCROW/Blueprints/Core/BP_ESCROW_OperatorCharacter`
- `/Game/ESCROW/Blueprints/Core/BP_ESCROW_MenuGameMode`
- `/Game/ESCROW/Blueprints/Core/BP_ESCROW_MenuPlayerController`

Vehicles:

- `/Game/ESCROW/Blueprints/Vehicles/BP_ESCROW_CommandVehicle`
- `/Game/ESCROW/Blueprints/Followers/BP_ESCROW_FollowerVehicle`

UI:

- `/Game/ESCROW/Blueprints/UI/WBP_ESCROW_MainMenu`
- `/Game/ESCROW/Blueprints/UI/WBP_ESCROW_TestHUD`
- `/Game/ESCROW/Blueprints/UI/WBP_ESCROW_CommandHUD`
- `/Game/ESCROW/Blueprints/UI/WBP_ESCROW_ExperimentSetup`
- `/Game/ESCROW/Blueprints/UI/WBP_ESCROW_PhysicalConsoleMirror`
- `/Game/ESCROW/Blueprints/UI/BP_MapHelper_Child_ESCROW`
- `/Game/ESCROW/Blueprints/UI/BP_MapZone_Child_ESCROW`
- `/Game/ESCROW/Blueprints/UI/BP_MapCustomMarker_Child_ESCROW`

## Marketplace / external asset roles

- `DD_Vehicles_Basic`: drivable vehicle base. Use only through ESCRoW child/wrapper Blueprints.
- `NavMeshVehicleAI`: low-level AI vehicle movement. Use through `BP_ESCROW_FollowerVehicle` or an ESCRoW adapter.
- `MapSystem`: minimap / marker layer. Keep functional; do not over-polish unless required for the current pass.
- `BattleField_Desert`: legacy/demo environment kit. Do not build the pilot map around heavy dependency on this pack.
- `CLM_MenuSystem`: menu plugin; current menu flow is minimal and ESCRoW-owned.

## Hard rules

- Do not edit marketplace/vendor asset source folders directly.
- Do not rewrite vendor Blueprints.
- Do not inspect or reproduce internal marketplace Blueprint graphs for assets marked AI usage not allowed.
- Use ESCRoW-owned child Blueprints, wrapper Blueprints, adapter components, widgets, structs, and managers.
- Preserve working behavior unless explicitly asked to change it.
- Keep changes narrow and testable.
- Do not use Save All.
- Save only assets intentionally modified for the current pass.
- Do not commit or stage unless explicitly told.
- Do not create branches unless explicitly told.
- Do not replace the project base again.
- Do not spend time on visual polish before mechanics pass.
- First inspect, then propose, then modify.

## LFS / repository hygiene rules

Generated Unreal built-data is not versioned.

- Do not add or track `*_BuiltData.uasset`, `*_BuiltData.ubulk`, or `*_BuiltData.uexp`.
- Do not build baked/static lighting for pilot maps.
- Use dynamic/movable lighting for pilot maps.
- Do not create large baked-lighting files.
- Do not commit generated folders such as `Saved/`, `Intermediate/`, `DerivedDataCache/`, `Binaries/`, `.vs/`, `LFS_AUDIT/`, or `LFS_AUDIT_REPORT.zip`.
- Keep screenshots, videos, rendered docs, and temporary exports out of this repo unless explicitly requested.

## Current dirty/quarantine risk areas

Before staging or committing, always inspect `git status --short` and separate intended changes from existing dirty assets.

Known recurring dirty/out-of-scope areas include:

- `Content/DD_Vehicles_Basic/...` vendor assets;
- `Content/ESCROW/Maps/MAP_ESCROW_DESERT_INTEGRATION_TEST.umap`;
- `Content/ESCROW/Blueprints/Core/BP_ESCROW_FeedCaptureManager.uasset`;
- `Content/ESCROW/Blueprints/Followers/BP_ESCROW_FollowerVehicle.uasset`;
- UI assets not in the current pass;
- docs not in the current pass.

Do not include these in a commit unless the user explicitly approves them as part of the pass.

## Architecture target

Use the hybrid architecture already in progress:

- C++ owns stable ESCRoW runtime state and logging systems when available.
- `UESCROWSessionSubsystem` stores participant/session/selected-mission state.
- `UESCROWFleetSubsystem` is the authoritative world-specific state, command, message, report, and takeover-lock bus for F1-F4.
- `UESCROWPlayerControlRouterComponent` is the only object/component allowed to change player control mode.
- `UESCROWEventLogSubsystem` handles structured logging.
- Blueprint/UMG owns HUD layout, console/tablet presentation, follower cards/tabs, map panels, feed panels, vehicle wrappers, camera presentation, and marketplace-asset adapters.
- `BP_ScenarioDirector` or an ESCRoW-owned scenario manager should own deterministic mission timing and scenario phase flow.

Ask before adding new C++ outside an approved pass.

## MCP / Claude workflow

- Unreal must be open.
- Unreal MCP server should be running at `http://127.0.0.1:8000/mcp`.
- Use serial tool calls only; do not issue overlapping Unreal MCP operations.
- Inspect relevant assets before modifying them.
- Keep final reports concise and action-oriented.
- Report exactly what changed, what should be tested, and what files are safe or unsafe to commit.
- After the user confirms the pass condition, ask the user before committing.

## Reporting format after each pass

Use this structure and avoid unnecessary narration:

1. Goal attempted.
2. Files created/modified.
3. What changed.
4. What was not touched.
5. Test performed / not performed.
6. Current `git status --short` summary.
7. Safe-to-commit files, if any.
8. Must-not-commit files.
9. Next recommended pass.

## Near-term build sequence

1. Update and stabilize shared command UI across HUD, physical console, and virtual tablet fallback.
2. Create `/Game/ESCROW/Maps/MAP_ESCROW_FLAT_PILOT_TEST` as the lightweight pilot map.
3. Add scenario markers/zones for phases A-F.
4. Route command/follower spawn and follower staging to the pilot map.
5. Implement one primary disruption: obstacle/constrained passage causing irregular spacing or takeover requirement.
6. Add Rally C plan-repair / waypoint-reset interaction.
7. Add deterministic logging for manual baseline.
8. Only then add AI support scaffolds.
