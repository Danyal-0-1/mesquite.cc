# B9 — Phone / SLAM — Phase 2

## 1. Scope
Ran `tools/offline_analysis.py` over all seven BVH captures. Read `js/custom_icm.js` position path.
Added the W1b guard counter (counting only — behaviour unchanged). Phone source still unavailable;
**three of the four W2-runnable measurements were run.**

## 2. Summary
Phase 2 §B9 was right that this agent's blocked status had been over-applied — most of its work was
never blocked. Two Phase 1 findings are now settled from existing data: **`PHN-05` is confirmed and
quantified** (up to 23.9% of frames sit within 10° of the gimbal singularity, against 0.0% for the
reference), and **`PHN-03` is refuted** (no relocalization signature exists; the only large jumps
are the reference's own frame-0 rest pose, which the comparison already trims). The mechanism behind
`PHN-05` turns out to be structural rather than incidental: Mesquite's `XYZ` root order puts yaw —
the one axis a hip root sweeps freely — in the gimbal-critical middle slot, while Rokoko's `YXZ`
puts pitch there.

## 3. Measurements run

### PHN-05 — confirmed, quantified (D1)
See `../MEASUREMENTS.md` D1 for the full table. Headline: 3.2%–23.9% of frames within 10° of ±90° on
the middle Euler axis across Mesquite captures, with 11 detected X/Z discontinuities in the longest
session; **0.0% in both Rokoko files**.

The asymmetry is the finding. In an Euler triple the middle axis is gimbal-critical. `XYZ` → middle
is **Y (yaw)**, which a hip-worn root rotates through freely. `YXZ` → middle is **X (pitch)**, which
stays near zero for an upright human. Rokoko's zero is not luck; it is what that rotation order buys,
and it is why BVH conventionally orders yaw first.

### PHN-03 — refuted for existing captures (D2)
Mesquite root displacement maxima are 2.65–6.66 units/frame (~80–200 units/s). With root Y ≈ 55 for a
standing subject, ~1 unit ≈ 1 cm, so the worst frame is ~0.8–2.0 m/s — fast but physically real. **No
teleports.** The 144–160 unit jumps are in the **Rokoko reference**, one per file, both at frame 0
(rest pose → capture position), and `metrics_all.json` records `trim_start_frames: 5`, so they are
already discarded.

Phase 1 called `root_error_max` 109.60 "suggestive" of relocalization. It was suggestive of nothing;
the underlying frames show smooth motion. Recorded in `../12_integration/PHASE1_SCORECARD.md`.

### Straight-line walk (PHN-04) — **not run**
Requires a purpose-recorded session: subject walks 10 m facing forward, comparing root travel
direction against torso forward axis. The existing captures contain unstructured motion, so any
angle computed from them would confound heading offset with genuine turning. Attempting it on this
data would have produced a number without meaning. Deferred to the W2 campaign, where the protocol
already includes a straight-line walk block.

## 4. Findings

### P2-B9-01 — `PHN-05` upgraded: the rotation order is the cause, not the trigger
- **Severity:** RISK · **Confidence:** high (was medium)
- **Location:** `js/bvh_converter.js:8` (`setFromQuaternion(q, "XYZ")`), `:124` (root CHANNELS)
- **Evidence:** `[fact-data]` D1.
- **Explains symptoms:** none in S1–S6 directly; corrupts exported root rotation.
- **Mechanism:** Near the middle-axis singularity the first and third Euler angles become
  numerically unstable and can swing between adjacent frames while representing nearly the same
  orientation. Mesquite spends up to a quarter of some sessions in that region because its middle
  axis is yaw. The quaternion carries no such defect — this is introduced entirely by the export's
  choice of order.
- **Falsifying measurement:** already run (D1). To go further: reconstruct quaternions from the
  exported Euler triples and compare against the pre-export quaternions frame by frame; divergence
  concentrated near the singularity confirms information loss rather than mere instability.
- **Proposed change:** change the declared channel order so yaw is not the middle axis (`ZXY` or
  `YXZ`, matching the reference). **W5/W6, not now** — it changes the file format for every
  consumer, and `WEB-07` notes many importers ignore the declared order.
- **Cost:** ~5 lines plus a compatibility review of every downstream consumer.
- **Risk:** breaks any consumer that hardcoded `XYZ`; invalidates comparison of new files against old
  ones unless the analysis honours declared order (`compare_bvh_suits.py` does — `:321-331`).

### P2-B9-02 — Mesquite root trajectory is clean; a positive result worth publishing
- **Severity:** n/a (negative finding) · **Confidence:** high
- **Evidence:** `[fact-data]` D2.
- **Mechanism:** Across five captures totalling ~7,100 frames there is not one discontinuity
  exceeding plausible human motion. Whatever the phone/SLAM path's faults, **it is not injecting
  jumps into the root** in these sessions. Given that `RETRACTIONS.md` withdraws several claims,
  a defensible positive result is worth having: the root trajectory is continuous and physically
  plausible throughout.
- **Falsifying measurement:** a session with deliberate tracking stress — occlusion, rapid turns,
  poor lighting — would test whether relocalization jumps appear under conditions the existing benign
  captures never created. **Absence of evidence here is not evidence of absence under stress.**
- **Cost:** one session. **Risk:** none.

## 5. W1b — instrumented, not fixed
`js/custom_icm.js:922-930` now counts how often the truthiness guard would reject a position whose
coordinates are all finite — i.e. how often `PHN-01`'s zero-coordinate bug actually fires. The guard
itself is **unchanged**, per Rule 1. The counter needs live phone data.

## 6. What I could not determine
`[unknown]` Everything requiring the phone source — reference space, update rate, units, handedness,
relocalization events, tracking-loss behaviour. The ten-question list in
`system_assessment/07_phone_slam/REPORT.md` §7 stands unchanged.
`[unknown]` Whether `PHN-01` fires in practice — counter added, never run.
`[unknown]` `PHN-04`'s frame offset — needs the straight-line walk session.

## 7. Measurements I need
The straight-line walk block from the W2 protocol; one phone-active session with W1b counting; and a
deliberately tracking-hostile session to test `PHN-03` under stress rather than under benign
conditions.
