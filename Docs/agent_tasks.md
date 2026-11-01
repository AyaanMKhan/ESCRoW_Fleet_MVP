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

### [ ] AT-10: Escalating stuck recovery (BP)
- Add `StuckAttempts` (int) and `LastStuckTime` (float) variables.
- Change `Escrow_StuckWatch` so each failure escalates:
  1. 1st stuck → reverse (current behavior).
  2. 2nd within 15 s → `Escrow_NavPlan` to a goal offset ±6 m sideways (the side picked by `LastSelSign`).
  3. 3rd → flip `LastSelSign` and replan again.
  4. 4th → set `bAutonomyDegraded = true`, hold the vehicle, and log `AutonomyEscalated`. The fleet status shows "Requesting assistance".
- Reset `StuckAttempts` after the vehicle moves 15 m.
- **Done when:** **T-04** passes (no more than 3 consecutive recoveries without escalating) and **T-01** stuck counts drop well below the baseline.

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
- [ ] **AT-42:** Per-tick input recording for deterministic replay.
- [ ] **AT-43:** A read-only state feed for the later AI-support condition.
