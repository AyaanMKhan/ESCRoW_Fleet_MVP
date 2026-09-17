# ESCRoW Fleet MVP — Display-Locus / Vehicle-Control-Role Condition Plan

## 1. Goal

Add a new experimental manipulation — where the primary command interface lives, and whether
the participant drives — as additive condition plumbing, while keeping the current HUD-only,
driving-by-default build exactly as-is. This pass (RA36A-1) is data/setup scaffolding only; no
runtime HUD, console, or possession behavior changes yet.

## 2. Factors

- **Display Mode**: `HUD Primary` (screen-space HUD is the command interface) vs.
  `Console Primary` (the physical in-vehicle `Console_Display` panel will eventually be primary).
- **Vehicle-Control Role**: `Driving` (participant drives the command vehicle) vs.
  `Not Driving` (participant supervises without driving).

Future four cells: HUD Primary + Driving (**current default**), HUD Primary + Not Driving,
Console Primary + Not Driving, Console Primary + Driving.

## 3. Additive architecture

- The physical console mesh/slot (`Console_Display` on `BP_ESCROW_CommandVehicle`) already
  exists in the vehicle asset; this pass does not add a widget to it yet.
- The long-term design is that the screen-space HUD (`WBP_ESCROW_CommandHUD`) and a future
  world-space console widget both read from the same underlying command/report/fleet state
  (`UESCROWFleetSubsystem`, `UESCROWSessionSubsystem`, `UESCROWEventLogSubsystem`) rather than
  each other — one shared source of truth, two presentations.
- `WBP_ESCROW_CommandHUD` remains the default/fallback presentation regardless of which cell is
  eventually active; nothing about its current behavior is conditioned on `DisplayMode` or
  `VehicleControlRole` yet.

## 4. What this pass actually added

- `EESCROWDisplayMode` (`HUDPrimary` / `ConsolePrimary`) and `EESCROWVehicleControlRole`
  (`Driving` / `NotDriving`) enums in `ESCROWTypes.h`.
- `DisplayMode` / `VehicleControlRole` fields (default `HUDPrimary` / `Driving`) on
  `UESCROWSessionSubsystem`, reset by `ResetSessionState()`, with `Set*`, `Set*ByIndex`, `Get*`,
  and `Get*Name` Blueprint accessors mirroring the existing `ComplexityCondition` pattern.
  Same two fields added to `FESCROWSessionMetadata` and populated in `GetSessionMetadata()`.
- `GetConditionSummary()` now appends both labels, e.g. `"Mission D - Driver + Takeover / Low
  Complexity / HUD Primary / Driving"`. Because `UESCROWEventLogSubsystem::AddEvent()` already
  stamps every logged event's `ConditionSummary` from this function, and that field already
  flows through to `EntryToCSVRow`/`BuildJSONLine`, every future event-log export automatically
  carries both new condition labels with zero further changes.
- `WBP_ESCROW_ExperimentSetup` gained two new rows (`CB_DisplayMode`, `CB_VehicleControlRole`,
  matching the existing `CB_Complexity` combo-box pattern) with the required one-line
  explanations, defaulting to `HUD Primary` / `Driving`.

## 5. Status

- **RA36A-1** added the `DisplayMode` / `VehicleControlRole` fields, enums, and Blueprint
  accessors on `UESCROWSessionSubsystem`, and the two combo-box rows in
  `WBP_ESCROW_ExperimentSetup` (UI only, not yet wired).
- **RA36A-2** completed the combo-box wiring after the successful cold C++ rebuild:
  `Escrow_ValidateAndStartSession` now reads `CB_DisplayMode`/`CB_VehicleControlRole`'s selected
  index and calls `SetDisplayModeByIndex`/`SetVehicleControlRoleByIndex`.
- **Display Mode and Vehicle-Control Role are now selectable and persisted** — a session
  started from the setup screen stores whichever combination was chosen in
  `UESCROWSessionSubsystem`, and `GetConditionSummary()` (and therefore the Reports-tab session
  line and every event-log export) reflects it automatically.
- **No runtime HUD/console/camera/possession behavior changes have been implemented yet.**
  Nothing reads `DisplayMode`/`VehicleControlRole` to alter HUD, console, camera, or possession
  behavior — this remains pure metadata plumbing.
- **Current HUD + Driving behavior remains the default fallback**: `HUDPrimary` + `Driving` are
  the subsystem's defaults, unchanged from before either pass, and any path that bypasses Setup
  (e.g. loading the desert map directly) still uses exactly that default.

## 6. Next phases

1. Add Switch Seat key/action for Driver ↔ Passenger/Console seat.
2. Add placeholder WidgetComponent on Console_Display.
3. Mirror selected report/status/follower/session state to the physical console.
4. Add condition-start behavior for HUD Primary/Console Primary and Driving/Not Driving.
5. Add full console interaction only after the display/mirroring path is stable.

## 7. Safety rules for every future phase

- Do not refactor `WBP_ESCROW_CommandHUD` into a shared core widget before the console mirror
  is proven necessary — extend additively first.
- Do not edit vendor assets (`DD_Vehicles_Basic`, `NavMeshVehicleAI`, `MapSystem`,
  `BattleField_Desert`, `CLM_MenuSystem`).
- Do not touch vehicle physics/possession/movement.
- After every phase, re-test the HUD Primary + Driving path end-to-end (F1-F4, MAP, REPORTS,
  minimap, the RA35A report-deck key `8`) before touching anything console-related — that cell
  must never regress, since it is the only cell anyone can currently run.
