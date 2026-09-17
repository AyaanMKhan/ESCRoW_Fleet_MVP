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
7. Before pressing `F5` or `8`, the persistent status line reads **"Press 8 for report demo,
   or open COMMANDER for Mission D controls."** — this is expected and is not a stuck/blank
   state; it is only shown when no scripted messages have arrived yet. (RA35A-1 changed this
   line's default text to point at the new report-deck path below; `F5`/Mission D still works
   exactly as before — see §4 — but is no longer the primary thing a facilitator is told to try
   first.)

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
| `8` | Deliver next report (toast + REPORTS panel update) — see §8 |

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

**Follower pages (F1-F4):** the first action button, labeled **Report Record** (renamed
from "Request Status"), is a toggle: `Enter` opens a small persistent panel (in the area
previously showing "LOCATION — grid placeholder") showing two lines — `Report Record — F<N>`
and `Follow target: <COMMAND or F#>`; `Enter` again closes it. This was verified visually in a
live PIE session on all four follower pages (RA33B-2). The panel intentionally shows only the
follow-target field: Callsign, State, Autonomy, Objective, Report-ready, and Takeover-required
are already visible at all times in the header directly above the live feed, so repeating them
in the panel would be redundant and (when tried during RA33B-2) overflowed into the bottom
hint-bar row. Report Record never writes into the persistent mission status line — it only
shows/hides its own panel, so it cannot cover or replace a mission/commander message that's
currently displayed there. The mission status line under the tab bar is visible on every
follower page too, so commander/scenario messages stay visible while monitoring a follower —
look for it directly under the F1-F4 tab bar, in the same position as on MAP/COMMANDER/REPORTS.
**Set Follow Target** feedback (e.g. "Follow target selection cancelled.") renders as a small
muted line under the map/report area instead of a large bold-yellow line — this is a
presentation-only change; the underlying follow-target logic is unchanged.

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
  build; a second press typically works. Don't treat one missed keypress as a broken demo. This
  was observed again during RA33B-2's automated PIE testing (intermittently on F3 specifically,
  resolved on retry) while `Up`/`Down`/`Q`/`E` navigation worked reliably — this appears to be
  specific to automated/scripted input rather than a live keyboard+mouse session. If `Enter`
  seems unresponsive on a real machine, try clicking once inside the game viewport first, then
  press `Enter` again.
- The F1-F4 live feed still shows the driver's arms/hands/steering-wheel-view from the vendor
  vehicle's occupant mesh — a from-camera view of the vehicle interior, not a clean
  exterior/road-only feed. This is a known, unresolved limitation (see §7).
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
- The F1-F4 follower feed still shows the vendor vehicle's driver/occupant mesh (arms, hands,
  steering wheel) rather than a clean exterior-only view — confirmed unresolved as of RA33C via
  two fresh live PIE checks. The mesh (`Driver`, defined on the vendor Blueprint
  `/Game/DD_Vehicles_Basic/Blueprints/Vehicles/BP_BaseCar`, inherited into
  `BP_ESCROW_FollowerVehicle`) could not be hidden from the feed capture through the available
  MCP tooling without risking an edit to the shared vendor asset (see RA33B-1 final report for
  detail). **RA33C found that `BP_BaseCar`'s own `Driver` component now has `bHiddenInGame=true`
  set directly on the vendor asset** (not a proper per-child override — done outside any RA pass,
  most likely by hand in the editor) — this does not work (hands are still visible after a fresh
  PIE restart) and, because `BP_ESCROW_CommandVehicle` inherits from the same vendor chain, it
  is a live change sitting on the Command Vehicle's driver mesh too. See
  `Docs/ESCRoW_Morning_Triage_RA33C.md` for full detail; this should be reviewed/reverted before
  committing. **The correct fix remains a manual one**: open `BP_ESCROW_FollowerVehicle` in the
  editor, select the inherited `Driver` component in the Class Defaults / Components panel (this
  creates a proper per-Blueprint override, distinct from editing the vendor asset), and set
  **Hidden in Scene Capture** to true — this hides it from the feed only, without touching the
  vendor asset or the Command Vehicle.
