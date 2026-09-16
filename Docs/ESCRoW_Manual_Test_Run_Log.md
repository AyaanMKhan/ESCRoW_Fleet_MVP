# ESCRoW Fleet MVP — Manual Test Run Log

Use this file to record the results of a manual playtest pass. Copy the table below (or add rows to it) for each test session. Pair with `ESCRoW_Demo_Hardening_Checklist.md` for the recommended run order.

**Session info:** Date: __________  Tester: __________  Build/commit: __________  Map: `MAP_ESCROW_DESERT_INTEGRATION_TEST`

| Test ID | Area | Step | Expected Result | Observed Result | Pass/Fail | Severity | Screenshot/Video Filename | Notes | Suggested Fix |
|---|---|---|---|---|---|---|---|---|---|
| T01 | Main Menu | Launch game from `MAP_ESCROW_MAIN_MENU` | Menu background art visible, title/subtitle/buttons readable, no gray box | | | | | | |
| T02 | Main Menu | Click "Start Experiment / Session Setup" | Experiment Setup screen opens | | | | | | |
| T03 | Experiment Setup | Fill Researcher ID, Participant ID, generate Session ID, pick Mission/Condition/Complexity | Fields accept input, Session ID Generate works, Condition Summary updates | | | | | | |
| T04 | Experiment Setup | Click "Start Session" | Loads into desert map, command vehicle scenario begins | | | | | | |
| T05 | Main Menu | Click "Quick Start Desert Demo" | Loads directly into desert map with default session values | | | | | | |
| T06 | Command Vehicle | Drive command vehicle (WASD or configured controls) | Vehicle moves, physics feel unchanged from prior builds | | | | | | |
| T07 | Monitoring | Open Monitoring overlay (M) | Selected status, directive toast, and `FEED: <SOURCE> / <CAMERA>` line all populated | | | | | | |
| T08 | Command Console | Tab to focus console, Q/E to change pages | MAP / F1 / F2 / F3 / F4 / COMMANDER / REPORTS pages all reachable | | | | | | |
| T09 | MAP / Live Markers | On MAP page, observe CV and F1-F4 marker icons over the live map image | Markers visible at readable size, selected follower marker highlighted (yellow) | | | | | | |
| T10 | MAP / Live Markers | Drive/move the command vehicle | CV marker position updates within ~0.3-0.5s | | | | | | |
| T11 | Minimap | Observe bottom-right minimap | CV marker moves live, "SEL" marker shows for the selected follower, N indicator readable | | | | | | |
| T12 | Command Feed | On MAP page, confirm feed shown without pressing anything | Header shows "LIVE FEED: COMMAND / <CAMERA>" immediately | | | | | | |
| T13 | Command Feed | Activate "Cycle Command Camera" (mouse and keyboard) | Camera label cycles FRONT → REAR → LEFT → RIGHT → back to FRONT | | | | | | |
| T14 | F1 Feed | Open F1 page | Header shows "LIVE FEED: F1 / FRONT" with no button press | | | | | | |
| T15 | F1 Camera Cycle | Activate "Cycle Camera" (mouse and keyboard) | Label cycles through all 4 directions | | | | | | |
| T16 | F2 Feed | Open F2 page | Header shows "LIVE FEED: F2 / FRONT" with no button press | | | | | | |
| T17 | F2 Camera Cycle | Activate "Cycle Camera" (mouse and keyboard) | Label cycles through all 4 directions | | | | | | |
| T18 | F3 Feed | Open F3 page | Header shows "LIVE FEED: F3 / FRONT" with no button press | | | | | | |
| T19 | F3 Camera Cycle | Activate "Cycle Camera" (mouse and keyboard) | Label cycles through all 4 directions | | | | | | |
| T20 | F4 Feed | Open F4 page | Header shows "LIVE FEED: F4 / FRONT" with no button press | | | | | | |
| T21 | F4 Camera Cycle | Activate "Cycle Camera" (mouse and keyboard) | Label cycles through all 4 directions | | | | | | |
| T22 | Commander / Preflight | Open COMMANDER page | "Preflight: ..." and "Preflight OK/warning: ..." lines both populate, compact, no huge text | | | | | | |
| T23 | Scenario Control | Click Start Scenario / press F5 | Scenario begins, status updates | | | | | | |
| T24 | Scenario Control | Click Pause/Resume / press F7 | Scenario pauses and resumes | | | | | | |
| T25 | Scenario Control | Click Next Event / press F6 | Scenario advances one event, status/log updates | | | | | | |
| T26 | Scenario Control | Click Reset Scenario / press F8 | Scenario resets to start state | | | | | | |
| T27 | F3 Issue/Takeover | Trigger F3 issue, request takeover, confirm, then return | Takeover overlay appears, possession changes, return restores command vehicle control | | | | | | |
| T28 | Pause Menu | Press P | Pause Menu opens reliably | | | | | | |
| T29 | Pause Menu | Click Resume | Pause Menu closes, game continues | | | | | | |
| T30 | Pause Menu | Click Save Session | Bounded "Session saved: <path>" or bounded failure message shown | | | | | | |
| T31 | Pause Menu | Click Return to Main Menu | Loads `MAP_ESCROW_MAIN_MENU` | | | | | | |
| T32 | Pause Menu | Click Quit Game | Game quits cleanly | | | | | | |
| T33 | Reports/Export | On REPORTS page, click Export Summary | Bounded export status + file path shown, event count visible | | | | | | |
| T34 | Reports/Export | Check exported file on disk at reported path | File exists and contains session events | | | | | | |
| T35 | Reset/Repeat | Reset scenario/demo, repeat a prior test | Behavior matches first run, no leftover state from previous run | | | | | | |

## Severity scale
- **Blocker** — cannot continue testing / crashes
- **Major** — feature does not work as intended
- **Minor** — cosmetic or low-impact issue
- **Note** — observation, not a defect
