# ESCRoW Fleet MVP — Demo Hardening Checklist

Walk through this checklist in order before a facilitated demo or manual test session. Each numbered section maps to a block of rows in `ESCRoW_Manual_Test_Run_Log.md` — use that file to record pass/fail detail as you go. This checklist is the quick pass/fail pass; the run log is the detailed record.

Recommended keyboard reference (also shown in Pause Menu → Keybinds):
`Tab` = focus console · `Q` / `E` = change page · `Up` / `Down` = change action · `Enter` = activate action · `P` or `Esc` = Pause Menu · `F5` = Start Scenario · `F6` = Next Event · `F7` = Pause/Resume Scenario · `F8` = Reset Scenario

## 1. Main Menu
- [ ] Launches to `MAP_ESCROW_MAIN_MENU` with background art (not a flat gray/charcoal box)
- [ ] Title "ESCRoW Fleet MVP" and subtitle visible
- [ ] Start Experiment / Quick Start / Quit buttons all present and clickable

## 2. Experiment Setup
- [ ] Researcher ID / Participant ID fields accept text
- [ ] Session ID "Generate" produces an ID
- [ ] Mission / Condition / Complexity selectors work
- [ ] Condition Summary updates when selections change
- [ ] Start Session loads the desert map

## 3. Quick Start
- [ ] "Quick Start Desert Demo" loads directly into the scenario with sane defaults

## 4. Command Vehicle Driving
- [ ] Command vehicle drives normally (no physics regression)
- [ ] Camera behaves as expected while driving

## 5. Monitoring
- [ ] Monitoring overlay opens
- [ ] Selected-follower status line populates
- [ ] Feed status line shows `FEED: <SOURCE> / <CAMERA>` (not just `FEED: <SOURCE>`)
- [ ] Directive toast shows latest HUD message or "No directive yet."

## 6. Command Console Navigation
- [ ] Tab focuses the console
- [ ] Q/E cycle MAP / F1 / F2 / F3 / F4 / COMMANDER / REPORTS
- [ ] Up/Down cycle the action list; Enter activates the highlighted action
- [ ] B backs out / cancels as expected

## 7. MAP / Live Markers
- [ ] CV and F1-F4 marker icons are visible over the live map image at normal resolution
- [ ] Selected follower's marker is visibly highlighted (yellow) vs. the others (gray)
- [ ] Markers stay within the map bounds when the actor is off-map
- [ ] Route/waypoint marker still visible and unaffected
- [ ] Moving the command vehicle visibly moves its marker

## 8. Minimap
- [ ] CV marker live and moving
- [ ] Selected-follower "SEL" marker appears when a follower is selected, hidden otherwise
- [ ] N indicator is readable
- [ ] Vendor minimap is NOT re-enabled

## 9. Command Feed
- [ ] MAP page shows a command feed automatically (no button press needed)
- [ ] "Cycle Command Camera" cycles FRONT/REAR/LEFT/RIGHT by mouse AND by keyboard
- [ ] Only-one-camera case shows "Only one camera available" instead of a broken cycle

## 10. F1-F4 Default Feeds
- [ ] F1 shows a feed immediately on page open
- [ ] F2 shows a feed immediately on page open
- [ ] F3 shows a feed immediately on page open
- [ ] F4 shows a feed immediately on page open

## 11. F1-F4 Camera Cycling
- [ ] F1 Cycle Camera works by mouse AND keyboard
- [ ] F2 Cycle Camera works by mouse AND keyboard
- [ ] F3 Cycle Camera works by mouse AND keyboard
- [ ] F4 Cycle Camera works by mouse AND keyboard

## 12. Commander / Preflight
- [ ] COMMANDER page shows the original Preflight line (`Preflight OK: ...` or a bounded warning)
- [ ] COMMANDER page shows the new compact Demo Health line (`Preflight: Fleet X/4 | Map OK | CV OK | Feeds OK | Scenario OK | Logs OK`)
- [ ] Neither line overflows its panel or uses oversized text

## 13. Scenario Controls
- [ ] Start Scenario button and F5 both work
- [ ] Pause/Resume button and F7 both work
- [ ] Next Event button and F6 both work
- [ ] Reset Scenario button and F8 both work
- [ ] Scenario status/readout updates after each action

## 14. F3 Issue / Takeover / Return
- [ ] F3 issue can be triggered
- [ ] Takeover request/confirm flow works
- [ ] Returning from takeover restores command vehicle control cleanly

## 15. Pause Menu
- [ ] P (and Esc) reliably opens the Pause Menu
- [ ] Resume closes it and returns control
- [ ] Return to Main Menu loads `MAP_ESCROW_MAIN_MENU`
- [ ] Save Session shows a bounded success or failure message (not a crash or blank)
- [ ] Quit Game exits cleanly
- [ ] Keybinds/Settings buttons show bounded informative text, not "not yet implemented"

## 16. Reports / Export
- [ ] Export Summary produces a bounded status message including a file path
- [ ] The exported file exists on disk at the reported path
- [ ] Event count increases as you cycle feeds, run scenario events, and export

## 17. Reset / Repeat
- [ ] Scenario/demo reset returns to a clean starting state
- [ ] A second full pass behaves the same as the first (no stuck state, no duplicate markers, no leaked feed sources)

## Known limitations to expect (not bugs)
- Monitoring feed status suffix now shows source + camera; this was verified via code and should show e.g. `FEED: F3 / FRONT`.
- Minimap shows CV + selected-follower markers only, not all of F1-F4 simultaneously (by design, to avoid clutter).
- Settings menu has no adjustable options yet — this is expected, not a bug.

## RA31 update note (mission-script hardening pass)

As of RA31A-1A/1B/GYM-PASS, several items above are superseded:

- **Mission D tasking now targets F4** (was F2) for the initial waypoint/investigate objective; the F3
  issue/takeover/recovery sequence is unchanged. Commander-tab guidance text now reads CMD-001 through
  CMD-005 in order: mission start, assign F4, monitor F3, takeover required, checkpoint complete.
- **Full MAP page no longer auto-shows a command-feed inset** (contradicts old §9 line 1 above — that
  behavior was intentionally removed in RA30C). Command feed remains available via the F1-F4 follower
  pages.
