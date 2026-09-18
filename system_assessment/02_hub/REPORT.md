# A2 — Hub / Data Sink — Assessment

## 1. Scope and files read

| File | Lines | Read |
|---|---|---|
| `Device code/Dongle_Binary/Dongle_Binary.ino` | 355 | **In full, line by line** |
| `Device code/Dongle__JSON/Dongle__JSON.ino` | 224 | Compared as legacy variant |
| `Mesquite_benchmarks/pod_watch__1_/code/dongle_combined/dongle_combined.ino` | — | Compared as earlier snapshot |

Skipped: `ESPAsyncWebServer` library source — not vendored here, so claims about its internals are
`[fact-doc]` or `[unverified]`.

## 2. Summary

The `max_connection` / four-node hypothesis is **dead**: pods reach the hub over ESP-NOW and never
associate with the soft-AP, so no AP client cap can gate them. The hub's real risk is that it writes
to `Serial` from two unsynchronised task contexts — the ESP-NOW receive callback and the WebSocket
handler — onto a single stream that the browser must frame by sync bytes, so phone traffic can be
interleaved into the middle of a pod packet. `loop()` treats *any* inbound serial byte as a command to
reboot every pod, with no verb and no guard. Two of the three FreeRTOS tasks are empty no-ops.
`ws.cleanupClients()` is never called, which leaks disconnected WebSocket clients over a session.

## 3. How this subsystem actually works

- `Serial.begin(921600)` `[fact-code]` `Device code/Dongle_Binary/Dongle_Binary.ino:277`. The board
  header comments specify `USB CDC On Boot: Enabled`, `USB Mode: Hardware CDC and JTAG` `[fact-code]`
  `:12-13`, so this is **native USB CDC** and the baud figure is advisory rather than a real line rate
  `[fact-doc]`.
- `WiFi.mode(WIFI_AP_STA)` `[fact-code]` `:280`, then
  `WiFi.softAP("MM-" + mac, "12345678", ESPNOW_WIFI_CHANNEL, /*hidden=*/0)` `[fact-code]` `:293`.
  **`max_connection` is not passed**, so it takes the core default `[fact-code]`.
- Channel is pinned to 1 `[fact-code]` `:46`, re-forced after `softAP()` at `:299`, with
  `esp_wifi_set_ps(WIFI_PS_NONE)` `:300` and `esp_wifi_set_max_tx_power(80)` `:301`.
- `esp_now_register_recv_cb(OnDataRecv)` `[fact-code]` `:309`. `OnDataRecv` `:179-224` validates
  `len == 16` and the `0xAA 0x55` sync `:186-190`, rejects `id >= NUM_PODS` `:193`, then calls
  **`Serial.write(incomingData, POD_PACKET_LEN)`** `:201`. It does not decode fields.
- Liveness is tracked under a spinlock: `podConnected[id] = true; podLastSeen[id] = millis();`
  inside `portENTER_CRITICAL(&stateMux)` `[fact-code]` `:204-207`. `podTimeoutTask` `:145-163` clears
  `podConnected[i]` after `POD_TIMEOUT_MS` (5000) `:59`, sweeping once a second `:161`.
- Pods are registered as unicast peers on first sight `[fact-code]` `:213-222`, so `sendReset()` can
  reach them. `NUM_PODS` is **17** `[fact-code]` `:60`, matching the 17-entry `POD_ABBR` table `:69-88`
  (15 limb/torso bones plus Left/Right Shoulder at ids 15 and 16).
- A WebSocket server runs on port 80 at path `/ws` `[fact-code]` `:127-128`, `:255-258`, `:291`.
  Inbound text messages are handled by `handleWebSocketMessage` `:227-234`, which does
  **`Serial.println((char *)data)`** `:232`.
- Three tasks are created `[fact-code]` `:348-350`: `espNowTask` pinned to core 0 and `webSocketTask`
  pinned to core 1 are both `for(;;) vTaskDelay(pdMS_TO_TICKS(100));` — **pure no-ops** `:265-271`.
  Only `podTimeoutTask` does work.
