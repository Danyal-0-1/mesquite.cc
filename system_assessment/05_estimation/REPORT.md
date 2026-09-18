# A5 — Estimation & Sensor Fusion — Assessment

## 1. Scope and files read

`Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino` (814, fusion path and DMP config),
`js/custom_icm.js` (60 KB, searched exhaustively for filter stages), `js/bvh_converter.js` (5 KB, in
full), `index.html` (dependency loading), and the captured evidence in
`Mesquite_benchmarks/outputs/bvh_verification/` (`metrics_all.json`, both sessions'
`per_joint_errors.csv`) plus `Mesquite_benchmarks/scripts/compare_bvh_suits.py` (1522 lines; parser
and rotation-composition sections read to validate the metrics before citing them).

## 2. Summary

Fusion happens **entirely on the ICM-20948's DMP**, in 6-axis Game Rotation Vector mode with the
magnetometer disabled, and **no filtering runs anywhere downstream** — the browser's Kalman path is
commented out. Yaw is therefore unobservable and its error is an unbounded random walk, which the
captured data confirms directly: the two benchmark sessions required yaw alignments of **−71.2°** and
**+99.2°**, differing by 170° between two recordings minutes apart. The error budget is dominated by
this whole-body heading failure, not by per-limb estimation: global joint RMSE is nearly uniform at
**42–54°** across every joint *including the root*, while root-relative pose RMSE is only
**15–16°**. Int16 quaternion quantisation contributes ~0.006° and is negligible. The single largest
recoverable error is the missing heading reference; the second is the timebase corruption that makes
the measurements themselves unreliable.

## 3. How this subsystem actually works

- **All fusion is on the DMP.** `[fact-code]` `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:345`
  enables `INV_ICM20948_SENSOR_GAME_ROTATION_VECTOR`; the host reads the resulting quaternion from the
  FIFO at `:725-745` and does nothing further to it. There is no complementary filter, Madgwick,
  Mahony or Kalman step in the firmware.
- **6-axis, not 9-axis.** `[fact-code]` `:350` — the
  `INV_ICM20948_SENSOR_MAGNETIC_FIELD_UNCALIBRATED` line is commented out. `[fact-doc]` TDK's Game
  Rotation Vector is defined as a gyro+accel fusion with **no magnetometer input and therefore no
  absolute heading reference**; it is gravity-referenced only.
- The host reconstructs the scalar part: `q0 = sqrt(1 - (q1²+q2²+q3²))` `[fact-code]` `:742`, since the
  DMP transmits only Q1..Q3 scaled by 2^30 `[fact-code]` `:738-740`.
- **No downstream filtering is active.** `[fact-code]` `js/bvh_converter.js:10` — the sole call to
  `smoothOrientation` is commented out, so the three per-axis `KalmanFilter` instances defined at
  `:15-31` never run, despite `kalmanjs` being loaded at `index.html:20`.
- **No gyro bias estimation exists in project code.** Grep across the firmware and webapp finds no
  stationary-bias routine, no warm-up hold, and no bias storage `[fact-code]`. Whatever bias tracking
  occurs is internal to the DMP `[unverified]`.
- **No per-sensor calibration constants.** There is no per-node offset, scale-factor or misalignment
  table anywhere in firmware or webapp `[fact-code]`; fifteen physically distinct chips are treated as
  identical. What *does* exist is per-**bone** mounting and T-pose offsets in the browser
  (`js/custom_icm.js:12-18`, `:98-123`), which is a different thing — geometry, not sensor calibration.
- Wire quantisation: each component is scaled by 32767 into an int16 `[fact-code]`
  `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:174-180`, and restored by dividing by 32767
  `[fact-code]` `js/webserialnative.js:84-87`.

## 4. Findings

