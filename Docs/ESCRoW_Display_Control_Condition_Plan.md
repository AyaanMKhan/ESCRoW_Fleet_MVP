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

## 6. RA36B-1: manual Switch Seat foundation

Added a manual runtime seat toggle so the operator can move between the driver seat and the
passenger/console seat. This is **not** condition-start behavior — nothing here reads
`DisplayMode`/`VehicleControlRole` to decide the seat; it is a plain manual toggle, independent
of the RA36A condition metadata, that the future condition-start phase will eventually drive
automatically.

- New key **`N`** (chosen because the originally-preferred `X` was already bound to
  `Escrow_Cancel`, shared with `Backspace`) calls a new `Escrow_ToggleSeat` function on
  `BP_ESCROW_PlayerController`.
- New state: a plain bool, `bESCROWInPassengerConsoleSeat` (default `false` = Driver), **not**
  the existing `EESCROWPlayerControlMode`/`CurrentPlayerMode` on
  `UESCROWPlayerControlRouterComponent` — that enum is already actively used for an orthogonal
  HUD-interaction-mode axis (`CompactHUD`/`FocusedCommandHUD`/`FullMap`/`RemoteTakeover`/
  `ReturnToAutonomyTransition`), and reusing it for seat state would have collided with that
  existing state machine. A plain bool was the safer additive choice.
- Camera: toggles `SetActive` on the vendor `Camera_InCar` (driver) and the ESCROW-owned
  `Camera_InCar_Passenger` components on the currently-possessed `BP_ESCROW_CommandVehicle` —
  confirmed switching correctly both directions in live PIE, including a visible glimpse of the
  `Console_Display` panel from the passenger seat.
- Driving-input suppression is **deferred, not implemented**: the vendor vehicle's
  throttle/steering input binding internals are black-boxed per project policy, so disabling
  them safely could not be confirmed without inspecting/reproducing vendor input graphs, which
  is out of scope. Currently pressing `N` only switches the camera and prints a status line;
  driving input remains active in both seats.
- Status feedback: a one-shot `PrintString` ("Seat: Driver" / "Seat: Passenger/Console"), no
  HUD widget touched, no per-tick logging.
- Physical console widget is still not implemented. Current HUD + Driving remains the default
  fallback and was re-verified end-to-end (Tab/console, MAP, `8`/report-deck toast, minimap) with
  no regressions.

