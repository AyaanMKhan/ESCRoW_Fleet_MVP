# ESCRoW Fleet MVP — Facilitator Demo Runbook

## 1. Purpose

This demo illustrates supervisory control of a command vehicle and its autonomous follower
fleet under scripted mission events: tasking, alerts, takeover demand, recovery, and report
submission. A facilitator runs the whole loop — mission start, follower tasking, an F3 issue
that demands direct control, recovery, report submission, and mission checkpoint — in about
3–5 minutes.

## 2. Pre-demo setup

1. Open the project in Unreal Editor (`ESCRoW_Fleet_MVP.uproject`), UE 5.8.
2. Confirm the currently-open level is `/Game/ESCROW/Maps/MAP_ESCROW_DESERT_INTEGRATION_TEST`
   (Content Browser → open the map directly). This is more reliable for a live demo than
   relying on the main-menu → "Quick Start Desert Demo" flow, since that flow has not been
   re-verified end-to-end in recent passes.
3. Press **Play** (PIE), in-viewport.
4. If the viewport shows a magenta/green heatmap instead of normal lighting, the editor's
   View Mode has been left on **Shader Complexity** — switch it back to **Lit** from the
   viewport's view-mode dropdown (top-left of the viewport toolbar) before or during PIE.
5. Press **Tab** to open the Command Console.
6. Confirm you can see: the tab bar (MAP / F1 / F2 / F3 / F4 / COMMANDER / REPORTS), the
   persistent status line just below it, the tactical map with fleet markers, and the
   bottom-right minimap with a heading-up arrow and F1–F4 markers.

## 3. Controls / shortcuts

| Key | Action |
|---|---|
| `Tab` | Open/focus the Command Console |
| `Q` / `E` | Switch tabs: MAP → F1 → F2 → F3 → F4 → COMMANDER → REPORTS (and back) |
| `Up` / `Down` | Move the highlighted action in the current tab's action list |
| `Enter` | Activate the highlighted action |
| `B` | Back / cancel |
| `P` or `Esc` | Pause menu |
| `F5` | Start Mission D |
| `F6` | Next scenario event (manually advances even past milestone waits) |
| `F7` | Pause/Resume scenario |
| `F8` | Reset scenario |

**Waypoint placement:** on the MAP tab, use `Up`/`Down` + `Enter` to select **Set Waypoint**,
then move the placement cursor and `Enter` to confirm (or `B` to cancel).

**Report/export:** on the REPORTS tab, `Up`/`Down` + `Enter` over **Send Pending Report** /
**Clear Completed (Mock)** / **Export Mission Log**.

