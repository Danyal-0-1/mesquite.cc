# A9 — Integration Referee & Evaluation Lead — Assessment

## 1. Scope and files read

All eight subsystem reports, reconciled. Independently verified the load-bearing citations in each
rather than accepting them. Primary sources re-read: both current `.ino` files in full, the WebSerial
parser in full, the BVH converter in full, and the benchmark corpus.

Sample data inventory read directly: `Mesquite_benchmarks/` — `metrics_all.json`, both sessions'
`per_joint_errors.csv`, all five exported `.bvh` headers and root channels, and
`scripts/compare_bvh_suits.py` (parser and rotation composition, `:218-345`).

Companion documents: `FINDINGS.md`, `INSTRUMENTATION.md`, `OPEN_QUESTIONS.md`,
`CONTRADICTIONS.md`, `RATES_TABLE.md`, `BOTTLENECK_VERDICT.md`, `EVALUATION_DESIGN.md`.

## 2. Summary

Four of the master prompt's central hypotheses are refuted by code, and the refutations are more
useful than confirmations would have been: the transport is ESP-NOW so `max_connection` cannot gate
pods, the DMP is already enabled, sequence numbers and node timestamps already exist, and
sensor-to-segment calibration is implemented. The system's real ceiling is the DMP's ~55 Hz output
rate, which makes 60 Hz unreachable regardless of any network work. The intermittency in S3/S4 is not
a throughput phenomenon at all but three independent *latching* failures, each requiring a power cycle
or page reload to clear. The accuracy problem is whole-body rather than per-limb: global joint RMSE is
flat at 42–54° across every joint including the root, while root-relative pose RMSE is only 15–16°.

## 3. How the system actually works

Corrected against the master prompt's §3 diagram, which is wrong in two structural ways.

```
15-17 x T-Watch 2019 node              ESP32-S3 dongle            browser (mesquite.cc)
┌──────────────────────────┐        ┌──────────────────┐      ┌─────────────────────┐
│ ESP32-D0WDQ6, 2 cores    │        │ WIFI_AP_STA      │      │ WebSerial parser    │
│  core 1: TaskReadIMU     │        │ ch 1 pinned      │      │  dual-mode:         │
│    ICM-20948 I2C 400kHz  │        │                  │      │  0xAA55 -> binary   │
│    pins 21/22 (shared)   │        │ OnDataRecv:      │      │  '{'    -> JSON     │
│    DMP Quat6 ~55 Hz      │        │  validate 16 B   │      ├─────────────────────┤
│    + RAW gyro/accel      │        │  Serial.write ───┼──┐   │ skeleton compose    │
│      (enabled, UNUSED)   │        │                  │  │   │ T-pose + mounting   │
│  core 0: TaskWifi        │        │ WS /ws :80 ──────┼──┤   │ offsets applied     │
│    fps=32 -> 31 ms gate  │        │  Serial.println  │  │   │ BVH export @ 1/30   │
│    16-byte packet        │        │  (phone JSON)    │  │   └─────────────────────┘
│    ms_lo @ TRANSMIT      │        │                  │  │             ▲
│  display ON, touch poll  │        │ loop(): ANY byte │  └─ USB CDC ───┘
└────────────┬─────────────┘        │   -> reboot ALL  │     921600 (advisory)
             │                      └──────────────────┘
             └─ ESP-NOW UNICAST to hardcoded MAC DC:DA:0C:17:10:A0, ch 1, PS off, TX max
                                            ▲
   Android phone ── WiFi ── softAP ── WebSocket ┘   (Hips: px/py/pz + orientation fallback)
```

Structural corrections `[fact-code]`:
- **Node → hub is ESP-NOW unicast**, not Wi-Fi association. Pods never call `WiFi.begin()`.
- **Hub → browser is USB CDC serial**, not WebSocket. The WebSocket carries the *phone*, and its
  payload is `Serial.println`'d into the same stream as the binary IMU packets — the root of the
  framing hazard in `HUB-02`/`WEB-02`/`PHN-02`.
- **`Hips` is the phone; `HipsAlt` is the hip IMU pod.** The phone supplies position always and
  orientation only as a fallback after a 1 s timeout (`js/custom_icm.js:581,602-603`).
- **17 bone ids, not 15** (`NUM_PODS 17`), including Left/Right Shoulder at 15–16.
- Per-node identity is a compile-time constant selected by comment-toggling — every node needs its
  own hand-edited build (`NODE-05`).

## 4. Findings

Cross-cutting only; subsystem findings are in `FINDINGS.md`.