- **Minimap now shows ME (arrow only, no text) plus live F1-F4 heading-up markers** (contradicts old §8
  and the "known limitation" above, both superseded by RA30D-2/2B). "HDG" text label is hidden; no "N"
  compass is shown since the minimap rotates with heading.
- **The COMMANDER tab is the live mission-control display**: "Start Mission D" / "Pause/Resume" /
  "Next Event" / "Reset Scenario" drive `UESCROWScenarioDirectorSubsystem` directly; the action list
  (Preflight Check / Next Demo Step / Trigger F3 Issue / Force Takeover Required / Force Report Ready /
  Force Recovery / Clear Alerts / Reset Demo) remains available for manual/facilitator control.
- **The REPORTS tab is the report/export display**: shows F4 task status, F3 recovery status, final
  fleet-export readiness, and session/event-count info. "Export Summary" is now labeled **"Export
  Mission Log"** (it already writes real CSV/JSONL via the event log subsystem). "Clear Completed" is
  now labeled **"Clear Completed (Mock)"** since it is still a status-string-only mock, not a real
  action — do not expect it to change report state.
- The MAP tab's waypoint-assignment control is now labeled **"Set Waypoint"** (was "Assign Task"); the
  non-functional "Open Full Map" placeholder and "Cycle Command Camera" are hidden from the MAP tab
  (camera cycling remains on the F1-F4 follower pages; moving/duplicating it onto the COMMANDER tab was
  evaluated and deferred as not low-risk this pass).
- The blue on-screen BeginPlay debug print ("ESCRoW: CommandVehicle CanThrottle enabled at BeginPlay")
  has its on-screen display disabled in the currently-open editor session, but **this specific
  fix has not yet persisted to disk** — a repeated, reproducible `save_assets` limitation isolated to
  `BP_ESCROW_CommandVehicle` (confirmed across multiple attempts across two passes). A manual save
  (open the Blueprint, Ctrl+S) is needed to make the fix durable.

### Remaining known UI limitations (not addressed in RA31A-GYM-PASS, out of scope)
- Full MAP page tactical-map squish/checkerboard-style artifacts, if any remain, are not addressed here.
- The fixed/bounded global HUD frame redesign has not been implemented; Commander/Reports layout still
  uses ad hoc wrapping (`AutoWrapText` is already on for the scenario readout and status lines) rather
  than a true bounded frame.
- Minimap marker clustering (F1-F4 overlapping when followers are close together) is unresolved —
  documented separately in `ESCRoW_Map_Minimap_Architecture.md`.
- A dedicated full tactical-map mode (as distinct from the current single live-capture MAP page) is not
  implemented.

## RA32A-1 update note (fixed command-console frame)

- **The Command Console shell no longer resizes between tabs.** Root cause: `SizeBox_6` (wrapping the
  MAP/F1-F4/COMMANDER/REPORTS `PageSwitcher`) had a fixed `WidthOverride` but no `HeightOverride`, so the
  console auto-sized to whichever page's content was tallest. It now has a fixed `HeightOverride` (650,
  matching the MAP page's tactical-map size as the largest/reference layout), with `bOverride_HeightOverride`
  explicitly enabled — shorter pages (Commander, Reports) now render inside the same fixed region with
  blank space below rather than shrinking the whole console.
- **Reports tab now reads as a deliberate panel**: retitled "MISSION REPORTS", with labeled sections
  "LIVE STATUS" (F4/F3/final-fleet dynamic rows), "EXPORT" (export status), and "REPORT ACTIONS" (the
  three buttons), styled to match the existing Commander "SCENARIO CONTROL" sub-header look. The RA31A-2
  dynamic `RPT_Item1`/`RPT_Item2` chains and the hidden `RPT_StatusText` are unchanged.
- **Remaining limitations:**
  - Reports/Commander status rows still refresh on tab entry (or on the action that changes them via
    existing calls), not instantly on every action — a low-risk "refresh Reports on Send/Export" wiring
    was attempted but blocked by the same Unreal MCP tool limitation noted in
    `ESCROW_Map_Minimap_Architecture.md` (cannot create new call-nodes to custom Blueprint functions);
    not fixed this pass.
  - Tactical-map squish/checkerboard, minimap marker clustering, and a dedicated full tactical-map mode
    remain out of scope, unchanged from RA31.
  - Full HUD visual redesign was not attempted; only sizing and Reports content/labels changed.

## RA33A-1 update note (facilitator-ready demo pass)

- **New doc**: `Docs/ESCRoW_Facilitator_Demo_Runbook.md` — a step-by-step 3-5 minute demo script,
  control reference, expected cues, and troubleshooting for running the demo live.
- **Commander tab now shows an in-HUD shortcut hint**: "Demo controls: F5 Start • F6 Next • F7 Pause •
  F8 Reset", a small muted line under the scenario button row — a facilitator no longer needs to
  memorize the keys from documentation alone.
- **Reports tab report button is now demo-robust.** Renamed "Send Selected Report" → **"Send Pending
  Report"**; `Escrow_SendSelectedFollowerReport` now checks the selected follower first, then falls back
  to F4, then F3, before reporting "No pending reports." — a participant no longer gets a confusing
  "No report ready for F3" just because F3 happened to be selected while F4's report was the one ready.
  Verified live for the original "selected follower has a report" path (unchanged, still fires
  correctly) and structurally verified for the new fallback branches (clean compile, all pin wiring
  confirmed); the fallback branches themselves could not be confirmed with a clean live screenshot this
  pass due to intermittent `Enter`-key unresponsiveness in the PIE automation environment (same class of
  flakiness noted in RA32A-1) combined with tab navigation unavoidably re-selecting F4 en route to
  REPORTS from F1-F3. Recommend a manual (human) pass to confirm the fallback text on-screen.
- **F4 tasking and F3 takeover/recovery clarity**: inspection found these were already adequately
  communicated by existing text (CMD-002 explicitly names F4; Reports LIVE STATUS already shows
  "F3 recovery status — takeover required" / "recovered"; Commander guidance already lists CMD-004
  "Direct control required for F3 recovery") — no changes made, since forcing the exact literal phrase
  "TAKEOVER REQUIRED: F3" would require editing the C++ scenario schedule text, out of scope for this
  pass without asking first.
