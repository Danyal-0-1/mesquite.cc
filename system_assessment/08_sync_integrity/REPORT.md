# A8 — Time Synchronization & Data Integrity — Assessment

## 1. Scope and files read

Cross-cutting by design. Read: `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino` (timestamp
creation and transmit gating), `Device code/Dongle_Binary/Dongle_Binary.ino` (arrival handling,
liveness), `js/webserialnative.js` (unpacking, in full), `js/custom_icm.js` (timestamp consumption),
`js/bvh_converter.js` (frame timing, in full), and as evidence the headers and root channels of the
five exported captures under `Mesquite_benchmarks/`, plus `metrics_all.json` and
`Mesquite_benchmarks/scripts/compare_bvh_suits.py` (alignment logic).

## 2. Summary

The system's timestamping is worse than "absent" — it is **present, plausible-looking, and wrong**,
which is the harder failure to notice. `ms_lo` is stamped at transmit time rather than sample time, it
is the low 16 bits of an unwrapped counter that rolls every 65.536 s, each node's `millis()` starts at
its own boot so the fifteen values are **not mutually comparable even in principle**, and the browser
misinterprets the field as an age in milliseconds. Sequence numbers exist and are parsed but never
checked for gaps, so packet loss is invisible. Every export then asserts a uniform 1/30 s frame
interval over data sampled at ~32 Hz, imposing a ~7% progressive time dilation — and the benchmark's
own output shows the consequence: latency estimates of −300 ms and −2200 ms for two sessions of the
same activity, at correlations of 0.59. **Arrival-time jitter of ±10 ms converts to roughly 3–6° of
joint error at normal limb speeds**, comparable to the entire residual pose error budget.

## 3. Timestamp provenance — full table

| # | Timestamp | Created where | Clock domain | Resolution | Consumed by |
|---|---|---|---|---|---|
| 1 | DMP sample time | ICM-20948 internal | DMP oscillator, per chip | ~18 ms (55 Hz) | **Nobody — never read** |
| 2 | `millis()` at sample | not taken | — | — | — |
| 3 | `ms_lo` | `TaskWifi` at **transmit** | node `millis()`, per node from its own boot | 1 ms, **wraps at 65.536 s** | Browser, **misinterpreted** |
| 4 | `count` | `TaskWifi` at transmit | per-node counter, wraps at 65536 | 1 sample | Parsed, displayed, **never gap-checked** |
| 5 | `podLastSeen[id]` | hub `OnDataRecv` | hub `millis()` | 1 ms | Liveness timeout only |
| 6 | Browser arrival | implicit, at parse | browser wall clock | — | **The de facto frame clock** |
| 7 | BVH `Frame Time` | export | **none — a constant** | fixed 1/30 s | Every downstream consumer |
| 8 | Phone pose time | phone (source unavailable) | phone clock | `[unknown]` | Not forwarded |

`[fact-code]` citations: (3) `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:647`; (4) `:648`;
(5) `Device code/Dongle_Binary/Dongle_Binary.ino:205-206`; (6) `js/webserialnative.js:88-93`;
(7) `js/bvh_converter.js:157`.

**Eight timestamps, five clock domains, and not one of them survives to the exported file.** The BVH
frame index is the only surviving time reference and it is synthetic.

## 4. Findings

