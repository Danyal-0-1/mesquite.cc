# B1 — Node Instrumentation — Phase 2

## 1. Scope
Changed: `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino` — 814 → 996 lines. All instrumentation
inside `#if MESQ_INSTR` (default 0). **Nothing compiled, nothing run.**

## 2. Summary
All eight assigned instruments are implemented behind the compile switch, reporting once per second
from `TaskWifi` on core 0 so the sample task on core 1 is left alone. The Quat6-per-second counter
is the one that matters most: it settles `SENS-01`, the ~55 Hz DMP ceiling, which is the first
binding constraint in the entire system and currently rests only on a code comment. `I10` was
deliberately **not** implemented as specified — per §4.4 it would have tripled I²C traffic on the
sample task, so battery is read at the hub from the `batt` byte already on the wire instead.
Rule 2 is **not satisfied**: the self-cost accumulator exists but has never been read.

## 3. Implemented (all `#if MESQ_INSTR`)

| ID | What | Where | Resolves |
|---|---|---|---|
| **I9** | `esp_timer_get_time()` around `readDMPdataFromFIFO()`; min/mean/max µs; `FIFOMoreDataAvail` count | `:~760` | `SENS-01`, `SENS-02`, E2 |
| **I9** | **Quat6 packets/s** at the `DMP_header_bitmap_Quat6` branch | `:~790` | **`SENS-01` directly** |
| **I4** | Send-interval min/max + 6 buckets (`<20,20-29,30-39,40-49,50-59,60+` ms) | at `esp_now_send` | `NODE-03`, S2 |
| **I8** | `esp_reset_reason()`, `RTC_DATA_ATTR` boot counter, `INIT_RESULT: IMU_OK DMP_OK/FAIL` | boot + `setupIMU()` | `NODE-04`, S3/S4 |
| **I11** | Free heap in the 1 Hz line | `TaskWifi` | fragmentation |
| **N1** | Unit-norm check `x²+y²+z²+w²` outside `[0.999, 1.001]` before send | at send | `NODE-01` torn reads |
| **N2** | Negative-radicand counter; radicand hoisted into `_rad` | `:~795` | `SENS-03` |
| **N3** | Sample-to-send age (µs at Quat6 decode vs at send), mean/max ms | both tasks | `NODE-02`, `SYNC-02` |

Output, once per second on the pod's own USB serial (not the dongle's stream):
```
[INSTR] id=1 quat6/s=54 read_us(min/mean/max)=310/420/1180 fifoMore=12
        send(n=32 min=30 max=41 b=0/0/28/4/0/0) age_ms(mean/max)=7/23
        normBad=0 radNeg=0 heap=182340 boots=3 instr_us/s=410
```
The `b=` buckets are `NODE-03`'s test: a tight spike in one bucket means a clean cadence; mass split
across the 30-39 and 40-49 buckets is the tick quantisation Phase 1 predicted.

## 4. Findings

### P2-B1-01 — I10 as specified would have created the symptom it measures
- **Severity:** RISK (averted) · **Confidence:** high
- **Location:** `Pod_Watch_Binary.ino:~830` (existing 3 s battery timer); §4.4
- **Evidence:** `[fact-code]` `getBattery()` performs `adc1Enable()`, `isChargeing()` and
  `getBattPercentage()` — three AXP202 I²C transactions **on the same bus as the IMU** — and
  `handleBattDisplay()` adds a TFT redraw. `[fact-code]` this already runs on `TaskReadIMU`, the
  sample task. `PWR-01` identifies it as a periodic hitch.
- **Mechanism:** I10 asks for per-second battery logging. Moving 3 s → 1 s triples the I²C burden on
  the very task whose timing `I9` is measuring, on the very bus `§C8` is about. The instrument would
  have shifted the quantity being observed — precisely Rule 2's failure mode.
- **Resolution:** battery is instead decoded at the **hub** from the `batt` byte already in every
  packet (H3, `../02_hub_instrumentation/REPORT.md`). Zero pod-side cost, zero extra I²C, per-node
  battery once per second for free. The pod-side read stays at 3 s, untouched.
- **Falsifying measurement:** if hub-side `batt` proves too coarse (it is 0–100 integer percent, no
  voltage or current), a dedicated voltage/current read must go on a **separate task**, not the
  sample task, and its cost measured first.
- **Cost:** 0 on the pod. **Risk:** loses instantaneous voltage/current, so the C6 brownout theory is
  only partly testable — sag below one percentage point per second is invisible. Stated as a known
  limitation rather than papered over.

### P2-B1-02 — Rule 2 is not satisfied
- **Severity:** BLOCKER (for the W1 gate) · **Confidence:** high
- **Evidence:** `[fact-code]` `mesq_instrCostUs` accumulates the cost of the per-send instrumentation
  block and is emitted as `instr_us/s`. It has **never been read** — no hardware.
- **Mechanism:** Rule 2 requires an instrument perturbing its target by >5% to be treated as a bug.
  Two costs are unquantified: the per-send block (`instr_us/s` covers this) and the **1 Hz
  `Serial.printf` itself**, which is *not* covered — it runs on core 0 alongside the Wi-Fi stack and
  formats ~20 fields. At 115200 baud a ~200-character line is ~17 ms of transmission; if
  `Serial.printf` blocks when the TX buffer fills, that is a serious perturbation of the transmit
  task, on the order of half a send interval.
- **Falsifying measurement:** wrap the 1 Hz print itself in `esp_timer_get_time()` and report its
  duration in the *next* line. If it exceeds ~1 ms, buffer the line and emit it from a lower-priority
  task, or raise the pod's serial baud.
- **Proposed change:** add that self-timing before the first real session. **Cost:** ~5 lines.
- **Risk:** none; it is the measurement Rule 2 demands.

### P2-B1-03 — Instrumentation reports from core 0, by design
- **Severity:** QUALITY · **Confidence:** medium
- **Evidence:** `[fact-code]` the 1 Hz block sits in `TaskWifi` (core 0), not `TaskReadIMU` (core 1).
- **Mechanism:** Core 1 runs the DMP FIFO drain that `I9` times; adding a formatted serial print
  there would inflate the very number being measured. Core 0 already shares with the Wi-Fi stack, so
  the print competes with transmission instead — a real cost, but one that shows up in `I4`'s
  interval histogram where it is visible rather than hidden.
- **Falsifying measurement:** compare `I4` histograms with `MESQ_INSTR` on and off. A shift in the
  send-interval distribution is the instrument perturbing the measurement.
- **Cost:** 0. **Risk:** the trade is explicit and testable.

## 5. Not done, deliberately (Rule 1 / W3-W4)
`SENS-02`'s two-line disable of the unused raw DMP sensors, `SENS-03`'s radicand clamp,
`NODE-01`'s queue, `NODE-03`'s `vTaskDelayUntil`, `NODE-04`'s bounded retry. Each is instrumented,
none is fixed. Fixing before measuring would make the baseline a moving target.

## 6. Measurements I need
One pod on USB with `-DMESQ_INSTR=1`, ten minutes. That single run produces `SENS-01`'s Quat6 rate,
`NODE-03`'s interval histogram, `SENS-02`'s FIFO cost with and without the raw sensors, and the N1/N2
counters — four Phase 1 findings resolved in one session.
