# A1 — Node Firmware & Sensor Acquisition — Assessment

## 1. Scope and files read

| File | Lines | Read |
|---|---|---|
| `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino` | 814 | **In full, line by line** |
| `Device code/Pod_Watch_JSON/Pod_Watch_JSON.ino` | 678 | Compared against binary variant |
| `Mesquite_benchmarks/pod_watch__1_/pod_watch__1_.ino` | 328 | Compared as earlier snapshot |

Skipped: `TTGO_TWatch_Library` and `ICM_20948` library sources — not vendored in this workspace,
so library-internal claims below are `[fact-doc]` or `[unverified]`, never `[fact-code]`.

## 2. Summary

The DMP is enabled and its Game Rotation Vector output is the sole source of orientation, so the
"unused DMP" hypothesis in the master prompt is false — but the DMP's maximum output rate is ~55 Hz,
which means **60 Hz is unreachable on this hardware path regardless of any network fix**. The
firmware runs two pinned FreeRTOS tasks that share the quaternion through an unsynchronised global
struct, so transmitted quaternions can be torn across two samples. The node timestamp `ms_lo` is
written at *transmit* time, not sample time, so it does not date the measurement it travels with.
Two raw sensors are enabled in the DMP FIFO whose data is never used, inflating every FIFO read.
`sendID` is chosen by comment-toggling, so all 15–17 nodes require individually hand-edited builds.

## 3. How this subsystem actually works

- The IMU is an **ICM-20948 on I²C**, `Wire.begin(21, 22)` at **400 kHz** `[fact-code]`
  `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:298-301`, instantiated as `ICM_20948_I2C myICM`
  `[fact-code]` `:90`, address select `AD0_VAL 0` `[fact-code]` `:75`.
- Pins 21/22 are the T-Watch 2019 shared sensor bus, also carrying PCF8563, BMA423 and AXP202
  `[fact-doc]` (LilyGO TTGO_TWatch_Library pinmap). So §C8 contention is real, not hypothetical.
- **The DMP is initialised and enabled** `[fact-code]` `:325` `initializeDMP()`, `:365` `enableFIFO()`,
  `:368` `enableDMP()`, `:371` `resetDMP()`, `:374` `resetFIFO()`.
- Enabled DMP sensors `[fact-code]` `:345-349`: `GAME_ROTATION_VECTOR` (6-axis quaternion),
  **plus `RAW_GYROSCOPE` and `RAW_ACCELEROMETER`**. The magnetometer line is commented out `:350`.
- Only `DMP_ODR_Reg_Quat6` has its ODR set, to 0 = maximum `[fact-code]` `:357`. The Accel and Gyro
  ODR lines are commented out `:358-359`, so those two streams run at the DMP's full rate too.
- Orientation is recovered in `TaskReadIMU` `[fact-code]` `:713-775`: `readDMPdataFromFIFO()`, then
  Q1/Q2/Q3 divided by 2^30, with `q0 = sqrt(1 - (q1²+q2²+q3²))` `:742`.
- **`loop()` is empty** `[fact-code]` `:554`. All work lives in two tasks created in `setup()`:
  `TaskWifi` pinned to **core 0** `[fact-code]` `:518-527`, `TaskReadIMU` pinned to **core 1**
  `[fact-code]` `:530-535`, both at priority 1.
- Transport is **ESP-NOW unicast to a hardcoded MAC** `[fact-code]`: `mac_address_str =
  "DC:DA:0C:17:10:A0"` `:9`, parsed into the misleadingly-named `broadcastAddress` `:452`, used as the
  `esp_now_send` destination `:649`. It is a specific address, not `FF:FF:FF:FF:FF:FF`, so frames are
  unicast and therefore MAC-layer ACKed and retried `[inference from fact-code + 802.11 fact-doc]`.
- Radio is pinned: `WIFI_STA` `:464`, channel 1 `:476`, `WIFI_PS_NONE` `:477`, TX power 80 (20 dBm)
  `:478` `[fact-code]`.