- `loop()` `[fact-code]` `:333-339`: `if (Serial.available() > 0) { Serial.readString(); sendReset(); }`.

## 4. Findings

### HUB-01 — `max_connection` cannot gate the pods; the four-node theory is misdirected
- **Severity:** QUALITY (a correction, not a defect)
- **Confidence:** high
- **Location:** `Device code/Dongle_Binary/Dongle_Binary.ino:293`; pods at
  `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:464-478`
- **Evidence:** `[fact-code]` the pods call `WiFi.mode(WIFI_STA)` and never call `WiFi.begin()` — there
  is no association attempt anywhere in the pod firmware. They transmit via `esp_now_send` `:649`.
  `[fact-doc]` ESP-NOW is a connectionless link-layer protocol; peers exchange frames without
  association, so soft-AP client limits do not apply to them.
- **Explains symptoms:** none — this finding **removes** a candidate explanation for S3
- **Mechanism:** The soft-AP's client cap governs devices that associate: the phone, and any browser
  connecting over Wi-Fi. Pods are not among them. A fifteen-pod fleet is therefore not throttled by
  `max_connection`, and experiment E1 as written cannot produce the four-node symptom.
- **Falsifying measurement:** Print `WiFi.softAPgetStationNum()` once a second alongside the count of
  distinct pod ids seen in `OnDataRecv` (instrumentation I7 + I1). If pod count tracks AP station
  count, this correction is wrong.
- **Proposed change:** None to code. Re-target the S3 investigation at NODE-04 (pod init hang) and
  NET-03 (transmit synchronisation).
- **Cost:** 0.
- **Risk of the fix:** n/a.

### HUB-02 — Two task contexts write to one serial stream with no mutual exclusion
- **Severity:** CORRECTNESS
- **Confidence:** high
- **Location:** `Device code/Dongle_Binary/Dongle_Binary.ino:201` and `:232`, `:240`, `:244`
- **Evidence:** `[fact-code]` `Serial.write(incomingData, 16)` at `:201` runs inside `OnDataRecv`, the
  ESP-NOW receive callback. `[fact-code]` `Serial.println((char *)data)` at `:232` runs inside
  `handleWebSocketMessage`, invoked from the AsyncWebSocket event path. `[fact-code]` `:240` and `:244`
  also `Serial.printf` client-connect and client-disconnect lines. No mutex, queue or critical section
  guards any of these.
- **Explains symptoms:** contributes to S1, S2 whenever the phone is streaming
- **Mechanism:** ESP-NOW callbacks and the async web server run in different FreeRTOS task contexts.
  Arduino's `HardwareSerial` on ESP32 is not documented to be atomic for multi-byte writes across
  tasks `[fact-doc]`, so a 16-byte binary packet and a JSON line can interleave. The browser's parser
  frames on `0xAA 0x55`, and — critically — its JSON branch swallows every byte once a line is open
  (see WEB-01), so bytes of a pod packet landing mid-JSON-line are consumed as text and lost. The loss
  rate rises with phone message frequency, which is exactly when the system is under load.
- **Falsifying measurement:** Run a session with the phone WebSocket **disconnected** and one with it
  connected, and compare per-bone packet counts at the browser (`window._podRx`, already implemented
  at `js/webserialnative.js:56`) against the hub's own per-id receive counts (instrumentation I1). If
  the browser/hub ratio is unchanged by phone activity, this finding is wrong.
- **Proposed change:** After measurement, serialise all writes through a single queue drained by one
  task, and give the phone payload its own binary framing rather than raw text on the same stream.
- **Cost:** ~40 lines.
- **Risk of the fix:** Changes the wire format the browser parses; must land with the webapp change.

