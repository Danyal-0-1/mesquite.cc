# Drift reduction — what to implement, and why it works here

## The problem stated precisely

`[fact-code]` The DMP runs **Game Rotation Vector** — 6-axis, gyro + accelerometer, magnetometer line
commented out (`Pod_Watch_Binary.ino:345,350`). That has a hard consequence:

| Axis | Reference | Observable? |
|---|---|---|
| Roll | gravity | **Yes** — bounded error |
| Pitch | gravity | **Yes** — bounded error |
| **Yaw / heading** | *nothing* | **No** — open-loop gyro integration |

Gravity fixes two of three rotational degrees of freedom. Rotation *about* gravity leaves the
accelerometer reading unchanged, so nothing constrains heading. It integrates freely.

**The measured evidence agrees, and points somewhere specific:**
- `[fact-data]` global joint RMSE is **flat at 42–54° across every joint including the root**, while
  root-relative pose RMSE is only **15–16°**. The articulated skeleton is fine; the *whole-body
  frame* is wrong.
- `[fact-data]` required yaw alignment was **−71.19°** in one session and **+99.16°** in another
  recorded minutes later. Sign-flipping, session-scale.
- `[fact-doc]` the ICM-20948's 0.015 °/s/√Hz predicts only **~0.37° of angle random walk over ten
  minutes** — two orders of magnitude too small to explain 71–99°.

**That gap is the single most important clue in this document.** Random walk cannot produce the
observed error. Something else does, and there are only two candidates:

1. **Uncorrected arbitrary initial heading.** Each DMP initialises to whatever yaw it happens to
   start at. Fifteen pods, fifteen unrelated yaw origins, none aligned to each other or to the world.
2. **Residual gyro bias**, which produces error growing *linearly* in time rather than as √t. `[fact-doc]`
   the datasheet zero-rate offset is **±5 °/s** part-to-part; even 1% of that uncompensated is 3°/min.

**These need different fixes and U3 is still unresolved.** Run M4 first — one node, stationary, ten
minutes, log heading. Flat means initialisation; a ramp means bias. Phase 2 proved this cannot be
answered from the existing captures (`../system_assessment_2/MEASUREMENTS.md` D4).

---

## A — Per-session heading alignment at T-pose
**Fixes: initialisation. Cost: ~20 lines. Confidence it is the dominant term: high.**

At T-pose the subject faces a known direction, so every segment's *nominal* heading is known. Capture
each pod's reported yaw at that instant and store the difference as a per-pod yaw offset, applied for
the rest of the session.

```
For each pod p at T-pose:
    yaw_p        = heading component of q_p            (rotation about gravity)
    yaw_offset_p = yaw_nominal_p − yaw_p
Thereafter:
    q_corrected = R_gravity(yaw_offset_p) · q_p        (pre-multiply, world frame)
```

**Why this is first:** it is the only fix whose size matches the observed error. It costs almost
nothing, it slots into the calibration path that already exists (`js/custom_icm.js:268-298` already
solves for a mounting offset from a T-pose), and if Phase 1's arithmetic is right it removes most of
the 71–99°.

**Important detail — apply it on the correct side.** The mounting offset is a *body-frame* rotation
and is right-multiplied. A heading correction is a *world-frame* rotation about gravity and must be
**left**-multiplied. Getting this backwards produces plausible-looking but wrong motion, which is
exactly the silent failure class Phase 1 warns about.

**What it does not fix:** anything that accumulates *after* T-pose. If M4 shows real drift, A alone
will look good for the first minute and degrade after.

---

## B — ZARU: zero angular-rate updates for gyro bias
**Fixes: the linear-in-time drift component. Cost: ~30 lines on the node. No wire change.**

When a segment is genuinely still, its true angular rate is zero, so whatever the gyro reports *is*
the bias. Average it and subtract.

```
if ‖ω‖ < ω_thresh  for N consecutive samples:
        b̂ ← (1−α)·b̂ + α·mean(ω over the window)
ω_corrected = ω − b̂
```

**Three reasons this fits your system unusually well:**