- **Export/end-state clarity**: CMD-005 ("Mission checkpoint complete. Fleet status report ready for
  export.") and the Reports EXPORT section/Export Mission Log button were confirmed already present and
  working from prior passes; no changes made.
- No map/minimap/vehicle/follower/C++ files were touched this pass.

## RA33B-2 update note (graph integrity fix + live Report Record)

- **Root cause of the RA32A-2 giant-banner bug**: `Escrow_FollowerAction`'s real, live `ActionId==0`
  handler was a *different* branch node than the one the RA33B-1 pass had edited. `read_graph_dsl`
  had printed the RA33B-1 edits inline as if they were the active `ActionId==0` body, but tracing
  actual pin connections from `FunctionEntry` showed that branch (`IfThenElse_22` and its whole
  sub-chain, including the RA33B-1 Report Record toggle) had zero incoming exec connection — dead
  code that never ran. The real button press always executed a separate, untouched branch that
  still built the full "Callsign: X | State: Y | ..." diagnostic string. **Lesson: do not trust
  `read_graph_dsl` alone for execution-path truth on this function; verify via direct pin tracing
  from `FunctionEntry`.**
- **Cleanup**: removed ~65 proven-unreachable nodes across two dead-code groups (the RA33B-1
  Report Record toggle attached to the disconnected branch, and the old diagnostic-string builder's
  now-orphaned tail) after individually confirming each had zero live consumers.
- **Report Record reimplemented on the real live path.** The panel now shows only `Report Record —
  F<N>` and `Follow target: <COMMAND or F#>` — deliberately narrower than the original suggested
  format, because Callsign/State/Autonomy/Objective/Report-ready/Takeover-required are already
  shown in the always-visible follower-page header, and including them again overflowed into the
  bottom hint-bar row (confirmed via live PIE screenshot before trimming). Verified open/close on
  all four follower pages via live PIE screenshots.
- Report Record no longer writes anything into `HUDMessages`/`HUD_MissionStatusText` — it only
  toggles its own panel, so it cannot suppress or overwrite a mission/commander message.
- RA32A-2's containment fixes (wrapping/clipping on the status line, follower header, and Reports/
  Commander/follower buttons) were confirmed still intact this pass.
- Follower-feed avatar/hands hiding was not re-attempted — the RA33B-1 tool limitation (cannot
  create a child-scoped component override for the inherited `Driver` mesh without risking the
  shared vendor `BP_BaseCar` asset) is unchanged; see the runbook's Known Limitations section.

## RA35A-1 update note (facilitator report-deck pivot)

- **New `8` key**: shows an upper-right "NEW REPORT" toast and delivers the next entry in a
  fixed 4-report mission-brief deck to a new panel on the REPORTS tab, additive to the existing
  RPT_ActionsBox/Send-Pending-Report/Export flow (untouched). See the runbook's new §8 for the
  full walkthrough and exact report text.
- **New REPORTS-tab controls**: `< Previous` / `Confirm Report` / `Next >` step through the
  deck; mouse-click only (not wired into the existing `Up`/`Down` action-highlight cycling for
  MAP/COMMANDER/REPORTS actions — a facilitator with a keyboard-only setup should use `8` to
  advance and rely on the toast/panel text rather than clicking, or add keyboard reach to these
  three buttons in a future pass).
- **HUD text de-emphasized F5**: the persistent status line's pre-start default and the
  Commander-tab hint line now read "Press 8 for report demo..." and "8 New Report • REPORTS
  Confirm/Next • COMMANDER for Mission D" respectively; `F5`/`F6`/`F7`/`F8` bindings themselves
  are unchanged and still start/advance/pause/reset Mission D exactly as before.