### INT-01 — Four master-prompt hypotheses refuted; the diagnosis was aimed at the wrong subsystems
- **Severity:** QUALITY
- **Confidence:** high
- **Location:** see `CONTRADICTIONS.md` C-02, C-03, C-08
- **Evidence:** `[fact-code]` §4.1's `max_connection` cannot gate pods (`HUB-01`); §2.2's "DMP may be
  sitting unused" is false (`SENS-01`); §C2's "no sequence numbers, no node timestamps" is false —
  both are on the wire (`SYNC-03`); §A6's "calibration probably ignored" is false (`KIN-02`).
- **Explains symptoms:** none — it redirects effort
- **Mechanism:** Each refutation redirects work. §4.1 was designated "cheap and first" and would have
  consumed the first day for nothing. Meanwhile the actual S3/S4 mechanisms — three latching failures
  — were not in the hypothesis space at all, and the actual 60 Hz ceiling (the DMP) was assumed to be
  an opportunity rather than a limit.
- **Falsifying measurement:** Each refutation carries its own in the owning report; collectively,
  I1 + I8 confirm or deny the latching-failure account of S3/S4.
- **Proposed change:** Retire E1. Re-scope E2 (see `BOTTLENECK_VERDICT.md`). Treat E3, E4, E8, E12 as
  answered.
- **Cost:** 0 — it saves work.
- **Risk of the fix:** Refutations rest on code reading; if the deployed firmware differs from this
  workspace, they weaken. Owner question 10 covers this.

### INT-02 — The system cannot measure itself, which is why every symptom is an anecdote
- **Severity:** BLOCKER
- **Confidence:** high
- **Location:** `Device code/Dongle_Binary/Dongle_Binary.ino:186-195`; `js/custom_icm.js:520,535`
- **Evidence:** `[fact-code]` the hub's two packet-rejection paths are bare `return;` with no counter.
  `[fact-code]` the browser parses `count` and uses it only to render a cumulative chip — no gap
  arithmetic anywhere. `[fact-code]` `js/bvh_converter.js:160-161` writes no wall-clock into exports.
- **Explains symptoms:** **masks S1–S5**
- **Mechanism:** Everything needed to quantify loss is already present — a per-node sequence counter
  on the wire, decoded in the browser — and is discarded. Consequently "frame rate degrades" cannot be
  distinguished from "frame rate is fine and half the packets are lost", which are different problems
  with different fixes. This is why the project's entire performance picture is one aggregate number.
  **The gap between what the system already knows and what it reports is about ten lines of
  arithmetic.**
- **Falsifying measurement:** n/a — an absence, confirmed by reading. The remedy is I5 + I1.
- **Proposed change:** I5 (~10 lines, browser only) and I1 (~20 lines, hub). Everything else waits
  behind these.
- **Cost:** ~30 lines total.
- **Risk of the fix:** I1's output must not go to the binary serial stream (`HUB-02`).

### INT-03 — Three latching failures, not one throughput curve, explain S3 and S4
- **Severity:** BLOCKER
- **Confidence:** medium
- **Location:** `Pod_Watch_Binary.ino:305-318,377-384`; `Dongle_Binary.ino:333-339`;
  `js/webserialnative.js:212,234`
- **Evidence:** `[fact-code]` a pod failing IMU or DMP init executes `while (1) ;` before any task is
  created. `[fact-code]` `if (Serial.available() > 0) { Serial.readString(); sendReset(); }` — bytes
  discarded unread, then every pod is rebooted. `[fact-code]` the browser's binary branch is gated on
  `_jsonLine.length === 0`, and `_jsonLine` clears only on a newline.
- **Explains symptoms:** **S3, S4**
- **Mechanism:** All three are **sticky**: a hung pod needs a power cycle, a latched parser needs a
  page reload. That is the property S4 demands — same code, different outcome — and a gradual
  saturation curve cannot produce it. They also compose: one stray serial byte reboots all pods, and
  an arbitrary subset then hangs in init, so a single event permanently reduces the fleet.
- **Falsifying measurement:** I1 + I8 together. Pods absent at the hub are init hangs; a simultaneous
  fleet-wide dropout is a reset; pods present at the hub but absent in the browser is the parser
  latch. Ten cold boots, counting how many of 15 report init success, bounds the first directly.
- **Proposed change:** After measurement — bound the init retry, require a command token before
  `sendReset()`, cap `_jsonLine`. Roughly 25 lines across three files.
- **Cost:** ~25 lines.
- **Risk of the fix:** Each is small and local; the parser cap needs a bound above real phone messages.

### INT-04 — The evidence base is contaminated in two independent ways
- **Severity:** BLOCKER (for publication, not for operation)
- **Confidence:** high
- **Location:** `js/bvh_converter.js:157`; `Mesquite_benchmarks/outputs/bvh_verification/`
- **Evidence:** `[fact-code]` `frameTime = 1/30` hardcoded over ~32 Hz data. `[fact-data]`
  `latency_ms` −300.0 vs −2200.0 at correlations 0.593/0.608. `[fact-data]` pelvis bone length 55.53
  vs 95.92 with a single 1.1023 scale, 14 of 22 joints mapped.