**Known oddity**: on the very first `N` press of a freshly-started PIE session, the printed
label was observed to read "Seat: Driver" instead of the expected "Seat: Passenger/Console" (the
bool's confirmed default is `false`, so the first toggle should report the passenger seat).
Every press after that alternates correctly and reliably. The underlying node graph was
inspected directly (not just via decompiled DSL) and is wired correctly; this looks like an
environment/first-frame timing quirk rather than a logic defect, but is unresolved and worth
re-checking in a real (non-automated) keyboard session.

## 7. RA36B-2 + RA36B-3-lite: physical console mirror foundation

- **New asset**: `WBP_ESCROW_PhysicalConsoleMirror` (`/Game/ESCROW/Blueprints/UI`), a small
  additive UMG widget — dark `Border` background, gold title, four white/gray text rows (Seat,
  Console Mirror, Display Mode, Vehicle Control Role). No bindings, no ticking. Exposes one
  update function, `Escrow_SetConsoleMirrorState(SeatText, MirrorStatusText, DisplayModeText,
  VehicleControlRoleText)`, which just re-composes each row's `Text` from plain strings.
- **Attachment (fully runtime, no `BP_ESCROW_CommandVehicle` edit)**: `BP_ESCROW_PlayerController`
  gained three new variables (`PhysicalConsoleMirrorComponent` — `UWidgetComponent`,
  `PhysicalConsoleMirrorWidget` — the widget instance, `PhysicalConsoleMirrorStatusLabel` —
  string) and three new functions:
  - `Escrow_EnsurePhysicalConsoleComponent()` — if the component doesn't exist yet, creates a
    `UWidgetComponent` on the possessed `CommandVehicleRef` via the generic `AddComponent`
    node (`bManualAttachment=false`, so it auto-attaches to the vehicle's root component at
    creation), creates the widget instance, and assigns it to the component
    (`SetWidget`/`SetWidgetSpace("World")`/`SetDrawSize(400x300)`).
  - `Escrow_AttachPhysicalConsoleComponent()` — reads `Console_Display` off the vehicle (a plain
    Blueprint-exposed `GetConsoleDisplay` getter, no vehicle edit needed) and, if valid,
    re-parents the widget component onto it with a small relative offset, setting the status
    label to `"Active on Console Display"`. If `Console_Display` isn't found, the component is
    left attached to the vehicle root (from creation) and the label is set to `"Placeholder
    diagnostic placement"`.
  - `Escrow_RefreshPhysicalConsoleMirror()` — calls the existing vehicle-capture helper, then the
    two functions above, then (if the widget is valid) reads the seat bool, the status label, and
    `DisplayMode`/`VehicleControlRole` off `UESCROWSessionSubsystem`, and pushes all four into
    `Escrow_SetConsoleMirrorState`.
  These three were split out of what was originally attempted as one deep function, specifically
  to keep each function's Blueprint-node nesting shallow — the original single-function DSL
  repeatedly failed with "Unclosed parenthesis" at ~9 levels of nested `IsValid` branches; three
  shallow functions (max depth ~4-5) compiled correctly on the first structurally-corrected
  attempt each.
- **Controller integration**: `Escrow_RefreshPhysicalConsoleMirror` is called once at the end of
  `EventBeginPlay` (appended after the existing `SetPauseMenuRef`, whose `then` pin was previously
  unconnected) and once at the end of `Escrow_ToggleSeat` (appended after its existing camera-
  switch logic, whose `then` pin was also previously unconnected) — both wired via direct node/pin
  creation (`create_node`/`connect_pins`), not by rewriting either existing event body, so nothing
  already in those graphs was touched.
- **DisplayMode/VehicleControlRole readability — confirmed readable**: both are read via
  `GetESCROWSessionSubsystem()` → `GetDisplayModeName()`/`GetVehicleControlRoleName()` on
  `UESCROWSessionSubsystem` (the same accessors RA36A-2 wired into the setup screen). Verified live
  in PIE by inspecting the actual `TextBlock.Text` values on the spawned widget instance: `"Display
  Mode: HUD Primary"` and `"Vehicle Control Role: Driving"` — the real session defaults, not
  placeholder "Unknown" text.
- **Live PIE verification**: performed via direct read of the live PlayerController/vehicle/widget
  instances after a fresh PIE `BeginPlay` (not by pressing keys — see the checklist doc's RA36B-2
  note for why). Confirmed: `PhysicalConsoleMirrorComponent` valid and attached under the live
  `BP_ESCROW_CommandVehicle` instance; `PhysicalConsoleMirrorWidget` a valid widget instance;
  `PhysicalConsoleMirrorStatusLabel == "Active on Console Display"`; all four widget text rows
  showing correct real values. No new Blueprint compile or runtime errors.
- **Deferred / not done this pass**: live keyboard confirmation of the `N` toggle re-triggering the
  refresh (tooling limitation, not a code gap — `Escrow_ToggleSeat`'s pre-existing logic itself was
  not modified); any visual/screenshot confirmation of the panel's on-screen appearance from the
  passenger seat; driving-input suppression (still explicitly out of scope); condition-start
  behavior (§12 item 3 below; unchanged in scope); full console interaction.

## 8. RA36B-FIX-1: passenger-seat usability fix

Human PIE testing after RA36B-2 found two problems: the passenger/console seat had no free
look (felt like a frozen view-target swap, not a real seat), and `Console_Display` visibly faced
the wrong way (up toward the roof/sky instead of toward the passenger).

- **Root cause, no free look**: `Escrow_ToggleSeat` only ever called `SetActive` on the two
  camera components — it never touched control-rotation routing. `Camera_InCar` (the vendor
  camera, defined on `BP_BaseCar`) achieves driver look through whatever mechanism the vendor
  pawn uses internally (not inspected, per the vendor black-box rule). `Camera_InCar_Passenger`
  (ESCROW-owned, added on `BP_ESCROW_CommandVehicle`) had `bUsePawnControlRotation = false` and
  no other rotation source, so it was visually static regardless of mouse/controller input.
- **Fix**: set `bUsePawnControlRotation = true` on `Camera_InCar_Passenger`'s Blueprint
  default (the standard, minimal Unreal mechanism for "this camera follows the controller's
  look rotation independent of the pawn body's own rotation"). No vendor asset touched, no new
  input plumbing, no change to `Escrow_ToggleSeat` itself.
- **Root cause, console orientation**: `Console_Display` (an `/Engine/BasicShapes/Plane`,
  normal = local +Z at identity) had `RelativeRotation = (Pitch 100, Yaw 180, Roll 180)` on its
  Blueprint SCS default. Verified against UE's actual rotation-matrix formula, this orientation's
  normal computes to roughly local `(-0.98, 0, +0.17)` — already close to facing the passenger,
  but with the plane's face reading as sky/ceiling-lit from most viewing angles, and no margin for
  error.
- **Fix**: recomputed the exact rotation that points the plane's normal directly at
  `Camera_InCar_Passenger`'s position (accounting for the real relative offsets of both
  components), landing on `RelativeRotation = (Pitch 73, Yaw 4, Roll 0)` — UE canonicalizes this
  to an equivalent `(73, 180, 180)` representation on read-back, confirmed mathematically
  identical. This keeps the mesh's long (0.6-scale) edge horizontal and short (0.4-scale) edge
  vertical — a landscape-oriented panel — while aiming squarely at the passenger's eye position
  instead of just approximately.
- **Both fixes applied to the Blueprint's SCS component defaults** on
  `BP_ESCROW_CommandVehicle` (not the currently-open map's placed instance, and not a per-instance
  override — confirmed the placed instance had no prior override, so the Blueprint default is the
  single source of truth here). The map was not saved.
- Verified in a fresh PIE session via direct state inspection: `Camera_InCar_Passenger`'s
  `bUsePawnControlRotation` reads `true`, `Console_Display`'s rotation reads the fixed value, and
  the RA36B-2 console-mirror widget system (component creation, attachment, text mirroring) still
  works with zero regressions. Live mouse-look/keyboard-toggle behavior itself could not be
  exercised by available tooling (same limitation noted under RA36B-2) — recommend a human
  keyboard/mouse pass to do the final confirmation.

## 9. RA36B-REPAIR-1: passenger look + console-mirror placement repair

Human PIE testing after RA36B-FIX-1 found it **did not** fix either problem: passenger seat
still had no free look, and the console mirror still appeared as a wrong, misplaced pane. This
pass replaces both fixes structurally instead of iterating on the same approach.

- **Root cause, no free look (corrected)**: RA36B-FIX-1's `bUsePawnControlRotation = true` did
  nothing because nothing in this project ever writes to the Controller's `ControlRotation` —
  confirmed by inspecting `BP_ESCROW_PlayerController`'s `EventGraph` directly: there is no
  Turn/Look/MouseX/MouseY axis event anywhere on it, and `EventTick`'s exec output is
  unconnected. Driver-seat look is achieved entirely inside the vendor pawn's own input handling
  (not inspected, per the vendor black-box rule), which never touches `ControlRotation` either.
  `bUsePawnControlRotation` had no rotation source to react to in either seat.
- **Fix**: `bUsePawnControlRotation` reverted to `false` on `Camera_InCar_Passenger` (avoids a
  dead flag). Added a genuine, self-contained look path instead: two new raw hardware-axis events
  (`Input|MouseEvents|MouseX`/`MouseY` — these fire independently of any Enhanced Input/Action
  mapping and don't touch or require anything vendor-owned) call a new function,
  `Escrow_UpdatePassengerLook(YawDelta, PitchDelta)`, on `BP_ESCROW_PlayerController`. That
  function only acts when `bESCROWInPassengerConsoleSeat` is true; it accumulates two new float
  variables (`PassengerLookYaw`, `PassengerLookPitch`, pitch clamped to ±60°) and calls
  `SetRelativeRotation` directly on `Camera_InCar_Passenger` each time. `Escrow_ToggleSeat` now
  also resets both variables to 0 and snaps the camera back to center whenever the passenger seat
  is entered. Driver mode is untouched — the new events are additive bindings that do nothing
  while `bESCROWInPassengerConsoleSeat` is false, and nothing about `Camera_InCar` or the vendor
  pawn was touched.
- **Root cause, wrong pane (corrected)**: the actual bug was structural, not orientation math.
  RA36B-2/FIX-1 created the console mirror as a **runtime-spawned `UWidgetComponent`**
  (`Escrow_EnsurePhysicalConsoleComponent`), auto-attached to the vehicle's root at creation and
  only *re-parented* to `Console_Display` afterward via `AttachComponentToComponent`. A
  `UWidgetComponent`'s own "unrotated" facing convention does not necessarily match a
  `StaticMeshComponent` plane's convention, so copying `Console_Display`'s carefully-computed
  mesh rotation onto the runtime widget did not make it face the same way — this is what produced
  the wrong-looking floating pane, independent of any rotation-math correctness.
- **Fix**: removed the runtime-spawn path entirely (`Escrow_EnsurePhysicalConsoleComponent` and
  `Escrow_AttachPhysicalConsoleComponent` function graphs deleted, along with the now-unused
  `PhysicalConsoleMirrorComponent`/`PhysicalConsoleMirrorWidget`/`PhysicalConsoleMirrorStatusLabel`
  variables). Added a new `WidgetComponent` named `ConsoleMirrorDisplay` directly to
  `BP_ESCROW_CommandVehicle`'s component tree as a **child of `Console_Display`** (confirmed via
  `get_parent_component`), with `RelativeLocation (0,0,2)` (a small local-Z nudge off the mesh
  face to avoid z-fighting), `RelativeRotation (0,0,0)` (inherits `Console_Display`'s orientation
  directly — no separate rotation math needed), `DrawSize (90,90)`, `Space = World`, and
  `WidgetClass = WBP_ESCROW_PhysicalConsoleMirror` set directly on the component (so UE
  auto-constructs the widget at BeginPlay — no runtime `CreateWidget` call needed at all).
  `Escrow_RefreshPhysicalConsoleMirror` was rewritten to fetch this component via a new
  `GetConsoleMirrorDisplay` getter, read its widget instance via `GetUserWidgetObject`, cast it,
  and call `Escrow_SetConsoleMirrorState` — nothing is spawned or attached at runtime anymore.
  There is now exactly one console-mirror surface, anchored to the display the user placed.
- **Editor-session limitation found during verification**: after adding `ConsoleMirrorDisplay`
  and compiling, the currently-open level's *already-placed* `BP_ESCROW_CommandVehicle` instance
  did not pick up the new component's saved defaults (`WidgetClass` read back as `None`,
  `DrawSize` as the engine default `500×500`) in this same long-running editor session, even
  after repeated compiles and manual property patches — a known Unreal Editor limitation where an
  already-placed actor's component data doesn't always fully refresh from a Blueprint's new SCS
  member without a level reload. **The saved Blueprint asset itself was directly verified correct**
  (`RelativeLocation (0,0,2)`, `DrawSize (90,90)`, `WidgetClass` correctly set) — this is a
  stale-instance display issue in the current session only, not a flaw in the fix. A human tester
  should reopen `MAP_ESCROW_DESERT_INTEGRATION_TEST` (File → Open Level, no save required) or
  restart the editor before verifying, so the vehicle instance constructs fresh from the corrected
  Blueprint.