1. **The data is already there and is being thrown away.** `[fact-code]`
   `Pod_Watch_Binary.ino:348-349` enables `RAW_GYROSCOPE` and `RAW_ACCELEROMETER` in the DMP FIFO,
   and `[fact-code]` `:~790` reads **only** the Quat6 branch. Phase 1 flagged those two streams as
   pure waste (`SENS-02`). They are not waste — **they are exactly the input ZARU needs**, already
   crossing the I²C bus every cycle. Before deleting them, consider using them.
2. **Stillness is measurable per-pod, with no whole-body reasoning.** A pod only needs to know *it*
   is still, from its own gyro magnitude. No stance detection, no kinematic model, no host
   involvement. It runs entirely on the node.
3. **`[fact-data]` stillness genuinely occurs.** Phase 2 measured contiguous low-motion runs in
   every capture, **up to 3.30 seconds** (`../system_assessment_2/MEASUREMENTS.md` D3). At 55 Hz a
   one-second window is ~55 samples — enough to average white noise down by ~7×.

**The caveat that decides whether this is worth doing:** `[unverified]` the DMP performs its own
internal gyro bias tracking, and TDK does not publish the algorithm. If its internal estimator is
already good, host-side ZARU adds nothing. **M4 answers this too** — a flat heading trace over ten
static minutes means the DMP's bias handling is adequate and B is unnecessary.

**Failure mode:** a slow, genuinely-rotating segment can be mistaken for stillness, and its real
rotation absorbed into the bias estimate. Guard with a conservative threshold, require accelerometer
stability as well as gyro, and rate-limit how fast `b̂` may move.

---

## C — Hinge-joint axis constraint
**Fixes: relative heading between adjacent segments. Cost: real work. No magnetometer needed.**

The knee and elbow are approximately 1-DOF hinges. For two IMUs spanning such a joint, the joint axis
is a *fixed vector in both sensor frames*. That is a strong, permanent constraint that costs nothing
to observe.

The relative angular velocity between the two segments must lie along the joint axis:

```
minimise over (j₁, j₂):   Σ_t  ‖ ω₁(t) × j₁ ‖ − ‖ ω₂(t) × j₂ ‖  ²
```

Solve once from a minute of ordinary movement — no special calibration pose. Thereafter, any relative
yaw drift between the two segments shows up as the two axis estimates ceasing to agree, and the
disagreement *is* the correction.

**Why this matters here specifically:** you have fifteen sensors on a closed kinematic chain and
currently exploit none of that redundancy (`KIN-01`). This is the single most powerful
magnetometer-free heading reference available, and it works indoors, near steel, with no external
infrastructure.

**Limitations, honestly:** knees and elbows are not perfect hinges — there is real secondary motion —
so the constraint is soft, not exact. It bounds *relative* heading between adjacent segments, not
absolute heading of the body. Pair it with A or D or E for an absolute datum.

**Reference:** Seel, Raisch & Schauer, *IMU-Based Joint Angle Measurement for Gait Analysis*, Sensors
2014 — the standard treatment of exactly this estimator.

---

## D — Magnetometer, re-enabled with disturbance gating
**Fixes: absolute heading. Cost: 1 line for the naive version, ~60 for the version you want.**

The AK09916 is physically present on every ICM-20948. Two ways to use it:

**D1 — the one-line version.** Switch `INV_ICM20948_SENSOR_GAME_ROTATION_VECTOR` to
`INV_ICM20948_SENSOR_ROTATION_VECTOR` (`Pod_Watch_Binary.ino:345`). The DMP then runs 9-axis fusion
and yaw becomes observable. It costs one extra FIFO stream and a small amount of I²C bandwidth.

**Do not ship D1 by itself.** A motion-capture volume contains steel framing, cabling, and fifteen
transmitting ESP32s. Uncorrected magnetic distortion routinely exceeds the drift it is meant to fix,
and — worse — it produces *confidently wrong* heading that varies with position in the room. Naive
9-axis is often worse indoors than 6-axis. That is very likely why the magnetometer was disabled in
the first place, and that decision was defensible.

**D2 — gated magnetometer.** Accept a magnetic heading update only when the field looks like Earth's:

```
accept if   | ‖B‖ − ‖B_local‖ |  <  ~20% of ‖B_local‖
      and   | dip(B, gravity) − dip_local |  <  ~10°
```
Tempe, AZ is roughly 47 µT total with ~59° inclination `[unverified — check NOAA for the exact site]`.
Reject everything else and coast on the gyro. Feed accepted updates through a **slow** complementary
correction (time constant of tens of seconds) so momentary distortions cannot snap the skeleton.