- Display: backlight opened `watch->openBL()` `[fact-code]` `:421`, bone label drawn at boot `:437-448`,
  battery banner redrawn on a timer `:706-711` → `handleBattDisplay()` `:541-552`. Screen auto-sleeps
  after 5 s idle `[fact-code]` `:692-694`, and touch is polled every task iteration `:698`.

## 4. Findings

### SENS-01 — DMP maximum output rate is ~55 Hz, so 60 Hz is unreachable
- **Severity:** BLOCKER
- **Confidence:** high
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:352-357`
- **Evidence:** `[fact-code]` the firmware's own comment states the formula
  `Value = (DMP running rate / ODR) - 1` and gives the worked example "for a 5Hz ODR rate when **DMP is
  running at 55Hz**". `[fact-doc]` the InvenSense ICM-20948 DMP runs its fusion output at a nominal
  55 Hz; `setDMPODRrate(..., 0)` requests every DMP cycle, i.e. ~55 Hz, not an arbitrary rate.
- **Explains symptoms:** S5, and bounds S1
- **Mechanism:** The DMP produces Quat6 samples at its own internal cadence of roughly 55 Hz. Asking
  the transmit loop for 60 Hz cannot create samples that do not exist; the extra sends would carry
  duplicated or torn state. Any "60 Hz" target is therefore above the sensor's ceiling before the
  radio is even considered.
- **Falsifying measurement:** On one node, count distinct Quat6 FIFO packets per second for 60 s
  (increment a counter each time `data.header & DMP_header_bitmap_Quat6` is true, print once a
  second). If the count sustains ≥60/s, this finding is wrong.
- **Proposed change:** Only after measurement — set the system target to the measured DMP rate, or
  move to raw-sensor acquisition with host-side fusion if >55 Hz is a hard requirement (that is a
  Phase 2 architectural decision, not a Phase 1 fix).
- **Cost:** measurement ~10 lines; the decision that follows is a design change.
- **Risk of the fix:** Re-targeting the rate touches every downstream assumption about frame timing.

### SENS-02 — Two unused raw sensor streams inflate every FIFO read
- **Severity:** PERFORMANCE
- **Confidence:** high
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:348-349`, consumed at `:713-775`
- **Evidence:** `[fact-code]` `RAW_GYROSCOPE` and `RAW_ACCELEROMETER` are enabled at `:348-349`, but
  the only branch that reads FIFO content is `if ((data.header & DMP_header_bitmap_Quat6) > 0)` at
  `:725`. No code reads `data.Raw_Accel` or `data.Raw_Gyro`. Their ODR lines are commented out
  (`:358-359`), so they emit at full DMP rate.
- **Explains symptoms:** contributes to S1, S5
- **Mechanism:** Each enabled DMP sensor adds its own bytes plus header to every FIFO cycle. Raw accel
  and raw gyro are 6 bytes each plus framing, roughly doubling to tripling the bytes that must be
  pulled across a shared 400 kHz I²C bus per cycle, for data that is discarded immediately. The cost
  is paid on the same bus the AXP202 battery read also uses.
- **Falsifying measurement:** Wrap `readDMPdataFromFIFO()` in `esp_timer_get_time()` and log mean/max
  microseconds per call (instrumentation I9). Then comment out `:348-349` and re-measure. If the
  duration does not drop materially, this finding is wrong.
- **Proposed change:** After measurement, disable the two unused `enableDMPSensor` calls.
- **Cost:** 2 lines.
- **Risk of the fix:** None functionally — nothing reads these streams. Re-enable if raw data is
  wanted later for calibration work.

### NODE-01 — Quaternion shared between two cores with no synchronisation (torn reads)
- **Severity:** CORRECTNESS
- **Confidence:** high
- **Location:** written `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:771-774`, read `:641-644`
- **Evidence:** `[fact-code]` `struct Quat { float x,y,z,w; } quat;` is a plain global `:239-244`.
  `TaskReadIMU` on core 1 writes all four fields at `:771-774`. `TaskWifi` on core 0 reads all four at
  `:641-644`. There is no mutex, critical section, atomic, or queue anywhere between them.
