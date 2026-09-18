# Keeping calibration valid, and offsetting drift, over a long session

## Separate two things that are currently conflated

Both present as "it was fine at the start and it is wrong now", but they have different causes,
different time constants, and different fixes. Treating them as one problem is why they are hard.

| | **Mounting offset decay** | **Heading drift** |
|---|---|---|
| What changes | the sensor's rotation *relative to the bone* | the estimate's rotation *relative to the world* |
| Cause | straps loosen, suit shifts, sensor rotates on the limb | unobservable yaw (`02_DRIFT_ALGORITHMS.md`) |
| Axis | all three | **yaw only** — roll and pitch stay gravity-locked |
| Time constant | minutes to tens of minutes, step-like when the suit shifts | continuous |
| Detectable by | kinematic residuals (below) | comparison against an absolute reference |
| Fix | re-estimate the mounting offset | a heading correction layer |

`[fact-code]` Today the system solves the mounting offset **once**, at T-pose
(`js/custom_icm.js:268-298`), and never revisits it. `[fact-code]` `mountingOffsets` is written at
calibration and read every frame thereafter (`:37-50`, `:588-898`). There is no decay detection and
no way to refresh without restarting.

---

## 1. Measure decay before trying to correct it

You cannot maintain what you cannot see. Three residuals are computable live, need **no ground
truth**, and each isolates a different failure.

### R1 — Double-stance closure
When both feet are planted, forward kinematics up the left leg and up the right leg must place the
pelvis in the same spot. The discrepancy is accumulated error.

```
e_closure(t) = ‖ FK_left_chain(t) − FK_right_chain(t) ‖
```
`[fact-data]` Phase 2 measured stance runs in every capture, up to 3.30 s, so this evaluates often.

### R2 — Hinge-axis stability
Estimate the knee and elbow joint axes from ordinary motion (see `02_DRIFT_ALGORITHMS.md` §C). The
axis is a *physical constant*. If the estimate rotates over a session, either that sensor moved on
the limb or its heading drifted — and R1 tells you which.

### R3 — Segment-length consistency
Bone lengths are fixed. If the distance implied between two joints changes, the chain has gone wrong.

**Each of these is worth building even if you never auto-correct anything.** Today the operator
discovers a bad session by looking at the skeleton and deciding it seems off. A single number per
residual, on screen, converts that into "pod 7 started degrading at 14:32" — and it makes every fix
in Phase 3 measurable rather than asserted.

---

## 2. Opportunistic re-calibration

A full T-pose interrupts capture. But T-pose is not the only pose with known geometry, and you do
not need the whole body at once.

**Detect calibration opportunities as they occur naturally:**

| Opportunity | Detect by | What it re-calibrates |
|---|---|---|
| Quiet standing | all pods below a motion threshold for ≥1 s | gravity alignment (roll/pitch) for every pod; gyro bias via ZARU |
| Foot in stance | R1's stance detector | that leg chain's heading, relative to the other |
| Arms hanging at rest | forearm gravity vector near vertical, low motion | arm-chain mounting offset |

`[fact-data]` Phase 2 measured quiet intervals in every capture, so these fire regularly without
asking the subject to do anything.

**Per-pod, not whole-body.** If one pod's strap slips, only that pod needs re-estimating. Whole-body
re-calibration on a whole-body trigger is why calibration currently feels like an interruption.

---

## 3. Slow offset observer, with gating

Once decay is detectable, correct it continuously and slowly rather than in jumps.

```
On each accepted calibration opportunity for pod p:
    offset_measured = solve_offset(p)              # same estimator as the T-pose path
    residual        = angle(offset_measured, offset_current)

    if residual > REJECT_THRESH:        # e.g. 30° - the suit was bumped, not drifted
        flag_for_operator(p); skip      # do NOT silently absorb this
    else:
        offset_current ← slerp(offset_current, offset_measured, α)   # α small, e.g. 0.02
```

**The three gates that keep this safe:**

1. **Slew-rate limit.** Cap how fast the offset may move — a few degrees per minute. Real strap
   slippage is slow. Anything faster is either a bump or a bad estimate, and absorbing it would
   silently eat real motion.
2. **Reject-and-flag, don't absorb.** A large jump means the suit was physically disturbed. That
   needs a human, not a filter. **Silently absorbing it is the dangerous failure** — the skeleton
   keeps looking plausible while encoding a wrong body model.
3. **Freeze during fast motion.** Only update from low-motion windows, where the estimate is
   trustworthy.

**Why slow correction is the right shape:** a fast observer will absorb genuine motion and produce
exactly the class of silently-wrong data Phase 1 ranks above crashes. A slow one can only track the
slow process it is meant to track.

---

## 4. Persist per-pod calibration

`[fact-code]` `Pod_Watch_Binary.ino:64` already includes `<EEPROM.h>` and nothing uses it.
`[fact-code]` Phase 1 established there are **no per-sensor calibration constants anywhere** — fifteen
physically distinct chips are treated as identical (`EST-04`).

Two different things are worth storing, and only one belongs on the pod:

| Quantity | Changes | Store where |
|---|---|---|
| Gyro bias, accel zero-g offset, axis misalignment | per chip, stable for months | **EEPROM on the pod** — it is a property of that hardware |
| Mounting offset | every time the suit is put on | **browser, per session** — it is a property of this wearing |

Characterise the first once per unit — `[fact-doc]` the datasheet allows ±5 °/s gyro zero-rate offset
and ±50 mg accel offset part-to-part, and M8 (15 pods flat and co-planar for 10 minutes) measures the
real spread in one session. Storing it on the pod means a unit carries its own correction regardless
of which body segment it is strapped to next.

---

## 5. Make calibration state visible and resettable

Two operational gaps that cost more than they look:

**Show what is loaded.** There is currently no way to see which mounting offsets are active, when
they were computed, or how far each has moved since. A small panel — per pod: offset age, current
residual, drift since calibration — turns a mysterious bad session into a diagnosable one.

**Allow per-pod re-calibration.** Today the only recovery is a full restart
(`01_BROWSER_STATE_POISONING.md` §F4 makes the same point for bone state). If pod 7's strap slips at
minute 12, the operator should be able to re-T-pose that one arm and continue.

---

## 6. Recommended order

| # | Item | Cost | Why here |
|---|---|---|---|
| 1 | **R1 double-stance closure as a live metric** | ~40 lines | Needs no ground truth; makes every later fix measurable |
| 2 | **Per-pod calibration state display** | ~30 lines | Turns invisible decay into an observable |
| 3 | **Per-pod re-calibration control** | ~30 lines | Removes the restart-to-recover workflow |
| 4 | **Opportunistic quiet-standing detection** | ~40 lines | The trigger everything else needs |
| 5 | **Slow offset observer with the three gates** | ~60 lines | Only once 1–4 show decay is real and how fast |
| 6 | **Per-unit EEPROM characterisation** | ~50 lines + M8 | Independent of the rest; do when convenient |

**Do 1 and 2 first even if you implement nothing else.** Right now you cannot tell whether
calibration decay is a real contributor or a suspicion — and `../system_assessment_2/12_integration/PHASE1_SCORECARD.md`
records that this project's confident-but-unmeasured claims have a poor track record. Measure the
residual for one session before building the observer that corrects it.

## What would prove this unnecessary

If R1's closure residual is flat across a 30-minute session, mounting offsets are **not** decaying,
and items 4–6 can be dropped entirely — leaving heading drift
(`02_DRIFT_ALGORITHMS.md`) as the whole problem. That is a genuinely possible outcome and it is worth
knowing before spending effort here: **one instrumented session distinguishes the two.**
