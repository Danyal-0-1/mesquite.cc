# OPEN QUESTIONS

Every `[unknown]` from all nine reports, with who can answer it and what it blocks.

## Tier 1 — Blocking, resolvable by measurement

| # | Question | Who/how | Blocks |
|---|---|---|---|
| 1 | **What PHY rate does ESP-NOW actually use?** Nothing calls `esp_wifi_config_espnow_rate()`, so it is the IDF default — and which default depends on the IDF version. | Monitor-mode sniffer on ch 1, **or** force 6 Mbps and re-measure I1 | `NET-01`, and the entire bottleneck verdict. **Swings channel utilisation between 12% and 57%.** |
| 2 | **What is `configTICK_RATE_HZ`?** Not set in any file; no `sdkconfig` exists in the workspace. | Print at boot | `NODE-03`. Decides whether the true transmit rate is ~32 Hz or **25 Hz**, and underpins the S2 explanation |
| 3 | **What is the DMP's actual Quat6 output rate?** The 55 Hz figure comes from the firmware's own comment, not measurement. | Count Quat6 FIFO packets/s at `Pod_Watch_Binary.ino:725` | `SENS-01` — the first binding ceiling in the system |
| 4 | **Is the yaw error drift or initialisation?** Observed 71–99°; random walk predicts only ~0.37° over 10 min. | **E11** — one node, stationary, 10 min | `EST-01`. Two very different fixes |
| 5 | **What does `readDMPdataFromFIFO()` actually cost?** | I9, then repeat with `:348-349` disabled | `SENS-02`, §C8, re-scoped E2 |
| 6 | **Does `Serial.write` block inside the ESP-NOW callback when the USB buffer fills?** | I3 — log the **maximum**, not the mean | `HUB-02`, head-of-line stalling, S2 |
| 7 | **Has `WEB-02` actually fired in the field, or is it latent?** | Log `_jsonLine.length` per second | `WEB-02`/`PHN-02` — a leading S3/S4 candidate |
| 8 | **What is the real inter-arrival jitter?** `SYNC-05`'s ±10 ms is an estimate, not an observation. | I6 — per-packet arrival timestamps | `SYNC-05`. Sets the 3–6° joint-error figure |

## Tier 2 — Owner questions (§14 seed list, answered where possible)

| # | Question | Status |
|---|---|---|
| 1 | **How is the T-ICM-20948 connected — I²C or SPI, which pins?** | **Partly answered from code:** `[fact-code]` I²C, `Wire.begin(21, 22)` at 400 kHz (`Pod_Watch_Binary.ino:298-301`). **Still needed:** whether pins 21/22 are *physically* shared with PCF8563/BMA423 on these units. **Schematic or photo requested.** |
| 2 | **Is the display used during capture, or is it dark?** | **Partly answered:** `[fact-code]` backlight on at boot (`:421`), auto-sleeps after 5 s idle (`:692-694`), any touch wakes it, touch polled every sample iteration (`:698`), battery banner redrawn every 3 s (`:706-711`). **Still needed:** whether operators touch the screens during sessions. |
| 3 | **Soft-AP, router, or phone hotspot?** | **Answered:** `[fact-code]` pods use ESP-NOW and join nothing. The dongle runs a soft-AP on ch 1 **for the phone**. **Still needed:** does the phone actually connect to it in practice, or to something else? |
| 4 | Physical layout — room size, hub position, metal in the space? | **Unanswered.** Needed for `NET-06`, `C7`, and E5/E6 design |
| 5 | Has anyone measured per-node packet rates? | **Answered: no.** `[fact-code]` no per-node counter exists anywhere (`HUB-05`, `SYNC-03`) |
| 6 | **Is there an OptiTrack capture paired with an IMU capture?** | **Not in this workspace.** The only reference present is **Rokoko** — itself an inertial suit, so it cannot validate heading. Important for `EVALUATION_DESIGN.md` |
| 7 | **Target application latency?** | **Unanswered and blocking.** Batching (`NET-04`/E7) costs ~94 ms. Interactive vs recording use have different answers, and the two can be satisfied separately |
| 8 | **Is 60 Hz a hard requirement, or a proxy for "the motion looks choppy"?** | **Unanswered and now urgent.** `SENS-01` shows the DMP tops out near 55 Hz, so 60 Hz needs an acquisition change. If the real complaint is choppiness, `SYNC-08` (held frames) and `WEB-01` (main-thread stalls) are likelier causes than rate |
| 9 | Battery life per session; have nodes reset mid-capture? | **Unanswered.** `PWR-01`, `PWR-02`, I8, I10. Note `HUB-03` means a stray serial byte reboots the fleet |
| 10 | **Do all 15 nodes run identical firmware?** | **Answered: no, and they cannot.** `[fact-code]` `sendID` is a compile-time constant selected by comment-toggling (`:33-56`) — each node needs its own hand-edited build (`NODE-05`). **Still needed:** is the deployed firmware the same revision as this workspace? All code-based refutations depend on it |