- **Verification note**: automated PIE keypress simulation (`PressKey` via the Slate inspector
  toolset) fired the `8` binding's Pressed event twice per logical call in this session's testing
  (confirmed by the deck landing on Report 2 after exactly one `PressKey "Eight"` call, both in
  this pass's first and second PIE sessions) — a testing-tool double-fire, not a code defect; a
  real physical keypress fires the event once. Also found: recompiling `WBP_ESCROW_CommandHUD`
  while a PIE session already had it live corrupts that live widget instance (every widget
  getter starts returning Accessed-None) — always `StopPIE` before any Blueprint edit/compile,
  never compile mid-session.

## RA36A-1 update note (Display Mode / Vehicle-Control Role condition plumbing)

- **Additive only, no behavior change**: added `EESCROWDisplayMode` (HUD Primary/Console
  Primary) and `EESCROWVehicleControlRole` (Driving/Not Driving) to `ESCROWTypes.h`, new fields
  + accessors on `UESCROWSessionSubsystem` (default HUD Primary + Driving, matching current
  behavior exactly), and both labels now appended to `GetConditionSummary()` — which already
  flows automatically into the Reports tab session line and every event-log CSV/JSONL export
  via the existing `ConditionSummary` field, with no further wiring needed.
- **Setup screen**: `WBP_ESCROW_ExperimentSetup` gained two new combo-box rows (Display Mode,
  Vehicle-Control Role) with one-line explanations, matching the existing Mission
  Condition/Complexity combo-box pattern. Visible and selectable now.
- **Known gap, next step**: the C++ module has not been rebuilt this pass (this session cannot
  close the editor to run a cold `Build.bat`), so the two new combo boxes are not yet wired into
  `Escrow_ValidateAndStartSession` — a selection made there does not yet persist. See
  `Docs/ESCRoW_Display_Control_Condition_Plan.md` §5-6 for the exact next step (close editor,
  rebuild, reopen, then wire the two combo boxes' selected index into
  `SetDisplayModeByIndex`/`SetVehicleControlRoleByIndex`).
- No HUD, console, map/minimap, scenario, vehicle, or follower behavior was touched this pass.

## RA36A-2 update note (combo boxes wired to session subsystem)

- Cold C++ rebuild confirmed successful (`SetDisplayModeByIndex`, `SetVehicleControlRoleByIndex`,
  `GetDisplayModeName`, `GetVehicleControlRoleName` all discoverable in the Blueprint editor).
- `Escrow_ValidateAndStartSession` now reads `CB_DisplayMode`/`CB_VehicleControlRole`'s selected
  index and calls the two new setters, spliced in as a small node insert right after the existing
  `SetComplexityConditionByIndex` call — no full-function rewrite, everything else in that
  function is untouched (confirmed via DSL read-back).
- Verified live: the Reports tab session line now reads "...Condition: Mission D - Driver +
  Takeover / Low Complexity / HUD Primary / Driving | Events: 12" at the default values — the
  new labels reach the HUD automatically via `GetConditionSummary()`, no HUD edit needed. Report
  deck toast (`8`), REPORTS panel, MAP tab (no feed overlay), and minimap all reconfirmed
  unaffected in the same PIE session.
- Could not live-test the alternate combo selections (Console Primary + Not Driving) — the Main
  Menu's UMG canvas isn't reachable via the available input-simulation tooling in this
  environment (pre-existing `CommonUI`/`LogUIActionRouter` focus limitation, unrelated to this
  pass). Correctness rests on the wiring being structurally identical to the already-proven
  Mission Condition/Complexity pattern.
- No new Blueprint compile errors or Accessed-None runtime errors.

## RA36B-1 update note (manual Switch Seat foundation)

- New key **`N`** (preferred `X` was already bound to `Escrow_Cancel`) toggles a new plain bool
  `bESCROWInPassengerConsoleSeat` on `BP_ESCROW_PlayerController` via a new `Escrow_ToggleSeat`
  function — deliberately not reusing `UESCROWPlayerControlRouterComponent`'s
  `CurrentPlayerMode`, since that enum already drives an unrelated HUD-interaction state machine
  and reusing it would have risked cross-talk.
- Camera switches correctly both directions in live PIE (driver cockpit ↔ passenger/console
  view, the latter showing the `Console_Display` panel) by toggling `SetActive` on the vendor
  `Camera_InCar` and ESCROW-owned `Camera_InCar_Passenger` components — no edit to
  `BP_ESCROW_CommandVehicle` was needed or made.
- Driving-input suppression is explicitly deferred (vendor input-binding internals are
  black-boxed); pressing `N` currently only switches the camera + prints a status line.
- Re-confirmed in the same PIE session: Tab/console, MAP tab (no feed overlay), `8` → report-deck
  toast, minimap — all unaffected.
- One unresolved oddity: the very first `N` press after a fresh PIE start printed "Seat: Driver"
  instead of the expected "Seat: Passenger/Console" once; every subsequent press alternates
  correctly. Node graph confirmed correct via direct pin inspection — likely a first-frame/
  environment timing quirk, not a logic bug. Worth re-checking with real keyboard input.
- No new Blueprint compile errors. (One transient "Accessed None"/"TestHUDRef" error batch was
  observed, but traced to a mid-PIE Blueprint compile mistake made earlier in this same pass,
  fully resolved by stopping PIE before recompiling — see the condition-plan doc for detail. The
  final, saved state compiles clean and was verified in a subsequent fresh PIE session with zero
  new errors.)

## RA36B-2 + RA36B-3-lite update note (physical console mirror foundation)

- New additive widget `WBP_ESCROW_PhysicalConsoleMirror` + a runtime-created `UWidgetComponent`
  (via `AddWidgetComponent` on the possessed `CommandVehicleRef`, no `BP_ESCROW_CommandVehicle`
  edit) mirror Seat / Console Mirror status / Display Mode / Vehicle Control Role onto a
  world-space panel attached to `Console_Display`.
- Verified live in a fresh PIE session via direct state inspection of the live actor/widget
  instances (see condition-plan doc for the exact values read back): component valid, attached to
  `Console_Display`, status label "Active on Console Display", and all four text fields showing
  correct real values ("Seat: Driver", "Display Mode: HUD Primary", "Vehicle Control Role:
  Driving") — confirming `DisplayMode`/`VehicleControlRole` are genuinely readable, not stubbed.
- **Could not live-test the `N` seat toggle in this pass**: the available input-simulation tooling
  (Slate `PressKey`) does not reach the PIE game viewport's input system — pressing `N` via that
  tool left `bESCROWInPassengerConsoleSeat` unchanged (confirmed by reading the live instance
  before/after). This is the same class of pre-existing limitation already noted under RA36A-2 for
  the Main Menu combo boxes. `Escrow_ToggleSeat`'s own internal nodes were not modified this pass
  (only a new call was appended after its existing, previously-unconnected `then` pin), and the
  whole Blueprint compiles clean, but the actual keyboard toggle should be re-confirmed by a human
  tester with real key input.
- Re-confirmed unaffected in the same PIE session (via clean log, no new runtime errors): Tab/
  console, MAP tab (no feed overlay), `8` → report-deck toast, minimap — none of that code was
  touched this pass.
- No new Blueprint compile or runtime errors in this pass's PIE session (log checked before and
  after; a handful of resolved development-time DSL syntax errors from earlier in this same pass
  are visible in the log but predate the verification PIE session and were all fixed before
  compiling/saving).

## RA36B-REPAIR-1 update note (passenger look + console placement repair)

- Human testing found RA36B-FIX-1 did not actually fix passenger free-look or console placement.
  Both were replaced structurally this pass — see the condition-plan doc §9 for full detail.
- Passenger look is now driven by two new `MouseX`/`MouseY` raw axis events on
  `BP_ESCROW_PlayerController` feeding a new `Escrow_UpdatePassengerLook` function, which directly
  rotates `Camera_InCar_Passenger` while `bESCROWInPassengerConsoleSeat` is true (pitch clamped to
  ±60°, reset to center on each seat entry). Driver mode's own look is untouched.
- The console mirror is no longer runtime-spawned. A `WidgetComponent` named
  `ConsoleMirrorDisplay` was added directly in `BP_ESCROW_CommandVehicle` as a child of the
  existing `Console_Display`, with `WBP_ESCROW_PhysicalConsoleMirror` assigned as its widget
  class. There is exactly one console-mirror surface now, anchored to the intended display.
- **Verification limitation found this pass**: the already-open level's placed vehicle instance
  did not pick up the new component's saved properties in this editor session (a known
  Blueprint-instance-refresh quirk), so the mirror widget did not visibly render in this
  session's PIE. The saved Blueprint asset was directly confirmed correct. **A human tester should
  reopen the map or restart the editor before verifying** — this is expected to resolve on a fresh
  level load.
- Repeated PIE start/stop cycling in this pass also triggered one unrelated, transient
  `TRASH_CameraComponent_2` error from the vendor `ABP_ThirdPerson_driving` Animation Blueprint —
  did not recur in a clean single PIE session; likely an artifact of rapid restarts, not of any
  change made this pass. Worth a clean-session re-check.