### SYNC-01 — Node timestamps exist but are not mutually comparable
- **Severity:** BLOCKER
- **Confidence:** high
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:647`
- **Evidence:** `[fact-code]` `myData.ms_lo = (uint16_t)millis();`. `[fact-doc]` Arduino `millis()`
  returns milliseconds since **that device's** boot. `[fact-code]` no epoch exchange, no sync packet,
  and no offset estimation exists anywhere in firmware or webapp.
- **Explains symptoms:** none of S1–S6 — this is a silent correctness failure
- **Mechanism:** Fifteen nodes power on at different moments, so node A's `ms_lo = 12345` and node B's
  `ms_lo = 12345` refer to instants that may be minutes apart. There is no arithmetic that recovers a
  common instant from these values, because the offsets were never measured. A consumer wanting to
  assemble a coherent skeletal frame — all fifteen segments at one instant — therefore **cannot**,
  regardless of how carefully it handles the field. This is more damaging than the jitter question
  because it is a structural impossibility rather than a precision loss. The system currently works
  around it by using browser arrival order, which substitutes network jitter for sample time.
- **Falsifying measurement:** Have all nodes report `ms_lo` while a single hub broadcast is received by
  all of them simultaneously; the spread of reported values is the offset spread. If it is under a few
  milliseconds the nodes happen to be aligned and this is less severe — but nothing maintains that.
- **Proposed change:** The sync scheme in SYNC-06. Do not attempt to use `ms_lo` cross-node before it.
- **Cost:** see SYNC-06.
- **Risk of the fix:** None; it is purely additive.

### SYNC-02 — `ms_lo` dates the transmission, not the sample
- **Severity:** CORRECTNESS
- **Confidence:** high
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:647` versus `:771-774`
- **Evidence:** `[fact-code]` the quaternion is produced in `TaskReadIMU` on core 1 at `:771-774`;
  `ms_lo` is written in `TaskWifi` on core 0 at `:647`. The two tasks are independent, with no
  handoff.
- **Explains symptoms:** contributes to S6
- **Mechanism:** `TaskWifi` transmits whatever is currently in the shared `quat` global, stamped with
  the moment it woke. The sample in that global may have been produced anywhere from microseconds to
  tens of milliseconds earlier, depending on scheduling. So even a consumer that solved SYNC-01 would
  be attaching the wrong time to the measurement. The DMP additionally has its own sample instant
  (row 1 of the provenance table) which is never read at all.
- **Falsifying measurement:** Log `micros()` at Quat6 decode and again at send; histogram the
  difference. A tight distribution under ~2 ms makes this academic; a broad one makes it material.
- **Proposed change:** Stamp at sample time and carry it through the queue that NODE-01 requires.
- **Cost:** ~5 lines on top of NODE-01.
- **Risk of the fix:** None.

### SYNC-03 — Sequence numbers are parsed but gaps are never detected
- **Severity:** CORRECTNESS
- **Confidence:** high
- **Location:** `js/webserialnative.js:90`; `js/custom_icm.js:520`, `:535`
- **Evidence:** `[fact-code]` `count: dv.getUint16(12, true)` is unpacked at
  `js/webserialnative.js:90`. `[fact-code]` in `handleWSMessage`, `var count = parseInt(obj.count || "-1")`
  `js/custom_icm.js:520` is used **only** to render a chip reading "N frames" `:535`. Grep finds no
  comparison against a previous value, no gap computation, and no loss counter anywhere.
- **Explains symptoms:** **masks S1, S2, S3, S5** — it makes them unmeasurable rather than causing them
- **Mechanism:** The wire format carries everything needed to detect loss exactly — a per-node
  monotonic counter — and the browser decodes it and then throws the information away. Consequently
  the project cannot distinguish "the system is running slowly" from "the system is running at full
  rate and half the packets are lost", which are different problems with different fixes. This is why
  the only available performance figure is a single aggregate rate, and why every symptom in the log
  is qualitative. **Two lines of arithmetic would turn every existing symptom into a number.**
- **Falsifying measurement:** n/a — this is an absence, confirmed by reading. The remedy is the
  measurement (instrumentation I5, already half-built).
- **Proposed change:** Track `lastCount[bone]`; on each packet compute
  `(count - lastCount + 65536) % 65536 - 1` as the gap and accumulate per bone. Expose received,
  expected and lost counts. This is the **cheapest high-value change in the entire assessment**.
- **Cost:** ~10 lines, browser only, no firmware change, no wire-format change.
- **Risk of the fix:** None. It is purely observational.

### SYNC-04 — `ms_lo` wraps every 65.536 s and is misread as an age
- **Severity:** CORRECTNESS
- **Confidence:** high
- **Location:** `js/custom_icm.js:526-531`; `js/webserialnative.js:88-93`
- **Evidence:** `[fact-code]` `var t = moment(new Date().getTime() - millis);` `js/custom_icm.js:527`
  treats the value as milliseconds elapsed. `[fact-code]` the parser's own comment concedes the
  problem: "ms_lo wraps every ~65 s. The display code in custom_icm.js treats millis as 'ms ago' and
  the original JSON path was already broken in the same way, so we preserve behaviour"
  (`js/webserialnative.js:89-92`).
