# ESCRoW Robust Long-Term MVP Build Queue

This document is the authoritative pass-by-pass build sequence for the ESCRoW Fleet MVP under the robust/hybrid architecture. It supersedes the earlier interaction-first `P01`–`P10` sequence in `CLAUDE.md`.

## Why hybrid (C++ + Blueprint)

- The earlier interaction-first plan correctly prioritized the command HUD, follower monitoring, messaging, waypoint/tasking, camera/feed views, and one-at-a-time remote takeover.
- Those interaction priorities remain the product goal. What changes is where the state and logic live:
  - **Source-controlled C++** owns stable ESCRoW core types and runtime systems — the things that must be correct, reviewable, and hard to accidentally break from a widget graph.
  - **Blueprint/UMG** owns HUD layout, follower cards/tabs, map panels, feed panels, vehicle wrappers, camera presentation, and marketplace-asset adapters.
- Marketplace/vendor assets (`DD_Vehicles_Basic`, `NavMeshVehicleAI`, `MapSystem`, `BattleField_Desert`, `CLM_MenuSystem`) remain black boxes. They are only ever touched through ESCRoW-owned wrapper/adapter Blueprints or components.

## Core C++ systems

- `UESCROWSessionSubsystem` — stores participant/session/selected-mission state.
- `UESCROWFleetSubsystem` — the authoritative world-specific state, command, message, report, and takeover-lock bus for F1-F4.
- `UESCROWPlayerControlRouterComponent` — the only object/component allowed to change player control mode.
- `UESCROWEventLogSubsystem` — handles structured logging.

## Process rules for every RA pass

- Each RA pass is bounded and requires manual user approval before starting.
- Build/test/commit between passes. Do not chain multiple RA passes into one uncommitted block of work.
- Preserve working behavior unless a pass explicitly changes it.
- Do not edit marketplace/vendor asset source folders or rewrite vendor Blueprints. Use ESCRoW-owned wrappers/adapters only.
- Compile and save after every focused pass.
- After a pass, report exactly what changed and what should be tested, then wait for the user to confirm the pass condition before starting the next RA pass.

## RA build queue

**RA00 — Update project guidance to the robust long-term architecture**
- Create this document and update `CLAUDE.md` to remove ambiguity about the hybrid C++/Blueprint architecture.
- Filesystem/source-control text edits only. No Content/maps/widgets/vehicles/vendor changes. No build.

**RA01 — C++ interaction data model**
- Add the core ESCRoW C++ types (enums/structs) for mission condition, player control mode, follower state, autonomy mode, follower status, directive, command, message, report, takeover-lock, and event log entry.
- Acceptance: project compiles; no gameplay behavior changes.

**RA02 — UESCROWFleetSubsystem**
- Implement the authoritative world-specific state/command/message/report/takeover-lock bus for F1-F4.
- Acceptance: subsystem exists, initializes in the desert map, and can report basic follower status.

**RA03 — UESCROWSessionSubsystem**
- Implement participant/session/selected-mission state, persistent across level transitions (menu → desert map).
- Acceptance: subsystem exists, survives the menu-to-desert transition, and holds a selected mission value.

**RA04 — PlayerControlRouter component shell**
- Implement `UESCROWPlayerControlRouterComponent` as the sole authority for changing player control mode (supervisor/operator vs. remote takeover), with no behavior wired to vehicles yet.
- Acceptance: component exists, compiles, exposes a control-mode change API, and no other object changes control mode directly.

**RA05 — EventLogSubsystem shell**
- Implement `UESCROWEventLogSubsystem` with a structured log-entry API, no callers wired yet.
- Acceptance: subsystem exists and can write a test structured log entry.

**RA06 — Blueprint bridge/facade utilities (if needed)**
- Add thin Blueprint-callable facade functions/library nodes over the C++ subsystems/components above, only if direct BlueprintCallable exposure proves awkward for HUD/wrapper use.
- Acceptance: Blueprints can read/write fleet, session, control-mode, and log state through ESCRoW-owned facades only.

