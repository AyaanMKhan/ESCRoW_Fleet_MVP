# ESCRoW Follower AI Behavior: Current State and Roadmap

Scope: autonomous behavior of the follower vehicles (F1–F4): how they decide what to do, how they drive, how they fail, and how the operator sees and controls that. This does **not** cover the operator-facing "AI support" layer (summaries and recommendations). That is a separate, later condition in CLAUDE.md.

Audit date: 2026-10-28. Sources: live editor inspection via Unreal MCP (ESCRoW-owned graphs only; vendor graphs not read), C++ source, and the 2026-10-09 pilot-run logs in `Saved/Logs/`.

---

## 1. Where things are today

### 1.1 Headline finding: there are no Behavior Trees

The project has **no Behavior Tree, Blackboard, EQS, or StateTree assets**, and the C++ module has no `AIModule`, `GameplayTasks`, or `StateTreeModule` dependency. Follower "AI" is a hand-built Blueprint control loop. The decision logic is spread across boolean flags and per-tick functions.

### 1.2 Architecture as it actually runs

```
UESCROWScenarioDirectorSubsystem (C++)
  │  faults, formation checks, phase flow
  │  calls by reflection: Escrow_ApplyNavOrder / Escrow_SetFollowerHold / Escrow_RestoreAfterTakeover
  ▼
BP_ESCROW_ConvoyTelemetryManager (Blueprint "convoy model")
  │  parallel arrays indexed 0..4 (0 = command vehicle):
  │  PathModes, ReferenceVehicleIDs, ActiveTargetLocations, StalledTimers, breadcrumbs (BC_*), alerts...
  │  order types: 0 None, 1 FollowLiveReference, 2 FollowBreadcrumbTrail (rejected), 3 RejoinViaBreadcrumbs,
  │               4 MoveToWaypoint, 5 HoldPosition, 6 ManualTakeover
  ▼
BP_ESCROW_FollowerVehicle (child of vendor BP_MilitaryCar_Extra_Roof)
  EventTick → Escrow_ReleaseAIHandbrake   ← this is the real "brain tick"
      ├─ Escrow_UpdateSwingOut
      ├─ Escrow_ApplyProximityGovernor     (only when own-drive is OFF)
      ├─ Escrow_UpdateFollowTarget         (target = leader pos − forward × FollowTrailDist)
      ├─ Escrow_OwnDriveTick               (only when bOwnDriveActive && !player && !hold)
      │     ├─ Escrow_LidarScan
      │     ├─ Escrow_NavPlan               (FindPathToLocationSynchronously, replans every 2 s)
      │     ├─ Escrow_PickPursuit           (first nav point ≥ 9 m ahead)
      │     ├─ Escrow_SelectHeading         → UESCROWAvoidanceLibrary::ComputeAvoidanceHeading (C++ sphere-sweep fan)
      │     ├─ Escrow_StuckWatch            (speed < 40 for 2.5 s → force BlockedTime)
      │     └─ reverse / forward state + direct Chaos throttle/steer/brake
      └─ hold handling (handbrake + brake)
```

- The vendor `NavMeshVehicleAI` component is effectively **switched off** during own-drive (`SetMaxSpeedMultiplier 0`). ESCRoW drives the Chaos inputs directly. That's good: we own the behavior without touching vendor graphs.
- Behavior "state" is implicit in flags: `bOwnDriveActive`, `bHoldActive`, `bReversing`, `bSwingOutActive`, `bGovernorBraking`, `bGovernorBlocked`, `bMoveCheckPending`, `bDetourFound`. No single variable answers "what is F3 doing right now and why?"
- C++ already defines richer intent enums that the vehicles never drive: `EESCROWFollowerState` (Idle, FollowingConvoy, MovingToWaypoint, Rejoining, IssueDetected, …) and `EESCROWAutonomyMode` (Autonomous, Supervised, AwaitingTakeover, DirectControl, ReturningToAutonomy, Degraded).
- Faults are scripted by the Director. "Follower stops" is implemented as order 5 (Hold) plus re-asserting it. The vehicle itself has no notion of "I am faulted".