### EST-01 — Yaw is unobservable, and the captured data shows it dominating
- **Severity:** CORRECTNESS
- **Confidence:** high
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:345`, `:350`
- **Evidence:** `[fact-code]` GRV enabled, magnetometer line commented out. `[fact-doc]` GRV is
  6-axis and gravity-referenced. `[fact-data]` `Mesquite_benchmarks/outputs/bvh_verification/metrics_all.json`:
  `yaw_alignment_degrees` = **−71.19°** (session 01) and **+99.16°** (session 02) — the offset the
  comparison had to apply to align Mesquite to Rokoko. Two sessions recorded minutes apart
  (13:20:59 and 13:22:52) needed corrections 170° apart.
- **Explains symptoms:** **S6**, and it is the dominant term in overall accuracy
- **Mechanism:** Gravity observes two of three orientation degrees of freedom, fixing roll and pitch.
  Rotation about the gravity vector — heading — leaves the accelerometer reading unchanged, so with no
  magnetometer there is no measurement that constrains it `[inference from standard observability
  theory]`. Heading is then pure open-loop gyro integration, and integrating a zero-mean noise process
  gives a random walk whose error grows without bound (as √t from angle random walk, and linearly in t
  from any residual bias). The datasheet figures make the scale concrete: `[fact-doc]` the ICM-20948
  gyroscope has a rate noise density of **0.015 °/s/√Hz**. Angle random walk over time t at bandwidth
  is σ(t) = 0.015 × √t degrees, giving ≈0.12° at 60 s, ≈0.27° at 5 min and ≈0.37° at 10 min. **Those
  numbers are far too small to explain a 71–99° misalignment.** Random walk alone is therefore *not*
  the mechanism; the observed error is one to two orders of magnitude larger, which points at an
  uncorrected constant offset — an unreferenced initial heading, with each of 15 sensors starting at
  whatever arbitrary yaw its DMP happened to initialise to.
- **Falsifying measurement:** Experiment E11 — one node, stationary, 10 minutes, log the quaternion at
  full rate and plot heading versus time. If heading drifts by tens of degrees, bias/random walk is
  implicated; if it stays within a couple of degrees, the error is initialisation, not drift, and the
  fix is a heading alignment step rather than a better filter.
- **Proposed change:** None yet — E11 must run first, because it discriminates two different fixes
  (per-session heading alignment versus bias estimation or a heading reference).
- **Cost:** measurement only.
- **Risk of the fix:** n/a at this stage.

### EST-02 — Error is whole-body, not per-limb: the data says so
- **Severity:** CORRECTNESS
- **Confidence:** high
- **Location:** evidence in `Mesquite_benchmarks/outputs/bvh_verification/`
- **Evidence:** `[fact-data]` session 01 `per_joint_errors.csv`, `global_rms_error` by joint:
  pelvis 44.24, spine_lower 45.31, spine_mid 46.84, spine_upper 49.39, neck 50.95, head 53.93,
  right_upper_leg 44.49, right_lower_leg 44.13, right_foot 46.42, right_toe 45.46,
  left_upper_leg 43.79, left_lower_leg 47.10, left_foot 48.19, left_toe 46.78.
  `[fact-data]` `metrics_all.json`: `global_joint_rmse_mean` 42.07 (s01) / 42.78 (s02) versus
  `pose_joint_rmse_mean` **14.98** (s01) / **16.02** (s02); `root_error_mean` 38.41 / 38.95.
- **Explains symptoms:** S6
- **Mechanism:** The global error is essentially flat across the whole skeleton — the **root itself**
  carries 44.2, and the most distal joints (toes, hands) are no worse than the pelvis. A
  sensor-to-segment calibration failure or a quaternion composition-order error would compound down
  each kinematic chain and show distal joints markedly worse than proximal ones. That pattern is
  absent. What is present is a large, near-constant offset applied to everything, plus a root error of
  the same magnitude as the joint errors. **That is the signature of a whole-body frame misalignment —
  heading and root placement — not of per-limb estimation error.** The corroboration is that removing
  the root (pose-relative RMSE) cuts the error by roughly two-thirds, to 15–16°: the articulated pose
  is substantially better than the global placement.
- **Falsifying measurement:** Re-run `compare_bvh_suits.py` with per-frame optimal yaw alignment
  (rather than one alignment for the whole session) and re-read `global_joint_rmse_mean`. If it
  collapses toward the ~15° pose figure, whole-body heading is confirmed as dominant. If it stays near
  42°, the error is not a heading offset and this finding is wrong.
- **Proposed change:** None in Phase 1 — this is the ranking, which is the deliverable.
- **Cost:** analysis only, no code change.
- **Risk of the fix:** n/a.

### EST-03 — Int16 quaternion quantisation is negligible; recording the arithmetic
- **Severity:** QUALITY (negative result)
- **Confidence:** high
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:174-180`;
  `js/webserialnative.js:84-87`
- **Evidence:** `[fact-code]` components scaled by 32767 into int16 and restored by division.
- **Explains symptoms:** none
- **Mechanism:** The quantisation step is 1/32767 ≈ 3.05×10⁻⁵ in a unit quaternion component. A
  quaternion component error δ maps to an angular error of approximately 2δ radians in the worst case,
  giving 6.1×10⁻⁵ rad ≈ **0.0035°**, and about 0.006° when the error is distributed across components.
  Against a measured pose RMSE of 15° this is **four orders of magnitude below the noise floor** and
  cannot matter. The 16-byte packet's compactness costs essentially nothing in precision — this design
  choice is sound and should not be revisited.
- **Falsifying measurement:** n/a — closed-form. Included so the question is not reopened.
- **Proposed change:** None.
- **Cost:** 0.
- **Risk of the fix:** n/a.