- Repeated PIE start/stop cycling during this repair also surfaced one unrelated instability: a
  vendor Animation Blueprint (`ABP_ThirdPerson_driving`) logged a `TRASH_CameraComponent_2 ...
  Camera_InCar` "pending kill" runtime error after several rapid PIE restarts in the same
  session. This did not recur in a subsequent clean, single PIE session and appears tied to rapid
  restart cycling rather than to anything changed this pass — worth watching for, but a fresh
  editor/PIE session is the recommended way to avoid it entirely.

## 10. RA36B-REGRESSION-FIX-DRIVER-GAZE: driver-gaze regression from RA36B-REPAIR-1

Human PIE testing after RA36B-REPAIR-1 found a **critical regression**: driver-seat mouse/gaze
control stopped working entirely after the passenger raw-mouse-look implementation was added.
This pass is a rollback of that one mechanism only — nothing else was changed.

- **Root cause**: RA36B-REPAIR-1 added `Input|MouseEvents|MouseX`/`MouseY` raw axis event
  bindings directly on `BP_ESCROW_PlayerController`, both created with the UE default
  `bConsumeInput = true` (confirmed by direct property read). Since the PlayerController receives
  input before its possessed Pawn in Unreal's input-handling order, a Controller-level binding
  with `bConsumeInput = true` marks the MouseX/MouseY axis events as **handled every frame**,
  regardless of what the Blueprint logic behind it actually does — this prevented the raw mouse
  axis from ever reaching the vendor pawn's own look-input handling, which is how driver gaze
  was implemented. This is a complete, input-stack-level blocking, not a logic bug in
  `Escrow_UpdatePassengerLook` itself.
