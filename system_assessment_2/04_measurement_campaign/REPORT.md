# B4 — Measurement Campaign — Phase 2

## 1. Scope
**No sessions were run.** No hardware is attached to this machine (`ls /dev/cu.*` shows only
`Bluetooth-Incoming-Port` and `debug-console`). This report delivers the campaign protocol and the
per-session checklist so M1–M11 can run the moment a fleet is available.

## 2. Summary
The W1 gate is closed — instrument self-cost is unmeasured (Rule 2) and no single-node pilot has
run — so the campaign could not begin even with hardware present. Everything M1–M11 needs is
specified below and every instrument it reads is implemented. The two highest-value sessions are
**M7** (10 cold boots) and **M2** (phone on vs off), because Phase 2's own fixture work narrowed the
S3/S4 hypothesis space to two candidates and M7 tests the stronger one directly.

## 3. Entry conditions not yet met

| Condition | State |
|---|---|
| W1 gate open | **No** — Rule 2 unsatisfied; no pilot run |
| Instrumented firmware compiles | **Unknown** — no toolchain here |
| Git tags for revert | **No** — git unusable (`../ROLLOUT.md`) |
| Corrected export in place for M1 | **Partly** — I12 landed, but `window.mesqFrameTiming` is not yet populated by the recording path |

**M1 must not be recorded until that last row is closed**, or the frozen baseline inherits the exact
contamination that ruined the existing captures. The export now labels itself
`ASSUMED_1_30_UNTRUSTWORTHY` when unwired, so the failure is at least self-announcing.

## 4. Reprioritised session order

Phase 2 listed M1 first. Two findings from this session argue for reordering:

1. **M7 (10 cold boots) should run first.** `[fact-fixture]` F1 removed `WEB-02` as an S3
   explanation, leaving `NODE-04` (init hang) and `HUB-03` (fleet reboot). M7 tests `NODE-04`
   directly, needs no full suit, and takes under an hour. If pods reliably fail init, **every
   subsequent session is contaminated** by a varying fleet size — including the baseline.
2. **M3 (U1, the PHY rate) should precede the baseline.** It determines whether the radio is a
   bottleneck at all, and if forcing 6 Mbps changes throughput materially, the baseline should be
   recorded in the configuration the project intends to keep.

Suggested order: **M7 → M3 → M1 (freeze) → M2 → M4 → M10/M11 → M5 → M6 → M8 → M9.**

## 5. Session protocol (every session)

1. Preflight: all 17 expected ids present in `window._podRxByteId`; `build/pods/MANIFEST.txt` shows
   17 distinct SHA-256s; note the firmware tag.
2. Sync event — sharp clap or foot stamp — **at start and at end**. The drift between them measures
   the time base directly and would have caught `SYNC-07` immediately.
3. T-pose held 5 s; log the computed mounting offsets.
4. Static hold 30 s.
5. Structured motion blocks: single-joint articulation → 360° turn in place → **straight-line walk
   10 m** (this is `PHN-04`'s test) → fast arm swings.
6. Static hold 30 s.
7. Sync event.
8. Record: wall-clock start/stop, room position, which limb was moving, and the **raw serial byte
   stream** to `data/replay/` for later fault-injection replay.

Never change code between the two halves of a comparison pair. **Never discard a session because it
looks bad** — a session where the fleet halved is the most valuable data the campaign can produce.

## 6. What each session resolves

| # | Session | Resolves | Notes |
|---|---|---|---|
| M7 | 10 cold boots, count init successes (I8) | `NODE-04`, `INT-03` | **Run first.** Now the leading S3 candidate. |
| M3 | U1 — sniffer on ch 1, or force 6 Mbps and re-measure I1 | `NET-01` + the whole bottleneck verdict | Swings utilisation 12%↔57% |
| M1 | Baseline, full suit, **10 min**, all instrumentation, corrected export | everything | Freeze it. Blocked on the I12 wiring. |
| M2 | Phone on vs off | `HUB-02`, `PHN-02`, `C-05` | `[fact-fixture]` F2 predicts a measurable delivered-fraction drop with the phone active |
| M4 | U3 / E11 — one node, static, 10 min | `EST-01` | Phase 1 arithmetic predicts initialisation, not drift |
| M10 | Node sweep 1/4/8/12/15/17 | S1, `NET-01` | The knee has never been measured |
| M11 | Rate sweep `fps` 20/25/32/40/55 | S5, `SENS-01` | With M10 gives a 2-D envelope |
| M5 | Bench control, 15 on a table | C7 vs `NET-01` | |
| M6 | 2 pm vs 10 pm + channel survey | `NET-06`, S4 | |
| M8 | 15-node static, co-planar | `EST-04` | |
| M9 | Display on vs off | `PWR-02` | |

## 7. Measurements I need
Hardware. Specifically: one pod and one dongle to close the W1 gate (instrument cost + pilot), then
the full fleet for M7 onward.
