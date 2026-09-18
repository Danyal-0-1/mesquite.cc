# B8 — Kinematics & Constraint Correction — Phase 2

## 1. Scope
Ran `tools/analysis_kin01.py` (new, with `tools/bvh_kin.py` — a BVH parser plus forward kinematics
honouring each file's declared channel order) over all five Mesquite captures. **No code fixed** —
items 2–6 are gated behind W2 and optical ground truth.

## 2. Summary
Item 1 was the gate on this agent's entire existence, and it passes: **stance phases are clearly
detectable** in every capture — 8 to 28 contiguous low-speed runs per session, up to 3.3 seconds
long. ZUPT is therefore available and `KIN-01` remains the highest-value accuracy direction. The
residual foot speed *during* those stance phases is 8–16 units/s rather than near zero, which is
both the evidence that a correction is needed and the size of the correction available. Everything
downstream of feasibility is deliberately not built, because validating it requires the optical
reference B10 does not yet have.

## 3. Item 1 — feasibility: **PASS**

Full table in `../MEASUREMENTS.md` D3. Stance = contiguous runs ≥5 frames (~165 ms) below the
session's own 25th-percentile foot speed.

Every capture yields stance-like structure; the longest single run is **3.30 s** (session
2026-6-8-13-22-52, LeftFoot). Totals run 5.5%–17.4% of session duration per foot.

**What makes this convincing rather than circular:** a relative threshold guarantees ~25% of *frames*
fall below it, but it does not guarantee they *cluster*. They do — into runs of 5 to 99 consecutive
frames. That temporal structure is the signature of real stance, not of threshold choice.

**What makes it incomplete:** during those runs the foot is still moving at 8–16 units/s. If
1 unit ≈ 1 cm (root Y ≈ 55 for a standing subject), a "planted" foot is drifting at 8–16 cm/s. Real
stance should be near zero. **That residual is the ZUPT's target and its expected yield.**

**An observation that should be resolved before building contact detection:** foot vertical
excursion is small — median foot height varies by roughly 1–4 units within a session, and in the
2026-5-20 captures the minimum and median differ by only 1.3 units. For normal walking one expects
several centimetres of foot lift. Either the subject was mostly standing and gesturing rather than
walking, or leg articulation is being under-estimated. **A contact detector tuned on data where the
feet barely lift will not generalise to walking**, so this needs answering first.

## 4. Findings

### P2-B8-01 — `KIN-01` is feasible; ZUPT is available
- **Severity:** n/a (feasibility result) · **Confidence:** high
- **Evidence:** `[fact-data]` D3.
- **Mechanism:** Fifteen sensors on a closed kinematic chain, and a foot in stance supplies a
  zero-velocity and zero-heading-rate observation. That is an absolute reference requiring **no
  magnetometer and no new hardware** — the only such reference available to this system, and Phase 1
  identified its absence as the dominant unaddressed accuracy gap.
- **Explains symptoms:** S6, by omission.
- **Falsifying measurement:** already run. The next question is not *whether* stance is detectable
  but whether a detector tuned on this data survives a walking session — see the foot-lift concern
  above.
- **Proposed change:** none yet. Building ZUPT requires optical ground truth to validate against;
  Rokoko cannot serve, since `[fact-data]` `../MEASUREMENTS.md` D5 shows the reference does not even
  map the arms, and §1.5 establishes it cannot validate heading.
- **Cost:** substantial (W5). **Risk:** false stance during a jump, or missed stance in fast gait,
  injects artifacts that look like real motion.

### P2-B8-02 — Foot vertical excursion is implausibly small
- **Severity:** RISK · **Confidence:** medium
- **Evidence:** `[fact-data]` D3 — foot height min vs median differs by 1.3 units in
  2026-5-20-9-11-30 and ~4 units in the 2026-6-8 sessions.
- **Mechanism:** Either the captures contain little locomotion, or the legs are under-articulating.
  These have opposite implications: the first means the data is unrepresentative for gait work, the
  second is a tracking defect. `[fact-data]` Phase 1 already noted a related anomaly in the opposite
  direction — spine ROM of 46.24° against the reference's 12.00° — so segment-level ROM disagreement
  is not new.
- **Falsifying measurement:** the W2 protocol's straight-line-walk block, scored for foot-lift
  amplitude against the optical reference. A walking session that still shows ~1–4 units of foot
  excursion is a tracking defect; one showing normal lift means the existing captures were simply
  not walking data.
- **Proposed change:** none until measured.
- **Cost:** one session block. **Risk:** none.

## 5. Not built, and why
Items 2–6 (foot-contact detection and ZUPT, segment-length root constraint, `KIN-03`'s
forward-recomposition assertion, `KIN-05`'s plausibility detector, `SYNC-08`'s held-frame marking)
are all gated. Items 2 and 3 need optical validation. `KIN-03` and `KIN-05` are cheap and offline,
and are the natural next pieces of work here — I stopped short of them because they are diagnostics
whose output is only interpretable against a trustworthy baseline, which I12 does not yet produce.

## 6. Measurements I need
The straight-line-walk block with optical reference (resolves P2-B8-02 and gives the ZUPT validation
target); and one I12-corrected capture so that any detector is tuned on data with a real time base.
