# A6 — Kinematics & Skeleton — Assessment

## 1. Scope and files read

`js/custom_icm.js` (60 KB — calibration, composition and per-bone blocks read in detail),
`js/bvh_converter.js` (5 KB, in full), `js/mappings.js` (1 KB), `glbs/rig.json`, `glbs/rig1.json`,
`trees/meta.json`, the bone tables in `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:12-56` and
`Device code/Dongle_Binary/Dongle_Binary.ino:69-88`, the `BONE_NAMES` table in
`js/webserialnative.js:31-49`, and as evidence: both sessions' `per_joint_errors.csv` and
`metrics_all.json` under `Mesquite_benchmarks/outputs/bvh_verification/`, plus
`Mesquite_benchmarks/scripts/compare_bvh_suits.py` (parser and rotation composition, lines 218-345).

## 2. Summary

Sensor-to-segment calibration **is properly implemented** — a T-pose capture solves
`offset = q_expectedBone · q_sensor⁻¹` per bone, including an axis-flip disambiguation — so the master
prompt's hypothesis that it is ignored is false, and the measured data agrees. Quaternion composition
is the correct `q_local = q_parent_global⁻¹ · q_child_global`, verified at every site. The four bone
tables across firmware, hub, browser parser and rig **all agree** on ids 0–16. The real kinematic gap
is the complete absence of any constraint-based drift correction: no foot contact, no zero-velocity
update, no closed-chain or segment-length constraint — all of which would supply an absolute reference
without a magnetometer, which is exactly what A5 shows the system lacks. The hierarchy is hand-unrolled
per bone rather than a generic tree walk, which is fragile but not currently wrong.

## 3. How this subsystem actually works

- **Bone id mapping is consistent across all four definitions** `[fact-code]`:
  `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:12-30` (`boneName[][2]`, 17 entries),
  `:33-56` (per-node `sendID` selection), `Device code/Dongle_Binary/Dongle_Binary.ino:69-88`
  (`POD_ABBR`, 17 entries), and `js/webserialnative.js:31-49` (`BONE_NAMES`, 17 entries). Ids 0–14 are
  the core skeleton; 15 and 16 are Left/Right Shoulder. I checked all three tables element by element
  and found **no disagreement**.
- **Sensor-to-segment calibration exists and is solved, not assumed** `[fact-code]`
  `js/custom_icm.js:268-298`: the sensor quaternion is first transformed by the static T-pose offset
  `:268`, then inverted `:275`, and the mounting offset is computed as
  `normalOffset = expectedBoneQ · transformedSensorQ⁻¹` `:276`. An alternative with a flip quaternion
  applied is computed at `:296-298` and the better of the two is selected — an axis-ambiguity
  disambiguation.
- **T-pose offsets are data-driven** `[fact-code]` `js/custom_icm.js:98-123`, composed from
  `boneConfig.tposeOffset` arrays in `trees/meta.json` `:108-120`, defaulting to identity `:123`.
- **Offsets are applied on the correct side** `[fact-code]` `js/custom_icm.js:37-50`:
  `result.multiply(tposeOffsets[bone])` then `result.multiply(mountingOffsets[bone])` — right
  multiplication, i.e. applied in the sensor's local frame, which is correct for a body-fixed mounting
  rotation.
- **Composition order is parent-inverse times child** `[fact-code]`, e.g.
  `js/custom_icm.js:630-631` `hipsQinverse = hipsQ.invert(); hipsCorrection = hipsQinverse.multiply(leftuplegQ).normalize()`,
  and identically at `:652-653` (right up leg), `:678-679` (spine), `:699-700` (head). This is the
  standard and correct construction of a local joint rotation from two global orientations.
- **The hierarchy is hand-unrolled**, not a tree walk `[fact-code]`: `js/custom_icm.js:583-900` is a
  sequence of per-bone blocks, each naming its parent explicitly (`mac2Bones["Hips"].global`,
  `mac2Bones["Spine"].global`, and so on). `applyMountingOffset` is called in every one of the ~18
  blocks `[fact-code]` `:588-898`.
- **BVH export** `[fact-code]` `js/bvh_converter.js`: Euler via `setFromQuaternion(q, "XYZ")` `:8`,
  channels declared `Xrotation Yrotation Zrotation` `:124,127`, root gets 6 channels `:124`,
  `Frame Time: 1/30` `:157,161`.

## 4. Findings

