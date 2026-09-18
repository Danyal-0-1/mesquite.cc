# Post-Processing Error Correction for Magnetometer-Free Inertial Motion Capture

**Danyal Khorami** · Machine Learning · Project Proposal

**Constraint, by design:** the magnetometer stays **off**. A mocap volume contains steel framing,
cabling, and fifteen transmitting radios; uncorrected magnetic distortion routinely exceeds the drift
it is meant to fix and produces *confidently wrong* heading that varies with position in the room.
The research question is therefore: **how far can learned post-processing go using only kinematic
structure and motion priors, with no absolute heading reference at all?**

---

## Part 1 — The problems (all measured, not assumed)

A prior three-phase diagnostic pass over this 15-sensor ICM-20948 suit produced a ranked, evidence-
backed defect list. Four are correctable in post-processing:

| # | Defect | Measured evidence |
|---|---|---|
| **P1** | **Heading is unobservable.** 6-axis fusion: gravity fixes roll/pitch, nothing fixes yaw. | Global joint RMSE **42–54°**, flat across every joint *including the root*, while root-relative pose RMSE is only **15–16°**. Required yaw alignment: **−71°** one session, **+99°** the next. |
| **P2** | **Packet loss is silent and mishandled.** A per-node sequence counter is on the wire and never gap-checked; missing samples hold the last value, recording artificial stillness followed by a jump. | Loss was *unmeasurable* until instrumentation was added; hold-last-value confirmed in the compositing path. |
| **P3** | **Timing is corrupted three ways.** Node timestamps mark *transmission*, not sampling; 15 unrelated clock origins are never reconciled; the exporter hardcoded 1/30 s over ~32 Hz data. | **~7% progressive time dilation** in every capture. Cross-correlation against reference fails to lock (r ≈ 0.59), yielding latency estimates of −300 ms and −2200 ms for the same activity. |
| **P4** | **Corrupt samples enter unchecked.** An unclamped `sqrt` produces NaN, quantized to a **zero quaternion** that looks valid on the wire; unsynchronized cross-core reads produce torn (non-unit) quaternions. | Both mechanisms confirmed in source; the legacy data path guarded against this, the current one does not. |

**The key insight from P1:** the datasheet predicts only **~0.37° of angle random walk over ten
minutes** — two orders of magnitude below the observed 71–99°. **This is not ordinary gyro drift.** It
is a per-sensor heading *reference* problem, and no amount of better single-sensor filtering can solve
it, because the quantity is unobservable from one sensor in isolation.

---

## Part 2 — Why machine learning, given no magnetometer

Heading is unobservable *per sensor* but **jointly constrained across fifteen sensors on one body.**
Classical remedies exploit only local constraints — zero-angular-rate updates during stance, hinge-axis
estimation at the knee and elbow, closed-chain closure when both feet are planted — and each needs
hand-derived per-joint models and careful gating, while real joints violate the idealized assumptions.

The ML formulation is stronger: **human motion is highly structured.** A set of fifteen orientations
with mutually wrong headings produces a pose that is anatomically *implausible* — legs splayed,
torso twisted, arms detached from gait phase. A model trained on large-scale real motion learns that
plausibility, so **heading becomes recoverable by optimizing the corrections that make the pose
look like real human motion.** This is the sparse-IMU pose literature (TransPose, PIP, TIP) adapted
to a different problem: not inferring pose from few sensors, but resolving an unobservable DOF from
many.

---

## Part 3 — The proposed pipeline (all offline, no firmware changes)

**Stage 1 — Integrity filter (addresses P4).** Detect corrupt samples via a unit-norm test
(`0.9 < ‖q‖² < 1.1`), which catches both the zero quaternion and torn cross-core reads in one test,
plus a learned outlier detector for physically implausible frame-to-frame rotation. Output: a clean
stream with a per-sample validity mask.

**Stage 2 — Temporal reconstruction (addresses P2, P3).** Reconstruct a true uniform timebase and
impute gaps. A sequence model (temporal CNN / bidirectional GRU) trained to fill masked spans on the
SO(3) manifold, conditioned on neighbouring sensors — a dropped forearm sample is strongly predicted
by the upper arm and hand. Baselines: hold-last-value (current behaviour), linear interpolation, SLERP.

**Stage 3 — Learned heading correction (addresses P1, the core contribution).** Predict per-sensor
heading offsets δψᵢ, applied as gravity-axis rotations that provably cannot corrupt roll or pitch:
- **Input:** window of 15 sensor orientations + derived angular velocities
- **Output:** 15 scalar heading corrections
- **Loss:** geodesic orientation loss + differentiable-FK joint position loss + a **motion-prior term**
  penalizing implausible poses
- **Architecture:** temporal transformer/TCN over the sensor set, with ablations against
  per-sensor-independent correction and the classical ZARU / hinge-axis / closed-chain baselines

**Stage 4 — Calibration refinement.** Sensor-to-segment mounting offsets are solved once at T-pose and
never revisited, though straps loosen over a session. Estimate them continuously from motion, gated by
a slew-rate limit so genuine movement is never absorbed.

---

## Part 4 — Data

Real paired captures are contaminated by P3, so training on them would build on sand. I will
**synthesize IMU measurements** from public mocap (AMASS/CMU) by placing virtual sensors on the
skeleton — giving unlimited data with *exact* ground-truth orientation. The degradation model is not
guessed: angle random walk and zero-rate offset from the datasheet, int16 quantization from the actual
wire format, and **packet-loss and timing-jitter distributions measured on the real hardware** by
instrumentation already built for this system. Real captures are reserved for held-out evaluation.

---

## Part 5 — Expected results

Baseline is quantified: **42–54° global, 15–16° pose.** That gap is the headroom — the articulated
skeleton is already good, so success means closing global error toward the pose floor.

- **Primary:** global joint RMSE, and heading error versus time, against classical baselines
- **Secondary:** imputation accuracy versus hold-last-value at measured loss rates; post-FK joint
  position error; per-stage ablation showing each defect's individual contribution

---

## Part 6 — What this unlocks

1. **A valid benchmark.** Stages 1–2 repair the integrity and timing corruption that currently
   prevents *any* trustworthy comparison against ground truth — a precondition for publishable numbers.
2. **A usable capture pipeline.** Correct global orientation is what animation retargeting and
   biomechanical analysis actually require; 42° of whole-body error makes output unusable regardless
   of how good the local pose is.
3. **A transferable result.** Every low-cost magnetometer-free IMU suit has exactly this problem. A
   post-processing tool that resolves heading from motion structure alone generalizes beyond this
   hardware.
4. **A defensible paper claim** — a characterized limitation with a quantified remedy, rather than an
   unqualified accuracy figure.

---

## Part 7 — Scope, timeline, risks

**In scope:** offline post-processing only — no firmware changes, no hardware purchases, no
magnetometer. **Out of scope:** replacing the on-chip attitude filter (measurement shows it is not the
bottleneck), real-time deployment. **Stretch:** quantized on-device stance detection (TinyML) using
the raw gyro/accel streams the firmware already reads and discards.

**Weeks 1–3** synthetic generator + calibrated noise model · **4–5** Stage 1–2 and metric harness ·
**6–7** classical heading baselines · **8–11** Stage 3 model and ablations · **12–13** Stage 4 +
held-out evaluation · **14** write-up.

**Risks.** *Sim-to-real gap* — mitigated by hardware-calibrated noise and fine-tuning on real
captures. *Ground-truth quality* — mitigated by training on synthetic data with exact labels.
*Heading may prove to be initialization rather than drift* — a ten-minute static-hold experiment
distinguishes these early and would simplify, not invalidate, Stage 3.