**Take Over / Return:** on the F3 tab, `Up`/`Down` + `Enter` over **Take Over** opens an
in-HUD confirmation box; `Enter` again confirms and possesses F3 directly. While in direct
control, a return-to-command-vehicle prompt (**"A/Enter Confirm Return · B/V/Backspace
Cancel"**) is available — the exact trigger for opening that prompt was not pinned down
precisely during this pass (see §6); rehearse this step before a live demo.

**Note:** the scenario auto-advances on its own timers even without pressing `F6` (Hybrid
playback mode), so events may arrive slightly ahead of a slow narration — that is expected,
not a bug.

## 4. Recommended 3–5 minute demo sequence

1. `Tab` to open the console (defaults to MAP tab).
2. `F5` to start Mission D. The persistent status line shows **CMD-001: Mission start...**
3. `E`×5 to COMMANDER. Point out the scenario readout and the same CMD-001 text there.
4. Wait for or `F6` to **CMD-002: Assign F4...** — F4 is auto-selected by this event.
5. `Q`/`E` to the MAP tab. Point out F4's marker and the **Set Waypoint** control; place a
   waypoint if time allows (`Enter` to confirm placement). The persistent line shows
   **"F4 waypoint command accepted..."**
6. `F6` to **F4 task status report ready.**
7. `E` to REPORTS. Point out the **LIVE STATUS** section: F4 task status = report ready.
8. `F6` to advance toward **CMD-003 / F3 issue detected** — persistent line shows
   **"F3 status degraded. Operator attention requested."**
9. `F6` again toward the takeover-required milestone — Commander/Reports tab badges
   (a small `●` on the tab label) light up, and the Reports LIVE STATUS row shows
   **F3 recovery status — takeover required**.
10. Go to the F3 tab, `Up`/`Down` to **Take Over**, `Enter`, `Enter` again to confirm —
    you now have direct control of F3.
11. Return control to the command vehicle (see §3 note), then use Commander's
    **Force Recovery** action, or advance with `F6`, to reach F3 recovered.
12. `E` to REPORTS — F3 recovery status now shows **recovered**, and the REPORTS tab badge
    indicates a report is pending.
13. `Up`/`Down` + `Enter` on **Send Pending Report**. The persistent line confirms
    **"F4 report submitted."** / **"F3 report submitted."** (whichever is pending) or
    **"No pending reports."** if nothing is queued.
14. `F6` to the end of the schedule — persistent line shows **CMD-005: Mission checkpoint
    complete. Fleet status report ready for export.**
15. On REPORTS, select **Export Mission Log** — the EXPORT section shows the export status
    text (and file path, once the export completes).

## 5. Expected visible cues

- **Persistent mission status line** — directly under the tab bar, visible on every tab,
  shows the latest scripted or manual mission message (e.g. CMD-001..CMD-005, F3/F4 state
  changes, report submissions).
- **Commander tab readout** — scenario name/mode/current step/time/status, plus the
  CMD-00x guidance list and a small **"Demo controls: F5 Start • F6 Next • F7 Pause •
  F8 Reset"** hint under the scenario buttons.
- **Reports tab** — "MISSION REPORTS" panel with **LIVE STATUS** (F4/F3/final-fleet rows),
  **EXPORT** status, and **REPORT ACTIONS** buttons.
- **Commander/Reports tab badges** — a small `●` appears next to the tab label when F3
  requires takeover (Commander) or a report is pending (Reports).
- **MAP tab** — CV/F1-F4 markers, route/objective labels (ROUTE, POS RED, ISSUE, REJOIN), no
  command-feed overlay.
- **Minimap** (bottom-right) — heading-up arrow for the currently-possessed vehicle, plus
  live F1-F4 markers.

## 6. Troubleshooting

- If the main-menu → session-setup flow is unreliable, load
  `MAP_ESCROW_DESERT_INTEGRATION_TEST` directly and press Play — this is the path this
  runbook and most hardening passes have been tested against.
- If the viewport looks magenta/green, switch View Mode back to **Lit**.
- If **Send Pending Report** says "No pending reports.", nothing is currently ready —
  advance the scenario (`F6`) or trigger a report-ready state from Commander's action list
  first. The button now automatically checks the selected follower, then F4, then F3 before
  reporting nothing pending, so you no longer need to hunt for the exact follower with a
  report ready.
- If `F6` skips further than intended, reset with `F8` and step more slowly, or use `F7` to
  pause between steps.
- Occasional single-keypress unresponsiveness (an `Enter` or similar that doesn't seem to
  register on the first press) has been observed during scripted/automated testing of this
  build; a second press typically works. Don't treat one missed keypress as a broken demo.
- Do not treat minimap marker clustering (F1-F4 icons overlapping when followers are close
  together) as a mission failure — it is a known, unresolved cosmetic limitation.

## 7. Known limitations

- Reports/Commander status rows refresh when you (re-)enter that tab or take the action that
  changes them, not on a background timer — revisiting a tab (`Q` then `E`) forces a refresh
  if something looks stale.
- Minimap marker clustering when followers are close together is unresolved.
- A dedicated full tactical-map mode (distinct from the current single live-capture MAP page)
  is not implemented.
- The MAP tab's squish/checkerboard concern raised in earlier planning was investigated in
  RA32B-1 and did not reproduce — treat it as resolved/non-issue unless a fresh screenshot
  shows otherwise.
- No packaging/build pass has been done yet — this demo runs from the editor via PIE only.
