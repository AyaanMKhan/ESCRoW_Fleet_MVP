# Follower AI Test Plan

This plan tests each roadmap task in [agent_tasks.md](agent_tasks.md). Run every test in PIE from `MAP_ESCROW_MAIN_MENU`, using the flat pilot mission (**Driver + Takeover, Low Complexity, HUD Primary**) unless a test says otherwise.

## Where to read results

- **Output log** (`Saved/Logs/ESCRoW_Fleet_MVP.log`): count matches with
  `grep -oE "STUCK RECOVERY F[0-9]|REVERSE START F[0-9]|GOV BLOCKED F[0-9]" Saved/Logs/ESCRoW_Fleet_MVP.log | sort | uniq -c`
- **Structured logs**: the run folder printed by `ESCROWEventLogSubsystem: run … started at …` contains `scenario_events.jsonl` and `participant_actions.jsonl`.
- **Live**: `showdebug`, plus the follower debug overlay once AT-20 is in.

## Baseline table

Fill in a row after each pass. The 2026-10-09 row comes from `ESCRoW_Fleet_MVP-backup-2026.10.09-23.49.06.log`.

| Date / build | Task | F1 stuck | F2 stuck | F3 stuck | F4 stuck | Reverses (total) | Gov blocked | Collisions F↔F | Notes |
|---|---|---|---|---|---|---|---|---|---|
| 2026-10-09 | (pre-roadmap) | 1 | 1 | 129 | 94 | 10 | 0 | ? | F3 looped for about 5 min |
| | AT-01 | | | | | | | | |

## Standard route (used by T-01, T-05, T-06)

1. Start at Rally A and wait 10 s with the convoy idle.
2. Drive to Rally B at normal speed, including at least one turn of 60° or more.
3. Complete the Rally B formation.
4. Drive toward Rally C. Handle the **F2 early fault** and the **F4 late fault**: take over, drive to the recovery point, release.
5. Complete Rally C plan repair and closeout.

## Tests

| ID | Name | Covers | Steps | Pass criteria |
|---|---|---|---|---|
| **T-01** | Full pilot regression | all | Standard route end to end | Mission completes; both faults trigger and resolve; stuck counts ≤ the previous row; no follower left behind more than 35 m when not faulted |
| **T-02** | Autonomy events logged | AT-02 | Run T-01, then open `scenario_events.jsonl` | Every `REVERSE START` / `REVERSE END` / `STUCK RECOVERY` / `GOV BLOCKED` output-log line has a matching `AutonomyReverseStart` / `AutonomyReverseEnd` / `AutonomyStuckRecovery` / `AutonomyGovernorBlocked` row (same count per follower). Check: `grep -c '"EventType":"AutonomyStuckRecovery"' scenario_events.jsonl` |
| **T-03** | Nav self-check | AT-04 | Start the mission and wait 15 s | `NavDiagnostics` reports that every follower can path to the command vehicle; no Recast agent warning |
| **T-04** | Stuck escalation | AT-10 | Park the command vehicle so a follower's target is behind a wall or rock cluster; wait 60 s | Escalation goes reverse → offset replan → flip → `AutonomyEscalated`. No more than 3 consecutive `StuckRecovery` events without escalating. The follower ends Degraded/Hold, not looping |
| **T-05** | Gap keeping | AT-11, AT-12 | Straight road at a steady 40 km/h for 60 s, then a hard brake | Follower gaps stay within 6–35 m while steady; no F↔F collisions on the hard brake |
| **T-06** | Takeover release | AT-13 | Take over F3 mid-convoy, drive 50 m off route, release | F3 replans within 2 s, rejoins within 30 s, and has no reverse loop |
| **T-07** | State transitions | AT-20, AT-21 | Run T-06 while watching the HUD follower card | `FollowerStateChanged` sequence: FollowingConvoy → DirectControl → ReturningToAutonomy/Rejoining → FollowingConvoy. The HUD shows the same states |
| **T-08** | StateTree parity | AT-22, AT-23 | Run T-01 with `bUseStateTree` false, then true | Both complete; stuck/collision counts within ±20% of each other; the StateTree debugger shows the expected active states |
| **T-09** | Narrow gate | AT-31, AT-32 | Place a 10 m gap between barriers on the B→C route; drive through it | Followers pass single file in order, with no stuck loop and no barrier collisions |
| **T-10** | Automated course | AT-41 | Run the automation test map (straight, slalom, gate, dead end) | Time-to-goal, stuck count and minimum gap stay within thresholds stored in the test |

## Regression rule

A task is only **done** when its own test **and T-01** pass. If T-01 gets worse than the previous baseline row, revert or fix the change before moving on.
