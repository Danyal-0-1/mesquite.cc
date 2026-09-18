# Evaluation Design

## Precondition — read this before designing any comparison

**§C2 must be resolved first.** Unsynchronised, time-dilated data cannot be scored against ground
truth, and the existing benchmark demonstrates the failure rather than avoiding it.

`[fact-data]` `Mesquite_benchmarks/outputs/bvh_verification/metrics_all.json`:

| | Session 01 | Session 02 |
|---|---|---|
| `latency_ms` | **−300.0** | **−2200.0** |
| `latency_correlation` | 0.593 | 0.608 |
| `yaw_alignment_degrees` | −71.19 | +99.16 |
| `pose_joint_rmse_mean` | 14.98° | 16.02° |
| `global_joint_rmse_mean` | 42.07° | 42.78° |

Two recordings of the same activity, minutes apart, cannot differ in latency by 1.9 seconds. At
correlations of ~0.6 the alignment step is not locking. The cause is `SYNC-07`: the export hardcodes
`frameTime = 1/30` (`js/bvh_converter.js:157`) over data sampled at ~32 Hz, so every file carries a
**~7% progressive time dilation** — and cross-correlation cannot find one consistent lag against a
signal that is being progressively stretched.

**Consequence: the existing captures cannot be repaired.** They carry no arrival timestamps, so the
true timing is unrecoverable. Any number derived from them — including the latency figures above —
should not be published. The error *pattern* is robust and can be relied on; the *magnitudes* cannot.

## A second, independent contaminant

`[fact-data]` the two skeletons differ substantially in proportion — pelvis bone length **55.53 vs
95.92**, spine_lower **9.99 vs 3.68** — reconciled by a single uniform
`pose_scale_applied_to_candidate` of **1.1023**, with only **14 of 22** joints mapped. Positional
error in BVH units is therefore part tracking error and part retargeting artifact.

**Both contaminants must be removed before the benchmark means anything quantitative.**

## Ground truth: note the substitution

The master prompt specifies OptiTrack. `[fact-data]` the available reference is **Rokoko**
(`Mesquite_benchmarks/data/rokoko/may8th2026/`), itself an inertial suit — so it shares the failure
modes under test, particularly heading drift. **A Rokoko reference cannot validate heading**, which
`FINDINGS.md` identifies as the dominant error. If OptiTrack is genuinely available (owner question
6), it should replace Rokoko for anything heading- or position-related.

## Required session structure

**1. Synchronisation event.** Begin every paired session with a sharp, unambiguous physical event
visible to both systems — a hand clap or a foot stamp produces a distinctive acceleration and
angular-velocity spike. This gives a hard temporal anchor independent of cross-correlation, which is
currently the only alignment mechanism and is demonstrably failing. Repeat at the end: the drift of
the second anchor relative to the first **directly measures the time-base error** and would have
caught `SYNC-07` immediately.

**2. T-pose, held and recorded.** `KIN-02` exonerates sensor-to-segment calibration based on code
reading, but `[unknown]` whether a T-pose is actually performed in normal operation. Hold it 5
seconds, record the computed offsets, and store them with the capture.

**3. Static hold, 30 s.** Subject stationary. Gives a per-session noise floor and, crucially,
separates `EST-01`'s two candidate mechanisms — a static segment shows whether heading error is drift
or a fixed initial offset.

**4. Structured motion, in blocks.** Rather than free movement, use separable blocks so error can be
attributed:
   - slow single-joint articulation (elbow, knee) — isolates per-limb accuracy
   - a full 360° turn in place — **directly probes heading**, the dominant error
   - straight-line walk, 10 m — probes root translation and the frame-alignment question in `PHN-04`
   - fast arm swings — probes the timing/jitter contribution (`SYNC-05`, which scales with ω)

**5. Static hold, 30 s, again.** Drift over the session is the difference between the two holds.

**6. Wall-clock recorded** at start and stop (instrumentation I12), plus room position and which limb
was moving — the three fields §13 identifies as making the tester's field log joinable.

## Scoring

Report each separately. Collapsing them into one number is what produced the current confusion.

| Metric | What it isolates | Notes |
|---|---|---|
| **Root-relative joint RMSE** (pose) | articulated accuracy | currently **15–16°**; the honest measure of the skeleton |
| **Global joint RMSE** | pose + frame error | currently **42–54°**; dominated by heading |
| **Heading error vs time** | `EST-01`, the dominant term | must be reported *separately*, not folded into global RMSE |
| **Root trajectory error** | SLAM + frame alignment | report in metres after per-bone normalisation, not raw BVH units |
| **Per-joint error by chain depth** | calibration vs frame error | distal-growing ⇒ chain error; flat ⇒ frame error. Currently flat (`KIN-02`) |
| **Packet loss per node** | data integrity | from `SYNC-03`/I5 — **currently unmeasured, and required to interpret everything above** |
| **Timing residual** | `SYNC-07` | drift between the opening and closing sync events |

**Normalise per bone length before quoting positional error**, rather than applying one global scale
(`KIN-06`).

## Analysis changes required in `compare_bvh_suits.py`

`[fact-code]` the script correctly honours each file's declared channel order
(`Mesquite_benchmarks/scripts/compare_bvh_suits.py:321-331`), so rotation convention is **not** a
source of error — that is worth stating, because it was a live suspicion.

Needed:
1. **Use the sync-event anchor** instead of, or as a prior for, cross-correlation. Report correlation
   quality and **refuse to report a latency below a threshold** (~0.85) rather than emitting the
   −2200 ms figure the current run produced.
2. **Per-bone length normalisation** in place of the single uniform scale.
3. **Per-frame yaw alignment as a diagnostic variant**, alongside the current whole-session alignment.
   The difference between the two *is* the heading-drift measurement, and it is `KIN-02`/`EST-02`'s
   falsifying test.
4. **Report mapped-joint coverage prominently** — 14 of 22 is a material caveat currently buried.

## Order of work

1. Fix the export (`SYNC-07`/I12: measured frame interval, arrival timestamps, wall-clock).
2. Add `SYNC-03`/I5 and `HUB-05`/I1 so loss is visible during the session.
3. Record one baseline session under the structure above, with full instrumentation.
4. Only then run paired comparisons and quote numbers.

**Steps 1 and 2 total roughly 40 lines and are prerequisites for every quantitative claim the project
wants to make.** Running more comparisons before them produces more numbers of the same
unreliability as the two already in hand.
