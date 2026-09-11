\# ESCRoW Fleet MVP — Claude Working Notes



This is a UE 5.8 project for an ESCRoW convoy / command-vehicle supervisory-control demonstrator.



\## Current actual project

\- Repository: `rhyse-bendell/ESCRoW\_Fleet\_MVP`

\- Project root: `C:\\UnrealProjects\\ESCRoW\_Fleet\_MVP`

\- Unreal project: `ESCRoW\_Fleet\_MVP.uproject`

\- Unreal version: UE 5.8

\- Main content root: `/Game/ESCROW`

\- Work on `main`. Do not create branches unless explicitly told.



\## Current working baseline

The project currently has a stable asset-integrated foundation:

\- Main menu map: `/Game/ESCROW/Maps/MAP\_ESCROW\_MAIN\_MENU`

\- Playable desert map: `/Game/ESCROW/Maps/MAP\_ESCROW\_DESERT\_INTEGRATION\_TEST`

\- Test map: `/Game/ESCROW/Test/MAP\_ESCROW\_ASSET\_INTEGRATION\_TEST`

\- Main menu loads the desert integration map.

\- First-person operator view works.

\- `BP\_ESCROW\_CommandVehicle` is enterable and drivable.

\- `BP\_ESCROW\_FollowerVehicle` has `NavMeshVehicleAI` and can follow or move to a target.

\- `WBP\_ESCROW\_TestHUD` displays a bottom-right minimap.

\- MapSystem marker smoke test works.

\- Battlefield desert environment opens and runs.

\- AI follower movement currently works but is slow; tune later, do not rebuild vehicle physics.



\## Current actual ESCROW-owned assets

Core:

\- `/Game/ESCROW/Blueprints/Core/BP\_ESCROW\_GameMode`

\- `/Game/ESCROW/Blueprints/Core/BP\_ESCROW\_PlayerController`

\- `/Game/ESCROW/Blueprints/Core/BP\_ESCROW\_OperatorCharacter`

\- `/Game/ESCROW/Blueprints/Core/BP\_ESCROW\_MenuGameMode`

\- `/Game/ESCROW/Blueprints/Core/BP\_ESCROW\_MenuPlayerController`



Vehicles:

\- `/Game/ESCROW/Blueprints/Vehicles/BP\_ESCROW\_CommandVehicle`

\- `/Game/ESCROW/Blueprints/Followers/BP\_ESCROW\_FollowerVehicle`



UI:

\- `/Game/ESCROW/Blueprints/UI/WBP\_ESCROW\_MainMenu`

\- `/Game/ESCROW/Blueprints/UI/WBP\_ESCROW\_TestHUD`

\- `/Game/ESCROW/Blueprints/UI/BP\_MapHelper\_Child\_ESCROW`

\- `/Game/ESCROW/Blueprints/UI/BP\_MapZone\_Child\_ESCROW`

\- `/Game/ESCROW/Blueprints/UI/BP\_MapCustomMarker\_Child\_ESCROW`



\## Installed / integrated asset roles

\- `DD\_Vehicles\_Basic`: Drivable Cars military vehicle base. Use through ESCROW child/wrapper Blueprints only.

\- `NavMeshVehicleAI`: low-level AI vehicle movement. Use through `BP\_ESCROW\_FollowerVehicle` and later an ESCROW-owned adapter.

\- `MapSystem`: current minimap / marker layer. Keep it functional but do not over-polish yet.

\- `BattleField\_Desert`: environment / route kit.

\- `CLM\_MenuSystem`: menu plugin enabled, but current menu flow is minimal and ESCROW-owned.



\## Hard rules

\- Do not edit marketplace/vendor asset source folders directly.

\- Do not rewrite vendor Blueprints.

\- Do not inspect or reproduce internal marketplace Blueprint graphs for assets marked AI usage not allowed.

\- Use ESCROW-owned child Blueprints, wrapper Blueprints, adapter components, widgets, structs, and managers.

\- Preserve working behavior unless explicitly asked to change it.

\- Keep changes narrow and testable.

\- Compile and save after every focused pass.

\- Ask before adding C++ classes unless the task clearly benefits from reviewable source code.

\- If a plugin blocks progress for more than one focused work session, use the fallback path and keep moving.