### HUB-03 — Any inbound serial byte reboots the entire suit
- **Severity:** RISK
- **Confidence:** high
- **Location:** `Device code/Dongle_Binary/Dongle_Binary.ino:333-339`, `:346-355`
- **Evidence:** `[fact-code]` `if (Serial.available() > 0) { Serial.readString(); sendReset(); }`. The
  bytes read are **discarded without inspection** — there is no command verb, magic number or
  checksum. `sendReset()` `:346-355` then sends `{0xAA,0x55,0xFF,0x01}` to every registered peer, and
  the pod's `OnDataRecv` responds with `ESP.restart()` `[fact-code]`
  `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:816-823`.
- **Explains symptoms:** **S3, S4**
- **Mechanism:** Any process that writes a byte to the port — a serial monitor, a stray
  `window.sWrite()` (exposed globally at `js/webserialnative.js:385-387`), an automation script,
  line-noise on a non-native-CDC adapter — reboots all 15–17 pods simultaneously. On reboot each pod
  re-runs `setupIMU()`, whose failure path hangs forever (NODE-04), so a single accidental byte can
  permanently drop an arbitrary subset of the fleet until they are power-cycled by hand. It also puts
  every pod's transmit phase back in lockstep (NET-03).
- **Falsifying measurement:** Log every `sendReset()` invocation with a timestamp to a counter that
  survives in the hub's status output, and correlate against sessions where pods went missing. If
  resets never fire during normal sessions, the risk is theoretical.
- **Proposed change:** After measurement, require an explicit command token (e.g. the literal
  `RESET\n`) before calling `sendReset()`.
- **Cost:** ~5 lines.
- **Risk of the fix:** Existing operator habits that rely on "send anything to reset" would break;
  document the new token.

### HUB-04 — `ws.cleanupClients()` is never called
- **Severity:** RISK
- **Confidence:** medium
- **Location:** `Device code/Dongle_Binary/Dongle_Binary.ino:255-258`, `:333-339`
- **Evidence:** `[fact-code]` `initWebSocket()` registers the handler and adds it to the server, but
  `cleanupClients()` appears nowhere in the file (verified by grep across `Device code/`). `loop()`
  `:333-339` contains only the serial check and a `vTaskDelay`.
- **Explains symptoms:** possible contributor to long-session degradation
- **Mechanism:** `[fact-doc]` ESPAsyncWebServer's `AsyncWebSocket` accumulates client objects for
  disconnected sockets unless `cleanupClients()` is called periodically; the documented usage pattern
  calls it from `loop()`. Without it, a session in which the phone reconnects repeatedly — which a
  hip-worn phone on a flaky link will do — leaks memory and file descriptors on the hub.
- **Falsifying measurement:** Log free heap and `ws.count()` once a second (instrumentation I11) over
  a long session with deliberate phone reconnects. Flat heap falsifies this.
- **Proposed change:** After measurement, call `ws.cleanupClients()` in `loop()`.
- **Cost:** 1 line.
- **Risk of the fix:** None; it is the documented pattern.

### HUB-05 — Dropped packets are silent
- **Severity:** PERFORMANCE
- **Confidence:** high
- **Location:** `Device code/Dongle_Binary/Dongle_Binary.ino:186-195`
- **Evidence:** `[fact-code]` both rejection paths are bare `return;` — the length/sync check `:186-190`
  and the `id >= NUM_PODS` check `:193-195`. Neither increments a counter nor logs.
- **Explains symptoms:** masks S1, S3 (makes them unmeasurable rather than causing them)
- **Mechanism:** The hub is the one place that sees every pod's traffic, and it discards malformed or
  unknown-id frames without record. There is consequently no hub-side measurement of how much is
  arriving, from whom, or how much is being thrown away — which is why the project's only rate figure
  is a single aggregate number from the browser.
- **Falsifying measurement:** n/a — this is an absence, confirmed by reading. The remedy *is* the
  measurement (instrumentation I1).
