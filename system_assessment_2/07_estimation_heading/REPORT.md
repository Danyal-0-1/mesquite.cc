# B7 — Estimation & Heading — Phase 2

## 1. Scope
**Entry condition not met.** §B7 states plainly: "Entry condition: **M4/E11 complete.** The fix
depends entirely on its outcome and must not be started before it." M4 requires hardware; none is
available. **No estimation code was written.**

What was done instead: an attempt to obtain U3's answer offline, from the existing paired captures,
so that B7 could start early. **It failed, for an instructive reason**, and that failure is this
report's content.

## 2. Summary
The offline surrogate for U3 does not work. Computing the per-frame optimal yaw between Mesquite and
Rokoko looked promising, but a lag-sensitivity sweep shows a ±1 second change in assumed time
alignment swings the estimate across 1,176° and reverses the drift slope's sign. **U3 cannot be
resolved from the existing captures at all**, and M4 remains mandatory. One result survives the
contamination and mildly weakens Phase 1's central accuracy claim: per-frame optimal yaw removes
only 24–32% of root-relative positional error, not the collapse `EST-02` predicts.

## 3. The attempt, and why it failed

Full data in `../MEASUREMENTS.md` D4. First pass gave apparently clean numbers — session 01 drift of
−2.80 °/s, session 02 of +0.14 °/s. Both are unusable:

- −2.80 °/s implies **−1679° over ten minutes**, i.e. 4.7 full revolutions. `[fact-doc]` the
  ICM-20948's 0.015 °/s/√Hz predicts ~0.37° over the same interval. No gyroscope behaves this way.
- The two sessions disagree by a factor of 20 **and in sign**.

The lag sweep explains it: mean optimal yaw runs +463° at lag −30, −713° at lag −20, −129° at lag −9,
−120° at lag 0. Two bodies rotating relative to one another produce an apparent yaw disagreement
proportional to turning rate whenever their streams are misaligned — and `SYNC-07`'s ~7% *progressive*
dilation means no single lag is correct for a whole session.

**This is `INT-04` destroying a new analysis, not just the old metrics.**

## 4. Findings

### P2-B7-01 — U3 is not resolvable offline; M4 is mandatory
- **Severity:** BLOCKER (for this agent) · **Confidence:** high
- **Evidence:** `[fact-data]` D4 lag sweep.
- **Mechanism:** Every offline route to U3 runs through the paired captures, and those are
  time-contaminated in a way that cannot be corrected after the fact — they carry no arrival
  timestamps, so the true frame interval is unrecoverable.
- **Why M4 is immune:** a single node, stationary, logged at full rate has **no second stream to
  align against**. Heading versus time is read directly from one device's own quaternion output.
  The contamination that defeats the offline approach does not exist in that experiment. **M4 is not
  merely the specified instrument; it is the only one that works.**
- **Falsifying measurement:** M4 itself.
- **Proposed change:** none. Run M4.
- **Cost:** one node, 10 minutes. **Risk:** none.

### P2-B7-02 — `EST-02`'s "heading dominates" is weakened, not refuted
- **Severity:** QUALITY · **Confidence:** medium
- **Evidence:** `[fact-data]` D4 — error reduction of 24%, 25%, 28%, 32% across all four alignments,
  i.e. **stable while everything else in the analysis is not.**
- **Mechanism:** A per-frame optimal yaw is the most generous heading correction available — free to
  differ every single frame. If whole-body heading dominated the residual after root removal, it
  should remove most of it. It removes roughly a quarter to a third.
- **The caveat that keeps this from being a refutation:** this metric is positional RMS in BVH units
  after root-centring and a single uniform scale, so it still carries `KIN-06`'s retargeting
  artifact, and it is **not** the joint-angle RMSE Phase 1 quoted (42° global vs 15° pose). The two
  are not directly comparable, and I am not claiming they are.
- **Falsifying measurement:** recompute in joint-angle terms with per-bone length normalisation on an
  I12-corrected capture — the clean form of `EST-02`'s own falsifying test.
- **Proposed change:** none. Recorded so `EST-02` is not treated as settled before M4 and re-capture.
- **Cost:** part of B10's comparison rebuild. **Risk:** none.

## 5. The decision table, unchanged and still pending

| M4 result | Diagnosis | Fix |
|---|---|---|
| Heading drifts tens of degrees over 10 min | random walk / residual bias | bias estimation, or a heading reference — hand to B8's ZUPT, which D3 shows is feasible |
| Heading stays within a few degrees | **uncorrected arbitrary initial heading** | per-session heading alignment at T-pose, jointly with `PHN-04` |

Phase 1's arithmetic still predicts the second row by two orders of magnitude. **That prediction is
now the only thing standing in for a measurement, and Phase 1's scorecard shows its magnitude
estimates are the part least worth trusting** (`../12_integration/PHASE1_SCORECARD.md`). Do not act
on it.

## 6. Not done, deliberately
No filter change of any kind. §B7's strongest instruction — do not replace the fusion filter — is
unaffected by anything found here: `EST-02` is weakened, but the articulated pose still measures far
better than the global placement, so a better filter would still address the smaller term.

## 7. Measurements I need
**M4.** One node, stationary, ten minutes, full-rate quaternion log. Nothing else in this agent's
scope can begin until it exists.