### 1.3 Test evidence

| Check | Result |
|---|---|
| Static read of all `Escrow_*` follower functions + convoy `ApplyNavOrder` | Done; summarized above |
| `Escrow_PickPursuit` loop (suspected missing break) | **OK**: `Break` is wired after `SetPathIdx` |
| PIE smoke test (2026-10-28) | Editor level was `MAP_ESCROW_MAIN_MENU`, so only the menu loaded; no convoy. A full run needs the menu→setup flow and a human driver. |
| 2026-10-09 pilot log (`ESCRoW_Fleet_MVP-backup-2026.10.09-23.49.06.log`) | **129× `STUCK RECOVERY F3`, 94× `STUCK RECOVERY F4`**, 7× reverse F2. F3 looped stuck→reverse→stuck every ~2.5 s for ~5 min (18:44:37 → 18:49:59) and never escaped. |
| Same log, nav build | `RecastNavMesh-ESCROW_Vehicle: AgentMaxStepHeight (35) … not high enough … AgentMaxSlope 44`: nav mesh config warning |
| Same log, run P01 | `ScenarioRuntimeFailed` on the flat pilot map (P02/P03 reached Ready); root cause not yet investigated |

### 1.4 Concrete problems found

1. **Stuck-recovery livelock.** `StuckWatch` only re-triggers reversing; it never escalates (wider replan, alternate goal, ask for help). Rear vehicles (F3/F4) are the worst, likely because they are boxed in by the vehicles ahead.
2. **No inter-vehicle gap control in own-drive.** When `bOwnDriveActive`, `ReleaseAIHandbrake` sets `GovAllowedSpeed = 100000` and skips the proximity governor. Spacing then relies only on the trail-distance target and lidar sweeps treating other pawns as obstacles. That explains bunching, and other followers become "obstacles" that cause stuck loops.
3. **Recovery events are invisible to research logs.** Reverse, stuck, and governor-blocked events go only to `PrintString`. They never reach `UESCROWEventLogSubsystem`, so we can't measure autonomy struggles or correlate them with operator detection.
4. **Implicit state machine.** Transitions are scattered `if` checks across 6+ functions. Adding a state like "Degraded" or "RequestingHelp" means touching several of them.
5. **Hard-coded fleet size.** IDs 1–4 and `range 1 5` appear in `ApplyNavOrder` and `TickPathManagement`; the convoy order is fixed at `F2→F1→F3→F4` by array index.
6. **Per-tick cost.** The governor calls `GetAllActorsOfClass` (vendor class) every tick per follower; there is a synchronous nav query every 2 s per follower; 29 sphere sweeps × up to 4 steps per tick. Fine for 4 vehicles, but it won't scale.
7. **Convoy manager carries test cruft.** `NavSeq_*`, `NavTest_*`, `Escrow_Test*_F1`, and `bDebugTestMode` all share the class with live logic.
8. **Order 2 (`FollowBreadcrumbTrail`) is always rejected**, even though breadcrumb infrastructure exists.

---

## 2. What we can implement

Priorities: **P0** fixes current failures, **P1** gives structure and research value, **P2** adds richer autonomy, **P3** comes later or is optional.

### P0: Stabilize current behavior (no new architecture)