- **Proposed change:** Add a per-id received counter and a per-reason drop counter, printed once a
  second. This is **instrumentation I1 and the highest-value single change in the whole assessment**,
  because it discriminates uniform airtime sag from specific-node collapse from queue-filling cliffs.
- **Cost:** ~20 lines.
- **Risk of the fix:** The status print must not go to the same serial stream as the binary data
  (see HUB-02); route it to the WebSocket or gate it behind a mode flag.

### HUB-06 — Two of three FreeRTOS tasks are empty
- **Severity:** QUALITY
- **Confidence:** high
- **Location:** `Device code/Dongle_Binary/Dongle_Binary.ino:265-271`, `:348-349`
- **Evidence:** `[fact-code]` `void webSocketTask(void *p) { for (;;) vTaskDelay(pdMS_TO_TICKS(100)); }`
  and `espNowTask` identically. Both are created with 4096-byte stacks at `:348-349`.
- **Explains symptoms:** none
- **Mechanism:** 8 KB of stack is reserved for two tasks that only sleep. Harmless at runtime, but
  their names imply the ESP-NOW and WebSocket work is happening there, when in fact both run entirely
  in callback contexts (which is what makes HUB-02 possible). The naming actively misleads.
- **Falsifying measurement:** n/a — read directly.
- **Proposed change:** Out of Phase 1 scope. Note that if HUB-02 is fixed by moving serial writes to a
  task, `espNowTask` is the natural home and already exists.
- **Cost:** 0 now.
- **Risk of the fix:** n/a.

### HUB-07 — `esp_now_add_peer` return value unchecked on the receive path
- **Severity:** RISK
- **Confidence:** medium
- **Location:** `Device code/Dongle_Binary/Dongle_Binary.ino:216-222`
- **Evidence:** `[fact-code]` `peerMacsInit[id] = true;` is set at `:217` **before**
  `esp_now_add_peer(&peerMacs[id]);` at `:221`, and the return value is discarded. Contrast `:319`,
  where the broadcast peer's return **is** checked.
- **Explains symptoms:** none for the data path; affects the reset path only
- **Mechanism:** `[fact-doc]` ESP-NOW enforces a maximum total peer count (20 by default in ESP-IDF,
  with a much smaller cap for encrypted peers). With 17 pods plus one broadcast peer the hub sits at
  18 of 20 — under the limit but with little headroom. If a call ever failed, `peerMacsInit[id]` would
  already be `true`, so the code would never retry and `sendReset()` would silently skip that pod
  forever. Note this affects only reboot delivery, **not** data reception, since data arrives without
  the receiver needing a peer entry.
- **Falsifying measurement:** Log the return of `esp_now_add_peer` at `:221` and count non-`ESP_OK`
  results over a full-suit session. Zero failures falsifies it.
- **Proposed change:** After measurement, set `peerMacsInit[id] = true` only on `ESP_OK`.
- **Cost:** 3 lines.
- **Risk of the fix:** None.

### HUB-08 — USB CDC is not the bottleneck; the arithmetic
- **Severity:** QUALITY (a negative result worth recording)
- **Confidence:** high
- **Location:** `Device code/Dongle_Binary/Dongle_Binary.ino:12-13`, `:277`, `:201`
- **Evidence:** `[fact-code]` 16 bytes per packet (`POD_PACKET_LEN`, `:65`), one `Serial.write` per
  received packet `:201`. `[fact-code]` board config is native USB CDC `:12-13`.
- **Explains symptoms:** none — this **eliminates** a candidate ceiling
- **Mechanism:** The arithmetic, shown in full:
  - 15 nodes × 32 Hz × 16 B = **7,680 B/s**
  - 17 nodes × 60 Hz × 16 B = **16,320 B/s**
  - Nominal 921600 baud 8N1 = 92,160 B/s → the 60 Hz case uses **18%**.
  - `[fact-doc]` Native USB CDC on ESP32-S3 is a USB full-speed bulk endpoint whose real throughput
    is on the order of hundreds of kB/s and is not governed by the `Serial.begin` argument at all.
  Either way there is more than 5× headroom. **Downstream serial bandwidth is not the constraint.**
  This matters because the browser opens the port at 115200 (`js/webserialnative.js:319`), which under
  native CDC is equally irrelevant — but would be a hard 11,520 B/s ceiling on any dongle using a real
  UART bridge, which the 60 Hz case would exceed.