**RA07 — Command HUD shell**
- Expand `WBP_ESCROW_TestHUD` (or a new ESCROW-owned HUD widget) into the command-console shell: left/center/right panels, preserving the existing minimap.
- Acceptance: UI appears, does not block vehicle entry/driving, and displays static/mock data.

**RA08 — HUD focus/input behavior**
- Wire HUD focus/input handling so the command console does not fight vehicle driving input.
- Acceptance: player can freely switch between driving and HUD interaction without input lockups.

**RA09 — Follower tabs/cards wired to FleetSubsystem**
- Bind follower cards for F1-F4 to live `UESCROWFleetSubsystem` state instead of mock data.
- Acceptance: HUD follower cards reflect real subsystem state.

**RA10 — Lead/follower messaging**
- Implement directive/message flow between the player and followers through `UESCROWFleetSubsystem`, surfaced in the HUD message log.
- Acceptance: a message sent through the system appears in the HUD log.

**RA11 — Waypoint/task/action command plumbing**
- Implement command plumbing (assign follower to task/waypoint) from HUD through `UESCROWFleetSubsystem` to a follower vehicle stub.
- Acceptance: a task command reaches a follower and updates fleet subsystem state.

**RA12 — Full map and waypoint selection**
- Add map-based waypoint selection UI feeding the RA11 command plumbing.
- Acceptance: user can pick a map location and assign it as a follower task target.

**RA13 — Follower feed/camera panel**
- Add a HUD panel showing a follower's camera/feed view.
- Acceptance: feed panel displays a selected follower's view.

**RA14 — Takeover lock UI**
- Surface `UESCROWFleetSubsystem` takeover-lock state in the HUD (who holds/needs takeover).
- Acceptance: takeover-required state is visible in HUD, map marker, and message log.

**RA15 — Actual remote takeover**
- Use `UESCROWPlayerControlRouterComponent` to route camera/input/control to a follower vehicle (starting with F3).
- Acceptance: user can take manual control of the target follower.

**RA16 — Handback/return to autonomy**
- Return control from the takeover target back to autonomous follower behavior and restore command/supervisor mode via the control router.
- Acceptance: user can hand back control and resume supervising.

**RA17 — Physical waypoint movement through adapter/fallback**
- Ensure RA11/RA12 task assignment drives actual follower movement via the `NavMeshVehicleAI` adapter (or a documented fallback if the plugin blocks progress).
- Acceptance: assigned follower visibly moves to the selected waypoint.

**RA18 — ScenarioDirector**
- Implement `BP_ScenarioDirector`, driven by the C++ subsystems, to own deterministic mission timing.
- Acceptance: a timed directive appears deterministically after mission start.

**RA19 — Facilitator controls**
- Add facilitator hotkeys/panel (toggle panel, next directive, anomaly, force takeover, force recovery complete, mark report ready, reset).
- Acceptance: demo can be advanced/recovered manually.

**RA20 — Mission D stitch**
- Wire the full Mission D path end-to-end: directives → tasking → anomaly → takeover → handback → report → mission end.
- Acceptance: Mission D can be run start to finish.

**RA21 — Mission A/B/C condition flags**
- Add mission condition flags/branches for Missions A, B, and C.
- Acceptance: each mission selection produces a distinguishable condition path.

**RA22 — Logging completion**
- Ensure `UESCROWEventLogSubsystem` logs mission start, mode changes, directives, follower commands, state transitions, takeover, handback, reports, and mission end to `Saved/Logs/ESCRoW/`.
- Acceptance: a full mission run produces a readable structured log file.

**RA23 — Demo hardening**
- Pass for stability/edge-case hardening ahead of a live demo. No new features.
- Acceptance: demo runs through Mission D without manual recovery from facilitator controls.