| # | Item | Where | Done when |
|---|---|---|---|
| 0.1 | **Escalating stuck recovery**: reverse → replan with a lateral offset goal → try an alternate pursuit side → after N failures set `AutonomyMode = Degraded` and raise a "requesting assistance" alert instead of looping forever | `Escrow_StuckWatch`, `Escrow_OwnDriveTick` | No follower logs >3 consecutive stuck recoveries without escalating |
| 0.2 | **Re-enable gap keeping in own-drive**: compute the governor speed against the *assigned leader only* (cheap) and feed it into OwnDriveTick's speed min | `Escrow_ReleaseAIHandbrake`, `Escrow_ApplyProximityGovernor` | Follower gap stays within `MinimumSafeBuffer`..`LaggingDistanceThreshold` (600–3500 cm) on straight road |
| 0.3 | **Log autonomy events** (ReverseStart/End, StuckRecovery, GovernorBlocked, NavPlanFailed, Escalated) to `UESCROWEventLogSubsystem` with follower ID, location, and speed | follower BP → existing EventLog BP library | Events appear in `scenario_events.jsonl` |
| 0.4 | Fix the nav mesh agent warning (`AgentMaxStepHeight` ≥ 40 or lower `AgentMaxSlope`) on the pilot map's `ESCROW_Vehicle` agent | Project nav settings | No Recast warning at PIE start |
| 0.5 | Investigate P01 `ScenarioRuntimeFailed` on the flat pilot map | Director logs | Root cause noted / fixed |
| 0.6 | Cache leader/neighbor refs instead of calling `GetAllActorsOfClass` per tick | Governor | Removed from tick path |

### P1: Make behavior explicit and observable

**1.1 Choose the decision framework: StateTree (recommended) vs Behavior Tree**

| | StateTree | Behavior Tree |
|---|---|---|
| Needs AIController possession | No (`UStateTreeComponent` works on any actor) | Yes, which conflicts with the vendor `VehicleAIController` / takeover possession flow |
| Fits continuous vehicle control | Yes; states have tick + enter/exit | Okay, but tasks are latent and better suited to discrete actions |
| Blueprint-authorable tasks/conditions | Yes | Yes |
| Debugger / rewind | StateTree debugger + Rewind Debugger | BT debugger |
| C++ needed | None for BP tasks; module dep only if C++ tasks are added later (**ask first**, per CLAUDE.md) | AIModule dep |

**Recommendation:** a StateTree on `BP_ESCROW_FollowerVehicle` handles the *decision layer* only. Existing functions become tasks: `Escrow_OwnDriveTick` = "Drive to target", `Escrow_SetHoldActive` = "Hold", and so on. The low-level driver (avoidance, pursuit, Chaos inputs) stays as-is. Possession and takeover remain owned by `UESCROWPlayerControlRouterComponent`.

**1.2 Proposed follower state model** (maps onto the existing `EESCROWFollowerState` / `EESCROWAutonomyMode`)

```
Root
├─ DirectControl          [cond: IsPlayerControlled]           → pause tree, no inputs
├─ Faulted / Hold         [cond: bHoldActive or Director fault] → brake, report AwaitingTakeover
├─ Recovering             [cond: stuck/blocked]                 → Reverse → Replan(offset) → Escalate → Degraded
├─ Order: MoveToWaypoint  → Drive → Arrive → PositionAchieved
├─ Order: Rejoin          → breadcrumb rejoin → FollowingConvoy
└─ Order: FollowConvoy    → Follow leader w/ gap keeping (default)
```

**1.3 Single source of truth for state.** Each follower writes its current state and autonomy mode to `UESCROWFleetSubsystem` on every transition. HUD, console, and tablet read from there. This matches the CLAUDE.md requirement that "the same supervisory state should drive HUD, console, and tablet".

**1.4 Transition logging.** Log every state transition (`FollowerStateChanged`: from, to, reason, time). That gives clean ground truth for detection-latency analysis: "autonomy degraded at t, operator selected the follower at t+Δ".

**1.5 Debug overlay.** Add a toggleable per-follower world text/line overlay showing current state, pursuit point, chosen heading, clear distance, and gap to leader. This replaces the scattered `PrintString` calls.

### P2: Richer convoy autonomy