\- Do not spend time on visual polish before mechanics pass.

\- Do not replace the project base again.



\## Current known issues / deferred polish

\- MapSystem background/texture is placeholder-looking.

\- Minimap zoom/capture should be configured later through `BP\_MapZone` / capture settings / map texture.

\- AI follower is slow on desert terrain; tune `NavMeshVehicleAI` exposed speed/throttle/acceptance settings later.

\- Command vehicle defaults to third-person vehicle view after entering; first-person/inside vehicle camera should be fixed later.

\- Only one follower is currently proven; final MVP needs F1-F4.



\## Architecture target

The full MVP should follow the attached ESCRoW build specification:

\- Player starts at a main menu, selects Mission A-D, loads into the command-vehicle scenario, supervises four followers, sees them on a minimap, receives directives, assigns tasks, opens follower feeds, and performs remote takeover of F3 when required.

\- `BP\_FollowerFleetManager` should become the authoritative source of follower state.

\- `BP\_ScenarioDirector` should own deterministic mission timing.

\- `BP\_PlayerControlRouter` should become the only object that changes player control mode.

\- `BP\_EventLogger` should write CSV or JSONL logs.



\## Near-term build sequence

Next passes should be done in this order:



P01 — Create fleet data model

\- Create enums/structs or Blueprint data assets for mission condition, player control mode, follower state, autonomy mode, follower status, directive, and event log entry.

\- Acceptance: project compiles; no gameplay behavior changes.



P02 — Create `BP\_FollowerFleetManager`

\- Tracks F1-F4 status.

\- Registers command vehicle, follower vehicles, target/task points, and map markers.

\- Acceptance: manager exists in desert map and can report basic follower status.



P03 — Expand HUD into command console shell

\- Preserve existing minimap.

\- Add left/center/right command-console panels.

\- Add follower cards for F1-F4, directive queue, message log, selected follower detail, report panel placeholder, camera/feed placeholder.

\- Acceptance: UI appears, does not block vehicle entry/driving, and displays static/mock fleet data.



P04 — Place/register four follower vehicles

\- Create F1-F4 instances in the desert map with callsigns.

\- Add markers for command vehicle, F1-F4, and task target.

\- Acceptance: four followers exist physically and are visible on HUD/map.



P05 — Implement basic task assignment

\- Button or debug control assigns F2 to Position Red.

\- `BP\_ESCROW\_FollowerVehicle` moves toward target via NavMeshVehicleAI.

\- FollowerFleetManager state updates from TaskAssigned to MovingToPosition.

\- Acceptance: F2 visibly moves and card/state updates.



P06 — Implement scenario director

\- Mission D priority path first.

\- Timed directive CMD-001 appears.

\- Later add CMD-002/CMD-003/CMD-004.

\- Acceptance: deterministic message/directive appears after mission start.



P07 — Implement takeover shell

\- F3 can enter IssueDetected / AwaitingTakeover.

\- HUD displays Takeover Required.

\- Do not perfect remote control until this state path is visible.

\- Acceptance: F3 issue state is visible in HUD, map marker/message/card.



P08 — Implement remote takeover / handback

\- Disable AI on F3.

\- Route camera/input/control to F3.

\- Drive through recovery segment.

\- Return to autonomy and restore command/supervisor mode.

\- Acceptance: user can take over F3 and return.



P09 — Implement logging

\- CSV or JSONL to `Saved/Logs/ESCRoW/`.

\- Log mission start, mode changes, directives, follower commands, state transitions, takeover, handback, reports, mission end.

\- Acceptance: log file is written and readable after run.



P10 — Add facilitator panel / hotkeys

\- F1 toggle panel, F2 next directive, F3 anomaly, F4 force takeover, F5 force recovery complete, F6 mark report ready, F9 reset.

\- Acceptance: demo can be advanced/recovered manually.



\## MCP / Claude workflow

\- Unreal must be open.

\- Unreal MCP server should be running on `http://127.0.0.1:8000/mcp`.

\- Use serial tool calls only. Do not issue overlapping Unreal MCP operations.

\- First inspect, then propose, then modify.

\- After modifying assets, report exactly what changed and what should be tested.

\- After the user confirms the pass condition, ask the user to commit.