- Tab/console, MAP tab, `8` report-deck toast, and minimap were not touched this pass; no new
  Blueprint compile errors. Live confirmation of the `N` toggle, look responsiveness, and the
  mirror's visible appearance all still require a human PIE pass (see limitations above).

## RA36B-REGRESSION-FIX-DRIVER-GAZE update note (critical driver-gaze rollback)

- **RA36B-REPAIR-1's passenger raw-mouse-look implementation caused a driver-gaze regression** in
  human testing — driver-seat mouse/look stopped working entirely after that pass.
- **RA36B-REGRESSION-FIX removed the risky input path**: the new `MouseX`/`MouseY` input events on
  `BP_ESCROW_PlayerController` were created with `bConsumeInput = true` (the UE default), which
  let the Controller-level binding block the mouse axis from ever reaching the vendor pawn's own
  look handling, since the Controller receives input before its possessed Pawn. Both events and
  their directly-wired helper nodes were deleted; nothing else was touched.
- **Passenger free-look is deferred** until a genuinely input-safe mechanism is designed.
  `Escrow_UpdatePassengerLook` and the `PassengerLookYaw`/`PassengerLookPitch` variables remain as
  inert, harmless dead code (unreferenced by any input path).
- **Driver-seat behavior is the baseline and must not be put at risk again** — any future
  input-related change must confirm it does not set `bConsumeInput = true` (or otherwise
  intercept input) on a PlayerController-level binding.
- `N` seat-toggle wiring and the console-mirror refresh call remain structurally intact
  (unaffected by this rollback). Compile clean, PIE log clean, no new errors. **Actual mouse input
  could not be programmatically injected in this environment** — a human must confirm driver gaze
  is restored with real mouse input before this is considered closed.

## RA36B-PASSENGER-SEAT-FIX update note (passenger look repaired without repeating the regression)

- **Passenger seat now supports look input.** Reintroduced `MouseX`/`MouseY` bindings on
  `BP_ESCROW_PlayerController`, this time with `bConsumeInput = false` explicitly set on both —
  the exact flag that caused the prior driver-gaze regression. With it false, both the vendor
  pawn's own driver-look handling and the new passenger-look logic receive the same input
  independently; nothing is consumed/blocked.
- **Physical console display now uses a clean, twist-free orientation** aligned to
  `Console_Display` (`Pitch 70, Yaw 0, Roll 0` — faces straight back toward the passenger with a
  20° upward tilt). `Console_Display` remains the sole physical display anchor; `ConsoleMirrorDisplay`
  (added in RA36B-REPAIR-1) is unchanged and still inherits this orientation as its child.
  Screenshots still show some keystoning, most likely a proximity/perspective artifact (the panel
  sits very close to the passenger camera) rather than a rotation defect — flagged for a possible
  follow-up on distance/placement, not further rotation changes.
- **Driver behavior remains the baseline and must be preserved in future passes.** Any future
  PlayerController-level input binding must confirm `bConsumeInput` stays `false` unless there is
  a specific, deliberate reason to consume that input.
- Hit and resolved a transient external file-lock issue (OneDrive-related, Windows Error Code 32)
  while saving; both assets saved successfully once it cleared, confirmed via fresh file
  timestamps.
- PIE-verified: clean compile, clean log in a session started after the successful disk save,
  baseline driver seat and corrected console rotation both confirmed live. Real mouse-driven look
  (both passenger and driver, including the round-trip via `N`) still requires human confirmation
  — this environment cannot inject mouse input.

## RA36C-1 update note (passenger look moved to Tick, console-placement instance-override found)

- **Passenger look replaced again, structurally**: the raw `MouseX`/`MouseY` Input Axis Key events
  on `BP_ESCROW_PlayerController` (from RA36B-PASSENGER-SEAT-FIX) were deleted outright rather than
  further tweaked, per this pass's explicit instruction not to keep layering the same approach.
  `EventTick` now branches on `bESCROWInPassengerConsoleSeat`; when true it calls the engine's
  `GetInputMouseDelta()` (a direct per-frame poll of `PlayerInput`, not an `InputComponent`
  binding) and feeds `DeltaX`/`-DeltaY` into the existing `Escrow_UpdatePassengerLook`. When false,
  the branch does nothing — no code runs, no variables touched. This sidesteps the whole class of
  input-stack/consumption uncertainty that raw Axis Key events carry, whatever the true cause of
  their unreliability was. Driver look is structurally untouched (no edit near `Camera_InCar` or
  any vendor input binding).
- **Root cause of the console "giant gray board" found and confirmed empirically, not guessed**:
  the *placed* `BP_ESCROW_CommandVehicle` actor in `MAP_ESCROW_DESERT_INTEGRATION_TEST.umap` carries
  its own per-instance property overrides on `Console_Display` (full transform, rotation
  `Pitch 70 / Yaw 180 / Roll 180` — not the `Yaw 0/Roll 0` RA36B-PASSENGER-SEAT-FIX believed it had
  set) and on `ConsoleMirrorDisplay` (`DrawSize` overridden to **500×500**, roughly 5-6× the
  Blueprint default). A passenger-camera screenshot taken from this pass's live PIE session shows
  exactly the reported symptom: a blank, badly-angled, frame-filling panel. This explains why every
  prior RA36B console-placement pass "fixed" the Blueprint asset but the live map never visibly
  changed — none of those passes had read the placed instance's actual override values, only the
  Blueprint CDO or a fresh/unplaced instance.
- **Fix applied this pass, scoped to the allowed asset only**: `Console_Display` on
  `BP_ESCROW_CommandVehicle`'s class defaults moved further from the passenger camera and lowered
  (location `X 72→100, Z 166.7→132`), shrunk (`Scale 0.40×0.60 → 0.35×0.45`), and re-tilted to a
  shallow `Pitch 25° (Yaw 0, Roll 0)` — a dash-mounted-screen angle instead of the near-vertical
  70°. `ConsoleMirrorDisplay`'s `DrawSize` was brought down to `42×54` to match the resized mesh.
  **This fix cannot visibly manifest in the current map** until the per-instance overrides above
  are cleared, because maps/level-actor edits are out of scope for this pass (hard constraint).
  Human/next-pass action needed: select the placed vehicle actor, and in its Details panel
  right-click → "Reset to Default" the `Console_Display` transform and `ConsoleMirrorDisplay`
  `DrawSize` overrides so the actor inherits the corrected Blueprint defaults above.