| # | Item | Notes |
|---|---|---|
| 2.1 | **Data-driven tuning**: move the magic numbers (900 cm pursuit, 2.5 s stuck, 1.2 s blocked, 700/450 reverse clear, speeds) into a `DA_ESCROW_FollowerDriveProfile` DataAsset | Enables per-condition profiles and avoids edits to the graphs |
| 2.2 | **Formation behavior**: slot-based formation following (offset from leader in leader space), used for the Rally B breakout | Currently formation is operator-driven only |
| 2.3 | **Breadcrumb trail follow (order 2)**: enable it with existing `BC_*` data so followers trace the leader's path through narrow passages instead of cutting corners | Directly helps the Phase C constrained-passage disruption |
| 2.4 | **Narrow-passage / single-file mode**: when clear width is under a threshold, switch to single-file, increase the gap, and lower the speed | Clean, deterministic disruption trigger |
| 2.5 | **Degraded autonomy modes as fault types**: comms loss (stale state, keeps last order), sensor degradation (shorter lidar range → slower, more cautious), drift/lag (offset target) | Gives the Director more fault types than "stop" |
| 2.6 | **Safe return-to-autonomy**: after takeover release, run a short "re-acquire" state (replan to the nearest breadcrumb, then resume the order) instead of immediately resuming | Replaces part of the manual `Escrow_RestoreAfterTakeover` patching |
| 2.7 | **Generic fleet size**: replace the `1..4` constants and fixed chain with a registry from `UESCROWFleetSubsystem` | Needed for scaling conditions |
| 2.8 | **Split convoy manager**: move `NavSeq_*` / `NavTest_*` test functions into a separate `BP_ESCROW_ConvoyNavTestHarness` | Smaller, safer live class |

### P3: Later / optional

- **Port the per-tick driver to C++** (an `UESCROWFollowerDriveComponent`) if Blueprint tick cost or readability becomes a problem. Requires approval for new C++.
- **Automated behavior tests**: an AutomationTest map with scripted obstacle courses (straight, slalom, narrow gate, dead end), measuring time-to-goal, stuck count, min gap, and collisions. MCP's `AutomationTestToolset` can run these.
- **Deterministic replay**: record follower inputs per tick so a session's autonomy behavior can be replayed exactly (physics is otherwise non-deterministic).
- **Hooks for AI-supported conditions**: expose state/transition data cleanly so a later AI assistant can *read* it. The assistant never drives the vehicles and is never the ground truth.

---

## 3. Suggested first pass

1. Add P0.3 (event logging of autonomy struggles) first. It is low-risk and makes every later change measurable.
2. Do P0.1 + P0.2 (escalating recovery + gap keeping) and verify against the F3/F4 stuck pattern.
3. Prototype a StateTree with just three states (FollowConvoy / Hold / Recovering) wrapping existing functions, behind a `bUseStateTree` toggle so the current path stays available.

### Test protocol for each pass

1. Open `MAP_ESCROW_MAIN_MENU` → run a FLAT_PILOT mission (Driver + Takeover, low complexity).
2. Drive the command vehicle start → Rally B → toward Rally C, including one tight turn.
3. Pass criteria: no stuck loop >3 cycles, no follower-follower collisions, gaps within 6–35 m on straights, and both pilot faults (F2 early, F4 late) still trigger, take over, and release correctly.
4. Grep the session log for `STUCK|REVERSE|GOV BLOCKED` counts and compare them with the 2026-10-09 baseline (F3: 129, F4: 94).

---

## 4. Guardrails (from CLAUDE.md)

- Change only ESCRoW-owned assets (`BP_ESCROW_FollowerVehicle`, `BP_ESCROW_ConvoyTelemetryManager`, new ESCRoW assets). Never edit or read `NavMeshVehicleAI` / `DD_Vehicles_Basic` graphs.
- Takeover and possession stay exclusively in `UESCROWPlayerControlRouterComponent`.
- Scenario timing and fault onset stay deterministic in the Director; autonomy behavior may vary, but it must be *logged*.
- Ask before adding new C++ or module dependencies. Save only intentionally modified assets; no Save All.