## Tier 3 — Remaining `[unknown]`s by subsystem

**A1 / node:** Arduino-ESP32 core version (a 2.x/3.x shim at `:194-198` proves it is unpinned); gyro
and accel full-scale range and DLPF (never set explicitly — whatever `initializeDMP()` chooses).

**A2 / hub:** the soft-AP's `max_connection` default for the core version in use (moot for pods, but
it bounds phone/browser clients); whether every deployed dongle is native-CDC or some use a UART
bridge — a real 11,520 B/s ceiling at 115200 that the 60 Hz case would exceed.

**A4 / webapp:** actual main-thread time in `handleWSMessage`; where recording accumulates frames and
whether it is bounded over a long session; whether the benchmark captures were recorded with
`smoothOrientation` enabled (the `mesquite_smooth` directory name suggests it may have been, but
`js/bvh_converter.js:10` has it commented out now).

**A5 / estimation:** what bias tracking the DMP performs internally (TDK does not publish the GRV
algorithm); the DMP's quaternion convention — handedness, reference frame, axis assignment; true
per-node drift rates; whether the benchmark captures used the current firmware.

**A6 / kinematics:** **whether a T-pose calibration is actually performed in normal operation** — the
code exists but the workflow is not determinable from source, and a negative answer reopens `KIN-02`;
what `trees/meta.json`'s `tposeOffset` values encode and whether they were derived or hand-tuned;
which of the eight `trees/` variants was used for the benchmark captures.

**A7 / phone:** everything needs the source — reference-space type, update rate, units and handedness
of `px/py/pz`, whether relocalization events are exposed, tracking-loss behaviour, camera-to-hip
extrinsic, thermal profile. Full list in `07_phone_slam/REPORT.md` §7. **Note four of its five
measurements need no source and can run today.**

**A8 / sync:** the true mean frame interval of existing captures (**unrecoverable** — no arrival
timestamps were saved); flash partition layout on deployed units (gates `SYNC-10`); the phone's clock
domain.

## Answered by this assessment — do not re-investigate

| Question | Answer |
|---|---|
| Is the DMP enabled? | **Yes** — `initializeDMP()`, GRV, FIFO all enabled (`:325,345,365,368`) |
| I²C or SPI, what clock? | **I²C, 400 kHz**, pins 21/22 — E4 answered |
| Is `max_connection` capping nodes? | **No** — pods use ESP-NOW and never associate. **E1 retired** |
| Are packets sequence-numbered? | **Yes** — `count`, on the wire and parsed, **but never gap-checked** |
| Is there node-side timestamping? | **Yes** — `ms_lo`, but stamped at *transmit*, wrapping at 65.5 s, from 15 unrelated clock origins |
| Is sensor-to-segment calibration handled? | **Yes** — solved from a T-pose with flip disambiguation. **E12 answered** |
| Where does fusion run? | **On the DMP.** No host-side filtering is active (`WEB-06`) |
| Is serial bandwidth a constraint? | **No** — 18% utilised at 17 nodes × 60 Hz |
| Do session recordings save wall-clock time? | **No** — §13's question. Only the export filename carries a timestamp, and it dates the export, not the capture |
| Is the phone path live or vestigial? | **Live** — `[fact-data]` root translates from X=7.5 to X=84.3 in a real capture |
| Does rotation-order mismatch invalidate the benchmark? | **No** — the comparison honours each file's declared channel order (`compare_bvh_suits.py:321-331`) |
