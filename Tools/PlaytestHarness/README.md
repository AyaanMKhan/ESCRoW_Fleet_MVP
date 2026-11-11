# ESCRoW playtest harness tools

Scripts that let Claude run automated playtests through the Unreal MCP server, using `BP_ESCROW_TestHarness`.

## Before running

- Unreal must be open with the MCP server running. On this Mac, run `ModelContextProtocol.StartServer` in the Unreal console.
- Run everything from this folder, and wrap anything that touches the editor or git in `lock.sh`.

## Running a test

    ./lock.sh me python3 -I runtest.py MAP_ESCROW_DESERT_INTEGRATION_TEST 90 1
    python3 -I analyze.py
    python3 -I rep.py F3

The arguments are the map, a time limit in seconds, and the scenario:

| ScenarioId | Test | Suggested limit |
|---|---|---|
| 0 | Assign the follow chain and drive the desert route | 200 |
| 1 | Recreate the WOLF circling position (AT-15) | 90 |
| 2 | Force a LYNX escalation and alert | 40 |
| 3 | Send followers through the rock field (AT-16); `cycle.sh` runs this with a report | 140 |
| 4 | Wedge LYNX on a real rock, then take over and release (AT-10) | 175 |
| 5 | Full flat-pilot mission A-F on `MAP_ESCROW_FLAT_PILOT_TEST`: gather, chain, Rally B formation, F2/F4 fault takeovers, Rally C plan repair, closeout. Automated T-01. | 900 |

`runtest.py` turns `bRunTestHarness` on in memory and back off when it finishes. If a run aborts, check it's off. Never save `BP_ESCROW_GameMode` with it on.

## Files

- `mcp.py`, `bp.py`, `h_common.py`: MCP client and Blueprint tool helpers.
- `runtest.py`: runs a scenario and saves the log as `last_run.log`.
- `analyze.py`, `rep.py`, `rock_report.py`: summarize `DIAG` lines and run data.
- `lock.sh`: lets only one job use the editor or git at a time.
- `quit.py`: quits Unreal gracefully, and never force-quits.
- `restart.py`: saves the harness and game mode, restarts Unreal, and restores `BP_ESCROW_GameMode` from git. It refuses to run during a play session. Don't use it if you have uncommitted game mode changes.
- `hflat.py`: adds the scenario 5 variables and functions to the harness and writes their graphs. `ids.py`: looks up node type IDs used by them.
- `HarnessDsl/`: the DSL used to build the harness and recovery graphs. The saved Blueprints are the source of truth.
- `Patches/rock_fix_v3.patch`: the rejected AT-16 attempt, kept for reference. See `Docs/agent_tasks.md`.

## Known limits

- Results depend on frame rate. `runtest.py` turns off Unreal's background CPU throttling, which resets on every editor restart. Never compare a throttled run with an unthrottled one.
- Rock runs vary a lot, so compare at least 3 runs per build.
- Scenarios 0-4 use desert map coordinates. Scenario 5 is the only flat-pilot scenario.
- The command vehicle plans each leg on the `ESCROW_Vehicle` nav mesh (`Escrow_HarnessSetLeg`), so legs go around barriers. A leg whose nav path is missing or partial logs `HARNESS leg nav path missing or partial` and falls back to a straight line.
- Harness `Cruise` defaults to 800 cm/s; scenario 5 sets 1500. The drive slows to 35% of cruise on sharp heading errors.
- Scenario 5 fault takeovers hold the takeover lock and send a waypoint order to the recovery point. They don't possess or drive the follower.
- One play session at a time. Don't run two jobs in parallel.
- Teleporting a vehicle into rocks launches it. Place vehicles clear of rocks and pin them at zero velocity.