- **Explains symptoms:** none — it undermines the measurement of all of them
- **Mechanism:** A ~7% progressive time dilation prevents cross-correlation from locking, and
  retargeting artifact contaminates positional magnitudes. Existing captures **cannot be repaired**
  because they carry no arrival timestamps. The error *pattern* survives both contaminants; the
  *magnitudes* do not.
- **Falsifying measurement:** Wall-clock elapsed ÷ frame count during a recording. A measured 33.33 ms
  falsifies the dilation.
- **Proposed change:** Fix the export (I12) before recording anything intended for publication.
- **Cost:** ~10 lines.
- **Risk of the fix:** Invalidates previously reported numbers — which is the point.

### INT-05 — Accuracy effort is aimed at the smaller term
- **Severity:** CORRECTNESS
- **Confidence:** high
- **Location:** evidence in `Mesquite_benchmarks/outputs/bvh_verification/*/per_joint_errors.csv`
- **Evidence:** `[fact-data]` global RMSE by joint: pelvis 44.24, spine_lower 45.31, head 53.93,
  right_toe 45.46, left_toe 46.78 — flat, with the root as bad as the extremities. `[fact-data]`
  `pose_joint_rmse_mean` 14.98/16.02 vs `global_joint_rmse_mean` 42.07/42.78. `[fact-data]`
  `yaw_alignment_degrees` −71.19 and +99.16.
- **Explains symptoms:** **S6**
- **Mechanism:** Removing the root cuts error by roughly two-thirds. The articulated skeleton is
  performing at 15–16° RMSE against a commercial reference; the system simply does not know which way
  the body faces. A better fusion filter addresses the 15° term at best and leaves the 42° term
  untouched. `[fact-doc]` gyro angle random walk at 0.015 °/s/√Hz predicts only ~0.37° over 10
  minutes — two orders of magnitude short of the observed 71–99°, so the mechanism is an uncorrected
  arbitrary initial heading, not drift.
- **Falsifying measurement:** **E11** — one node, stationary, 10 minutes. Tens of degrees of drift
  implicates random walk; a couple of degrees implicates initialisation. These need different fixes.
- **Proposed change:** None until E11. **Do not replace the fusion filter** (§C4, confirmed by
  measurement rather than asserted).
- **Cost:** measurement only.
- **Risk of the fix:** n/a.

## 5. Sample data inventory

| Item | Contents | Usable as |
|---|---|---|
| `data/mesquite_smooth/may8th2026/` | 2 Mesquite BVH, 2508 and 2172 frames | `[fact-data]` with the INT-04 caveat |
| `data/rokoko/may8th2026/` | 2 Rokoko BVH, 2563 and 2356 frames | reference — **inertial, not optical** |
| `Mocab may 20th/` | 3 Mesquite BVH, 657–1077 frames | unpaired; no reference |
| `outputs/bvh_verification/` | `metrics_all.json`, per-joint CSVs, time series, 12 SVG plots per session | `[fact-data]`, magnitudes caveated |
| `scripts/compare_bvh_suits.py` | 1522 lines; correctly honours declared channel order (`:321-331`) | sound on rotation convention |
| `pod_watch__1_/` | older firmware snapshot + stale webapp copy | historical only |

**What is missing to test the current hypotheses:**
- any per-node packet-rate or loss record (`I1`, `I5`) — **nothing in the corpus measures this**
- RSSI (`I2`) — makes §C7 untestable
- arrival timestamps — makes `SYNC-05`/`SYNC-07` unquantifiable retrospectively
- wall-clock in exports (`I12`) — blocks the field-log join in §13
- any static-hold segment — blocks `EST-01`/E11
- battery/current logs (`I10`) — blocks the §C6 brownout theory
- an OptiTrack pairing — the only reference present is another inertial suit

## 6. Symptom attribution

See `FINDINGS.md` "Symptom coverage". Every symptom S1–S6 has at least one candidate explanation;
none is unexplained. S3 and S4 have three, which is appropriate — they are the same phenomenon seen
from different starting conditions.

## 7. What I could not determine

Consolidated in `OPEN_QUESTIONS.md`. The three that block the most downstream work:
1. **ESP-NOW PHY rate** — swings channel utilisation between 12% and 57% at the current operating
   point, and decides whether the radio is a bottleneck at all.
2. **FreeRTOS tick rate** — decides whether the real transmit rate is ~32 Hz or 25 Hz, and underpins
   `NODE-03`'s account of S2.
3. **Whether heading error is drift or initialisation** (E11) — decides which of two very different
   accuracy fixes is correct.

## 8. Measurements I need

`I5` and `I1` first, together, before anything else. See `INSTRUMENTATION.md` for all of I1–I12 and
the re-scoped experiment table.