- `Camera_InCar_Passenger` has **no** instance override on this placed actor (confirmed identical
  to the Blueprint CDO), so the Tick-based passenger-look fix is not blocked by the same issue.
- Automated `N`-key simulation via the Slate `PressKey` tool still does not reach the PIE game
  viewport in this environment (no "Seat:" print appeared in the log after the call) — consistent
  with the same limitation logged under RA36A-2/RA36B-2. Seat-toggle wiring itself was inspected
  node-by-node and confirmed structurally correct (`N` Pressed → `Escrow_ToggleSeat` →
  `Escrow_RefreshPhysicalConsoleMirror`), but a human must confirm the live keypress.
- Both Blueprints compile clean and saved individually (no Save All); `WBP_ESCROW_ExperimentSetup`
  and both HUD widgets were not opened or touched. Real mouse feel for passenger look still
  requires human confirmation with physical input — this environment cannot inject mouse deltas.

## RA36C-2 update note (runtime console-display canonicalization; giant-board diagnosis corrected)

- **New function `Escrow_ApplyConsoleDisplayRuntimeDefaults` added to `BP_ESCROW_CommandVehicle`**,
  called once from the end of the existing `EventBeginPlay` chain (spliced in as a single new call
  node after the existing `PrintString`, exactly the "small node insert" pattern used since
  RA36A-2 — nothing else in `BeginPlay` was touched or reordered). At runtime it forces
  `Console_Display`'s relative Location/Rotation/Scale and `ConsoleMirrorDisplay`'s `DrawSize` to
  RA36C-1's intended values, overriding whatever per-instance values the placed map actor carries.
  This is deliberate: the map is not saved or edited, so the placed actor's saved data still has
  its bad overrides, but the live PIE instance now self-corrects every time it begins play.
  Verified directly on the live PIE actor after `BeginPlay`: `Console_Display` reads back
  Location `(100, 60.72, 132)`, Rotation `(Pitch 25, Yaw ≈0, Roll ≈0)`, Scale `(0.35, 0.45, 1)`;
  `ConsoleMirrorDisplay.DrawSize` reads back `(42, 54)` — down from the map's saved `500×500`.
  The map's *saved* per-instance values are untouched and still show the old broken numbers when
  queried outside PIE; only the live runtime instance is corrected, exactly as intended.
- **Important correction to the RA36C-1 diagnosis**: with the runtime fix active, the
  passenger-view "giant blank board" was re-screenshotted and **still present, pixel-for-pixel
  identical** to the pre-fix screenshot. To isolate the cause, `Console_Display` and then
  `ConsoleMirrorDisplay` were each temporarily hidden (`bHiddenInGame`) on the live PIE instance
  only, one at a time, and the board persisted through both. A screen-space raycast into the board
  (`ScreenCoordsToWorld`) hit a surface only ~11cm from the passenger camera — consistent with the
  passenger camera clipping into nearby vendor interior geometry (most likely a sun visor or
  headliner, given the passenger camera sits ~60 units higher and on the opposite side of the cab
  from the driver camera), not with either display component. **The giant board is not
  `Console_Display` or `ConsoleMirrorDisplay` at all.** Both components' visibility was restored
  before ending the PIE session; nothing about this diagnostic was saved. This pass still
  implements exactly what RA36C-1/RA36C-2 specified (the display components now canonicalize
  correctly at runtime, confirmed by direct property read-back), but that specific visual
  complaint needs a fresh, separate investigation into passenger camera clearance versus vendor
  interior trim — not further Console_Display transform changes.
- Passenger-look path (`EventTick` → `GetInputMouseDelta` → `Escrow_UpdatePassengerLook`, gated on
  `bESCROWInPassengerConsoleSeat`) was not modified this pass; re-inspected and confirmed unchanged
  and structurally intact. No raw `MouseX`/`MouseY` Input Axis Key events were reintroduced.
- Driver-relevant `BeginPlay` lines (`Camera_InCar` activation, `Rear_Camera`/`VRCamera`
  deactivation, `CanThrottle`) are byte-for-byte unchanged — confirmed via before/after DSL diff.
- A handful of `TRASH_CameraComponent_N` / `ABP_ThirdPerson_driving` runtime errors appeared during
  this session's PIE runs — this is the same pre-existing repeated-PIE-restart artifact already
  documented under RA36B-REPAIR-1, not a new regression; no vendor asset was touched this pass.
  `LogUIActionRouter`'s `CommonGameViewportClient` error is the same pre-existing, already-documented
  limitation from RA36A-2. No new Blueprint compile errors and no errors traced to
  `Escrow_ApplyConsoleDisplayRuntimeDefaults` or the passenger-look path.
- Only `BP_ESCROW_CommandVehicle.uasset` was saved this pass (not `BP_ESCROW_PlayerController`,
  since it was not touched). The map was loaded for PIE testing only — never edited or saved.

## RA36C-3 update note (passenger camera clearance fixed; giant board explained)

- **The giant gray/white board was camera clipping, not the console display** — confirmed this
  pass by moving `Camera_InCar_Passenger` and re-testing; it is very likely the passenger-side door
  window viewed at extreme close range (a few cm), not a bug in `Console_Display`/
  `ConsoleMirrorDisplay` (both already ruled out structurally in RA36C-2). `Console_Display`
  canonicalization is untouched and still verified correct at runtime.
- **A second stale per-instance override was found and fixed the same way**: the placed vehicle
  actor's `Camera_InCar_Passenger` also had a location frozen from an old value (`Z 185.8`) that a
  Blueprint class-default edit alone could not override — the identical class of problem RA36C-2
  found for `Console_Display`. New function `Escrow_ApplyPassengerCameraRuntimeDefaults` added to
  `BP_ESCROW_CommandVehicle`, called from `BeginPlay` right after
  `Escrow_ApplyConsoleDisplayRuntimeDefaults`, forcing `Camera_InCar_Passenger`'s relative
  Location/Rotation at runtime the same way. The map itself was never touched or saved.
