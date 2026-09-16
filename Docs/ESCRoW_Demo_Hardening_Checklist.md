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
