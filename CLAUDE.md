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

\- C++ is the intended home for ESCRoW core types and the `UESCROWSessionSubsystem` / `UESCROWFleetSubsystem` / `UESCROWPlayerControlRouterComponent` / `UESCROWEventLogSubsystem` runtime systems, built via the bounded RA passes in `/Docs/ESCRoW\_Robust\_Long\_Term\_MVP\_Build\_Queue.md`. Still ask before adding any C++ class outside that queue.

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

The project uses an intentional robust/hybrid architecture. This supersedes the earlier interaction-first, Blueprint-only plan. Long-term correctness and maintainability take priority over rapid Blueprint-only prototyping:

\- Source-controlled C++ owns stable ESCRoW core types and runtime systems:

  \- `UESCROWSessionSubsystem` stores participant/session/selected-mission state.

  \- `UESCROWFleetSubsystem` is the authoritative world-specific state, command, message, report, and takeover-lock bus for F1-F4.

  \- `UESCROWPlayerControlRouterComponent` is the only object/component allowed to change player control mode.

  \- `UESCROWEventLogSubsystem` handles structured logging.

\- Blueprint/UMG remains responsible for HUD layout, follower cards/tabs, map panels, feed panels, vehicle wrappers, camera presentation, and marketplace-asset adapters. Blueprints call into the C++ subsystems/components above rather than re-implementing their state or logic.

\- Marketplace/vendor assets (`DD\_Vehicles\_Basic`, `NavMeshVehicleAI`, `MapSystem`, `BattleField\_Desert`, `CLM\_MenuSystem`) remain black boxes. ESCRoW-owned wrapper/adapter Blueprints or components are the only integration point; never edit vendor source or reproduce internal vendor graphs.

\- Player starts at a main menu, selects Mission A-D, loads into the command-vehicle scenario, supervises four followers, sees them on a minimap, receives directives, assigns tasks, opens follower feeds, and performs remote takeover of a follower (F3 first) one at a time when required.

\- `BP\_ScenarioDirector` (Blueprint, driven by the C++ subsystems above) owns deterministic mission timing.

\- Work proceeds in bounded RA (Robust Architecture) passes, each requiring manual approval before starting and a build/test/commit checkpoint before the next pass begins.



\## Near-term build sequence

Superseded by the RA build queue in `/Docs/ESCRoW\_Robust\_Long\_Term\_MVP\_Build\_Queue.md`. That document (RA00-RA23) is the authoritative pass-by-pass sequence going forward. Do not resume the old P01-P10 Blueprint-only sequence.



\## MCP / Claude workflow

\- Unreal must be open.

\- Unreal MCP server should be running on `http://127.0.0.1:8000/mcp`.

\- Use serial tool calls only. Do not issue overlapping Unreal MCP operations.

\- First inspect, then propose, then modify.

\- After modifying assets, report exactly what changed and what should be tested.

\- After the user confirms the pass condition, ask the user to commit.

