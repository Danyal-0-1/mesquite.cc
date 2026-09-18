# RETRACTIONS — claims that must be withdrawn or recomputed

B11's audit, runnable now because it needs the existing data, not the paper.

## Classification

| Claim | Verdict | Reason |
|---|---|---|
| `latency_ms` = −300.0 (s01), −2200.0 (s02) | **UNSOUND — withdraw** | Two recordings of the same activity, minutes apart, cannot differ in latency by 1.9 s. At correlations of 0.593 / 0.608 the correlator is not locking. Cause: the ~7% progressive time dilation from `frameTime = 1/30` over ~32 Hz data (`SYNC-07`). |
| `latency_correlation` = 0.593, 0.608 | **UNSOUND as a quality claim** | These are *evidence the alignment failed*, not a result. Quote only as diagnostic. |
| Absolute positional error in BVH units (`root_error_mean` 38.41, `root_error_max` 109.60) | **NEEDS RECOMPUTATION** | `KIN-06`: pelvis bone length 55.53 vs 95.92, reconciled by one uniform scale of 1.1023, only 14 of 22 joints mapped. Part tracking error, part retargeting artifact. Not convertible to centimetres as they stand. |
| `yaw_alignment_degrees` = −71.19, +99.16 | **SAFE as a *pattern* claim; unsound as a *magnitude*** | The session-to-session sign change is robust and is the strongest single piece of evidence for unreferenced heading. The exact degrees depend on the same contaminated alignment. |
| `global_joint_rmse_mean` ≈ 42° vs `pose_joint_rmse_mean` ≈ 15° | **SAFE as a ratio; recompute the absolutes** | The ~3× gap between global and root-relative error is the central accuracy finding and survives both contaminants — it is a within-file comparison. |
| "Error is flat across joints including the root" | **SAFE** | D1-adjacent; a within-file pattern, independent of time base and scale. |
| Any claim that the reference is *ground truth* | **NEEDS QUALIFICATION** | §1.5: Rokoko is itself inertial and shares the failure mode under test. It **cannot validate heading**, which is the dominant error. |
| Any frame-rate claim of "30 Hz" | **UNSOUND** | `frameTime` was a hardcoded constant, never a measurement. Firmware requests 32 Hz; DMP produces ~55 Hz; export asserted 30 Hz. |
| "Relocalization jumps propagate into the root" | **WITHDRAW if asserted** | D2 refutes it for existing captures. |

## What is safe to say today

- Root-relative pose RMSE is roughly **3× better** than global joint RMSE, consistently across both
  sessions. **[fact-data]**
- Error magnitude is **near-uniform across the skeleton including the root**, which is the signature
  of a whole-body frame error rather than per-limb estimation error. **[fact-data]**
- A large whole-body **heading offset** is present and **changes sign between sessions**. **[fact-data]**
- Mesquite's root trajectory contains **no discontinuities**; frame-to-frame motion is physically
  plausible throughout. **[fact-data]** — D2, and this is a *positive* result worth stating.

## What cannot be said until re-capture

Anything with units, anything with a latency, and anything positioned as validated against ground
truth. **Existing captures cannot be repaired** — they carry no arrival timestamps, so the true
frame interval is unrecoverable. I12 fixes this prospectively only.

## Recommended positioning

The defensible headline is narrower and stronger than an unqualified accuracy number:

> Root-relative pose RMSE of ~15° against a commercial inertial suit, with a characterised and
> quantified whole-body heading failure arising from magnetometer-free 6-axis fusion, and a proposed
> constraint-based correction.

A measured, explained limitation is worth more to a reviewer than a headline accuracy figure resting
on an alignment that did not converge.
