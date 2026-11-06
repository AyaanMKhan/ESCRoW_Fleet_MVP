# Follower AI Agent: Implementation Tasks

Work through these tasks in order. Each one is a self-contained, testable pass sized for one sitting. Background is in [ESCRoW_Follower_AI_Behavior_Roadmap.md](ESCRoW_Follower_AI_Behavior_Roadmap.md), the target design is in [ESCRoW_Follower_AI_Architecture.md](ESCRoW_Follower_AI_Architecture.md), and test IDs (`T-xx`) refer to [ESCRoW_Follower_AI_Test_Plan.md](ESCRoW_Follower_AI_Test_Plan.md).

**Rules for every task:** edit ESCRoW-owned assets only (never `NavMeshVehicleAI` / `DD_Vehicles_Basic`). Save only the assets you changed (no Save All). Ask before adding C++ or module dependencies. Don't commit until the pass condition is confirmed.

Legend: `[ ]` todo · `[~]` in progress · `[x]` done · **BP** = Blueprint · **C++** = needs approval

---

## Phase 0: Baseline and observability (start here)

### [ ] AT-01: Record the baseline run
- Run the flat pilot mission (Driver + Takeover, low complexity) using test **T-01**.
- Save the counts of `STUCK RECOVERY`, `REVERSE START` and `GOV BLOCKED` per follower into the Test Plan's baseline table.
- **Files:** none (docs only).
- **Done when:** the baseline table is filled in for today's build. (Last known values from 2026-10-09: F3 = 129 stuck, F4 = 94 stuck.)

### [~] AT-02: Send autonomy events to the structured log (BP)
- In `BP_ESCROW_FollowerVehicle`, add a function `Escrow_LogAutonomyEvent(EventType, Detail)` that calls the existing EventLog Blueprint library (the same one `EventHit` uses for collisions).
- Call it next to each existing `PrintString`:
  - in `Escrow_OwnDriveTick`: `ReverseStart` and `ReverseEnd`;
  - in `Escrow_StuckWatch`: `StuckRecovery`;
  - in `Escrow_ApplyProximityGovernor`: `GovernorBlocked`.
- Payload: FollowerID, location, forward speed, `SelCost`, `SelDist`.
- **Done when:** test **T-02** passes and the events appear in `scenario_events.jsonl`.
- **Status (2026-11-01):** implemented, compiled, saved; awaiting T-02 run. Rows use `EventType` = `AutonomyReverseStart` / `AutonomyReverseEnd` / `AutonomyStuckRecovery` / `AutonomyGovernorBlocked`, `AffectedFollowerID` = follower, `PayloadJSON` = `{FollowerID, LocX, LocY, LocZ, ForwardSpeedCms, SelCost, SelDist, Detail}`. Only written while a run is active.

### [ ] AT-03: Log NavPlan failures (BP)
- In `Escrow_NavPlan`, when the path comes back empty (the branch that falls back to `[Final]`), log `NavPlanFailed` with the start and goal positions.
- **Done when:** driving a follower target into an unreachable spot logs the event once per replan, without spamming.

### [ ] AT-04: Fix the nav mesh agent warning
- In the project nav settings for the `ESCROW_Vehicle` agent, either set `AgentMaxStepHeight` to 40 or more, or lower `AgentMaxSlope` below 44.
- **Done when:** no `RecastNavMesh-ESCROW_Vehicle: AgentMaxStepHeight` warning appears at PIE start, and **T-03** (nav self-check) still passes.

### [ ] AT-05: Find the root cause of `ScenarioRuntimeFailed`
- Session P01 on 2026-10-09 reported `ScenarioRuntimeFailed` on `MAP_ESCROW_FLAT_PILOT_TEST`. Find out which check failed in `UESCROWScenarioDirectorSubsystem`.
- **Done when:** the cause is written up here, plus a fix or a follow-up task.

---

## Phase 1: Fix current failures

### [~] AT-10: Escalating stuck recovery (BP)
- Add `StuckAttempts` (int) and `LastStuckTime` (float) variables.
- Change `Escrow_StuckWatch` so each failure escalates:
  1. 1st stuck → reverse straight, then aim at a point 6 m to the side, away from the last rock hit (or by `LastSelSign` if there was no recent hit).
  2. 2nd within 20 s → reverse with the wheels turned to swing the nose to that side, then aim at the same kind of point.
  3. 3rd → flip the side and repeat.
  4. 4th → set `bAutonomyDegraded = true`, hold the vehicle, and log `AutonomyEscalated`. The objective shows "Requesting assistance", `RequestTakeover` sets the takeover flag, and `PostNotification` raises the "NEEDS ASSISTANCE" alert.
