# The Seven Rates (§C1)

"FPS" in this project refers to at least seven distinct quantities. Conflating them is why a single
aggregate number has been the only performance figure available.

| # | Rate | Where it is set | Current value | Evidence |
|---|---|---|---|---|
| **1** | **ICM-20948 DMP output rate** (Quat6) | `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:357` `setDMPODRrate(DMP_ODR_Reg_Quat6, 0)` = every DMP cycle | **~55 Hz** — a hardware ceiling | `[fact-code]` + the file's own comment at `:352-356` ("when DMP is running at 55Hz") |
| **1b** | *Unused* raw accel/gyro FIFO rate | `:348-349` enabled; ODR lines `:358-359` commented out, so full DMP rate | ~55 Hz each, **discarded** at `:725` | `[fact-code]` |
| **2** | **Node sampling loop rate** | `TaskReadIMU`, core 1. Free-running; `delay(10)` at `:779` when the FIFO is empty, `vTaskDelay(1)` at `:783` | `[unknown]` — bounded above by ~100 Hz by the `delay(10)`, and by rate 1 in practice | `[fact-code]`; needs I9 |
| **3** | **Node transmit rate** | `TaskWifi`, core 0. `fps = 32` at `:222`; gate `millis() > prev_ms + (1000/fps)` at `:634` — integer division gives **31 ms** | **nominal 32.26 Hz**; actual quantised to FreeRTOS tick multiples by `vTaskDelay(1)` at `:655` — possibly **25 Hz** | `[fact-code]`; tick rate `[unknown]`; needs I4 |
| **4** | **Hub receive rate, per node** | `Device code/Dongle_Binary/Dongle_Binary.ino:179-224` `OnDataRecv` | **never measured** — no per-id counter exists | `[fact-code]`; this is instrumentation **I1** |
| **5** | **Hub forward rate** | `:201` `Serial.write(...)`, one call per received packet — 1:1 with rate 4 | = rate 4 (no aggregation, no batching) | `[fact-code]` |
| **6** | **Browser message-arrival rate** | `js/webserialnative.js:206-302` `feedSerialBytes`; counted in `window._rxMode.binary` at `:65` | counter exists, **never sampled per second** | `[fact-code]`; needs I6 |
| **7a** | **Browser render rate** | `js/threejsscene.js:589-592` `requestAnimationFrame(animate)` | display-driven, typically 60 Hz; **decoupled from arrival** | `[fact-code]` |
| **7b** | **Recording / BVH write rate** | `js/bvh_converter.js:157` `frameTime = 1/30` — **hardcoded, not measured** | asserts exactly **30.0 Hz** | `[fact-code]`; `[fact-data]` all five exported captures carry `Frame Time: 0.03333333333333333` |

## What the table shows

**Three different rates are asserted in three different places and none of them agree:** the DMP
produces at ~55 Hz, the node transmits at a nominal 32.26 Hz (possibly 25 Hz after quantisation), and
the export declares 30.0 Hz. Roughly **40% of the samples the DMP generates are never transmitted**,
and the file then relabels whatever did arrive onto a 30 Hz grid it never occupied.

The gap between rates 3 and 7b is the source of `SYNC-07`/`WEB-03`'s ~7% time dilation: 2508 frames at
a true 32.26 Hz span 77.7 s, but the file asserts 2508 × 0.03333 = **83.6 s**.

**Rates 4 and 6 — the only two that would reveal loss — are the two that are not measured.** Rate 4
requires ~20 lines at the hub (I1); rate 6 requires sampling a counter that already exists (I6). The
`count` field needed to compute loss exactly is already on the wire and already parsed
(`js/webserialnative.js:90`) and is simply never used for it (`SYNC-03`).

**The number the owner has been reasoning from is rate 6 or 7a**, not the sample rate. `[fact-code]`
the browser's per-bone status display shows `count` as a cumulative frame total
(`js/custom_icm.js:535`) and a meaningless "ms ago" figure derived from a wrapping counter
(`:526-531`, `WEB-04`). Neither is a rate.

## Which rate each symptom refers to

| Symptom | Which rate is actually being observed |
|---|---|
| S1 "frame rate degrades with node count" | rate 6 (browser arrivals), conflated with rate 4 |
| S2 "oscillates between specific values" | rate 6, driven by quantisation in rate 3 |
| S5 "60 Hz works with a few nodes" | a **target** for rate 3 that rate 1 cannot supply |
| S3/S4 "nodes connect / don't" | not a rate — a latching failure (see `BOTTLENECK_VERDICT.md`) |
