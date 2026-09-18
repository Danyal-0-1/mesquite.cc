# INSTRUMENTATION — Build this before fixing anything

Current knowledge is one aggregate rate figure and a set of anecdotes. Everything below is
specified concretely enough to implement. **None of it changes behaviour; all of it is observational.**

## Priority order

I1 and I5 come first and are worth more than the rest combined. Together they turn every symptom into
a number, and each discriminates several competing hypotheses at once.

---

## I5 — Sequence-gap detection *(do this first)*
**Where:** browser, `js/webserialnative.js` in `unpackPodPacket` or `js/custom_icm.js` in `handleWSMessage`
**Cost:** ~10 lines. **No firmware change. No wire-format change.**

The `count` field is already on the wire and already parsed (`js/webserialnative.js:90`) and is
currently used only to render a chip. Track per bone:

```
gap = (count - lastCount[bone] + 65536) % 65536 - 1     // 0 when consecutive
received[bone]++;  lost[bone] += max(0, gap);  lastCount[bone] = count;
```

Expose `received`, `lost` and `lost/(received+lost)` per bone, sampled once a second.

**Why it discriminates:** it separates "running slowly" from "running at full rate with heavy loss" —
currently indistinguishable, and they have different fixes. Every one of S1–S5 is currently
unquantified for exactly this reason. Guard the modulo against the `count` reset that follows a pod
reboot (`HUB-03`), or a reboot will register as 65535 lost packets.

---

## I1 — Packets received per node per second *(the single most discriminating measurement)*
**Where:** hub, `Device code/Dongle_Binary/Dongle_Binary.ino`, counter in `OnDataRecv` (~`:201`),
printed once a second from `podTimeoutTask` (`:145-163`, which already ticks at 1 Hz)
**Cost:** ~20 lines

Per-id counter incremented on each valid packet; also count drops by reason at `:186-195` (bad
length, bad sync, unknown id), which are currently silent `return`s.

**Critical constraint:** this must **not** print to the binary serial stream — `HUB-02` shows that
mixing text into it corrupts pod packets. Route to the WebSocket, or gate behind a boot-time mode
flag that disables binary forwarding.

**Why it discriminates three hypotheses at once:**
- all nodes sag uniformly → shared airtime (`NET-01`)
- specific nodes collapse → those pods or their body position (`C7`, `NODE-04`)
- a cliff after a delay → a queue filling (`HUB-02`, `WEB-01`)

---

## I2 — RSSI per packet
**Where:** hub, `OnDataRecv` — `recv_info->rx_ctrl->rssi`
**Cost:** ~5 lines

Free at the receiver; **do not add it to the 16-byte pod packet**, which would cost airtime for
something already measured at the hub. Report min/mean/max per id per second alongside I1.
Makes §C7 (body absorption) testable for the first time — plot dropouts against pose.

---

## I9 — Sensor read duration *(gates the bottleneck verdict)*
**Where:** node, around `myICM.readDMPdataFromFIFO(&data)` at
`Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:714`
**Cost:** ~10 lines

`esp_timer_get_time()` either side; accumulate min/mean/max, print once a second.
Then repeat with `:348-349` (unused raw accel/gyro) commented out and compare — that is `SENS-02`'s
falsifying test.

Also count Quat6 packets per second at the header check `:725`. **This directly settles `SENS-01`**,
the ~55 Hz DMP ceiling, which is the first binding constraint in the whole system.

---

## I4 — Node loop and send interval, actual vs nominal
**Where:** node, `TaskWifi` around `esp_now_send` (`:649`), and `TaskReadIMU`
**Cost:** ~10 lines

Histogram of intervals between consecutive sends, plus min/mean/max. Print `configTICK_RATE_HZ` and
`ESP_ARDUINO_VERSION_STR` once at boot.

**Why:** `NODE-03` predicts the 31 ms gate is quantised by `vTaskDelay(1)` to tick multiples — 40 ms
(25 Hz) if the tick is 10 ms. A histogram showing discrete spikes rather than a tight distribution
confirms the mechanism behind **S2**, and the tick-rate print resolves an `[unknown]` that three
reports depend on.

---

## I3 — Hub serial-write duration and loop timing
**Where:** hub, around `Serial.write(incomingData, POD_PACKET_LEN)` at `:201`
**Cost:** ~10 lines

