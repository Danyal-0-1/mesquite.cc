# Phase 3 — Suggestions

**Suggestions only. No code was changed in this phase.**

Built on Phase 1 (`../system_assessment/`) and Phase 2 (`../system_assessment_2/`). Every claim is
tagged and cited; `[fact-data]`/`[fact-fixture]` results come from Phase 2's measured work.

| Document | Covers |
|---|---|
| **[01_BROWSER_STATE_POISONING.md](01_BROWSER_STATE_POISONING.md)** | The cache-clearing symptom — diagnosed, with the fix |
| **[02_DRIFT_ALGORITHMS.md](02_DRIFT_ALGORITHMS.md)** | Six drift-reduction algorithms, ranked |
| **[03_CALIBRATION_MAINTENANCE.md](03_CALIBRATION_MAINTENANCE.md)** | Keeping calibration valid over a long session |
| **[04_ARCHITECTURE_CLEANUP.md](04_ARCHITECTURE_CLEANUP.md)** | Structural changes worth making |

## Your cache-clearing bug: found it

Your instinct — "junk value accumulation" — is **half right**. It is not gradual build-up. It is
**one bad value permanently poisoning persistent state, with no recovery path.**

1. `[fact-code]` Firmware `sqrt(1 − q1²−q2²−q3²)` is unclamped → NaN → `q_to_i16` maps NaN to 0 → the
   packet carries a **zero quaternion** that looks completely valid on the wire.
2. `[fact-code]` The **legacy JSON path checks for this** (`_isBadNum`, with a comment explaining
   why). The **binary path — your current firmware — does not.** The guard did not move with the
   wire format.
3. `[fact-code]` It lands in `x.quaternion.slerp(...)` — a feedback loop on a *persistent* three.js
   bone, 17 call sites, **zero `normalize()` after any of them**. A zero quaternion shrinks the bone's
   state; a NaN propagates forever.
4. `[fact-code]` `initGlobalLocalLast()` is one-shot (`flag` set false at `:96`). **Nothing ever
   re-initialises bone state.** A page reload is the only reset — which is why clearing the cache
   appears to fix it. The cache is incidental; the reload is the cure.

It *looks* gradual because each pod has an independent chance of being poisoned, so bones fail one at
a time over a session. **That gradualness is a population effect, not accumulation.**

Contributing separately: `updateTrackingLine()` reallocates a **120 KB `Float32Array` every frame**
(~7 MB/s of garbage), which genuinely does degrade with runtime.

**Fix:** a norm check at the binary boundary (~10 lines) — it catches the zero quaternion *and* the
torn cross-core reads from `NODE-01`. Plus a clamp in firmware (1 line) and a "reset skeleton"
control so recovery does not require losing the session.

## Drift: what the evidence actually says

`[fact-doc]` The ICM-20948's angle random walk predicts **~0.37° over ten minutes**.
`[fact-data]` The observed heading error is **71–99°, and flips sign between sessions**.

**Random walk is two orders of magnitude too small.** So the dominant term is almost certainly an
**uncorrected arbitrary initial heading** — fifteen DMPs each waking at their own yaw origin, never
aligned to each other or to the world — not accumulated drift.

If that holds, the fix is small: **T-pose heading alignment (~20 lines) plus a heading-correction
layer (~50 lines)**, and most of the error goes away. Everything else becomes refinement.

**But it is unverified.** Phase 2 proved this cannot be answered from existing captures — a ±1 s
change in assumed time alignment swings the estimate across 1,176°. **Run M4 first:** one node,
stationary, ten minutes, log heading. Flat ⇒ initialisation. Ramp ⇒ bias. **That one session decides
which half of `02_DRIFT_ALGORITHMS.md` you need.**

## Ranked recommendations

| # | Change | Cost | Closes |
|---|---|---|---|
| 1 | **Norm check at the binary boundary** | ~10 lines | **your cache bug**, `NODE-01`, `SENS-03` |
| 2 | **Clamp the firmware radicand** | 1 line | `SENS-03` at source |
| 3 | **"Reset skeleton" control** | ~15 lines | removes restart-to-recover |
| 4 | **Run M4** (static drift session) | 1 node, 10 min | **U3** — gates all drift work |
| 5 | **T-pose heading alignment** | ~20 lines | the likely dominant error |
| 6 | **Heading-correction layer** (slew-limited, gated) | ~50 lines | where every drift fix plugs in |
| 7 | **Double-stance closure as a live metric** | ~40 lines | makes decay observable, no ground truth |
| 8 | **Stop the per-frame `Float32Array` realloc** | ~10 lines | ~7 MB/s of garbage |
| 9 | **Move per-packet DOM work off the packet** | ~30 lines | `WEB-01` |
| 10 | **Bound `NODE-04`, gate `HUB-03`** | ~15 lines | the two surviving latch failures (S3/S4) |

**Items 1–3 are about half an hour and would remove the operational pain you are hitting today.**
Item 4 gates everything after it.

## What not to do

- **Do not replace the fusion filter.** `[fact-data]` articulated pose is already 15–16° RMSE against
  a commercial suit while global error is 42–54°. A better attitude filter addresses the smaller term.
- **Do not enable the magnetometer naively.** Indoors, uncorrected distortion routinely exceeds the
  drift it fixes. Gate it (`02_DRIFT_ALGORITHMS.md` §D2) or leave it off.
- **Do not delete the "wasted" raw DMP streams yet** — ZARU needs that gyro data, and it is already
  crossing the bus.
- **Do not build the calibration observer before measuring decay.** If the closure residual is flat,
  items 4–6 of `03_CALIBRATION_MAINTENANCE.md` are unnecessary.