- **Explains symptoms:** S6 (as spurious jitter), and silent data corruption generally
- **Mechanism:** Two genuinely parallel cores touch a 16-byte non-atomic struct. A transmit on core 0
  can interleave with the four-field write on core 1, producing a packet holding x,y from sample N and
  z,w from sample N+1. The result is a non-unit quaternion representing an orientation the body never
  occupied — plausible-looking, never flagged.
- **Falsifying measurement:** In `TaskWifi`, immediately before sending, compute
  `x²+y²+z²+w²` and count how often it deviates from 1.0 by more than a tolerance (say 1e-3). Log the
  count per minute. If it is zero over a long moving capture, tearing is not occurring in practice.
- **Proposed change:** After measurement, publish the quaternion through a FreeRTOS queue, or
  double-buffer with a sequence-number guard, or copy under `portENTER_CRITICAL`.
- **Cost:** ~15 lines.
- **Risk of the fix:** A critical section on the sample path is short but must not wrap the I²C read.

### NODE-02 — `ms_lo` timestamps transmission, not sampling
- **Severity:** CORRECTNESS
- **Confidence:** high
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:647`
- **Evidence:** `[fact-code]` `myData.ms_lo = (uint16_t)millis();` is executed inside `TaskWifi`'s send
  block at `:647`, not in `TaskReadIMU` where the quaternion is produced (`:771-774`).
- **Explains symptoms:** contributes to S6 and to any timing-dependent artifact
- **Mechanism:** The packet's only time field records when the radio task happened to wake, while the
  quaternion it carries was produced by an independent task at an unrelated moment. The age of the
  sample at transmit is variable and unrecorded, so even a consumer that used `ms_lo` correctly would
  be dating the wrong event.
- **Falsifying measurement:** Record `micros()` in `TaskReadIMU` when a Quat6 packet is decoded, and
  again in `TaskWifi` at send; log the difference's distribution. If the spread is tight and small
  (say <2 ms), the distinction is academic. If it is broad, the timestamp is meaningless.
- **Proposed change:** Stamp at sample time in `TaskReadIMU` and carry that value through to the
  packet, alongside the queue fix in NODE-01.
- **Cost:** ~5 lines once NODE-01's handoff exists.
- **Risk of the fix:** None; strictly more accurate.

### NODE-03 — Transmit cadence is decoupled from sampling and integer-truncated
- **Severity:** PERFORMANCE
- **Confidence:** high
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:222`, `:634-652`
- **Evidence:** `[fact-code]` `int fps = 32;` `:222`. The gate is
  `if (millis() > (prev_ms + (1000 / fps)))` `:634` — integer division gives `1000/32 = 31`, not 31.25.
  `prev_ms = millis()` `:650` re-bases on actual wake time rather than advancing by a fixed period, so
  latency accumulates instead of being corrected. The loop then calls `vTaskDelay(1)` `:655`.
- **Explains symptoms:** S1, S2
- **Mechanism:** The send interval is 31 ms plus whatever scheduling latency the task incurred, and
  re-basing on `millis()` means the period ratchets rather than holding a phase. `vTaskDelay(1)`
  yields for one RTOS tick, so the achievable interval is quantised to tick multiples — if the tick is
  10 ms the real send interval snaps to 40 ms (25 Hz), and if it is 1 ms it lands near 32 Hz. **The
  tick rate therefore decides the actual transmit rate, and it is not set in this file** `[unknown]`.
  Quantisation to discrete tick multiples is a natural mechanism for S2's oscillation between specific
  values rather than smooth degradation.
- **Falsifying measurement:** Log the actual interval between consecutive `esp_now_send` calls
  (min/mean/max and a histogram) for 60 s — instrumentation I4. Also print `configTICK_RATE_HZ` at
  boot. If the histogram is a tight spike at 31 ms, this finding is wrong.
- **Proposed change:** After measurement, advance `prev_ms += period` rather than re-basing, and use
  `vTaskDelayUntil` for a true fixed cadence.
