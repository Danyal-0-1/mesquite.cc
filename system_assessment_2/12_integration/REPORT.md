# B12 — Integration Referee & Release Manager — Phase 2

## 1. Scope
Owns `../LEDGER.md`, `../MEASUREMENTS.md`, `../ROLLOUT.md`, `../OPEN_QUESTIONS.md`, `GATES.md`,
`PHASE1_SCORECARD.md`. Verified every load-bearing citation rather than inheriting it.

## 2. Summary
No wave gate is open, so no W3–W6 fix was written — which is Rule 1 working, not a shortfall. The
session's substantive output is W0/W1 tooling plus four evidence-backed corrections to Phase 1, the
most consequential being that **`WEB-02` does not latch permanently** and therefore can no longer
explain S3. Phase 1's scorecard on testable findings is three right, two right-but-understated, one
partly wrong, one wrong — and both errors ran toward overstating severity. Two hard external
blockers dominate everything else: git is unusable, and no hardware is attached.

## 3. Rulings

### R-01 — `WEB-02` loses its S3 attribution
`INT-03` claimed three latching failures explain S3/S4. `[fact-fixture]` F1 shows `WEB-02` self-clears
in ~34 ms across a fleet. **Two remain: `NODE-04` and `HUB-03`.** Both are permanent-until-intervention
and both are probabilistic, so the narrowed account still fits S3 *and* S4. Consequence: **M7 (10 cold
boots) is promoted to the first session of the campaign**, since it tests the stronger survivor
directly and needs no full suit.

### R-02 — `PHN-03` refuted; the reference, not the system, held the anomaly
`[fact-data]` D2. Phase 1 read a summary statistic (`root_error_max` 109.60) as evidence of
relocalization jumps. The underlying frames are smooth; the large jumps are Rokoko's frame-0 rest
pose, already trimmed at `trim_start_frames: 5`. **Ruling: do not spend W5 effort on relocalization
handling.** Re-test only under deliberately tracking-hostile conditions.

### R-03 — Tag discipline: `[fact-measured]` must stay honest
Phase 2 defines `[fact-measured]` as requiring real hardware. No hardware was available, so that
section of `../MEASUREMENTS.md` is **empty by necessity**. A new tag `[fact-fixture]` was introduced
for results obtained by executing the real algorithm on synthesised input — stronger than
`[inference]`, weaker than `[fact-measured]`. **Ruling: do not let fixture results be quoted as
measurements.** They establish that a mechanism *can* behave a certain way, not that it *does* in the
field.

### R-04 — I10 was redesigned rather than implemented as written
§4.4 warned that per-second AXP battery logging would triple I²C traffic on the sample task.
Implemented instead as H3 at the hub, decoding the `batt` byte already on the wire. **Zero pod cost.**
**Ruling: accepted, with the stated limitation** — hub-side `batt` is integer percent, so sub-percent
sag is invisible and the C6 brownout theory is only partly testable. If M9/M10 suggest brownout, a
dedicated voltage read on a **separate task** is required, with its cost measured first.

### R-05 — Rule 2 is unsatisfied and W1 cannot open
`mesq_instrCostUs` is emitted but never read, and the 1 Hz `Serial.printf` is not accounted at all.
At 115200 baud a ~200-char line is ~17 ms of transmission on core 0, against a 31 ms send interval —
potentially a large perturbation of the very task `I4` measures. **Ruling: W1 stays closed until the
print's own duration is measured** (`P2-B1-02`, ~5 lines).

## 4. Regression watch
No previously-measured quantity got worse, because none existed. Establishing that baseline is M1's
job. **One regression risk is already live:** the hub's pre-existing raw-text `Serial.printf` calls in
the WebSocket connect/disconnect handlers (`Dongle_Binary.ino:240,244`) remain unchanged and will
inject text into the binary stream during the first instrumented sessions. Expect that artifact in
the data; it is `HUB-02`'s fix and belongs to W4.

## 5. Contradictions
None between agents this session — most agents did not run. The one tension is between §B4's
session ordering (M1 first) and R-01's argument for M7 first; resolved in favour of M7, recorded in
`../04_measurement_campaign/REPORT.md` §4.

## 6. What I could not determine
`[unknown]` U1, U2, U3 — all three blocking unknowns remain open.
`[unknown]` Whether the firmware compiles.
`[unknown]` Whether deployed firmware matches this workspace (`INT-01`).
