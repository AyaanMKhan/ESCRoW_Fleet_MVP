# RA33C Overnight Demo Hardening — Morning Triage

**Pass:** RA33C-OVERNIGHT-DEMO-HARDENING — complete.

## ⚠️ READ THIS FIRST — unexpected dirty files found at session start

Before any edits this pass, `git status --short` showed **6 unexpectedly dirty files**, none of
which this pass touched, saved, or reverted:

```
 M Content/DD_Vehicles_Basic/Blueprints/Vehicles/BP_BaseCar.uasset            (VENDOR)
 M Content/DD_Vehicles_Basic/Blueprints/Vehicles/BP_MilitaryCar_Base.uasset   (VENDOR)
 M Content/DD_Vehicles_Basic/Blueprints/Vehicles/BP_MilitaryCar_Extra_Roof.uasset (VENDOR)
 M Content/ESCROW/Blueprints/Core/BP_ESCROW_FeedCaptureManager.uasset
 M Content/ESCROW/Blueprints/Followers/BP_ESCROW_FollowerVehicle.uasset
 M Content/ESCROW/Maps/MAP_ESCROW_DESERT_INTEGRATION_TEST.umap               (MAP FILE)
```

All six were saved to disk at essentially the same timestamp (01:16:11–01:16:18 AM), all as real
binary content changes (confirmed via LFS pointer OID/size diffs — not save-noise), all **after**
the last commit (`0c99b9c`).

**What they appear to be:** `BP_ESCROW_FollowerVehicle`'s size grew ~21%. Inspection found
`bHiddenInGame` is now `true` on the shared vendor `Driver` component template
(`BP_BaseCar.Driver_GEN_VARIABLE`) — this looks like an attempt, made outside any RA pass (most
likely by hand in the editor, following the RA33B-1 recommendation), to hide the follower driver
mesh. Three problems found:
1. **It was set on the vendor's own shared component template, not a proper per-child override**
   — a vendor-asset edit, which this project's own rules and every RA pass have said not to do.
2. **It does not actually work** — confirmed via two independent fresh PIE restarts tonight, the
   driver's hands are still clearly visible in the F1 follower feed.
3. Because `BP_ESCROW_CommandVehicle` also inherits `Driver` from the same vendor `BP_BaseCar`,
   this change is live on the Command Vehicle's driver mesh too, not just followers (not
   confirmed to cause a *visible* problem there tonight, since the Command Vehicle's dashboard
   camera angle doesn't naturally show the driver mesh either way, but the property is live and
   shared regardless).

**This pass did not touch, save, or revert any of these 6 files.** They remain exactly as found
on disk. **Do not commit these 6 files as-is** — see recommendation at the bottom.

---

## 1. What changed overnight (this pass's own edits)

- `Docs/ESCRoW_Facilitator_Demo_Runbook.md` — updated the avatar/hands known-limitation entry to
  document tonight's finding above (with a pointer to this doc), and added a short RA33C note
  confirming what was re-verified and the stale-PIE-session lesson (see §2).