### KIN-01 — No kinematic-constraint drift correction of any kind
- **Severity:** CORRECTNESS
- **Confidence:** high
- **Location:** absence across `js/custom_icm.js` and `js/bvh_converter.js`
- **Evidence:** `[fact-code]` grep across the webapp finds no foot-contact detection, no
  zero-velocity update, no closed-chain constraint, no segment-length enforcement, and no joint-limit
  clamping. The per-bone blocks `js/custom_icm.js:583-900` compose orientations and write them to the
  rig with no constraint stage anywhere in the path.
- **Explains symptoms:** **S6**, and it is the missing remedy for A5's rank-1 error
- **Mechanism:** A5 establishes that heading is unobservable with the magnetometer disabled, and the
  data shows a 71–99° whole-body yaw error. Kinematic constraints are the standard way to recover an
  absolute reference **without** a magnetometer: a foot planted on the ground supplies a zero-velocity
  and zero-heading-rate observation, and segment-length consistency constrains the root. This system
  has fifteen sensors on a closed kinematic chain and exploits none of that redundancy. The absence is
  worth stating plainly because it is the one class of fix that addresses the dominant error term
  without new hardware and without a magnetometer.
- **Falsifying measurement:** From an existing capture, compute the vertical velocity and angular rate
  of each foot segment across the session and identify stance phases (intervals where both are near
  zero). If clear stance phases are detectable in the existing BVH data, a zero-velocity update is
  implementable and this gap is real and closable. If the data is too noisy to detect stance, the
  remedy is not available and the finding's practical weight drops.
- **Proposed change:** None in Phase 1 — this is an architectural addition, explicitly out of scope.
  Record it as the highest-value Phase 2 direction for accuracy, contingent on the measurement above
  and on E11's outcome.
- **Cost:** substantial; Phase 2.
- **Risk of the fix:** Constraint-based correction can inject artifacts when contact detection is
  wrong (foot sliding, false stance during a jump). Needs careful validation against ground truth.

### KIN-02 — Error pattern is flat across the chain, exonerating per-limb calibration
- **Severity:** QUALITY (a negative result that redirects effort)
- **Confidence:** high
- **Location:** evidence in `Mesquite_benchmarks/outputs/bvh_verification/*/per_joint_errors.csv`
- **Evidence:** `[fact-data]` session 01 `global_rms_error` by joint: pelvis **44.24**, spine_lower
  45.31, spine_mid 46.84, spine_upper 49.39, neck 50.95, head 53.93, right_upper_leg 44.49,
  right_lower_leg 44.13, right_foot 46.42, right_toe **45.46**, left_upper_leg 43.79,
  left_lower_leg 47.10, left_foot 48.19, left_toe **46.78**.
- **Explains symptoms:** none directly — it **removes** a suspected cause
- **Mechanism:** A sensor-to-segment calibration error, or a composition-order error, compounds down a
  kinematic chain: each joint inherits its parent's error and adds its own, so distal joints (toes,
  hands) must be markedly worse than proximal ones. The measured data shows the opposite — the toes
  (45.46, 46.78) are **no worse than the pelvis** (44.24), and the total spread across the entire
  skeleton is only about 10°. The error is a near-uniform offset applied to everything. Combined with
  A5's finding that root-relative pose RMSE is only 15–16°, this is conclusive: **the articulated
  chain is composing correctly, and the error lives in the whole-body frame.** The one mild gradient
  present is up the spine to the head (44 → 54), consistent with a small accumulating error in the
  torso chain, and worth a second look — but it is a ~10° effect on top of a ~44° baseline.
- **Falsifying measurement:** Recompute per-joint error after applying a per-frame optimal whole-body
  yaw alignment. If distal joints then separate clearly from proximal ones, a chain-compounding error
  is hiding underneath the frame error and this finding is incomplete.
- **Proposed change:** None. Redirect calibration effort to the root/heading problem (A5 rank 1–2).
- **Cost:** 0.
- **Risk of the fix:** n/a.

### KIN-03 — Hierarchy is hand-unrolled per bone
- **Severity:** QUALITY
- **Confidence:** high
- **Location:** `js/custom_icm.js:583-900`
- **Evidence:** `[fact-code]` roughly eighteen near-identical blocks, each hardcoding its parent —
  `mac2Bones["Hips"].global` at `:628` and `:650`, `mac2Bones["Spine"].global` at `:697`, and a
  generic `parentBone.global` at `:672-676` for one case only. The pattern
  `refQInverse = refQuaternion.invert()` recurs at `:583`, `:604`, `:619`, `:641`, `:663`, `:690`,
  `:712` and onward.