- Reset `StuckAttempts` after the vehicle moves 15 m.
- Taking over the follower clears the held state and attempt count.
- **Done when:** **T-04** passes (no more than 3 consecutive recoveries without escalating) and **T-01** stuck counts drop well below the baseline.
- **Status (2026-11-06):** implemented, compiled, saved. Tested with **Harness scenario 4**: on a real rock wedge LYNX escalates about 9 s after first getting stuck, posts the alert, clears on takeover, and escalates again if nobody drives it clear. The reverse is capped at 2.5 m/s and 8 m, because full throttle uncapped ran 26 m backward into more rocks. Stuck detection now applies beyond 5 m from the target (was 8 m). Steering at the side goal and the side flip on attempt 3 are confirmed. Not confirmed: a follower actually freeing itself from rocks in a repeatable test, and the 15 m attempt reset. New `Escrow_EscalateStuck`, `Escrow_EscalateToOperator`, `Escrow_ApplyRecoverySteer`, `Escrow_ClearStuckRecovery` and `Escrow_RecoveryGoalActive`. `EventHit` stores `LastHitLoc` and `LastHitTime` for non-vehicle hits. Rows use `EventType` = `AutonomyRecoveryAttempt` / `AutonomyEscalated` / `AutonomyEscalationCleared`. The 15 s window from the original task became 20 s because recoveries in the 2026-11-06 run were up to 12.5 s apart. `EventUnpossessed` also fires when the AI hands over, so `Escrow_ClearStuckRecovery` runs at takeover start and must not change the autonomy mode. More AND/OR inputs that `read_graph_dsl` doesn't show were added in `Escrow_OwnDriveTick` and `Escrow_ReleaseAIHandbrake`.

### [ ] AT-11: Restore gap keeping in own-drive (BP)
- In `Escrow_ReleaseAIHandbrake`, stop forcing `GovAllowedSpeed = 100000` when `bOwnDriveActive`. Instead, compute the allowed speed against the **assigned leader only** (`FollowTargetActor`).
- `Escrow_OwnDriveTick` already takes the minimum against `GovAllowedSpeed`, so no change is needed there.
- **Done when:** **T-05** passes (gaps stay between 6 m and 35 m on straights, with no rear-end collisions).

