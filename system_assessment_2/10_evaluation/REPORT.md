# B10 — Evaluation & Ground Truth — Phase 2

## 1. Scope
Infrastructure work, explicitly ungated. Built `tools/bvh_kin.py` (BVH parser + FK) and
`tools/analysis_yaw.py`; ran the per-frame yaw diagnostic and a lag-sensitivity sweep.
`compare_bvh_suits.py` **not modified** — see §4.

## 2. Summary
The single most important evaluation result this session is negative: **a new analysis built to work
around `INT-04` was itself destroyed by it.** A ±1 second change in assumed time alignment swings the
per-frame yaw estimate across 1,176° and flips its sign, which means no analysis of the existing
captures can be trusted on anything time-dependent. Separately, the existing benchmark maps **no arm
joints at all** — 14 of 22, all torso and legs — so it is silent about the fastest-moving half of the
body. Both findings sharpen what the rebuilt comparison must do, and both argue that re-capture, not
re-analysis, is the critical path.

## 3. Findings

### P2-B10-01 — `INT-04` contaminates new analyses, not just the old metrics
- **Severity:** BLOCKER (for evaluation) · **Confidence:** high
- **Evidence:** `[fact-data]` `../MEASUREMENTS.md` D4. Lag sweep on session 01: mean optimal yaw runs
  +463.29° (lag −30) → −713.03° (lag −20) → −129.10° (lag −9) → −120.34° (lag 0); slope runs +8.06 to
  −10.82 °/s and changes sign.
- **Mechanism:** Two bodies rotating relative to each other produce an apparent yaw disagreement that
  varies with turning rate whenever the streams are misaligned in time. Since `SYNC-07`'s ~7%
  progressive dilation means alignment is not merely offset but *drifting*, no single lag is correct
  for the whole session, and any per-frame quantity inherits the error.
- **Falsifying measurement:** re-capture one paired session with I12 (measured frame interval) plus
  clap sync events at both ends, then repeat the sweep. Stability across lag would show the
  contamination was removed.
- **Proposed change:** **no further quantitative analysis of the existing captures.** Re-capture
  first. This is a scheduling decision, not a code change.
- **Cost:** one paired session. **Risk:** none — the alternative is producing more numbers of the
  same unreliability.

### P2-B10-02 — The benchmark maps no arm joints
- **Severity:** CORRECTNESS (methodology) · **Confidence:** high
- **Evidence:** `[fact-data]` D5. All 14 mapped joints are pelvis, spine ×3, neck, head, and 8 leg
  joints. No shoulder, arm, forearm or hand appears in either session's `per_joint_errors.csv`,
  though both skeletons define them.
- **Explains symptoms:** none — it **hides** them.
- **Mechanism:** Arms carry the fastest and largest-amplitude motion in most sessions, and angular
  velocity is exactly the multiplier in `SYNC-05`'s timing-error term (error = ω·Δt). The segments
  most exposed to the system's known timing problem are the ones excluded from measurement. Any
  accuracy claim derived from this benchmark is a claim about the torso and legs only.
- **Falsifying measurement:** extend the mapping to the arm chain and recompute. If arm RMSE matches
  the leg figures, the exclusion was harmless; if it is markedly worse, every published accuracy
  number is optimistic.
- **Proposed change:** extend the joint mapping in `compare_bvh_suits.py` and report coverage
  prominently. **Do this before the next comparison run.**
- **Cost:** ~20 lines plus a naming reconciliation between the two skeletons.
- **Risk:** results may get worse, which is the point.

### P2-B10-03 — Per-frame yaw removes only 24–32% of root-relative error
- **Severity:** QUALITY (a check on Phase 1's central claim) · **Confidence:** medium
- **Evidence:** `[fact-data]` D4 — error reduction is 24%, 25%, 28%, 32% across all four distinct
  alignments tested, i.e. **stable even though everything else in the analysis is not.**
- **Mechanism:** `EST-02` holds that whole-body heading dominates. If so, a *per-frame optimal* yaw —
  the most generous possible heading correction, free to differ every frame — should remove most of
  the root-relative residual. It removes about a quarter to a third.
- **Why this is not yet a refutation:** the metric here is positional RMS in BVH units after
  root-centring and one uniform scale, so it still carries `KIN-06`'s retargeting artifact, and it is
  not the joint-angle RMSE Phase 1 quoted. The two numbers are not directly comparable.
- **Falsifying measurement:** recompute in joint-angle terms, with per-bone length normalisation, on
  an I12-corrected capture. That is the clean version of `EST-02`'s falsifying test.
- **Proposed change:** none. Flagged so `EST-02` is not treated as settled.
- **Cost:** part of the comparison rebuild. **Risk:** none.

## 4. Why `compare_bvh_suits.py` was not modified
§B10 lists four required changes. Three of them — the correlation threshold, per-bone normalisation,
the per-frame yaw variant — only matter once there is a capture worth comparing, and P2-B10-01 shows
the existing ones are not. Rewriting 1,522 lines to analyse contaminated data more carefully would
be effort spent in the wrong place, and it would risk breaking the one thing the script already does
right: `[fact-code]` `:321-331` honours each file's declared channel order, so rotation convention is
**not** a source of error. That behaviour must be preserved in any rewrite.

**The one change worth making now** is P2-B10-02's arm mapping, because it is independent of time
alignment and it changes what future sessions measure.

## 5. Ground truth — the reference problem is unchanged
§1.5 stands: `Mesquite_benchmarks/data/rokoko/` is a Rokoko inertial suit, which shares the failure
mode under test and cannot validate heading. D5 adds that it does not measure the arms either.
**Establishing an OptiTrack pairing is still the critical path for publication**, and owner question
6 is still unanswered.

## 6. Measurements I need
One OptiTrack + Mesquite paired session, recorded under the W2 protocol with clap sync events and an
I12-corrected export. Until that exists, evaluation work is blocked on data, not on code.