### EST-04 — Fifteen chips treated as identical, with no per-sensor calibration
- **Severity:** CORRECTNESS
- **Confidence:** medium
- **Location:** absence across `Device code/` and `js/`
- **Evidence:** `[fact-code]` no per-node bias, scale-factor or axis-misalignment constants exist
  anywhere; the firmware is identical across nodes except for the `sendID` line
  (`Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:33-56`).
- **Explains symptoms:** contributes to S6
- **Mechanism:** `[fact-doc]` the ICM-20948 datasheet specifies a gyroscope zero-rate offset of
  **±5 °/s** over temperature and an accelerometer zero-g offset of ±50 mg, part-to-part. The DMP
  performs its own internal gyro bias tracking `[unverified — TDK does not publish the algorithm]`,
  which is why heading does not diverge as fast as an uncompensated ±5 °/s bias would imply (that
  would be 300°/minute). But nothing in project code measures, stores or applies any per-unit
  correction, so any residual differs per node and cannot be characterised. Accelerometer offsets
  additionally bias the gravity reference, tilting the roll/pitch datum slightly differently on each
  node.
- **Falsifying measurement:** Place all 15 nodes stationary and co-planar on a flat surface for 10
  minutes, log every quaternion, and compare the reported gravity direction and heading drift rate
  across units. Tight clustering falsifies this; a spread of several degrees confirms it.
- **Proposed change:** None in Phase 1. If confirmed, a one-off per-unit characterisation stored in
  EEPROM is cheap — but only after E11 establishes whether drift or initialisation dominates.
- **Cost:** measurement only.
- **Risk of the fix:** n/a.

### EST-05 — Timestep is fixed at 1/30 s while samples arrive at ~32 Hz and jitter
- **Severity:** CORRECTNESS
- **Confidence:** high
- **Location:** `js/bvh_converter.js:157`; `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:222,634`
- **Evidence:** `[fact-code]` `frameTime = 1/30` hardcoded; `[fact-code]` `fps = 32` with integer
  division giving a 31 ms gate. `[fact-data]` all exported BVH files carry
  `Frame Time: 0.03333333333333333`.
- **Explains symptoms:** corrupts the measurement of S1, S2 and S6
- **Mechanism:** No integration happens downstream (the DMP integrates internally on its own clock),
  so this does not inject orientation error directly — which is why it ranks below EST-01. What it
  does is make every *measurement* of the system wrong: a 7% time-base dilation, applied
  progressively, means velocity, angular speed and latency comparisons against ground truth are all
  systematically off, and cross-correlation cannot lock. `[fact-data]` the benchmark's own output
  shows this: `latency_correlation` 0.593 and 0.608, with `latency_ms` estimates of −300 ms and
  −2200 ms for two sessions of the same activity. Those are not measurements of a physical latency.
- **Falsifying measurement:** As WEB-03 — wall-clock elapsed divided by frame count during recording.
  A measured 33.33 ms mean interval falsifies it.
- **Proposed change:** Write the measured interval, and record per-frame arrival timestamps. This must
  land **before** any further ground-truth comparison (see A8, A9).
- **Cost:** ~10 lines.
- **Risk of the fix:** Invalidates previously reported benchmark numbers — which is the point.

