# Follower AI Architecture Plan

This is the target design for the follower vehicle agents. It should be read with [ESCRoW_Follower_AI_Behavior_Roadmap.md](ESCRoW_Follower_AI_Behavior_Roadmap.md) (current state) and [agent_tasks.md](agent_tasks.md) (implementation steps).

## 1. Design principles

1. **The Director stays deterministic.** Scenario phases, fault onset and ground truth live in `UESCROWScenarioDirectorSubsystem`. The vehicle agents may behave non-deterministically because of physics, but every decision they make is logged.
2. **Separate the layers.** Deciding *what* to do (the decision layer) is kept apart from *how* to drive (the control layer). Either layer can then be replaced without touching the other.
3. **Single source of truth.** Follower state is published to `UESCROWFleetSubsystem`. The HUD, console, tablet and loggers read only from there.
4. **Possession has one owner.** Only `UESCROWPlayerControlRouterComponent` changes who controls a vehicle. The agent reacts to possession; it never causes it.
5. **No vendor edits.** The vendor `NavMeshVehicleAI` and `DD_Vehicles_Basic` stay as base classes or idle components. All behavior lives in ESCRoW-owned classes.

## 2. Layered architecture

```
┌───────────────────────────────────────────────────────────────────────┐
│ SCENARIO LAYER (C++)                                                  │
│ UESCROWScenarioDirectorSubsystem: phases A–F, fault injection, scoring│
└───────────────┬───────────────────────────────────────▲───────────────┘
                │ orders / fault flags                   │ state + events
┌───────────────▼───────────────────────────────────────┴───────────────┐
│ FLEET LAYER                                                           │
│ UESCROWFleetSubsystem (C++): authoritative follower status, takeover  │
│                              lock, messages                           │
│ BP_ESCROW_ConvoyTelemetryManager (BP): convoy model, breadcrumbs,     │
│                              order routing (Escrow_ApplyNavOrder),    │
│                              spacing alerts                           │
└───────────────┬───────────────────────────────────────▲───────────────┘
                │ order (type, target, leader)           │ state change
┌───────────────▼───────────────────────────────────────┴───────────────┐
│ AGENT LAYER: per follower (BP_ESCROW_FollowerVehicle)                 │
│ ┌───────────────────────────────────────────────────────────────────┐ │
│ │ DECISION: StateTree ST_ESCROW_Follower (UStateTreeComponent)      │ │
│ │   evaluators: order, possession, hold/fault, stuck metrics, gap   │ │
│ │   states:     DirectControl | Faulted | Recovering | Waypoint |   │ │
│ │               Rejoin | FollowConvoy                               │ │
│ │   output:     DriveGoal {target, speed cap, mode}                 │ │
│ └────────────────────────────┬──────────────────────────────────────┘ │
│ ┌────────────────────────────▼──────────────────────────────────────┐ │
│ │ CONTROL (existing own-driver, kept as-is):                        │ │
│ │   NavPlan → PickPursuit → SelectHeading (C++ avoidance) →         │ │
│ │   gap governor → Chaos throttle/steer/brake                       │ │
│ └───────────────────────────────────────────────────────────────────┘ │
│ DriveProfile: DA_ESCROW_FollowerDriveProfile (tuning)                 │
└───────────────────────────────────────────────────────────────────────┘
                                │ events
┌───────────────────────────────▼───────────────────────────────────────┐
│ UESCROWEventLogSubsystem: FollowerStateChanged, StuckRecovery,        │
│ AutonomyEscalated, NavPlanFailed, GovernorBlocked, ...                │
└───────────────────────────────────────────────────────────────────────┘
```

## 3. Why StateTree, not a Behavior Tree

- A **Behavior Tree** requires an `AAIController` to possess the pawn. The followers' controller comes from the vendor (`VehicleAIController`), and takeover swaps possession to the player. A BT would either conflict with the vendor controller or need a custom controller, which would touch the takeover flow.
- A **StateTree** with `UStateTreeComponent` runs on any actor, regardless of who possesses it. It has explicit enter/tick/exit per state, which suits continuous driving. Tasks and conditions can be written in Blueprint, so no C++ is needed for the prototype.
- Debugging: the StateTree debugger and Rewind Debugger show the active state over time. This is useful for research review.