- **Explains symptoms:** none currently
- **Mechanism:** The skeleton topology is encoded implicitly across hundreds of lines rather than
  declared once in `glbs/rig.json` and walked. Every bone's parent relationship must be maintained by
  hand, so adding a segment or changing the hierarchy requires editing many places consistently. A
  mistake would be silent — it produces plausible-looking but wrong motion, the exact failure class
  the master prompt flags. The code is currently correct; the risk is in change.
- **Falsifying measurement:** Assert at runtime that each bone's composed local rotation, when
  re-composed forward through the declared rig hierarchy, reproduces the sensor's global orientation
  to within a tolerance. A mismatch localises a wrong parent immediately.
- **Proposed change:** None in Phase 1 (refactoring is explicitly out of scope). The runtime assertion
  above is a cheap guard that is not a refactor.
- **Cost:** ~20 lines for the assertion.
- **Risk of the fix:** None for an assertion.

### KIN-04 — Two calibration helpers are dead code
- **Severity:** QUALITY
- **Confidence:** high
- **Location:** `js/custom_icm.js:25-35`, `:52-90`, `:626`, `:648`
- **Evidence:** `[fact-code]` `adjustQuaternionForThickness` `:25-35` is defined and **never called**
  anywhere in the file. `applySensorOffsetCompensation` `:52-90` is defined, and its only two call
  sites are **commented out** at `:626` and `:648`.
- **Explains symptoms:** none
- **Mechanism:** Both matter mainly as traps. `adjustQuaternionForThickness` multiplies all four
  quaternion components by `1 + thickness` `:28-34`, which scales a unit quaternion into a non-unit one
  — it does not represent any rotation adjustment, and its own comment calls it "a placeholder
  implementation". If it were ever wired in it would corrupt every affected bone.
  `applySensorOffsetCompensation` `:79-81` applies a hand-rolled heuristic,
  `zCompensation = sin(xRotation) · (offset_z / |offset|) · compensationStrength`, which approximates
  the effect of a sensor displaced from the joint centre — a real geometric effect, but this is a
  scalar fudge rather than the rigid-body transform the situation calls for. Both being inactive is
  the correct current state.
- **Falsifying measurement:** n/a — established by reading.
- **Proposed change:** None in Phase 1. Flag both so neither is re-enabled without a proper derivation.
- **Cost:** 0.
- **Risk of the fix:** n/a.

### KIN-05 — No joint limits or biomechanical constraints
- **Severity:** QUALITY
- **Confidence:** high
- **Location:** absence across `js/custom_icm.js`
- **Evidence:** `[fact-code]` no angle clamping, no range-of-motion enforcement, and no plausibility
  check exists on any composed local rotation.
- **Explains symptoms:** none — but it removes a diagnostic signal
- **Mechanism:** Because each segment's orientation is set independently from its own sensor, the
  skeleton can adopt anatomically impossible poses (a knee bending backwards, a forearm rotated past
  its limit) whenever a sensor's estimate is wrong. Such poses are a *symptom* of an upstream error,
  not a cause — but with no check, they pass through silently instead of flagging the bad sensor. The
  benchmark data hints at exactly this: `[fact-data]` at spine_lower, `reference_rom_deg` is 46.24
  against the reference's 12.00, a 34° range-of-motion excess, meaning Mesquite's spine articulates
  far more than the ground truth says the body did.
- **Falsifying measurement:** Add an offline plausibility pass over an existing capture that counts
  frames per joint exceeding conservative anatomical limits. A near-zero count means the poses are
  anatomically sane and this adds nothing; a high count localises which sensors are misbehaving.
- **Proposed change:** None in Phase 1. Applying limits would mask errors rather than fix them; the
  value here is the offline *detector*, not an in-line clamp.
- **Cost:** ~40 lines offline.
- **Risk of the fix:** Clamping in the live path would hide upstream faults — do not do it.

### KIN-06 — Skeleton proportions differ substantially from the reference
- **Severity:** RISK (benchmark methodology)
- **Confidence:** high
- **Location:** evidence in `Mesquite_benchmarks/outputs/bvh_verification/`
- **Evidence:** `[fact-data]` `per_joint_errors.csv` session 01: pelvis `reference_bone_length` 55.53
  versus `candidate_bone_length` 95.92; spine_lower 9.99 versus 3.68. `[fact-data]`
  `metrics_all.json` applies a single uniform `pose_scale_applied_to_candidate` of 1.1023.
  `[fact-data]` `mapped_joint_count` is 14, while the Mesquite rig declares 22 joints.