### [ ] AT-12: Cache neighbor refs (BP)
- Replace the per-tick `GetAllActorsOfClass` call in the governor with a cached array, filled in `Escrow_RegisterWithFleet` (or read from the convoy manager's `VehicleRefs`).
- **Done when:** the Unreal Insights / `stat game` tick cost for followers drops, and behavior is unchanged under **T-05**.

### [ ] AT-13: Gentler return after takeover (BP)
- After `EventUnpossessed` → `Escrow_RestoreAfterTakeover`, force an immediate `Escrow_NavPlan` and set `StuckAttempts = 0`, so the follower doesn't resume with an old path.
- **Done when:** **T-06** passes (the follower rejoins within 30 s after release with no reverse loop).

### [~] AT-14: Stop parked followers reversing to reach the trail point (BP)
- **Problem:** a follower's target is a point about 16 m directly behind its leader. A follower parked closer than that has its target behind it, so assigning a follow target starts a reverse (logged as "Blocked or stalled; reversing to clear") and can start a wide full-lock loop.
- **A: Close-range hold.** While the leader is stopped and the gap to it is under about 18 m, the follower stays parked. It starts when the leader moves away or the gap opens. Use a separate, larger start threshold so it doesn't jitter between hold and drive.
- **C: Reverse only when blocked.** A target behind the vehicle must not start a reverse. Reverse stays for real obstacles and stalls (see AT-10).
- A gap under the 6 m minimum from **T-05** also holds instead of reversing.
- If the follower is ahead of its leader and within 30 m, it stays parked until the leader passes and shows "Waiting for leader". Beyond 30 m it drives normally, so a far-off follower can't deadlock the Rally A gather.
- The 18 m hold and 20 m release values are starting points; tune them in play. **D** (following the leader's track) stays in AT-31.
- **Done when:** assigning the full chain to a parked fleet gives no reverse events and no wide loops; all four followers start in order when the command vehicle drives off; and **T-05** and **T-01** do not get worse.
- **Status (2026-11-06):** implemented, compiled, saved. Play test on the desert map: parked followers no longer reverse on assignment and the gap looks right. T-05 and T-01 still to run. New `Escrow_UpdateFollowGapHold` and `Escrow_EndFollowGapHold`, with `bFollowGapHold`, `bFollowGapWaiting`, `bFollowHasLeader` and `FollowGapSavedObjective`. Rows use `EventType` = `AutonomyFollowHoldStart` / `AutonomyFollowHoldEnd`. The "target behind" reverse now applies only when `bFollowHasLeader` is false, so waypoint reversing is unchanged. Two ANDs in `Escrow_OwnDriveTick` now have a third input that `read_graph_dsl` doesn't show, so don't rewrite that graph from its DSL.

### [~] AT-15: Stop followers driving to the edge of the nav mesh (BP)
- **Problem:** when the target is off the nav mesh, `FindPathToLocationSynchronously` returns a partial path ending at the mesh edge. `Escrow_NavPlan` only fell back to the target when the path was empty, so the follower steered at that edge point and could circle it forever while its real target was metres away. Reproduced with **Harness scenario 1** (WOLF at about 8 m from its target steered at a point frozen at `6700, -11100`).
- **Fix:** if the path is partial, or its last point is more than 3 m from the target, `Escrow_NavPlan` replaces it with the target itself and the avoidance fan does the steering.
- **Done when:** scenario 1 shows no circling, and **T-01** does not get worse.
- **Status (2026-11-06):** implemented, compiled, saved. Scenario 1 passes (largest steering 22 degrees, stops 18 m behind HAWK). Desert baseline passes (no reverses, no stuck events). **T-01** on the flat pilot still to run.

---

## Phase 2: Explicit state and the StateTree

### [ ] AT-20: Follower state enum on the vehicle (BP)
- Add `CurrentFollowerState` (`EESCROWFollowerState`) and `CurrentAutonomyMode` (`EESCROWAutonomyMode`) variables.
- Add a setter `Escrow_SetBehaviorState(State, Mode, Reason)` that logs `FollowerStateChanged` (from, to, reason) and only fires on an actual change.
- Call it from the existing branch points: hold, takeover, reverse, degraded, arrival.
- **Done when:** **T-07** passes (the transition log matches what you saw on screen).

### [ ] AT-21: Push state to the Fleet subsystem
- Write the current state and mode into `UESCROWFleetSubsystem`'s follower status on every transition, so the HUD, console and tablet all read it from one place.
- This uses the existing BP library; check whether a setter is exposed. **If it needs a new C++ setter, ask first.**
- **Done when:** the follower card on the HUD shows the live state.

### [ ] AT-22: Enable StateTree and build the prototype (BP)
- Enable the `StateTree` and `GameplayStateTree` plugins (**confirm with the team first**: this changes the `.uproject`).
- Create `ST_ESCROW_Follower`, with `StateTreeComponent` as the schema, plus Blueprint tasks:
  - `STT_ESCROW_FollowConvoy` (wraps the existing own-drive path),
  - `STT_ESCROW_Hold`,
  - `STT_ESCROW_Recover`.
- Add a `bUseStateTree` toggle on the follower (default **false**) so the old path keeps working.
- **Done when:** **T-08** passes, i.e. both toggle values complete the pilot mission.

### [ ] AT-23: Make the StateTree the default
- Add the MoveToWaypoint and Rejoin states, and flip `bUseStateTree = true`.
- Remove the now-dead flag checks only after **T-01** passes on both settings.

---

## Phase 3: Richer convoy autonomy

### [ ] AT-30: `DA_ESCROW_FollowerDriveProfile` data asset
- Move these magic numbers into a data asset: pursuit distance 900, stuck time 2.5 s, blocked time 1.2 s, reverse clearances 700 and 450, cruise and steering lock, governor parameters.
- Create two profiles: `Default` and `Cautious`.

### [ ] AT-31: Breadcrumb trail follow (order 2)
- Stop `Escrow_ApplyNavOrder` rejecting order 2. Make the follower pursue the leader's `BC_Locations` instead of the live position.
- **Done when:** **T-09** passes (in the narrow gate, the followers trace the leader's line).

### [ ] AT-32: Narrow-passage single-file mode
- When `SelDist` stays below about 8 m on both sides, switch to single file: a larger gap and a slower cap. Log `SingleFileEnter` and `SingleFileExit`.

### [ ] AT-33: Degraded-autonomy fault types for the Director
- Add `SensorDegraded` (shorter lidar range plus lower speed) and `CommsStale` (follower keeps its last order and stops updating status). These sit beside the existing "stop" fault.
- The Director must trigger them deterministically. **C++ change: ask first.**

### [ ] AT-34: Formation slot following
- Add leader-relative slot offsets for the Rally B breakout, as an optional autonomous assist (off in the manual baseline).

### [ ] AT-35: Separate the test harness
- Move the `Escrow_NavSeq_*`, `Escrow_NavTest_*` and `Escrow_Test*_F1` functions out of `BP_ESCROW_ConvoyTelemetryManager` into `BP_ESCROW_ConvoyNavTestHarness`.

### [ ] AT-36: Generic fleet size
- Replace the hard-coded `1..4` ranges and the fixed `F2→F1→F3→F4` chain with a list taken from the Fleet subsystem.

---

## Phase 4: Later (needs approval)

- [ ] **AT-40:** Port the per-tick driver to a C++ `UESCROWFollowerDriveComponent`.
- [ ] **AT-41:** Build an automated obstacle-course map plus automation tests (**T-10**).
  - **Started 2026-11-06:** `/Game/ESCROW/Blueprints/Test/BP_ESCROW_TestHarness` runs without the menu or player input. Set `bRunTestHarness` on the `BP_ESCROW_GameMode` class defaults for the session only, and never save it as true. `ScenarioId` 0 assigns the follow chain and drives the desert route, 1 recreates the WOLF circling position, and 2 forces a LYNX escalation. It sets `bDriveDiagLog` on the followers, which writes `DIAG` lines to the output log.
- [ ] **AT-42:** Per-tick input recording for deterministic replay.
- [ ] **AT-43:** A read-only state feed for the later AI-support condition.