- **Explains symptoms:** none — it **hides** the latency needed to answer primary question 5
- **Mechanism:** The displayed value is `now − (node uptime mod 65536)`, which sweeps a 65-second ramp
  independent of any real delay. Sessions run for minutes, so the field wraps repeatedly mid-capture.
  The team therefore has no working latency readout at all, which is why the end-to-end latency budget
  cannot be constructed from the live system and had to be inferred (badly) from BVH cross-correlation.
- **Falsifying measurement:** n/a — established by reading and conceded in-code.
- **Proposed change:** Requires SYNC-06 for a true latency. Interim: display per-bone arrival-interval
  statistics, which are meaningful and need no sync.
- **Cost:** ~10 lines interim.
- **Risk of the fix:** None.

### SYNC-05 — Arrival-time jitter is recorded as motion; the conversion to degrees
- **Severity:** CORRECTNESS
- **Confidence:** medium
- **Location:** `js/bvh_converter.js:157`; consumption path `js/custom_icm.js:512-515`
- **Evidence:** `[fact-code]` frames are appended as they arrive and labelled `1/30` s apart;
  no arrival timestamp is retained.
- **Explains symptoms:** contributes to **S6**; corrupts all ground-truth comparison
- **Mechanism:** With samples effectively arrival-stamped, transport jitter is written into the file as
  though it were motion. **The conversion to angular error:** a limb segment rotating at angular
  velocity ω, sampled with timing error Δt, is placed at an orientation wrong by ω·Δt.
  - Jitter sources: `[inference]` CSMA backoff with 15 contenders (A3 estimates ~1.2 ms per packet at
    1 Mbps, with retries multiplying this), plus USB CDC polling at 1 ms granularity `[fact-doc]`,
    plus the node's own `vTaskDelay(1)` quantisation (A1's NODE-03). A conservative combined ±10 ms is
    reasonable; ±5 ms is optimistic.
  - Limb angular speeds: a normal walking arm swing runs about 100–200 °/s; a brisk gesture reaches
    300–600 °/s `[fact-doc, biomechanics literature]`. The benchmark's own measurements corroborate
    the low end: `[fact-data]` `reference_mean_angular_speed_dps` is 25.14 at the pelvis and 17.36 at
    spine_lower — but these are *means over a whole session* including stationary intervals, so peaks
    are far higher.
  - **At ±10 ms and 300 °/s: 3.0° of joint error. At ±10 ms and 600 °/s: 6.0°. At ±5 ms and 300 °/s: 1.5°.**
  Against A5's measured residual pose RMSE of 15–16°, a 3–6° timing-induced component is roughly a
  fifth to a third of the entire remaining error budget — not dominant, but not negligible either, and
  entirely avoidable.
- **Falsifying measurement:** Log per-packet arrival timestamps in the browser and histogram the
  inter-arrival interval per bone. The measured jitter, multiplied by measured per-joint angular
  speed from the same capture, gives the actual figure rather than this estimate. If jitter proves to
  be under ~2 ms, the contribution drops below 1° and can be deprioritised.
- **Proposed change:** Record arrival timestamps now (it costs nothing and makes the measurement
  possible), then SYNC-06 for the real fix.
- **Cost:** ~5 lines for the timestamps.
- **Risk of the fix:** None for recording.

### SYNC-06 — Minimum viable sync scheme, specified
- **Severity:** n/a (this is the proposed remedy for SYNC-01/02/05)
- **Confidence:** high that it is sufficient
- **Location:** would extend `Device code/Dongle_Binary/Dongle_Binary.ino:346-355` and
  `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:816-823`
- **Evidence:** `[fact-code]` the mechanism already exists — the hub broadcasts a 4-byte control
  packet `{0xAA, 0x55, 0xFF, 0x01}` to all peers `:349-353`, and each pod already decodes control
  packets by the `0xFF` id marker (`Pod_Watch_Binary.ino:816-822`).
