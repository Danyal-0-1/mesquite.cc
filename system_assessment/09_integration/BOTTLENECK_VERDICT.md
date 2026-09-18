# Bottleneck Verdict

The master prompt requires adjudication between three candidate ceilings — sensor bus (§C8), radio
airtime (§C7/A3), and downstream consumption (§C5/A4) — and requires naming the one that dominates.

## Verdict

**There is no single dominant ceiling, because two independent hard limits bind before any of the
three candidates does. Naming one of the three would be wrong.**

Ranked by which binds first:

### 1. The DMP output rate — ~55 Hz — binds before anything else
`[fact-code]` `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:352-357`. The firmware's own comment
states the DMP runs at 55 Hz, and `setDMPODRrate(DMP_ODR_Reg_Quat6, 0)` requests every DMP cycle.
`[fact-code]` `:222` the transmit task is separately capped at `fps = 32`.

**60 Hz is unreachable regardless of radio, bus or browser.** No amount of network or parser work
produces samples the sensor does not generate. This alone answers primary question 1's "where exactly
is the ceiling" for the 60 Hz case, and it was not among the three candidates offered.

### 2. The transmit task's tick quantisation — binds at ~32 Hz today
`[fact-code]` `:634` gates on `1000/fps` = **31** ms by integer division, then `:655` calls
`vTaskDelay(1)`. The achievable interval is quantised to FreeRTOS tick multiples, and the tick rate is
`[unknown]` — not set in this file and no `sdkconfig` exists in the workspace. If the tick is 10 ms,
the real interval snaps to 40 ms (**25 Hz**), not the nominal 32.

This is the **most likely explanation for S2's oscillation between specific values**: quantisation
produces discrete rates, not a smooth curve.

### 3. Of the three candidates offered, radio airtime is the strongest — but it is conditional
`[fact-doc]` + `[inference]`, full arithmetic in `../03_network/REPORT.md` NET-01. The 16-byte payload
sits inside a ~59-byte on-air frame, so airtime scales with **packet count**, not payload bytes.

| PHY rate | 15 nodes @ 32 Hz | 15 nodes @ 60 Hz |
|---|---|---|
| 1 Mbps (802.11b) | **57% utilisation** | **106% — oversubscribed** |
| 6 Mbps (OFDM) | 12% | 22% |

`[fact-code]` **nothing in the firmware calls `esp_wifi_config_espnow_rate()`**, so the rate is the IDF
default — and which default applies depends on the IDF version, itself `[unknown]`. CSMA degrades
sharply above ~50–60% offered load, so at 1 Mbps the current configuration sits exactly at the knee.
**This single unconfigured register swings the answer between "the radio is the bottleneck" and "the
radio is a non-issue."** Determining it is the highest-priority measurement in this assessment.

## The other two candidates, explicitly adjudicated

**Downstream consumption (§C5/A4) — a real and serious constraint, second only to the radio.**
`[fact-code]` `js/custom_icm.js:519-560` performs a `moment.js` `.fromNow()` call, a full `innerHTML`
rebuild and a jQuery selector **per packet** — ~480/s at 15 nodes, ~900/s at 60 Hz, all on the main
thread. When the main thread saturates, the WebSerial reader is starved and throughput drops in steps.
This presents as a network problem and is not one. It is the most *cheaply fixable* of the real
ceilings.

**Serial bandwidth — eliminated.** `[fact-code]` 16 B × 17 nodes × 60 Hz = 16,320 B/s against a
nominal 92,160 B/s at 921600 baud (18%), and `[fact-doc]` native USB CDC on the ESP32-S3 is not
governed by the baud figure at all. Ample headroom. **Not a constraint.**

**Sensor bus contention (§C8) — probably not decisive, but unmeasured.**
`[fact-code]` the ICM-20948 is on I²C at **400 kHz** (`:298-301`), not 100 kHz — so E4 is already
answered and the pessimistic case does not apply. Two aggravating factors are confirmed:
`[fact-code]` `:348-349` enable `RAW_GYROSCOPE` and `RAW_ACCELEROMETER` whose data is **never read**
(`:725` reads only Quat6), inflating every FIFO transfer; and `[fact-code]` `:706-711` performs three
AXP202 I²C transactions plus a display redraw every 3 seconds on the sample task. Both cost real bus
time. But the DMP ceiling at 55 Hz binds first, so relieving the bus cannot raise the rate past it.

**§C8's standing instruction — "no agent may declare the radio the bottleneck until this is
measured" — is respected. The radio is not declared the bottleneck.** It is ranked third,
conditionally, pending the PHY-rate measurement.

## What actually explains the symptoms

No single ceiling explains the symptom log. The symptoms partition:

- **S5** (60 Hz fails) — the DMP ceiling, item 1. Settled by code reading.
- **S1** (degrades with node count) — airtime (conditional on PHY rate) plus per-packet browser cost.
  Both scale with node count; nothing else in the system does.
- **S2** (oscillates between specific values) — tick quantisation (item 2), unicast retry feedback,
  and main-thread starvation recovery. All three produce discrete rather than smooth behaviour.
- **S3 / S4** (intermittent, sometimes only ~4 nodes) — **not a throughput phenomenon at all.**
  Three independent latching failure modes: a pod that fails IMU/DMP init hangs forever
  (`:305-318`, `:377-384`); any inbound serial byte reboots the whole suit
  (`Device code/Dongle_Binary/Dongle_Binary.ino:333-339`); and an unterminated JSON line permanently
  halts all binary parsing in the browser (`js/webserialnative.js:212,234`). Each is sticky —
  recovery requires a power cycle or page reload — which is exactly why the same code gives different
  outcomes on different days.
- **S6** (yaw drift) — orthogonal to all of the above. See `../05_estimation/REPORT.md`.

## The measurement that settles this

**Experiment E2, extended.** The master prompt is right that E2 is the most important experiment, but
as written it tests one node with the radio disabled, which cannot distinguish items 1 and 2 from
each other.

Run instead, on a single node, in this order:
1. Count **Quat6 FIFO packets per second** (the DMP's true rate) — settles item 1.
2. Print `configTICK_RATE_HZ` and histogram the actual `esp_now_send` interval — settles item 2.
3. Time `readDMPdataFromFIFO()` (instrumentation I9), then repeat with `:348-349` commented out —
   settles §C8's magnitude.
4. Only then, with 15 nodes: capture the ESP-NOW PHY rate, or force 6 Mbps and re-measure per-node
   delivered rate (I1) — settles item 3.

Steps 1–3 need one node and roughly 30 lines of instrumentation. **Until they run, any statement
about which ceiling dominates is a guess** — including the ranking above, which rests on code reading
and arithmetic rather than on measurement.