## 4. State model

| State | Enter condition (priority order) | Control output | FollowerState / AutonomyMode |
|---|---|---|---|
| **DirectControl** | `IsPlayerControlled` | none (player drives) | ExecutingAction / DirectControl |
| **Faulted** | Director hold or fault active | brake + handbrake | IssueDetected / AwaitingTakeover |
| **Recovering** | stuck or blocked metrics exceeded | reverse → offset replan → flip → escalate | IssueDetected / Degraded (on escalation) |
| **ReturningToAutonomy** | just unpossessed (≤ 3 s) | immediate replan, low speed cap | ReturningToAutonomy / ReturningToAutonomy |
| **MoveToWaypoint** | order 4 | goal = waypoint; arrival radius 450 | MovingToWaypoint → PositionAchieved / Supervised |
| **Rejoin** | order 3 | goal = nearest leader breadcrumb ahead | Rejoining / Autonomous |
| **FollowConvoy** | order 1 or 2 (default) | goal = leader trail point or breadcrumb; gap governor on leader | FollowingConvoy / Autonomous |

Transitions are evaluated top-down each tick, so higher rows pre-empt lower ones. Every transition calls `Escrow_SetBehaviorState(State, Mode, Reason)`, which:

1. updates the vehicle's variables;
2. pushes them to `UESCROWFleetSubsystem`;
3. logs `FollowerStateChanged {from, to, reason, t, loc}`.

## 5. Recovery escalation ladder

```
stuck (speed < 40 cm/s for 2.5 s, > 8 m from goal)
  └─ attempt 1: reverse (existing)
      └─ attempt 2 (within 15 s): replan to goal + 6 m lateral (side = LastSelSign)
          └─ attempt 3: flip side, replan
              └─ attempt 4: Degraded → hold, alert "F# requesting assistance", log AutonomyEscalated
reset attempts after 15 m of progress or on a new order
```

The ladder ends in a request for help instead of a livelock. Escalations are deliberately visible to the operator; that fits the research question about moving from monitoring to intervention.

## 6. Data contracts

**Order** (Convoy manager → agent; already exists through `Escrow_ApplyNavOrder`):
`FollowerID, OrderType {0 None, 1 FollowLive, 2 FollowTrail, 3 Rejoin, 4 Waypoint, 5 Hold, 6 Manual}, TargetVehicleID, TargetLocation, BreadcrumbIndex, Reason`

**DriveGoal** (decision → control; new, internal to the follower):
`Target (vector), SpeedCap (cm/s), Mode {Forward, Reverse, Hold}, LeaderActor (optional), ArrivalRadius`

**Behavior event** (agent → EventLog):
`EventType, FollowerID, FromState, ToState, Reason, Location, ForwardSpeed, ClearDist, Cost, Attempt`

**DriveProfile** (DataAsset):
`PursuitLookahead=900, StuckSpeed=40, StuckTime=2.5, BlockedTime=1.2, ReverseClearStart=700, ReverseClearEnd=450, ReplanInterval=2.0, LateralOffset=600, MinGap, Decel, LateralHalfWidth, CruiseSpeed, SteerLockDeg`

## 7. Migration path (keeps the old path working)

| Step | Change | Fallback |
|---|---|---|
| 1 | Logging + escalation + gap fixes in the current graphs (AT-02…AT-13) | none needed; changes are additive |
| 2 | Add a `CurrentFollowerState` variable and the setter; call it from the existing branches (AT-20/21) | still flag-driven |
| 3 | Add `ST_ESCROW_Follower` behind `bUseStateTree` (default off) (AT-22) | toggle off |
| 4 | Parity testing (T-08), then default on (AT-23) | toggle off |
| 5 | Remove dead flag logic; optionally port the control layer to C++ (AT-40, needs approval) | git revert |

## 8. Open questions for the team

- Should escalation to Degraded count as a scripted disruption in the manual baseline, or only as unscripted noise that we log?
- Is enabling the StateTree plugins (a `.uproject` change) approved for this phase?
- Should formation-slot following (AT-34) be off in the manual baseline and only on in assisted conditions?
- Should the C++ owner of follower state be `UESCROWFleetSubsystem` (preferred), or does a new per-follower component need approval?