- **Cost:** ~5 lines.
- **Risk of the fix:** A fixed cadence removes the accidental jitter that currently de-correlates
  nodes; see NET-03 on synchronised transmission.

### NODE-04 — Pod hangs forever if IMU or DMP init fails
- **Severity:** RISK
- **Confidence:** high
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:305-318`, `:377-384`
- **Evidence:** `[fact-code]` `while (!initialized) { myICM.begin(...); ... delay(500); }` at `:305-318`
  retries forever with no bound. If DMP setup fails, `:380-384` prints a message then executes
  `while (1) ;` — an unconditional permanent hang, before any task is created and before any packet
  is ever sent.
- **Explains symptoms:** **S3, S4**
- **Mechanism:** A pod whose ICM-20948 does not come up on the shared I²C bus never reaches
  `xTaskCreatePinnedToCore` and never transmits. It is indistinguishable from a radio failure at the
  hub. Because I²C bring-up on a shared bus is timing- and power-sensitive, the same fleet can produce
  a different number of live pods on each power-up — which is exactly S3/S4's "same code, different
  outcome". Recovery requires a manual power cycle.
- **Falsifying measurement:** Add a boot-time serial print of init success/failure per pod, and count
  how many of 15 report success across 10 consecutive cold boots (instrumentation I8). If all 15
  succeed every time, this is not the S3 mechanism.
- **Proposed change:** After measurement, bound the retry loop and either reboot via
  `ESP.restart()` or continue in a degraded reporting state so the node is visible as failed.
- **Cost:** ~10 lines.
- **Risk of the fix:** A rebooting pod loops if the fault is permanent; add a retry ceiling.

### PWR-01 — Battery read and screen redraw sit on the sample task every 3 s
- **Severity:** PERFORMANCE
- **Confidence:** medium
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:706-711`, `:541-552`, `:759-766`
- **Evidence:** `[fact-code]` `if (millis() > (prev_ms1 + 1000 * 3))` `:706` calls
  `handleBattDisplay()`, whose comment says "read battery every minute" while the code says 3 seconds —
  a comment/code mismatch. `getBattery()` `:759-766` performs `adc1Enable(...)`, `isChargeing()` and
  `getBattPercentage()` — three AXP202 transactions on the **same I²C bus as the IMU** `[inference]`.
  `handleBattDisplay()` then does `fillRoundRect(0,205,240,35,...)` plus a text draw over SPI.
- **Explains symptoms:** contributes to S2 (periodic hitch)
- **Mechanism:** Every 3 s the sample task blocks on three PMU I²C transactions and a display update,
  during which no FIFO draining happens. The FIFO continues filling at the DMP rate, so this produces
  a periodic latency bump and possible FIFO backlog on a 3-second cycle.
- **Falsifying measurement:** Time `handleBattDisplay()` with `esp_timer_get_time()` and log the
  duration; simultaneously log FIFO depth before and after. If it costs under ~1 ms, ignore it.
- **Proposed change:** After measurement, move the PMU read off the sample task and reconcile the
  3 s / 60 s comment discrepancy.
- **Cost:** ~10 lines.
- **Risk of the fix:** Battery display becomes staler; irrelevant during capture.