- **Mechanism and specification:**
  - **Hub → all pods, once per second**, a 8-byte broadcast beacon:
    `[0xAA][0x55][0xFF][0x02][seq:uint16][hub_ms:uint32]`. The `0xFF` marker already distinguishes
    control from data, and `0x02` is an unused command, so this slots into the existing decoder
    without disturbing the reboot path.
  - **Each pod**, in `OnDataRecv`, records `local_us = esp_timer_get_time()` on arrival and stores the
    pair `(beacon_seq, local_us)`. This is a few instructions in an existing callback.
  - **Each pod's data packet** gains 3 bytes: `beacon_seq:uint16` (the last beacon it saw) and
    `offset_ms:uint8` — the elapsed time from that beacon's arrival to this sample, which at a 1 Hz
    beacon rate always fits in 0–255 ms if beacons are not missed, and otherwise saturates and is
    discarded. Packet grows 16 → 19 bytes, a 19% payload increase but under 5% of the ~59-byte
    on-air frame (A3), so the airtime cost is negligible.
  - **The browser** reconstructs a common timebase: every sample's instant is
    `hub_ms(beacon_seq) + offset_ms`, in the hub's single clock domain, for all fifteen nodes.
  - **Residual error** is the spread in beacon propagation and interrupt latency across pods —
    sub-millisecond, since all pods receive the same broadcast frame at essentially the same instant
    and the only differences are receive-path latencies. That is **an order of magnitude better than
    the ±10 ms** in SYNC-05, taking the timing contribution to joint error from 3–6° down to well
    under 1°.
  - **It also delivers a true end-to-end latency measurement** for free, which is primary question 5:
    browser arrival wall-clock minus reconstructed sample instant.
- **Falsifying measurement:** After implementation, verify by having two pods sample a shared physical
  event (tap them together, producing a simultaneous acceleration spike) and confirming the
  reconstructed instants agree to within the claimed bound.
- **Cost:** ~25 lines hub, ~20 lines pod, ~15 lines browser. Wire format change.
- **Risk of the fix:** Hub, pods and webapp must be deployed together. A missed beacon must be handled
  (saturate and drop, as specified) rather than producing a wrong instant.

### SYNC-07 — Exported frame interval is a constant, dilating every capture by ~7%
- **Severity:** CORRECTNESS
- **Confidence:** high
- **Location:** `js/bvh_converter.js:157,161`
- **Evidence:** `[fact-code]` `frameTime = 1/30`. `[fact-data]` all five exported captures carry
  `Frame Time: 0.03333333333333333` — the two `may8th2026` files and all three in
  `Mesquite_benchmarks/Mocab may 20th/`. `[fact-code]` the node's transmit gate is 31 ms
  (`Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:222,634`, integer division of 1000/32).
- **Explains symptoms:** corrupts the measurement of every other symptom
- **Mechanism:** 2508 frames labelled at 33.33 ms assert an 83.6 s session; the same 2508 frames at a
  true 32.26 Hz span 77.7 s — a **6-second discrepancy accumulating linearly**. Because the error is
  progressive rather than a constant offset, cross-correlation against ground truth cannot find a
  single consistent lag. `[fact-data]` this is exactly what `metrics_all.json` reports:
  `latency_ms` of **−300.0** (session 01) and **−2200.0** (session 02), with `latency_correlation`
  of only **0.593** and **0.608**. Two recordings of the same activity minutes apart cannot have
  latencies differing by 1.9 seconds. **These are not latency measurements; they are a correlator
  failing to lock on time-dilated data.** Any published figure derived from them is unsound.
- **Falsifying measurement:** Record wall-clock at recording start and stop; divide elapsed seconds by
  frame count. A measured 33.33 ms ±1% falsifies this.
- **Proposed change:** Write the measured mean interval, and record per-frame arrival timestamps
  alongside so non-uniformity is visible rather than assumed.
- **Cost:** ~10 lines.
- **Risk of the fix:** Invalidates previously reported benchmark numbers — which is the intent.