- **Explains symptoms:** none — it qualifies the evidence everyone else is citing
- **Mechanism:** The two skeletons have different bone lengths and different joint counts, and the
  comparison reconciles them with one global scale factor. Positional error expressed in BVH units is
  therefore part tracking error and part retargeting artifact, and the absolute magnitudes
  (`root_error_mean` ≈ 38, `global_joint_rmse_mean` ≈ 42) should not be read as physical distances.
  Only 14 of 22 joints are compared at all. The *shape* of the error — flat across joints,
  root-dominated — is robust to this, which is why KIN-02 and A5's ranking stand; the *magnitudes*
  are not.
- **Falsifying measurement:** Re-run the comparison with per-bone length normalisation instead of a
  single global scale, and check whether the flat error profile survives. If per-joint errors reorder
  substantially, the current numbers are dominated by retargeting and need rebuilding.
- **Proposed change:** None to the pipeline. A9 should record this caveat in the evaluation design.
- **Cost:** analysis only.
- **Risk of the fix:** n/a.

## 5. Symptom attribution

| Symptom | Contributes? | Evidence |
|---|---|---|
| **S1** degrades with node count | No evidence | Kinematics runs per-frame on whatever data arrives; it has no node-count-dependent cost. |
| **S2** oscillates between values | No evidence | No queueing or rate-dependent behaviour in the skeleton path. |
| **S3** only ~4 nodes connect | No evidence | Bone tables agree across all four definitions, so a mapping mismatch is not hiding pods. |
| **S4** sometimes all 15 fine | No evidence | Same. |
| **S5** 60 Hz fails with many nodes | No evidence | No rate dependence here. |
| **S6** yaw drift | **Yes — by omission** | KIN-01: no kinematic constraint supplies the absolute heading reference the magnetometer would have. KIN-02 shows the chain composition itself is sound, so the gap is the missing correction, not a bad transform. |

## 6. What I could not determine

- `[unknown]` **Whether a T-pose calibration is actually performed in normal operation.** The code
  exists (`js/custom_icm.js:268-298`) but the operator workflow — whether the tester reliably captures
  a T-pose at session start, and whether the benchmark captures included one — is not determinable from
  source. Owner question, and it materially changes how KIN-02's result should be read.
- `[unknown]` **The DMP's frame convention** (handedness, axis assignment, reference direction).
  Shared with A5. Without it, whether `tposeOffsets` from `trees/meta.json` are expressed in the right
  frame can only be inferred from the fact that results look approximately correct.
- `[unknown]` **Where the phone's SLAM position attaches to the skeleton and how root orientation is
  determined.** The `Hips`/`HipsAlt` split (`js/custom_icm.js:128,133`) suggests two root sources —
  one from a pod and one from the phone — but which drives the exported root, and how they are
  reconciled, was not established. This is A7's boundary and is unresolved on both sides.
- `[unknown]` **What `trees/meta.json`'s `tposeOffset` values actually encode**, and whether they were
  derived or hand-tuned. Owner question.
- `[unknown]` **Whether `treeType` varies between sessions** — `trees/` holds eight variants
  (`bc`, `honey`, `smooth`, `spiny`, `thorny`, `velvet`, `velvetGreen`), each potentially with
  different offsets. Which was used for the benchmark captures is unrecorded.
- `[unverified]` The claim that stance phases are detectable in existing captures (KIN-01's
  measurement) is untested.

## 7. Measurements I need

1. **Forward-recomposition assertion** (KIN-03) — cheap, and it permanently rules out a whole class of
   silent hierarchy errors.
2. **Per-frame yaw-aligned re-analysis** (KIN-02's falsifying test), shared with A5. This is the single
   analysis that would separate whole-body frame error from residual chain error.
3. **Foot stance detection** over existing captures (KIN-01). Determines whether the highest-value
   Phase 2 fix is actually available.
4. **Per-bone-length normalised re-comparison** (KIN-06). Needed before any benchmark magnitude is
   quoted in a paper.
5. **A recorded T-pose segment at the start of every session**, with the calibration result logged.
   Without it, KIN-02's exoneration of the calibration path rests on code reading alone.
6. **Anatomical plausibility counter** offline (KIN-05), to localise misbehaving sensors.