### NODE-05 — Per-node identity requires hand-edited builds
- **Severity:** QUALITY
- **Confidence:** high
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:33-56`
- **Evidence:** `[fact-code]` seventeen mutually exclusive lines of the form
  `String bone = "Spine"; int sendID = 1; ...`, sixteen commented out and one live (currently `Spine`
  at `:38`). The bone identity is a compile-time constant selected by editing comments.
- **Explains symptoms:** S3 indirectly (a mis-flashed duplicate ID is invisible)
- **Mechanism:** Fifteen to seventeen physically distinct firmware images must be maintained by hand.
  Two pods flashed with the same `sendID` will both transmit under that id; the hub and browser key
  purely on id, so one silently overwrites the other and one bone appears dead — which presents as "a
  node did not connect". Nothing detects the collision.
- **Falsifying measurement:** Inspect `window._podRxByteId` in the browser during a full-suit session
  (already implemented, `js/webserialnative.js:56`). A collision shows as one id at roughly double
  rate and another absent. If every expected id appears at a similar rate, no collision is occurring.
- **Proposed change:** Out of Phase 1 scope (that is a refactor). Record as a build-process risk;
  the cheap interim control is the `_podRxByteId` check above as a pre-session preflight.
- **Cost:** 0 for the check.
- **Risk of the fix:** n/a.

### SENS-03 — Degenerate quaternion when the DMP norm exceeds unity
- **Severity:** CORRECTNESS
- **Confidence:** medium
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:742`, `:174-180`
- **Evidence:** `[fact-code]` `double q0 = sqrt(1.0 - ((q1*q1) + (q2*q2) + (q3*q3)));` at `:742` with no
  clamp on the radicand. `[fact-code]` `q_to_i16` at `:174-180` maps NaN to 0. So a NaN `q0` is
  transmitted as `w = 0`, and if the other components are also out of range the packet carries
  `(0,0,0,0)`.
- **Explains symptoms:** none directly; corrupts individual frames
- **Mechanism:** The DMP emits Q1..Q3 and the host reconstructs Q0 from the unit-norm identity. When
  noise or drift makes the sum of squares exceed 1, the square root of a negative number is NaN. The
  guard in `q_to_i16` converts that to a zero quaternion, which is not a valid rotation. The webapp's
  own comments confirm this happens in the field — `js/webserialnative.js:169-172` describes firmware
  emitting `nan` literals from exactly this expression.
- **Falsifying measurement:** Count occurrences of a negative radicand at `:742` over a 10-minute
  moving capture. Zero occurrences falsifies it. `[fact-code]` the webapp already counts the
  downstream symptom in `window._nanFrameDropped` (`js/webserialnative.js:276`).
- **Proposed change:** After measurement, clamp the radicand at zero before the square root.
- **Cost:** 1 line.
- **Risk of the fix:** None.

### NODE-06 — Dead Euler-angle computation on every sample
- **Severity:** QUALITY
- **Confidence:** high
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:747-763`
- **Evidence:** `[fact-code]` roll, pitch and yaw are computed with two `atan2` and one `asin` call at
  `:749-763`. The variables `roll`, `pitch` and `yaw` are never read again anywhere in the file — only
  `quat.w/x/y/z` are assigned at `:771-774`.
- **Explains symptoms:** none
- **Mechanism:** Three transcendental functions in double precision execute per sample on an FPU that
  is single-precision only, so these run in software. It is small but it is pure waste on the sample
  path.
- **Falsifying measurement:** Time the block; if it is under ~50 µs it is not worth touching. Fold
  into the I9 measurement.
- **Proposed change:** After measurement, delete `:747-763`.
- **Cost:** 17 lines removed.
- **Risk of the fix:** None.

### PWR-02 — Backlight and display active during capture
- **Severity:** PERFORMANCE
- **Confidence:** medium
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:421`, `:692-704`
- **Evidence:** `[fact-code]` `watch->openBL()` at `:421` turns the backlight on at boot; the screen
  auto-sleeps only after 5 s of idle `:692-694`, and any touch wakes it again `:697-703`. Touch is
  polled via `watch->getTouch(x, y)` on **every** iteration of the sample task `:698`.
- **Explains symptoms:** contributes to S3/S4 via battery sag `[inference]`
- **Mechanism:** The backlight is the largest single consumer on the board `[fact-doc]`, and it is on
  for the first 5 s of every session and after every accidental touch. Touch polling adds an I²C
  transaction per sample iteration on the FT6236 bus. Neither serves a body-worn sensor.
- **Falsifying measurement:** Log AXP202 battery voltage and current once per second (instrumentation
  I10) with the screen on versus forced off, and compare both current draw and achieved sample rate
  (experiment E10). If the delta is small, deprioritise.
- **Proposed change:** After measurement, keep the display dark during capture and gate touch polling.
- **Cost:** ~5 lines.
- **Risk of the fix:** Loses the on-body indication of which bone a pod is, which the team may rely on
  when strapping the suit — make it a capture-mode toggle rather than a removal.