### EST-06 — Torn quaternions and NaN frames inject non-physical orientations
- **Severity:** CORRECTNESS
- **Confidence:** medium
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:641-644` vs `:771-774`, and `:742`
- **Evidence:** cross-referenced from A1's NODE-01 and SENS-03. `[fact-code]` the `quat` global is
  written on core 1 and read on core 0 with no synchronisation; `[fact-code]` `q0 = sqrt(1 - ...)` has
  no clamp. `[fact-code]` the webapp counts the downstream symptom in `window._nanFrameDropped`
  (`js/webserialnative.js:276`) and its comments describe firmware emitting `nan` literals from
  exactly this expression (`:168-172`).
- **Explains symptoms:** contributes to S6 as spurious high-frequency error
- **Mechanism:** A torn read produces a non-unit quaternion that does not correspond to any orientation
  the body occupied, and a negative radicand produces NaN, which `q_to_i16` converts to a zero
  quaternion. Both enter the pipeline looking like data. Neither is currently counted at the node.
- **Falsifying measurement:** As NODE-01 — count unit-norm violations before transmit, and count
  negative radicands at `:742`, over a 10-minute moving capture. Zero on both falsifies it.
- **Proposed change:** Clamp the radicand (1 line) and synchronise the handoff — but only after the
  counters show it happens.
- **Cost:** 1 line plus ~15.
- **Risk of the fix:** None for the clamp.

## 5. Ranked error budget — the central deliverable

Ranked by contribution to total observed orientation error, with the evidence for each rank.

| Rank | Source | Magnitude | Confidence | Basis |
|---|---|---|---|---|
| **1** | **Unreferenced heading (whole-body yaw)** | **71–99° per session** | high | `[fact-data]` `yaw_alignment_degrees` −71.19 / +99.16; `[fact-data]` global RMSE flat at 42–54° across all joints including root, versus 15–16° pose-relative |
| **2** | **Root / global position error** | **38.4–39.0 units mean** | high | `[fact-data]` `root_error_mean`; A7 owns the SLAM source |
| **3** | **Timebase corruption (measurement validity)** | ~7% dilation | high | `[fact-code]` `frameTime = 1/30` vs 32 Hz; `[fact-data]` correlation 0.59/0.61, latency −300/−2200 ms |
| **4** | **Residual per-limb pose error** | **15–16° RMSE** | high | `[fact-data]` `pose_joint_rmse_mean` 14.98 / 16.02 — this is the real articulation accuracy once the root is removed |
| **5** | Missing per-sensor calibration | unquantified, ≤ a few ° | medium | `[fact-doc]` ±5 °/s gyro ZRO, ±50 mg accel offset; DMP compensates to an unknown degree |
| **6** | Torn quaternions / NaN frames | rare, unmeasured | medium | `[fact-code]` unsynchronised cross-core access, unclamped sqrt |
| **7** | Gyro angle random walk | **0.12° @ 1 min, 0.37° @ 10 min** | high | `[fact-doc]` 0.015 °/s/√Hz — two orders of magnitude too small to matter here |
| **8** | Int16 quantisation | **0.006°** | high | closed-form; negligible |

**The headline: ranks 1 and 2 are whole-body frame problems, not estimation problems.** Improving the
orientation filter would address rank 4 at best, leaving roughly two-thirds of the observed error
untouched. This is the concrete form of the master prompt's §C4 warning, and it is confirmed by
measurement rather than asserted. **Do not replace the filter.** The DMP's articulated pose is
performing at 15–16° RMSE against a commercial reference; the system's accuracy problem is that it
does not know which way the body is facing or where it is standing.

An important caveat on ranks 2 and 4: `[fact-data]` the two skeletons have substantially different
proportions — `reference_bone_length` 55.53 versus `candidate_bone_length` 95.92 at the pelvis, 9.99
versus 3.68 at spine_lower — and the comparison applies a single uniform
`pose_scale_applied_to_candidate` of 1.102. Some of the global positional error is therefore
retargeting artifact rather than tracking error, and the absolute unit figures should not be read as
centimetres of true error. The *pattern* (flat across joints, root-dominated) is robust to this; the
*magnitude* is not.

## 6. What I could not determine

- `[unknown]` **Whether the 71–99° heading error is drift or initialisation.** These demand different
  fixes and the existing captures cannot distinguish them, because no static-hold segment was
  recorded. Resolved by experiment E11.
- `[unknown]` **What bias tracking the DMP performs internally.** TDK does not publish the GRV
  algorithm. Resolved only empirically, by E11 and the 15-node static comparison in EST-04.
- `[unknown]` **The DMP's actual quaternion convention** (handedness, reference frame, axis
  assignment) beyond "6-axis gravity-referenced". This matters for A6's composition analysis.
  Resolved from the InvenSense DMP documentation or by a controlled single-axis rotation test.
- `[unknown]` **Whether the benchmark captures used the same firmware as the current code.** The
  captures are dated 2026-05-20 and 2026-06-08; the binary wire format may postdate them, and the
  directory name `mesquite_smooth` hints that `smoothOrientation` may have been active. Owner question.
- `[unknown]` **True per-node drift rates.** Never measured; requires the 15-node static test.
- `[unverified]` The 0.015 °/s/√Hz rate noise density is the ICM-20948 datasheet figure for the
  gyroscope; the DLPF and full-scale settings actually in force are unknown (A1's `[unknown]`), and
  they modify the effective noise bandwidth.

## 7. Measurements I need

1. **E11 — static drift test.** One node, stationary, 10 minutes, full-rate quaternion log. This is
   the highest-value estimation measurement and it discriminates rank 1's two candidate mechanisms.
2. **15-node static comparison**, all units flat and co-planar for 10 minutes. Gates EST-04 and gives
   the per-unit spread that no measurement currently provides.
3. **Per-frame arrival timestamps in recordings** (shared with A4/A8). Without these, EST-05 cannot be
   corrected and no future comparison is trustworthy.
4. **Re-analysis with per-frame yaw alignment**, to confirm EST-02's falsifying test.
5. **Unit-norm and NaN counters at the node.** Gates EST-06.
6. **A single-axis controlled rotation** (turntable or rate table if available) to establish the DMP's
   frame convention empirically, resolving the convention `[unknown]` for A6.