min/mean/**max** microseconds. `Serial.write` runs inside the ESP-NOW receive callback; if it ever
blocks on a full USB TX buffer it stalls packet reception — a head-of-line problem and a candidate for
**S2**. The maximum matters far more than the mean here.

---

## I7 — Connected client and pod counts
**Where:** hub, in the existing 1 Hz `podTimeoutTask`
**Cost:** ~5 lines

`WiFi.softAPgetStationNum()` (associated Wi-Fi clients — the phone/browser) alongside the count of
distinct pod ids seen in the last second. **These are different populations**; printing both
confirms `HUB-01` (that `max_connection` cannot gate pods) and shows whether the soft-AP has any
clients at all — if it does not, its beacons are pure airtime cost (`NET-06`).

---

## I8 — Node reset and init-failure reporting
**Where:** node, `setup()` before task creation
**Cost:** ~15 lines

Print reset reason (`esp_reset_reason()`) and IMU/DMP init success at boot. Increment an RTC-memory
counter that survives reset so pods report their own restart count in a status field.

**Why:** `NODE-04` predicts pods hang forever in init on failure; `HUB-03` predicts fleet-wide reboots.
This is the only way to distinguish "the pod never started" from "the pod started and its packets are
lost" — the core ambiguity in **S3/S4**. Run 10 consecutive cold boots and count how many of 15
report success.

---

## I10 — Battery voltage and current from the AXP PMU
**Where:** node, in the existing 3-second timer at `:706-711`
**Cost:** ~10 lines

Voltage, current, charging state, logged per second (not per 3 s — and note the comment there says
"every minute" while the code says 3 s, a discrepancy worth resolving). Correlate sag against
dropouts to test the `C6` brownout theory behind **S3/S4**.

---

## I6 — Browser arrival timestamps and queue depth
**Where:** browser, `js/webserialnative.js` in `feedSerialBytes`
**Cost:** ~10 lines

`window._rxBytes` (`:105`) and `window._rxMode.binary` (`:65`) already exist and are simply never
sampled — log both once a second. Add: per-packet arrival timestamp (needed by `SYNC-05`/`SYNC-07`),
inter-arrival histogram per bone, and **`_jsonLine.length`** — a value growing monotonically past a
few hundred characters is `WEB-02`'s smoking gun and confirms the parser has latched.

---

## I11 — Free heap and largest free block
**Where:** node and hub, once per second
**Cost:** ~5 lines each

Plus `ws.count()` on the hub, which tests `HUB-04` (`cleanupClients()` never called). Detects
fragmentation and leaks over long sessions.

---

## I12 — Wall-clock in recordings *(new; §13's requirement)*
**Where:** browser, `js/bvh_converter.js`
**Cost:** ~5 lines

**Currently no absolute time is recorded anywhere in an export** — `js/bvh_converter.js:160-161`
writes only `Frames:` and `Frame Time:`. The filename embeds a timestamp to the second
(`MMcap_bvh_2026-6-8-13-20-59.bvh`) but that dates the *export*, not the capture start.

Record ISO-8601 wall-clock at recording start and stop, and the **measured** mean frame interval
rather than the hardcoded `1/30` (`SYNC-07`). This is the one-line-value fix §13 anticipates: with it,
the tester's field log becomes joinable against machine data to the second, and the failure taxonomy
that join produces is a publishable artifact.

---

## Baseline protocol

One session, all instrumentation on, **no other code changes**. Record it. That recording is the
reference every later change is measured against.

Specifically:
1. Full suit, normal capture volume, 10 minutes, subject moving through the usual range.
2. All of I1–I12 logging to file, with I1's output routed **off** the binary serial stream.
3. Record wall-clock start/stop and the room/position notes the field log needs.
4. Immediately repeat with the phone WebSocket **disconnected** — this is `C-05`'s resolving
   measurement and costs one extra session.

Without a baseline, every subsequent "improvement" is a vibe.

## What each experiment now needs

| Exp | Status after this assessment |
|---|---|
| **E1** `max_connection` | **Retired** — refuted by `HUB-01`; pods never associate |
| **E2** bus vs radio | **Re-scoped** — must first count Quat6/s and print the tick rate (see `BOTTLENECK_VERDICT.md`); the original one-node/radio-off form cannot separate the two ceilings that actually bind |
| **E3** DMP help | **Answered** — DMP already enabled (`SENS-01`); the live question is `SENS-02`'s unused raw streams |
| **E4** I²C clock | **Answered** — already 400 kHz (`Pod_Watch_Binary.ino:301`) |
| **E5** body vs air | **Unchanged and still needed** — bench 15 nodes, hub 1 m, no human |
| **E6** congestion vs code | **Unchanged** — add a channel survey (`NET-06`) |
| **E7** batching | **Blocked on owner question 7** (latency budget) — batching costs ~94 ms |
| **E8** where stamped | **Answered** — at transmit, not sample (`SYNC-02`); and 15 clock domains never reconciled (`SYNC-01`) |
| **E9** browser keeps up | **Re-scoped** to a DevTools profile of `handleWSMessage` (`WEB-01`) |
| **E10** display cost | **Unchanged** — pair with I10 |
| **E11** yaw drift | **Promoted** — now the key discriminator for `EST-01`/`C-04`: is the 71–99° error drift or initialisation? |
| **E12** sensor-to-segment | **Answered** — implemented (`js/custom_icm.js:268-298`) and exonerated by the flat error profile (`KIN-02`) |