- **Fix**: deleted the two `MouseX`/`MouseY` input event nodes and their directly-wired helper
  nodes (`Escrow_UpdatePassengerLook` call nodes, the `NegateFloat` macro instance) from
  `BP_ESCROW_PlayerController`'s `EventGraph`. This removes the only thing that was consuming the
  mouse axis at the Controller level. Nothing else was touched: `EventBeginPlay` (input
  mode/cursor setup), `Escrow_ToggleSeat`, the `N` key binding, and the console-mirror refresh
  call are all unchanged from RA36B-REPAIR-1.
- **Passenger free-look is now fully deferred.** The `Escrow_UpdatePassengerLook` function,
  `PassengerLookYaw`/`PassengerLookPitch` variables, and `Escrow_ToggleSeat`'s seat-entry reset of
  those variables/camera rotation were left in place as inert, harmless dead code (per this pass's
  explicit instructions) — they are unreferenced by any input path now and only ever touch the
  ESCROW-owned `Camera_InCar_Passenger`, never the vendor `Camera_InCar` or anything
  input-related, so leaving them is safe. A future pass should design a genuinely input-safe
  passenger-look mechanism (e.g. explicit `bConsumeInput = false`, or routing through the
  vendor's own input path if one can be found without inspecting vendor internals) before
  attempting this again.
- **Driver-seat behavior is the baseline going forward and must not be put at risk again**
  without first confirming any new input-related change does not set `bConsumeInput = true` (or
  otherwise intercept input) on anything bound at the PlayerController level.
- Verified: clean compile, clean PIE log (no new errors, no references to the deleted nodes), `N`
  → `Escrow_ToggleSeat` → `Escrow_RefreshPhysicalConsoleMirror` chain structurally intact
  (console mirror and seat-toggle wiring preserved, unaffected by this rollback). **Actual mouse
  input could not be programmatically injected in this environment** (a known, previously-
  documented tooling limitation) — a human must confirm driver gaze is restored with real mouse
  input before this is considered closed.

## 11. RA36B-PASSENGER-SEAT-FIX: passenger look + console orientation repair

Human-tested state entering this pass: driver gaze restored and must not regress; seat switching
and Tab HUD work; passenger seat still has no look control; the console pane, though repositioned
from earlier passes, still doesn't read as facing the passenger.

- **Passenger look, reimplemented safely**: recreated the same `MouseX`/`MouseY` raw axis
  bindings removed in the regression-fix pass, but this time with **`bConsumeInput` explicitly set
  to `false`** (the confirmed root cause of the earlier regression was this flag defaulting to
  `true`, which let the Controller-level binding block the vendor pawn's own input entirely). With
  `bConsumeInput = false`, the PlayerController's binding and the vendor pawn's own driver-look
  binding both receive the same mouse axis value independently — nothing is stolen. The bindings
  call the existing (unchanged) `Escrow_UpdatePassengerLook`, which only acts while
  `bESCROWInPassengerConsoleSeat` is true and only ever rotates `Camera_InCar_Passenger` — never
  `Camera_InCar` or anything vendor-owned.
- **Console orientation, simplified**: replaced the previous precisely-aimed-at-a-point rotation
  (`Pitch 73, Yaw 4, Roll 0`, which left a small residual twist) with a clean, symmetric
  `Pitch 70, Yaw 0, Roll 0` — this points the panel's normal straight back along the vehicle's
  local -X axis (directly toward where the passenger sits, since the console is mounted forward of
  the passenger) with a 20° upward tilt, and — verified against the rotation matrix — leaves
  *zero* residual twist: the mesh's short edge maps to pure world-up and its long edge to pure
  local-right, with no cross-axis lean. Applied to `Console_Display`'s Blueprint SCS default on
  `BP_ESCROW_CommandVehicle` (still the sole physical display surface; `ConsoleMirrorDisplay`, the
  child `WidgetComponent` added in RA36B-REPAIR-1, was left untouched and still inherits this
  orientation automatically as a child).
- **Observation, not acted on this pass**: screenshots taken from the passenger camera's actual
  position/rotation still show a keystoned (trapezoidal) panel outline even with this cleaner,
  twist-free rotation. Cross-checking against the vehicle-local geometry, this is very likely a
  **perspective/proximity artifact** — the console sits only ~66 UE units (~0.66 m) from the
  passenger camera, which is unusually close for a flat panel that large, and any nearby large
  flat surface viewed off-axis will keystone regardless of its true orientation being correct.
  `Console_Display`'s *position* was left untouched per this pass's scope (orientation only, per
  instructions to use the existing component and avoid extreme transform changes) — if it still
  looks off after passenger look is confirmed working, the next step would be examining distance/
  placement, not further rotation tweaks.
- **Environment note**: saving `BP_ESCROW_PlayerController` and `BP_ESCROW_CommandVehicle` hit a
  transient external file lock (Windows Error Code 32, most likely OneDrive sync catching up after
  this session's heavy activity) that took several minutes to clear. Both saves succeeded once the
  lock released, confirmed via fresh file timestamps; no project files were corrupted or lost.
- **Driver-seat behavior remains the baseline.** Any future change to `BP_ESCROW_PlayerController`
  input handling must confirm `bConsumeInput` stays `false` on any new PlayerController-level
  binding, per the lesson from the regression-fix pass.
- PIE-verified this pass: clean compile on both Blueprints, clean PIE log in a fresh session
  started *after* the disk save succeeded (no new errors, no Accessed-None), baseline driver seat
  confirmed, `Console_Display`'s corrected rotation confirmed live. **Actual mouse-driven look
  still could not be programmatically tested** (same longstanding tooling limitation) — a human
  must confirm both passenger look and continued driver look with real mouse input.

## 12. Next phases

1. ~~Add placeholder WidgetComponent on Console_Display.~~ Done (RA36B-2).
2. ~~Mirror selected report/status/follower/session state to the physical console.~~ Started
   (RA36B-2 mirrors seat/mirror-status/DisplayMode/VehicleControlRole; report/follower/fleet state
   mirroring is still future work).
3. Add condition-start behavior for HUD Primary/Console Primary and Driving/Not Driving (this is
   where `DisplayMode`/`VehicleControlRole` finally drive automatic behavior instead of just being
   displayed).
4. Add driving-input suppression in the passenger/console seat, once the vendor vehicle's input
   binding mechanism has been safely characterized.
5. Add full console interaction only after the display/mirroring path is stable.

## 13. Safety rules for every future phase

- Do not refactor `WBP_ESCROW_CommandHUD` into a shared core widget before the console mirror
  is proven necessary — extend additively first.
- Do not edit vendor assets (`DD_Vehicles_Basic`, `NavMeshVehicleAI`, `MapSystem`,
  `BattleField_Desert`, `CLM_MenuSystem`).
- Do not touch vehicle physics/possession/movement.
- After every phase, re-test the HUD Primary + Driving path end-to-end (F1-F4, MAP, REPORTS,
  minimap, the RA35A report-deck key `8`) before touching anything console-related — that cell
  must never regress, since it is the only cell anyone can currently run.

## 14. RA38B — DisplayMode now drives runtime behavior (item 3 from section 12, done)

- **HUD Primary is unchanged** — the screen-space `CommandHUD` instance (embedded inside
  `WBP_ESCROW_TestHUD`) stays exactly where it was, exactly as before. No new code runs for this
  mode.
- **Console Primary**: at `BeginPlay`, once `CommandVehicleRef` is valid, a new
  `BP_ESCROW_PlayerController` function `Escrow_ApplyDisplayModeRouting` reads `DisplayMode` from
  `UESCROWSessionSubsystem` and, if `ConsolePrimary`, **relocates the exact same `CommandHUD`
  widget instance** from `TestHUD`'s screen canvas onto `Console_Display`'s existing
  `ConsoleMirrorDisplay` `WidgetComponent` (`Widget|RemoveFromParent` then
  `UserInterface|SetWidget`), and bumps `ConsoleMirrorDisplay`'s `DrawSize` to `(800, 1000)` for
  readability. This is the *same* `CommandHUD` object, not a second instance — every existing
  keyboard-driven function (`Escrow_HideHUD`, `Escrow_ShowFocused`, `Escrow_ShowMonitoring`,
  `Escrow_CycleNext/Prev`, `Escrow_ActionNext/Prev`, `Escrow_Confirm/Cancel`,
  `Escrow_ReportDeckShortcut`, etc.) still calls `TestHUDRef.GetCommandHUD()` completely
  unmodified and keeps working, because that reference still resolves to the (now relocated)
  widget. This was chosen specifically to avoid rewriting the ~15 functions that call
  `GetCommandHUD()` directly, and to guarantee zero duplicated fleet/session state — there is
  only ever one `CommandHUD` instance, just mounted in a different place.
- The old `WBP_ESCROW_PhysicalConsoleMirror` diagnostic placeholder (Title/Seat/MirrorStatus/
  DisplayMode/VehicleControlRole text) is simply superseded — in Console mode it gets replaced by
  the real `CommandHUD` the moment `Escrow_ApplyDisplayModeRouting` runs; in HUD mode it's still
  the resting widget on the physical display (harmless, matches prior behavior, not visible from
  the driver seat).
- `Console_Display`'s transform was **not touched** — only `ConsoleMirrorDisplay`'s `DrawSize` and
  runtime `Widget` were changed, both ordinary runtime properties, not placement.
- `WBP_ESCROW_ExperimentSetup` already fully writes `DisplayMode`/`VehicleControlRole` via
  `SetDisplayModeByIndex`/`SetVehicleControlRoleByIndex` in `Escrow_ValidateAndStartSession` (the
  Start Session button) — confirmed by inspection, not modified this pass.
- **Still requires human testing**: selecting Console Display in the real Experiment Setup screen
  and confirming the physical display actually shows the command console content, and that Tab/Q/
  E/Enter/8 all visibly update it. This pass could not drive the ComboBoxString or reach the
  `UESCROWSessionSubsystem` singleton through tooling to force/verify this end-to-end in PIE
  (same longstanding Slate/PIE input-automation limitation as every prior pass) — only the HUD
  Primary baseline and the routing code's structure were verified this pass.