- **Falsifying measurement:** Count bytes per second at the browser (`window._rxBytes`, already present
  at `js/webserialnative.js:105`) and compare against the hub's transmitted count (I1). A shortfall
  that scales with load would indicate a real serial ceiling.
- **Proposed change:** None. Record the negative result, and confirm with the owner that every deployed
  dongle is native-CDC rather than UART-bridge (see OPEN_QUESTIONS).
- **Cost:** 0.
- **Risk of the fix:** n/a.

## 5. Symptom attribution

| Symptom | Contributes? | Evidence |
|---|---|---|
| **S1** degrades with node count | Partly | HUB-02 loses packets whenever phone traffic interleaves, and the loss is load-proportional. HUB-08 rules out serial bandwidth. |
| **S2** oscillates between values | **Yes** | HUB-02: interleaving is bursty and tied to phone message timing, producing discrete rather than smooth loss. |
| **S3** only ~4 nodes connect | **Yes — strong candidate** | HUB-03: one stray serial byte reboots every pod, and NODE-04 means an arbitrary subset then hangs in init and never returns. |
| **S4** sometimes all 15 fine | **Yes** | Same mechanism: whether a reset was triggered, and how many pods survive re-init, varies per session. |
| **S5** 60 Hz fails with many nodes | No evidence from the hub | HUB-08 shows ample serial headroom at 60 Hz; the ceiling is SENS-01 and airtime. |
| **S6** yaw drift | No evidence | The hub does not decode or transform orientation; it forwards raw bytes (`:201`). |

## 6. What I could not determine

- `[unknown]` **The soft-AP's `max_connection` default for the exact core version in use.** Not passed
  at `:293`, and no build config pins the core version. Moot for pods (HUB-01) but it does bound how
  many phones/browsers can associate. Resolved by printing the core version at boot.
- `[unknown]` **Whether `Serial.write` blocks when the USB TX buffer fills, and what that does inside
  the ESP-NOW callback.** Requires the core's `HardwareSerial`/TinyUSB source. If it blocks, the ESP-NOW
  receive path stalls — a serious head-of-line problem. Resolved by timing `Serial.write` at `:201`
  and logging the maximum.
- `[unknown]` **The WebSocket's actual role in deployment** — whether the phone connects to it, or
  whether it is vestigial. The code path exists and forwards to serial `:232`; whether anything uses it
  is A7's question and an owner question.
- `[unknown]` **Whether all deployed dongles are native-CDC** or some use a UART bridge, which would
  make the 115200/921600 mismatch a real ceiling. Owner question.
- `[unverified]` ESP-NOW's 20-peer default is from ESP-IDF documentation; the limit for the specific
  IDF version in use should be confirmed once the core version is known.

## 7. Measurements I need

1. **I1 — per-id packets received per second at the hub**, printed once a second for all 17.
   This is the single most discriminating measurement available and the hub is the only place it can
   be taken. Route it off the binary serial stream.
2. **I3 — `Serial.write` duration** at `:201`, min/mean/max. Settles the blocking-callback unknown.
3. **I7 — `WiFi.softAPgetStationNum()` and distinct-pod-id count**, once a second. Confirms HUB-01.
4. **Drop counters** by reason at `:186-195`. Gates HUB-05.
5. **I11 — free heap and `ws.count()`** once a second. Gates HUB-04.
6. **`sendReset()` invocation counter.** Gates HUB-03.
7. Boot-time print of core/IDF version and soft-AP `max_connection`. Resolves two `[unknown]`s.