- Report Record's persistent panel is confirmed working, visually, on all four follower pages
  (RA33B-2, live PIE screenshots). Note for future passes: the RA33B-1 pass had wired this
  toggle onto a disconnected/dead branch of `Escrow_FollowerAction` — pressing the button never
  actually reached that code, and the real button handler kept running the old giant-diagnostic
  message. RA33B-2 traced the real live execution chain from `FunctionEntry` (not just
  `read_graph_dsl`, which is not reliable for reachability — it will print disconnected/orphaned
  node chains inline as if they were live), removed the dead branch and its dead-end message
  construction (~65 nodes total across both), and re-implemented the toggle directly on the
  live `ActionId==0` path. If Report Record is extended again, verify the edited branch is
  actually reachable from `FunctionEntry` by tracing pin connections rather than trusting a
  DSL read alone.
- **RA33C overnight pass (stabilization only, no Blueprint edits needed)**: re-verified in a
  fresh live PIE session that Report Record still opens/closes correctly on F1 with bounded
  text, that the persistent mission line still shows CMD-001 and later scenario messages while
  staying on a follower page (no separate "mission feed" label was added — the existing
  behavior already satisfies this without further changes), that MAP/Reports/minimap are still
  clean, and that Set Follow Target's cancel feedback is already small/muted as documented
  above. One occurrence of `Enter` not registering for several consecutive presses was traced
  to a *stale* PIE session that had been running for hours before this pass started — a fresh
  Stop/Start of PIE resolved it immediately. If `Enter` seems unresponsive during a demo
  rehearsal, restarting PIE (not just retrying the key) is the more reliable fix.

## 8. Facilitator Report-Deck Demo Path (RA35A-1)

This is a smaller, more reliable alternative (or a lead-in) to the scripted `F5`/`F6` Mission D
flow in §4 — a facilitator-driven sequence of four fixed mission-brief reports, triggered
entirely by one key, that does not depend on the `ScenarioDirector`'s timers or on any follower
being in a particular state.

1. From any HUD state (Monitoring or Command Console, any tab), press **`8`**. A dark
   translucent **"NEW REPORT"** toast appears in the upper-right corner: **"Mission brief
   received. Open REPORTS."** This toast is independent of Monitoring vs. Console mode — it
   stays visible across both, and does not cover the minimap (bottom-right) or the Monitoring
   stack (top-left).
2. Open the Command Console (`Tab`) and switch to the **REPORTS** tab (`Q`/`E`). A new
   **MISSION REPORTS** panel above the existing LIVE STATUS/EXPORT sections shows the current
   report's title, priority, source, report index ("Report 1 of 4", etc.), body, tasking, and
   status, styled in charcoal/gold to match the console's existing look.
3. Three buttons under the report text — **`< Previous`**, **Confirm Report**, **`Next >`** —
   step backward/forward through the fixed 4-report deck, or acknowledge the current one:
   - **Confirm Report** shows "Report confirmed." and advances to the next report (or "Report
     confirmed. No newer reports." if already on Report 4 of 4).
   - **Next** shows "Next report loaded." or "No newer reports." at the last report.
   - **Previous** shows "Previous report loaded." or "No previous reports." at the first report.
4. Press **`8`** again to deliver the next report in the deck and re-show the toast. The deck
   always holds exactly these four reports, in order: (1) Initial Mission Brief, (2) Commander
   Tasking: F2 Position (tasks the facilitator to select F2 and Set Waypoint on the MAP tab),
   (3) Fleet Coordination Update (tasks setting F3 to follow F1), (4) Recovery Demonstration
   (tasks Take Over F3).
5. The original **Send Pending Report** / **Clear Completed (Mock)** / **Export Mission Log**
   buttons in the REPORT ACTIONS box are unchanged and still work exactly as in §4 — the new
   Previous/Confirm/Next row is a separate control group, not a replacement.

**Known gaps in this path (by design, not oversight):** the four reports are fixed text, not
tied to actual follower/vehicle state — confirming a report does not itself move F2/F3/F1 or
change any Fleet subsystem state; a facilitator narrating the demo still performs the
MAP/FOLLOWER actions the report describes. The REPORTS tab's existing `●` badge (which lights up
when F3 needs takeover or a real report is ready) is not wired to this new deck — it continues
to reflect only the original Fleet-state-driven condition. No event-log entry format beyond the
existing `ESCROWEventLogSubsystem` calls (`DemoReportReceived` on `8`, `DemoReportConfirmed` on
Confirm) was added.