- **A related discovery**: the vendor driver camera (`Camera_InCar`) *also* carries a per-instance
  override on this placed actor — its live position (`X 26.98, Y -61.58, Z 185.19`) differs from
  its own Blueprint CDO default (`X 9.28, Y -35.69, Z 125.80`). This is presumably an intentional
  hand-tuned adjustment from whoever placed this actor, since the driver seat has been confirmed
  working throughout every pass. It was read-only reference data this pass, not edited — `Camera_InCar`
  is a vendor component and was never touched.
- **Final passenger camera position, found by iterating and screenshotting from the actual live
  camera position each time** (not guessed once): `(X 15.0, Y 15.0, Z 170.0)`, rotation unchanged
  `(0, 0, 0)`. Earlier attempts at mirroring the driver's position one-to-one (`Y ≈ ±62`) still put
  the camera right against the passenger-side window at any plausible seat height; moving inward
  toward the cab's centerline resolved it. The resulting view shows dashboard, gauges, console
  screen, cup holders, and gear shifter clearly — a real interior view, not a clipped board. A
  strip of bright window remains at the right edge of frame, which is normal/expected for a
  seated-passenger view near a side window, not a clipping artifact.
- Passenger-look path (`EventTick` → `GetInputMouseDelta` → `Escrow_UpdatePassengerLook`) and the
  `N` seat-toggle wiring were re-inspected and confirmed unchanged and structurally intact; nothing
  in `BP_ESCROW_PlayerController` was touched or saved this pass.
- No new Blueprint compile errors. The same pre-existing `TRASH_CameraComponent`/
  `ABP_ThirdPerson_driving` and `CommonGameViewportClient` log noise from repeated PIE
  restarts in this long session reappeared, already documented under RA36B-REPAIR-1/RA36A-2 — not
  a new regression, and no vendor asset was edited.
- **Still requires human confirmation**: real mouse feel for passenger look, the actual `N` keypress
  round-trip, and a human eyeballing the final passenger camera framing in a real headset/monitor
  view rather than an offline screenshot, since "acceptable" framing is inherently subjective.

## RA36D-1 update note (diagnostic instrumentation only, no fix)

