# MEASUREMENTS — the `[fact-measured]` registry

## A necessary tag correction, stated up front

Phase 2 defines **`[fact-measured]`** as "obtained from an instrumented run on **real hardware**".
**No hardware was available in this session** — no pod, no dongle, no serial device
(`ls /dev/cu.*` shows only `Bluetooth-Incoming-Port` and `debug-console`). **This file therefore
contains no `[fact-measured]` rows.** Claiming otherwise would defeat the purpose of the tag.

What it does contain is two weaker but real evidence classes, kept explicitly distinct:

- **`[fact-data]`** — measured from existing captured files. Phase 1's tag, unchanged.
- **`[fact-fixture]`** — **new.** Obtained by executing the real algorithm under the replay
  harness (`tools/replay_harness.js`), with no hardware. Stronger than `[inference]` because code
  actually ran and produced the result; weaker than `[fact-measured]` because the *inputs* were
  synthesised rather than captured from a live fleet. Every row cites the test id.

When hardware becomes available, the W2 campaign fills the `[fact-measured]` section, which is
currently empty by necessity rather than by omission.

---

## Section 1 — `[fact-data]`: measured from existing captures

Instrument: `tools/offline_analysis.py`. Inputs: the five Mesquite and two Rokoko BVH files under
`Mesquite_benchmarks/`. No hardware required; reproducible today.

### D1 — PHN-05 gimbal exposure — **CONFIRMED and quantified**

Phase 1 rated this RISK / medium confidence from a **single frame**. Measured across all captures:

| Capture | Root order | Frames within 10° of ±90° | % | X/Z discontinuities there |
|---|---|---|---|---|
| MMcap 2026-5-20-9-11-30 | XYZ | 157 / 657 | **23.9%** | 0 |
| MMcap 2026-5-20-9-12-22 | XYZ | 0 / 1077 | 0.0% | 0 |
| MMcap 2026-5-20-9-12-53 | XYZ | 23 / 728 | 3.2% | **4** |
| MMcap 2026-6-8-13-20-59 | XYZ | 166 / 2508 | 6.6% | **11** |
| MMcap 2026-6-8-13-22-52 | XYZ | 275 / 2172 | **12.7%** | 2 |
| Rokoko MesBench1_2 | **YXZ** | 0 / 2563 | **0.0%** | 0 |
| Rokoko MesBench1_3 | **YXZ** | 0 / 2356 | **0.0%** | 0 |

**The mechanism is now explained, not just observed.** In an Euler triple the *middle* axis is the
gimbal-critical one. Mesquite's `XYZ` order puts **Y (yaw)** in that slot — and yaw is the one axis a
hip-worn root sweeps through freely. Rokoko's `YXZ` puts **X (pitch)** there, which for an upright
human stays near zero. That is why the reference scores 0.0% in both sessions and Mesquite reaches
23.9%. This is a design consequence of the rotation order, not bad luck.

**Resolves:** `PHN-05` upgraded from RISK/medium → **RISK/high, quantified**.
**Raw data:** re-runnable via `python3 tools/offline_analysis.py`.

### D2 — PHN-03 relocalization jumps — **REFUTED for these captures**

Phase 1 suspected SLAM relocalization jumps propagating into the root, citing `root_error_max`
109.60 against a mean of 38.41 as "suggestive".

Measured root displacement per frame:

| Capture | median | p99 | max | implied max speed |
|---|---|---|---|---|
| MMcap 2026-6-8-13-20-59 | 0.304 | 1.800 | 2.941 | 88 units/s |
| MMcap 2026-6-8-13-22-52 | 0.249 | 1.450 | 2.652 | 80 units/s |
| MMcap 2026-5-20-9-11-30 | 0.622 | 2.027 | 6.656 | 200 units/s |
| Rokoko MesBench1_2 | 0.642 | 2.953 | **160.049** | 4802 units/s |
| Rokoko MesBench1_3 | 0.413 | 2.499 | **144.297** | 4329 units/s |