### SYNC-08 — Missing-sample behaviour is hold-last-value, silently
- **Severity:** CORRECTNESS
- **Confidence:** medium
- **Location:** `js/custom_icm.js:512-515`, `:583-900`
- **Evidence:** `[fact-code]` `handleWSMessage` writes into `mac2Bones[bone].last` `:512-515`, and the
  per-bone composition blocks `:583-900` read whatever is currently in that slot. There is no
  staleness check, no timeout on the value, and no interpolation. The only timeout in the system is
  the hub's 5-second liveness flag (`Device code/Dongle_Binary.ino:59`) and the 1-second
  HipsAlt/Hips arbitration (`js/custom_icm.js:581`), neither of which gates ordinary bone data.
- **Explains symptoms:** contributes to **S6** and to perceived judder
- **Mechanism:** When a node's packet is lost, its bone silently holds its previous orientation until
  the next arrival. The rendered and recorded result is a segment that freezes and then jumps — which
  reads as motion artifact, not as data loss. Because loss is also undetected (SYNC-03), there is no
  way to tell a frozen limb from a genuinely still one. In the exported BVH the held value is written
  as a real sample, so the file asserts the limb was stationary when in fact the data was missing.
- **Falsifying measurement:** With SYNC-03's gap detection in place, correlate detected gaps against
  frames where a bone's orientation is bit-identical to the previous frame. A high correlation
  confirms hold-last-value is being recorded as data.
- **Proposed change:** After measurement, at minimum mark held frames so they are distinguishable
  downstream. Interpolation is a Phase 2 decision.
- **Cost:** ~15 lines.
- **Risk of the fix:** Marking changes the export schema; consumers must tolerate the extra channel.

### SYNC-09 — Crystal drift is real but negligible beside the other terms
- **Severity:** QUALITY (a negative result, to close the question)
- **Confidence:** medium
- **Location:** n/a — analysis
- **Evidence:** `[fact-doc]` ESP32 modules typically specify a ±10 ppm crystal, with ±20 ppm at the
  tolerance edge over temperature.
- **Mechanism:** Over a 10-minute session, ±10 ppm is ±6 ms of accumulated divergence per node, and
  the worst-case spread between two nodes at opposite tolerance extremes is ~12 ms (±20 ppm gives
  ~24 ms). Converting via SYNC-05's method at 300 °/s, 12 ms is about **3.6° at the very end of a
  ten-minute session**, growing from zero. Compare this against SYNC-01, where the *initial* offsets
  between nodes are unbounded because they derive from independent boot times — potentially minutes,
  not milliseconds. **Drift is a second-order effect and should not be worked on until SYNC-06 exists**,
  at which point the 1 Hz beacon re-synchronises every node every second and drift never accumulates
  beyond one beacon interval (~10 ns at 10 ppm — utterly negligible).
- **Falsifying measurement:** With SYNC-06 in place, log the per-node correction applied at each
  beacon; its slope is the measured drift. If it exceeds ~20 ppm, the crystals are out of spec.
- **Proposed change:** None. SYNC-06 subsumes it.
- **Cost:** 0.
- **Risk of the fix:** n/a.

### SYNC-10 — Decoupled logging is viable but not currently possible
- **Severity:** QUALITY
- **Confidence:** low
- **Location:** n/a — assessment of a proposed capability
- **Evidence:** `[fact-doc]` the T-Watch 2019 carries 16 MB flash. `[fact-code]` the firmware includes
  `<EEPROM.h>` (`Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:64`) but no filesystem — no SPIFFS,
  no LittleFS, no SD library is included or mounted anywhere.
- **Mechanism:** The storage arithmetic is favourable. At 16 bytes per sample and 55 Hz (the DMP's
  real ceiling), one node produces 880 B/s — **about 53 KB/minute, so a 10-minute session is 528 KB**.
  Even reserving a conservative 4 MB partition allows over 75 minutes. Full-rate local logging plus a
  decimated live stream would decouple recording fidelity from radio capacity entirely, which directly
  addresses S1 and S5 for the *recording* use case while leaving the live preview lossy but adequate.
  The costs are real: flash wear (modest at this volume), CPU and bus time for writes on a task that
  A1 shows is already carrying the battery read and display, added complexity in retrieving and
  time-aligning fifteen separate logs, and the fact that **without SYNC-06 those fifteen logs share no
  common timebase and cannot be merged** — which makes the sync work a prerequisite rather than an
  alternative.