**This gives you a bounded absolute heading without the failure mode that made you turn the mag off.**
It also yields a free diagnostic: log the acceptance rate per pod and you have a map of where in the
room the magnetic environment is usable.

---

## E — Use the phone's SLAM heading as the world datum
**Fixes: absolute heading, using a reference you already have. Cost: ~40 lines.**

`[fact-code]` The hip-worn phone supplies position **and** orientation (`js/custom_icm.js:922-923`,
`:581`, `:602-603`), and `[fact-data]` Phase 2 confirmed the SLAM path is live — the root translates
realistically across every capture, with no discontinuities in ~7,100 frames.

`PHN-04` records that the **WebXR world frame and the IMU heading frame are never reconciled.** You
are running two independent estimates of the same world and never comparing them.

Visual-inertial SLAM heading is drift-bounded in a way inertial heading is not — it is anchored to
visual features. Two uses:

1. **At T-pose:** align the IMU heading datum to the WebXR world frame. This is fix A, but with an
   externally meaningful reference rather than an assumed one.
2. **Continuously:** treat the phone's yaw as a slow absolute reference for the *root*, with the same
   gating discipline as D2 — reject updates during rapid motion or on tracking-state changes.

**Caveat:** a hip-mounted camera sees mostly floor and swinging limbs, which is close to a worst case
for feature tracking. Gate on the tracking-quality signal if the phone app exposes one — which is
question 6 on the still-outstanding phone list.

---

## F — Double-stance closure
**Fixes: relative heading between the two leg chains. Cost: ~40 lines. Cheap and self-checking.**

When both feet are planted, the pelvis position computed by forward kinematics up the left leg must
equal the pelvis position computed up the right leg. Any mismatch is accumulated error, and its
yaw component is directly correctable.

`[fact-data]` Phase 2 measured stance runs in every capture, so the opportunity occurs routinely.

**Bonus:** even without applying a correction, the *magnitude* of this mismatch is an excellent live
quality metric — it needs no ground truth, so it can run during every session and tell you when the
suit has gone bad before the operator notices.

---

## G — How to apply corrections without replacing the DMP

Phase 1 and Phase 2 both concluded: **do not replace the fusion filter.** The measured articulated
pose is 15–16° RMSE against a commercial suit; the global error is 42–54°. A better attitude filter
addresses the smaller term.

That conclusion does **not** forbid a *heading-only correction layer downstream* of the DMP, which is
a different thing:

```
q_final = R_gravity(δψ_p) · q_DMP,p
```
one scalar per pod, updated slowly from whichever reference (A, B, C, D, E, F) is available, with:
- **a slew-rate limit** (e.g. ≤2 °/s) so no correction can be mistaken for real motion
- **a dead-band** so noise does not drive it
- **a confidence gate** so a rejected reference simply leaves `δψ` coasting

This is a handful of state per pod, it cannot corrupt roll or pitch, and it is trivially disabled for
A/B comparison. It is the right place to put every algorithm above.

---

## Recommended order

| # | Fix | Cost | Gated on |
|---|---|---|---|
| 0 | **Run M4** — is it drift or initialisation? | 1 node, 10 min | nothing |
| 1 | **A** — T-pose heading alignment | ~20 lines | M4 |
| 2 | **G** — heading correction layer | ~50 lines | — |
| 3 | **F** — double-stance closure *as a metric first* | ~40 lines | — |
| 4 | **B** — ZARU | ~30 lines | M4 (may be redundant) |
| 5 | **E** — phone heading datum | ~40 lines | phone source |
| 6 | **D2** — gated magnetometer | ~60 lines | site magnetic survey |
| 7 | **C** — hinge-axis constraint | substantial | offline prototype first |

**Do 0, 1 and 2 before anything else.** If M4 shows initialisation dominates — which the arithmetic
strongly predicts — then A plus G may remove most of the observed error for about seventy lines of
code, and everything below it becomes refinement rather than rescue.

## What would prove this plan wrong

If M4 shows heading drifting tens of degrees over ten static minutes, then the error *is*
accumulating, A alone will not hold, and the priority order flips: B (bias) and D2/E (absolute
reference) move to the top. **One ten-minute session decides which half of this document you need.**
