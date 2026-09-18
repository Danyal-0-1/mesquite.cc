# GATES — wave gate declarations

A gate opens only when its factual condition is met. **No gate is open.**

## W0 — Provenance and build control — **CLOSED**

| Condition | State | Evidence |
|---|---|---|
| Deployed firmware confirmed identical to workspace | **NOT MET** | No hardware. `INT-01`'s risk to all four Phase 1 refutations stands. |
| Core/IDF version and `configTICK_RATE_HZ` printed and recorded (**U2**) | **NOT MET** | Banner written (`Pod_Watch_Binary.ino:485-510`, `Dongle_Binary.ino:285-300`); never executed. |
| Per-node build system exists | **MET** | `tools/build_pods.sh`; `#error` guard at `Pod_Watch_Binary.ino:51-56`. Not executed — no `arduino-cli`. |
| Raw serial replay fixture captured | **PARTLY MET** | Harness delivered and productive (`tools/replay_harness.js`, 23/23). **No raw capture from a real session exists** — the fixture currently runs on synthesised input only. |

**Declaration: W0 remains closed.** Two of four conditions unmet, both requiring hardware.
Additionally §4.1 cannot be executed at all — git is unusable (`../ROLLOUT.md`).

## W1 — Instrumentation — **CLOSED**

| Condition | State | Evidence |
|---|---|---|
| I1–I12 implemented behind `MESQ_INSTR` | **MET (code)** | Pod: I4, I8, I9, I11, N1, N2, N3. Hub: I1, I2, I3, I7, I11, H1, H2, H3. Browser: I5, I6, I12, W1b. |
| Each instrument's own cost measured, under the Rule 2 threshold | **NOT MET** | `mesq_instrCostUs` exists and has never been read. The 1 Hz `Serial.printf` cost is not even accumulated (`P2-B1-02`). |
| Instrumented build passes the single-node pilot | **NOT MET** | No hardware, and nothing has been compiled. |

**Declaration: W1 remains closed.** Rule 2 is explicitly unsatisfied.

**Deviation recorded:** I12 (export fix) and the W1b counter change what is written to file and add
a hook to `custom_icm.js`. §B3 authorises I12 explicitly ("land it anyway, and land it early"). W1b
is counting only — the guard's behaviour is unchanged.

## W2 — Baseline campaign — **NOT STARTED**
Requires W1. No sessions run. U1 and U3 unresolved.

## W3–W6 — **BLOCKED**
Rule 1: no fix may be written before its falsifying measurement has been run and recorded. Since W2
has produced no measurements, **no W3–W6 fix was written.** This is the intended behaviour of the
gate, not a shortfall.

**The two exceptions, both authorised:**
- `NODE-05` (per-node build) — §4.2 promotes it to a BLOCKER and assigns it to W0.
- `SYNC-07` via I12 — §B3 requires it before the baseline is recorded.

Everything else on the Tier-1 list — `SENS-02`'s two lines, `SENS-03`'s clamp, `HUB-04`'s one-liner,
`PHN-01`'s guard fix — is instrumented and deliberately **not fixed**.

## What would open W0 and W1

One pod and one dongle on USB, plus a working toolchain:
1. `sudo xcodebuild -license`, confirm `git status` — unblocks §4.1
2. `tools/build_pods.sh 1` and a dongle build, with `MESQ_INSTR` unset **and** set — closes `P2-B0-02`
3. Flash one pod, read the banner — **resolves U2**, confirms provenance
4. Read `instr_us/s` and add the 1 Hz print self-timing — satisfies **Rule 2**
5. Rule 3 pilot: one modified pod alongside ≥4 unmodified, 10 minutes, I1 confirms no regression
6. Capture a raw serial stream to `data/replay/` — completes the W0 fixture condition

Roughly half a day with hardware present. Steps 1–3 are the ones that unblock the most.