- `Docs/ESCRoW_Demo_Hardening_Checklist.md` — **not modified this pass** (nothing needed fixing
  there; RA33B-2's entry is still accurate).
- `Docs/ESCRoW_Morning_Triage_RA33C.md` — this new file.
- **No Blueprint, map, or vehicle assets were modified this pass.** Every subtask's baseline was
  already correct from RA33B-2; nothing needed a graph edit. `WBP_ESCROW_CommandHUD` was **not**
  touched or saved this pass.

## 2. What was verified in PIE tonight

All of the following were confirmed via live PIE screenshots in **freshly-restarted** PIE
sessions (see the stale-session note below):

- F1 opens; persistent mission line bounded; pre-start text reads "Mission not started. Press
  F5 or open Commander to Start Mission D."
- Report Record opens and closes cleanly on F1 (`Report Record — F1` / `Follow target: COMMAND`),
  no overlap with feed/hint-bar/buttons.
- F5 starts Mission D; CMD-001 appears on the persistent line while still on F1.
- F6 advances to CMD-002 (F4 tasking) — message appears while still on F1.
- F6 advances to CMD-003 (F3 issue) — message appears while still on F1.
- Report Record does not overwrite or suppress these mission messages (tested opening the panel
  while a mission message was showing; the mission line was unaffected).
- Set Follow Target → cancel produces a small muted "Follow target selection cancelled." line
  under the feed, not a large dominant banner — already acceptable, no change made.
- MAP tab: no feed overlay, CV/F1-F4 markers and route labels intact.
- Commander tab: full scenario readout (Current/Next/Time/Status), CMD-00x guidance list,
  action buttons all wrap cleanly with no overlap.
- Reports tab: `F4 task status = report ready`, `F3 recovery status = takeover required` rows
  visible; buttons fit their bounds.
- **Send Pending Report** (original path, F4 had a ready report): pressed, persistent line
  showed "F4 report sent."
- **Export Mission Log**: pressed, showed "Export complete: .../Saved/ESCROW/Logs/
  ESCROW_S001_events.csv" — confirmed the file actually exists on disk (5,652 bytes) plus a
  matching `.jsonl`.
- No `LogBlueprint` compile errors and no `Accessed None`/Blueprint runtime errors at any point
  tonight.
- **Follower feed avatar/hands**: confirmed still visible (driver's hands on the wheel, clearly
  rendered) in the F1 feed after a fresh PIE restart — the pre-existing out-of-band vendor edit
  described above does not fix this. Not re-attempted via any other method this pass (see below).

**Stale-PIE-session lesson:** early in this pass, `Enter` failed to register on Report Record for
several consecutive presses, and separately `Up` failed to move the action-list highlight for two
consecutive presses, in a PIE session that had (per the Output Log) been running continuously for
several hours before this pass started. A full Stop/Start of PIE resolved this immediately and it
did not recur for the rest of the night. If this comes up again, **restart PIE first**, before
concluding anything is broken.

## 3. What still needs human verification

- **F3 Take Over → direct control → Return-to-command-vehicle flow** was not re-tested this pass
  (not required by this pass's subtasks; the exact trigger for the return prompt was already
  flagged as unpinned-down in an earlier pass's runbook note). Rehearse this before a live demo.
- **Report fallback path** for Send Pending Report (when the *selected* follower has no report
  but F4 or F3 does) — verified structurally in RA33A-1 but not re-exercised live tonight; only
  the original "F4 already had a ready report" path was exercised tonight.
- **Clear Completed (Mock)** button — not pressed tonight; it's documented as a status-string-only
  mock action, not expected to change state.
- The vendor-asset/`bHiddenInGame` situation above — needs your decision (see recommendation).

## 4. What to test first when you return

1. Look at the 6 unexpectedly-dirty files above and decide whether to keep, discard, or manually
   redo the driver-hiding fix (exact steps are in the runbook's Known Limitations section).
2. Do a live human-keyboard pass of Report Record on F2/F3/F4 (only F1 was exercised with full
   screenshots tonight, though F2-F4 were all verified working in the prior RA33B-2 pass on the
   same code, which was not touched tonight).
3. Rehearse the F3 Take Over → Return flow once, since it's the one part of the demo not
   re-checked tonight.

## 5. Exact git status at end of this pass

```
 M Content/DD_Vehicles_Basic/Blueprints/Vehicles/BP_BaseCar.uasset
 M Content/DD_Vehicles_Basic/Blueprints/Vehicles/BP_MilitaryCar_Base.uasset
 M Content/DD_Vehicles_Basic/Blueprints/Vehicles/BP_MilitaryCar_Extra_Roof.uasset
 M Content/ESCROW/Blueprints/Core/BP_ESCROW_FeedCaptureManager.uasset
 M Content/ESCROW/Blueprints/Followers/BP_ESCROW_FollowerVehicle.uasset
 M Content/ESCROW/Maps/MAP_ESCROW_DESERT_INTEGRATION_TEST.umap
 M Docs/ESCRoW_Facilitator_Demo_Runbook.md
?? Docs/ESCRoW_Morning_Triage_RA33C.md
```

## 6. Exact assets/files modified by this pass

- `Docs/ESCRoW_Facilitator_Demo_Runbook.md` (edited, intentional)
- `Docs/ESCRoW_Morning_Triage_RA33C.md` (new, intentional)

Nothing else. No `.uasset`, `.umap`, or Blueprint graph was touched this pass.

## 7. Files intentionally not committed

Per this pass's explicit instructions, **nothing was committed**. Both files in §6 are ready to
commit whenever you choose.

## 8. Assets that became dirty unexpectedly, and whether reverted

See the "READ THIS FIRST" section at the top. The 6 files were **already dirty before this pass
started** — this pass did not cause them and did not revert them, per the instruction not to
touch unrelated dirty assets without direction.

## 9. Do-not-commit warning

**Do not commit the 6 files listed at the top as-is.** They contain a real, saved edit to three
vendor `.uasset` files (`BP_BaseCar`, `BP_MilitaryCar_Base`, `BP_MilitaryCar_Extra_Roof`) and to
the desert `.umap`, none of which any pass has been authorized to change, and the specific change
present (`bHiddenInGame=true` on the vendor `Driver` component) does not achieve its apparent
goal. Recommended options, for you to choose (not done automatically):
- **Discard** the 6 files back to the last commit (`git checkout -- <paths>`) if you don't want
  to keep whatever else may be bundled into that map/vehicle save, then redo the driver-hiding
  fix using the exact manual steps in the runbook (Class Defaults override + "Hidden in Scene
  Capture", not "Hidden in Game" on the vendor asset).
- **Keep** them if you know they contain other intentional changes you want (e.g. if you made
  other edits to the map or FeedCaptureManager in that same editor session) — but then also
  manually fix the vendor `Driver` component's hidden state back to its shipped default before
  committing, and redo the hiding fix as a proper child-scoped override on
  `BP_ESCROW_FollowerVehicle` only.
Either way, don't commit the vendor `.uasset` files with an intentional property change baked in
— re-verify their diff is empty (or revert them specifically) before committing anything else in
this batch.