- Human testing after RA36C-3 found passenger gaze still doesn't work, despite the passenger
  camera no longer clipping. Full structural re-inspection of the `N` → `Escrow_ToggleSeat` →
  camera-activation path and the `EventTick` → `GetInputMouseDelta` → `Escrow_UpdatePassengerLook`
  path found nothing obviously broken — both are wired correctly, verified at the actual exec-pin
  level, not just from the DSL text dump (which cosmetically reorders pure-node binds in a way
  that first looked like a bug but wasn't). Confirmed `Escrow_ToggleSeat` uses `SetActive` only —
  no `SetViewTargetWithBlend` anywhere — meaning the driver/passenger camera swap relies entirely
  on `AActor::bFindCameraComponentWhenViewTarget`'s implicit first-active-camera-component
  discovery. This was flagged as the leading suspect.
- Added temporary on-screen/log diagnostic (`PrintString`, key `RA36D_Diag`, refreshes in place)
  to `EventTick`'s passenger branch, showing mouse delta, passenger look yaw/pitch, and
  `Camera_InCar_Passenger`'s live relative rotation, so a human could observe live values while
  pressing `N` and moving the mouse. No camera placement or `Console_Display` changes made.

## RA36D-2 update note (explicit view-target camera proxies replace implicit SetActive-only switching)

- Human testing confirmed the RA36D-1 diagnostic prints appear correctly in passenger mode
  (`EventTick`'s passenger branch is definitely executing), but the rendered view still didn't
  respond to mouse movement — confirming the RA36D-1 suspicion that `SetActive`-only camera
  switching is architecturally insufficient.
- **`Escrow_ToggleSeat` now explicitly controls the render view target** via
  `SetViewTargetWithBlend`, instead of relying on Unreal's implicit first-active-camera-component
  discovery. Two lightweight `CameraActor` proxies (`DriverCameraProxy`, `PassengerCameraProxy`)
  are lazily spawned and owned by `BP_ESCROW_PlayerController` (new functions
  `Escrow_EnsureDriverCameraProxy` / `Escrow_EnsurePassengerCameraProxy`), aligned to the
  corresponding vehicle camera's *world* transform (`Transformation|GetWorldTransform`, not the
  relative transform used for the diagnostic), and set as the view target with `BlendTime 0.0`
  (instant, matching the existing immediate seat-swap feel). `SetActive` calls are kept alongside
  (harmless) but are no longer the only mechanism.
- **New function `Escrow_SyncPassengerCameraProxy`**, called every tick from `EventTick`'s
  passenger branch right after `Escrow_UpdatePassengerLook`, keeps `PassengerCameraProxy`'s world
  transform following `Camera_InCar_Passenger` each frame — so as the vehicle moves and the mouse
  rotates the camera component, the actual rendered view target tracks it in real time.
- No `Console_Display` or `Camera_InCar_Passenger` placement changes this pass, per instruction.
  `Camera_InCar` (vendor) was read-only (`Class|BPBaseCar|GetCamerainCar`, for the driver proxy's
  world transform) — never written to.
- Compiles clean; the transient "Spawn Transform pin invalid" errors seen mid-session were from
  an intermediate, incomplete write attempt and did not affect the final saved state (re-verified
  with a clean compile immediately before saving). No Accessed-None errors observed in PIE.
- **Tooling could not trigger `N` in PIE** (same pre-existing Slate `PressKey`-doesn't-reach-the-
  game-viewport limitation documented since RA36A-2) and a direct property write to
  `bESCROWInPassengerConsoleSeat` via the reflection tool was refused — so the new view-target
  switch could only be verified structurally (exec-graph pin wiring, self-reference targets,
  clean compile), not through an actual live toggle. Human testing is required to confirm the
  passenger view now actually moves.

## RA36E-0 update note (emergency stabilization — RA36D-2 proxy/view-target system rolled back)

- Human testing found the RA36D-2 explicit view-target proxy system made things worse, not better:
  pressing `N` no longer cleanly toggled Driver ↔ Passenger, the rendered view jumped between
  strange interior positions, and driver mouse control could be lost or compromised after toggling.
  This pass treated that report as a stop-build signal and rolled the proxy/view-target work back
  rather than patching it further.
- **Root cause identified**: the RA36D-2 proxy system was asymmetric. `Escrow_SyncPassengerCameraProxy`
  re-synced `PassengerCameraProxy`'s world transform every tick, but there was never an equivalent
  per-tick sync for `DriverCameraProxy` — so after a toggle back to driver, the render view target
  was left pointing at a stale, one-time snapshot of the driver camera's transform instead of
  tracking it live. This fully explains the reported symptoms.
- **Fix, scoped to `BP_ESCROW_PlayerController` only**: `Escrow_ToggleSeat` now always forces
  `bESCROWInPassengerConsoleSeat` to `false` and prints "Passenger mode disabled pending rebuild" —
  passenger seat is intentionally disabled, not fixed. `EventTick` was simplified back to its
  pre-RA36D-2 form (the passenger-look call is now permanently dead code since the flag can never
  become true, left in place rather than deleted). The three RA36D-2 proxy functions
  (`Escrow_EnsureDriverCameraProxy`, `Escrow_EnsurePassengerCameraProxy`,
  `Escrow_SyncPassengerCameraProxy`) and the two proxy variables (`DriverCameraProxy`,
  `PassengerCameraProxy`) were deleted. `BP_ESCROW_CommandVehicle`'s runtime-defaults BeginPlay
  hooks (`Escrow_ApplyConsoleDisplayRuntimeDefaults`, `Escrow_ApplyPassengerCameraRuntimeDefaults`)
  were inspected, confirmed unrelated to the breakage, and left untouched. No map was touched or
  saved. HUD/report/MAP widgets were not touched.
- Compiles clean; PIE verification after save showed the driver seat rendering normally at
  `BeginPlay` (steering wheel, dashboard, gauges all correct) with `CommandVehicle CanThrottle
  enabled at BeginPlay` logging as expected, and no new compile or runtime errors. Live `N`/`Tab`/
  `8` keypress round-trips still cannot be triggered by tooling (same long-documented Slate
  `PressKey`-doesn't-reach-the-game-viewport limitation) and require human confirmation.
- **Recommended next passenger-seat architecture**: do not resume building on
  `SetViewTargetWithBlend` + spawned `CameraActor` proxies layered onto the existing controller.
  Build a dedicated passenger pawn/controller-possession swap (`Possess`/`UnPossess` a small
  passenger-seat pawn with its own camera component), or a single reusable `CameraActor` per seat
  that is parented and continuously synced by a proper tick-driven component on both the driver and
  passenger paths symmetrically — not just one side. Whichever approach is chosen, verify the
  symmetry explicitly (both directions of the toggle re-sync every tick) before considering it done.

## RA38A update note (passenger seat rebuilt as a seat-anchor retarget, not a proxy camera)

- **Passenger seat is now a seat-state / shared-view-rig retarget, not a proxy camera or
  view-target architecture.** There is no `CameraActor`, no `SetViewTargetWithBlend`, and no
  reliance on Unreal's implicit "first active camera component" discovery being switched between
  multiple cameras. The same vendor `Camera_InCar` component that already renders the working
  driver view is reused for both seats — only its `RelativeLocation` moves, between two new
  ESCROW-owned anchor markers (`ESCROW_DriverSeatViewAnchor`, `ESCROW_PassengerSeatViewAnchor`,
  both plain `SceneComponent`s parented under the vendor `Cameras` component on
  `BP_ESCROW_CommandVehicle`). `Camera_InCar`'s rotation/look mechanism is never touched, so
  whatever already made driver mouse-look work continues to work identically in both seats.
- The driver anchor is not a hardcoded value — `EventBeginPlay` captures `Camera_InCar`'s actual
  starting `RelativeLocation` into `ESCROW_DriverSeatViewAnchor` at runtime, so it always matches
  the live per-instance value on the placed map actor (which differs from the Blueprint's class
  default), without ever needing to read or save the map. The passenger anchor defaults to
  `(15, 15, 170)`, the same position validated by hand in the RA36C-3 pass.
- **`N` still toggles Driver ↔ Passenger** via `Escrow_ToggleSeat` in `BP_ESCROW_PlayerController`.
  It flips `bESCROWInPassengerConsoleSeat`, calls the command vehicle's new
  `Escrow_SetSeatView(bPassenger)` to move `Camera_InCar` to the correct anchor, and calls the new
  `Escrow_SetDrivingEnabled(bEnabled)` to gate driving. Possession never changes.
- **Passenger disables driving input but preserves gaze and HUD controls.** Driving is gated
  through the vendor-exposed `CanThrottle` property (the same property already used at `BeginPlay`
  to enable throttle) rather than by touching vendor input graphs. Tab/HUD, MAP/minimap, and the
  key-8 report deck are handled entirely independently of seat state and are unaffected either way.
  `EventTick` no longer runs any passenger-specific per-frame logic — the old manual
  `GetInputMouseDelta` → `Escrow_UpdatePassengerLook` path is now fully unreachable (disconnected
  from `EventTick`), since gaze no longer needs a separate passenger-only update path.
- **Exit is expected to work identically from both seats**, because exit is not ESCROW-owned logic
  at all — structural inspection this pass found no ESCROW `E`-to-exit binding anywhere in
  `BP_ESCROW_PlayerController` or `BP_ESCROW_CommandVehicle`; the only `E` binding ESCROW owns maps
  to `Escrow_CycleNext` (follower cycling). Vehicle exit, if implemented, lives entirely inside the
  vendor vehicle pawn class via the legacy `Interact` action mapping (`E` / gamepad face-button-left,
  see `Config/DefaultInput.ini`), which this pass never touched. Since seat switching never changes
  possession or consumes additional input, exit behavior should be exactly whatever it already was
  before this pass — this still requires human confirmation, the same as before.
- Console Display condition and physical console rendering remain future work, per instruction —
  not implemented this pass.
- Structural verification only: Blueprint graphs read back clean, both Blueprints compile with no
  errors, and a fresh PIE session confirms the driver view renders correctly at `BeginPlay` with
  `CanThrottle` true and both seat anchors holding the correct live values. Live `N` keypress
  toggling still cannot be triggered by tooling (same long-documented Slate `PressKey`-doesn't-
  reach-the-game-viewport limitation) and requires human testing to confirm the passenger view
  actually appears and gaze/driving-gating behave as designed.
