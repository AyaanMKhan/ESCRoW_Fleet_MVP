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