## 5. Symptom attribution

| Symptom | Contributes? | Evidence |
|---|---|---|
| **S1** degrades with node count | Partly | Node-side cost is per-node and constant; SENS-02 and NODE-03 raise the per-node cost but do not scale with fleet size. The scaling term is transport (A3). |
| **S2** oscillates between values | **Yes** | NODE-03: `vTaskDelay(1)` quantises the send interval to RTOS tick multiples, giving discrete rates rather than smooth degradation. PWR-01 adds a 3 s periodic hitch. |
| **S3** only ~4 nodes connect | **Yes — leading node-side candidate** | NODE-04: a pod that fails IMU/DMP init hangs forever and never transmits. NODE-05: duplicate `sendID` silently hides a pod. |
| **S4** sometimes all 15 fine | **Yes** | NODE-04's failure is a power-up race on a shared I²C bus, so the same fleet gives different results per boot. |
| **S5** 60 Hz fails with many nodes | **Yes — decisive** | SENS-01: the DMP cannot produce 60 Hz. The ceiling is upstream of the radio entirely. |
| **S6** yaw drift | **Yes** | Magnetometer disabled at `:350`, so Game Rotation Vector is 6-axis and yaw is unreferenced. Magnitude is A5's to quantify. NODE-01 and SENS-03 add spurious orientation noise on top. |

## 6. What I could not determine

- `[unknown]` **FreeRTOS tick rate** (`configTICK_RATE_HZ`). Not set in this file and no
  `sdkconfig` is present in the workspace. This decides whether `vTaskDelay(1)` is 1 ms or 10 ms and
  therefore what the real transmit rate is. Resolved by printing it at boot.
- `[unknown]` **Arduino-ESP32 core version.** The file contains a 2.x/3.x callback-signature shim
  (`:194-198`, `:795-802`) guarded on `ESP_ARDUINO_VERSION_MAJOR`, proving it is built against an
  unfixed core version. No `platformio.ini`, `sdkconfig` or `arduino-cli` config exists in the
  workspace to pin it. Resolved by printing `ESP_ARDUINO_VERSION_STR` at boot or asking the owner.
- `[unknown]` **Per-sample acquisition cost in microseconds.** Cannot be derived without the library
  source and the actual FIFO packet layout; requires instrumentation I9. This is experiment E2 and the
  bottleneck verdict depends on it.
- `[unknown]` **Whether the T-ICM-20948 is on the shared 21/22 bus or a separate breakout.** The code
  proves the *software* uses `Wire` on 21/22 alongside the AXP202; whether the physical wiring shares
  those pins with PCF8563/BMA423 needs the schematic (§12 hardware request).
- `[unknown]` **Gyro/accel full-scale range and DLPF.** Never set explicitly, so whatever
  `initializeDMP()` configures is in force. Requires the SparkFun library source to state.
- `[unverified]` The DMP's 55 Hz figure is taken from the firmware's own comment and general TDK
  documentation; the exact rate for this configuration should be counted empirically (SENS-01's
  measurement).

## 7. Measurements I need

1. **I9 — sensor read duration.** `esp_timer_get_time()` around `readDMPdataFromFIFO()`; log
   min/mean/max per second. Gates SENS-01, SENS-02 and the entire bottleneck verdict (E2).
2. **I4 — actual send interval.** Histogram of deltas between `esp_now_send` calls. Gates NODE-03.
3. **Quat6 packets per second**, counted at the header check `:725`. Directly settles SENS-01.
4. **Boot config dump:** `configTICK_RATE_HZ`, `ESP_ARDUINO_VERSION_STR`, DMP init success. Gates
   NODE-03 and NODE-04.
5. **Unit-norm violation counter** before send. Gates NODE-01.
6. **I10 — AXP202 voltage and current per second.** Gates PWR-01, PWR-02 and the S3/S4 brownout theory.
7. **Sample-to-send age distribution.** Gates NODE-02.
