# Mesquite Phase 2 — Instrument, Measure, Fix

**Status: W0 and W1 partially delivered as code. No gate is open. No measurements were taken on
hardware.**

## Read first

1. **[MEASUREMENTS.md](MEASUREMENTS.md)** — what was actually established, and the tag correction
   that keeps `[fact-measured]` honest
2. **[12_integration/PHASE1_SCORECARD.md](12_integration/PHASE1_SCORECARD.md)** — were Phase 1's
   inferences right? (3 right, 2 understated, 1 partly wrong, 1 wrong)
3. **[LEDGER.md](LEDGER.md)** — every finding ID with status
4. **[12_integration/GATES.md](12_integration/GATES.md)** — why no gate opened
5. **[ROLLOUT.md](ROLLOUT.md)** — **read before flashing anything**
6. **[RETRACTIONS.md](RETRACTIONS.md)** — which published claims are unsound

## Two blockers, both outside my reach

**Git is unusable.** Every `git` command returns "You have not agreed to the Xcode license
agreements." §4.1's whole discipline — branch per wave, tag before every flash, documented revert —
cannot run. Fix: `sudo xcodebuild -license` (needs sudo). **Until then, do not flash a fleet: there
is no tag to return to.**

**No hardware is attached.** No `usbmodem`/`usbserial` device exists. That makes W2 unrunnable, and
Rule 1 gates W3–W6 behind W2. **No W3–W6 fix was written** — that is the gate working, not a
shortfall.

## What was delivered

| Wave | Item | State |
|---|---|---|
| W0 | Per-node build system (`NODE-05`, §4.2 blocker) | **Done** — `#error` if `MESQ_POD_ID` undefined; `tools/build_pods.sh` builds 17 with SHA-256 manifest and a duplicate-image check. Not executed (no `arduino-cli`). |
| W0 | Provenance banners resolving **U2** | Code written, **never run** |
| W0 | Replay fixture (§4.5) | **Done and productive** — `tools/replay_harness.js`, 23/23 passing |
| W1 | Node instruments I4, I8, I9, I11, N1, N2, N3 | Code written, never run |
| W1 | Hub instruments I1, I2, I3, I7, I11, H1, H2, H3 | Code written, never run |
| W1 | Browser I5, I6, W1b | **Implemented and fixture-verified** |
| W1 | I12 export fix | **Landed early** as instructed; needs one wiring change (see `OPEN_QUESTIONS.md` N1) |
| — | Offline analyses on existing captures | **Run** — five results, four Phase 1 findings moved |

## Six results from existing data and the fixture

1. **`WEB-02` does not latch permanently.** It self-clears in ~16 packets (~34 ms across a fleet),
   because `0x0A` occurs freely in binary payloads. Severity **BLOCKER → CORRECTNESS**, and **it can
   no longer explain S3**. `INT-03`'s "three latching failures" becomes two.
2. **`PHN-03` is refuted.** No relocalization signature in ~7,100 frames. The only large jumps are
   the *reference's* frame-0 rest pose, already trimmed by the comparison.
3. **`PHN-05` is confirmed and quantified.** Up to **23.9%** of frames sit within 10° of the gimbal
   singularity — because `XYZ` puts yaw in the middle Euler slot. Rokoko's `YXZ` scores **0.0%**.
4. **`HUB-02` is confirmed and understated.** One interleave destroys *both* the pod packet and the
   JSON line.
5. **`KIN-01` is feasible.** Stance phases are detectable in every capture, with runs up to **3.3 s**
   — so ZUPT, the one accuracy fix needing no magnetometer and no new hardware, is available.
6. **The benchmark maps no arm joints** — 14 of 22, all torso and legs. It is silent about the
   fastest-moving half of the body, which is also the half most exposed to timing jitter.

And one important negative: **U3 cannot be resolved offline.** A ±1 s change in assumed time
alignment swings the per-frame yaw estimate across **1,176°** and flips its sign. `INT-04`
contaminates new analyses, not just the old metrics — so **M4 (one node, static, 10 min) is not just
the specified instrument, it is the only one that works.**

The pattern, recorded in the scorecard: **Phase 1 was reliable about mechanism and unreliable about
magnitude and frequency**, and both errors ran toward overstating severity.

## Reproduce

```
node tools/replay_harness.js          # 23 assertions, incl. fault injection
python3 tools/offline_analysis.py     # gimbal + root-displacement analysis
tools/build_pods.sh 1                 # needs arduino-cli
```

## Next, in order

1. `sudo xcodebuild -license`, confirm `git status`
2. Compile both sketches, `MESQ_INSTR` unset **and** set
3. Flash one pod, read the banner → **resolves U2**
4. Measure the 1 Hz print's own cost → **satisfies Rule 2**, opens W1
5. Wire `window.mesqFrameTiming` → unblocks a trustworthy M1
6. **M7 (10 cold boots) before M1** — see `12_integration/REPORT.md` R-01

## Agent reports

[B0 provenance](00_provenance/REPORT.md) · [B1 node](01_node_instrumentation/REPORT.md) ·
[B2 hub](02_hub_instrumentation/REPORT.md) · [B3 browser](03_browser_instrumentation/REPORT.md) ·
[B4 campaign](04_measurement_campaign/REPORT.md) · [B9 phone/SLAM](09_phone_slam/REPORT.md) ·
[B12 integration](12_integration/REPORT.md)

[B7 estimation](07_estimation_heading/REPORT.md) · [B8 kinematics](08_kinematics_constraints/REPORT.md) ·
[B10 evaluation](10_evaluation/REPORT.md)

B5 (reliability) and B6 (protocol v2) produced no report — both are gated behind W2 measurements by
Rule 1, and neither has ungated analysis work. B11's retraction audit, which needs no paper, is
delivered as [RETRACTIONS.md](RETRACTIONS.md).