**No teleport signature exists in any Mesquite capture.** With root Y ≈ 55 units for a standing
human, ~1 unit ≈ 1 cm, so Mesquite's worst frame-to-frame motion is ~0.8–2.0 m/s — fast, but
physically achievable.

The two enormous jumps are in the **reference**, and both are the **same single artifact**: frame 0
sits at a rest pose `(0.00, 95.90, 2.28)` and frame 1 jumps to the real capture position. One
occurrence per file, no consecutive pairs. `[fact-data]` `metrics_all.json` records
`trim_start_frames: 5`, so the comparison already discards it and the benchmark is **not**
contaminated by this.

**Resolves:** `PHN-03` → **refuted for existing captures** (remains a theoretical risk; a
purpose-recorded session with deliberate occlusion would test it properly).

### D3 — KIN-01 stance-phase feasibility — **FEASIBLE**

B8's scope depends entirely on this: if stance phases are undetectable, ZUPT is unavailable.
Instrument: `tools/analysis_kin01.py` (forward kinematics via `tools/bvh_kin.py`, honouring each
file's declared channel order).

Stance defined as contiguous runs ≥5 frames (~165 ms) with foot speed below the session's own 25th
percentile:

| Capture | LeftFoot runs / % of session | RightFoot runs / % | Longest run |
|---|---|---|---|
| 2026-5-20-9-11-30 | 11 / 13.7% | 9 / 16.0% | 0.93 s |
| 2026-5-20-9-12-22 | 9 / 5.5% | 17 / 11.5% | 0.57 s |
| 2026-5-20-9-12-53 | 9 / 7.8% | 8 / 7.2% | 0.33 s |
| 2026-6-8-13-20-59 | 28 / 13.6% | 25 / 11.4% | 2.13 s |
| 2026-6-8-13-22-52 | 19 / 17.4% | 26 / 14.0% | **3.30 s** |

**Verdict: stance phases are detectable in every capture.** Clear temporal structure — contiguous
low-speed runs up to 3.3 s — not just noise below a threshold. **`KIN-01` is feasible and B8's scope
survives.**

Two honest caveats:
1. The threshold is *relative* (session 25th percentile), so ~25% of frames fall below it by
   construction. What is meaningful is that they cluster into contiguous runs, not that they exist.
2. **Residual foot speed during "stance" is 8–16 units/s** (~8–16 cm/s if 1 unit ≈ 1 cm). A truly
   planted foot should be near zero. That residual is precisely what a ZUPT would correct — so it is
   an argument *for* the fix, and it also sets the expected improvement.

Incidental observation: foot vertical excursion is small (median height varies ~1–4 units within a
session). Either the subject was mostly standing and gesturing rather than walking, or leg
articulation is under-estimated. Worth resolving before designing contact detection.

### D4 — EST-01/EST-02 offline yaw surrogate — **INCONCLUSIVE, and instructively so**

Instrument: `tools/analysis_yaw.py`. Computes the per-frame optimal yaw aligning Mesquite's
root-centred pose to Rokoko's, over the 14 mapped joints.

First pass looked like a result:

| Session | mean yaw | sd | drift slope | implied over 10 min |
|---|---|---|---|---|
| 01 | −129.10° | 116.58 | −2.7984 °/s | **−1679°** |
| 02 | +85.30° | 60.74 | +0.1362 °/s | +82° |

**Both figures are unusable.** −1679° is 4.7 revolutions in ten minutes — no gyroscope drifts like
that. The two sessions disagree by a factor of 20 and in sign.

A lag-sensitivity sweep confirms why (session 01; only the four distinct offsets are shown — a
harness limitation meant positive lags all applied zero offset and are omitted):

| lag (frames) | mean yaw | sd | slope °/s | error reduction |
|---|---|---|---|---|
| −30 | **+463.29°** | 245.66 | **+8.06** | 24% |
| −20 | **−713.03°** | 324.06 | **−10.82** | 25% |
| −9 | −129.10° | 116.58 | −2.80 | 28% |
| 0 | −120.34° | 102.31 | −2.26 | 32% |

**A ±1 second change in assumed alignment swings the mean yaw estimate across 1,176° and flips the
slope's sign.** The analysis is dominated by time-alignment error, which is `INT-04` — the very
contamination it was meant to work around.

**Conclusions:**
1. **This does not resolve U3.** M4 (one node, static, 10 minutes) remains necessary, and is now
   better justified: a static single-node session has *no* time-alignment problem, which is exactly
   why it is the right instrument.
2. **`INT-04` contaminates new analyses, not just the old metrics.** Any future work on these
   captures must assume alignment is unreliable until re-capture with I12.
3. **One robust result survives:** error reduction from per-frame optimal yaw is **24–32% across
   every alignment tested**. Even a maximally generous per-frame yaw correction removes only about a
   quarter to a third of root-relative positional error. If heading dominated the residual after
   root removal, that number should be far higher. This **weakens** — it does not refute —
   `EST-02`'s "whole-body heading dominates" claim, with the caveat that this metric is positional
   RMS in BVH units (still subject to `KIN-06`'s retargeting artifact), not the joint-angle RMSE
   Phase 1 quoted.

### D5 — Joint mapping covers no arms

`[fact-data]` The 14 mapped joints are pelvis, 3 spine, neck, head, and 8 leg joints.
**Not one arm joint is mapped** — no shoulder, arm, forearm or hand appears in
`per_joint_errors.csv`, despite both skeletons having them. `mapped_joint_count: 14` of 22.

The arms carry the fastest and largest-amplitude motion in most capture sessions, and they are the
segments most exposed to `SYNC-05`'s timing-jitter error (which scales with angular velocity). **The
existing benchmark is silent about the half of the body most likely to show error.** This is a
methodology gap for B10, not a system defect.

---

## Section 2 — `[fact-fixture]`: executed under the replay harness

Instrument: `tools/replay_harness.js` (§4.5). 23 assertions, all passing.
Reproduce: `node tools/replay_harness.js`

### F1 — WEB-02 — **RE-CHARACTERISED. Phase 1 overstated the severity.**

Phase 1: "an unterminated JSON line leaves `_jsonLine` non-empty forever, permanently halting all
binary parsing" — **BLOCKER, confidence high**, and a headline explanation for S3/S4.

Measured (test T6):

| Condition | Result |
|---|---|
| (a) Latch injected, then 50 ordinary packets sent | **49 decoded, 1 lost.** The latch cleared almost immediately. |
| (b) Latch injected, then 50 packets whose payload bytes avoid `0x0A` | **0 decoded of 50.** `_jsonLine` grew to 823 chars. Permanent latch **reproduced**. |
| (c) Expected latch duration, random payload bytes | **mean 16.3 packets** ≈ 511 ms for one pod at 32 Hz; ≈ **34 ms** across a 15-pod fleet |

**The mechanism Phase 1 missed:** a stuck JSON line is terminated by *any* `0x0A` byte, and `0x0A`
occurs freely inside binary pod payloads — in `count`, in `ms_lo`, in quaternion bytes. With 16
payload bytes the per-packet chance is ~6%, so ordinary traffic self-heals the latch in tens of
milliseconds.

**Consequences:**
1. `WEB-02` severity **BLOCKER → CORRECTNESS**. It causes bounded, repeated packet loss, not a
   permanent halt.
2. **`WEB-02` can no longer carry S3** ("only ~4 nodes connect"). A failure that clears in ~34 ms
   cannot produce a session-long shortfall. Phase 1's `INT-03` attributed S3 to three latching
   failures; **only two of them actually latch** — `NODE-04` (init hang) and `HUB-03` (fleet reboot).
3. The fix (cap `_jsonLine`) is still correct and still ~10 lines, but it is now a loss-reduction
   fix, not a hang fix — and it should be prioritised accordingly.

### F2 — HUB-02 — **CONFIRMED by fault injection**

Test T7: a 16-byte pod packet written into the middle of a JSON line.

**Result: `binaryFrames = 0`, `jsonLines = 0`.** Both payloads destroyed by one interleave — the pod
packet is swallowed as JSON text, and the JSON line is corrupted by the binary bytes. Phase 1
predicted the first; the second is new.

**Resolves:** `HUB-02` CORRECTNESS/high — **confirmed**, and its blast radius is larger than stated.

### F3 — Parser chunk-straddling — **CONFIRMED correct**

Test T2: 50 packets fed in 7-byte chunks, so every frame straddles boundaries.
**Result: 50/50 decoded, 0 lost.** Phase 1's disproof of the chunk-loss hypothesis (`_rxBuf`
persistence at `js/webserialnative.js:97-99`) is verified by execution.

### F4 — I5 sequence-gap arithmetic — **VERIFIED**

| Test | Scenario | Result |
|---|---|---|
| T1 | 100 consecutive packets | 100 received, 0 lost |
| T3 | 4 packets dropped (ids 10,11,12,40) | 96 received, **4 lost — exact** |
| T4 | Counter wraps 65535 → 0 | 12 received, **0 lost** (wrap handled) |
| T5 | Pod reboot, counter resets 519 → 0 | 40 received, **0 lost, 1 resync** |

T5 is the important one: without the `RESYNC_THRESHOLD` guard a single pod reboot would report
**65,535 phantom lost packets** and destroy the loss statistic. Phase 2 §B3 flagged this; it is
implemented (`js/mesq_instr.js:26`) and verified.

### F5 — Framed hub status (0xFE) — **VERIFIED non-disruptive**

Test T9. The new `[0xAA][0x55][0xFE][len][payload]` status frame is decoded without disturbing pod
framing, **including when split across chunk boundaries** (15/15 and 9/9 packets decoded, 0 lost).

This was a live hazard, not a formality: without an explicit branch the existing 16-byte reader
would consume a status frame as a pod packet and mis-frame everything after it.

### F6 — False-sync impossibility — **CONFIRMED**

Test T8. `0xAA` cannot appear in ASCII, so `0xAA 0x55` cannot occur inside a JSON line. The sync
marker is safe against the phone's text payload.

---

## Section 3 — `[fact-measured]`: **EMPTY — requires hardware**

Every row below is specified, implemented, and unrunnable here. The instrumentation code exists and
compiles-by-inspection; none of it has executed on a device.

| Instrument | Resolves | Status |
|---|---|---|
| Provenance banner (`TICK_RATE_HZ`, core/IDF, reset reason) | **U2** | code written, **never run** |
| I9 read duration + Quat6/s | **U1-adjacent, SENS-01, SENS-02** | code written, never run |
| I4 send-interval histogram | `NODE-03`, S2 | code written, never run |
| I8 boot count + init result | `NODE-04`, S3/S4 | code written, never run |
| N1 unit-norm violations | `NODE-01` | code written, never run |
| N2 negative radicand | `SENS-03` | code written, never run |
| N3 sample-to-send age | `NODE-02`, `SYNC-02` | code written, never run |
| I1 per-id rx + drop reasons | `HUB-05` — **the key measurement** | code written, never run |
| I2 per-id RSSI | `NET-05`, C7 | code written, never run |
| I3 `Serial.write` duration (max) | `HUB-02` blocking question | code written, never run |
| I7 softAP station count | `HUB-01`, `NET-06` | code written, never run |
| I11 heap + `ws.count()` | `HUB-04` | code written, never run |
| H1/H2/H3 reset calls, peer fails, battery | `HUB-03`, `HUB-07`, `PWR-01` | code written, never run |
| **Rule 2: instrumentation self-cost** | gates W1 | **NOT SATISFIED** — `mesq_instrCostUs` is emitted per second but has never been read |

**U1 (ESP-NOW PHY rate)** and **U3 (drift vs initialisation)** are untouched. Both need hardware.
U1 additionally needs either a monitor-mode capture or a fleet reflash.