- **Falsifying measurement:** Confirm the actual flash partition layout on a deployed unit
  (`esp_partition` enumeration at boot) and measure the cost of a flash write on the sample task.
  If writes stall the task by more than a sample interval, the approach needs a buffer and a
  dedicated task.
- **Proposed change:** None in Phase 1. Record as a Phase 2 option contingent on SYNC-06.
- **Cost:** substantial; Phase 2.
- **Risk of the fix:** Retrieval workflow for 15 devices is an operational burden that may exceed the
  benefit.

## 5. Symptom attribution

| Symptom | Contributes? | Evidence |
|---|---|---|
| **S1** degrades with node count | **Masks it** | SYNC-03: loss is undetectable, so degradation cannot be distinguished from loss. |
| **S2** oscillates between values | **Masks it** | SYNC-03 and SYNC-04: no per-node rate or loss figure exists to characterise the oscillation. |
| **S3** only ~4 nodes connect | **Masks it** | Without gap detection, "not connected" and "connected but 90% lost" are indistinguishable. |
| **S4** sometimes all 15 fine | **Masks it** | Same — "fine" is unquantified. |
| **S5** 60 Hz fails with many nodes | **Masks it** | The rate being measured is arrival rate at the browser, conflated with several others (§C1). |
| **S6** yaw drift | **Yes — contributes** | SYNC-05: 3–6° of timing-induced joint error; SYNC-08: held values recorded as real stillness. |

The pattern is the point: this subsystem **causes** relatively little and **conceals** almost
everything. That is why it went unexamined, and it is why instrumentation must precede any fix.

## 6. What I could not determine

- `[unknown]` **Actual inter-arrival jitter.** Never measured; SYNC-05's ±10 ms is an estimate built
  from A3's airtime model and USB CDC polling granularity, not an observation.
- `[unknown]` **The true mean frame interval of the existing captures.** Unrecoverable — the files
  carry no arrival timestamps, only the synthetic constant. This is why SYNC-07 can only be fixed
  prospectively.
- `[unknown]` **Whether the two benchmark sessions used identical firmware.** Dated 2026-05-20 and
  2026-06-08; the binary wire format may postdate one or both.
- `[unknown]` **The FreeRTOS tick rate**, shared with A1. It sets the quantisation floor on both the
  transmit interval and any timestamp taken in a task.
- `[unknown]` **The phone's clock domain and whether any phone timestamp exists.** Blocked on A7.
- `[unknown]` **Flash partition layout on deployed units.** Gates SYNC-10.
- `[unknown]` **Whether session recordings save wall-clock time** — §13 asks this directly. **Answer:
  no.** `[fact-code]` `js/bvh_converter.js` writes only `Frames:` and `Frame Time:` (`:160-161`); no
  absolute time is recorded anywhere in the export. The filenames embed a date and time
  (`MMcap_bvh_2026-6-8-13-20-59.bvh`) to one-second resolution, which is the **only** wall-clock
  anchor that exists. It is enough to join against a field log to the second, so §13's proposed join
  is feasible today — but it dates the *export*, not the capture start, and those differ by however
  long the operator took to click save.
- `[unverified]` The ±10 ppm crystal specification is typical for ESP32 modules; the actual part on
  the T-Watch 2019 was not confirmed.

## 7. Measurements I need

1. **I5 — sequence-gap detection in the browser.** Ten lines, no firmware change, no wire-format
   change, and it converts every symptom in the log from an anecdote into a number. **This is the
   single highest value-per-line change in the assessment and it should be done first.**
2. **Per-packet arrival timestamps** logged in the browser. Gates SYNC-05 and makes SYNC-07 fixable.
3. **Wall-clock start/stop recorded into the export.** One line; enables the field-log join in §13
   properly rather than by filename inference.
4. **I4 — node send-interval histogram** and the tick rate. Shared with A1.
5. **Sample-to-send age distribution.** Gates SYNC-02.
6. **Beacon-spread test** after SYNC-06, to verify the claimed sub-millisecond residual.
7. **Held-frame correlation** against detected gaps. Gates SYNC-08.
