# A3 — RF & Network Transport — Assessment

## 1. Scope and files read

Read with a transport lens (both files also read in full by A1/A2):
`Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino` (814), `Device code/Dongle_Binary/Dongle_Binary.ino`
(355), plus the legacy JSON pair for comparison of wire-format evolution.

Skipped: nothing in scope. ESP-NOW and 802.11 frame-structure claims are `[fact-doc]`.

## 2. Summary

Transport is **ESP-NOW unicast** to a hardcoded hub MAC, not the Wi-Fi association the master prompt
assumed, and not TCP — so the "TCP retransmission causes S2" hypothesis is dead. The decisive unknown
is the **PHY rate**: nothing in the firmware calls `esp_wifi_config_espnow_rate()`, and if ESP-NOW
falls back to the 1 Mbps 802.11b basic rate then 15 nodes at 32 Hz already consume roughly **57% of
channel airtime**, and 60 Hz is **oversubscribed at ~107%** — which explains S1 and S5 exactly. At
6 Mbps the same traffic is trivial (12% and 22%), so this one register decides whether the radio is
the bottleneck or a non-issue. Because sends are unicast they are ACKed and retried, so contention
produces retry storms with discrete rate steps rather than smooth degradation — the natural mechanism
for S2. All 15 nodes rebooting together (HUB-03) leave transmit phases synchronised, maximising
collisions.

## 3. How this subsystem actually works

- **ESP-NOW, not associated Wi-Fi.** Pods call `WiFi.mode(WIFI_STA)` `[fact-code]`
  `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:464` and never call `WiFi.begin()`.
- **Unicast, not broadcast.** The destination is parsed from the string literal
  `"DC:DA:0C:17:10:A0"` `[fact-code]` `:9` into a variable named `broadcastAddress` `:452`, then used
  at `esp_now_send(broadcastAddress, ...)` `:649`. The name is misleading: this is a specific hub MAC,
  so frames are unicast and therefore **ACKed and retried at the 802.11 MAC layer** `[fact-doc]`.
- Both ends pin **channel 1** `[fact-code]` (`:476` pod, `Dongle_Binary.ino:46,299` hub), disable power
  save (`WIFI_PS_NONE`), and set TX power to 80 (20 dBm) `[fact-code]`.
- Payload is **16 bytes** `[fact-code]` `Dongle_Binary.ino:65`; one packet per sample, **no batching**
  `[fact-code]` `Pod_Watch_Binary.ino:634-652`.
- Nominal transmit rate is `fps = 32` `[fact-code]` `Pod_Watch_Binary.ino:222`, gated by
  `millis() > prev_ms + (1000/fps)` `:634` — integer division yields a 31 ms period.
- **No rate configuration.** Grep across `Device code/` finds no `esp_wifi_config_espnow_rate`,
  `esp_now_set_peer_rate_config`, or `WIFI_PHY_RATE_*` `[fact-code]`. The PHY rate is therefore the
  IDF default for ESP-NOW frames.
- **RSSI is available but unused.** The hub's callback receives `esp_now_recv_info_t *recv_info`
  `[fact-code]` `Dongle_Binary.ino:179`, which carries `rx_ctrl->rssi` `[fact-doc]`. `OnDataRecv` reads
  only `recv_info->src_addr` `:180` and never touches `rx_ctrl` `[fact-code]`.

## 4. Findings

### NET-01 — Airtime is the scaling term, and the PHY rate decides whether it binds
- **Severity:** PERFORMANCE
- **Confidence:** medium (high on the arithmetic, medium on the rate assumption)
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:649`; absence of rate config across
  `Device code/`
- **Evidence:** `[fact-code]` 16-byte payload, one frame per sample, no rate configuration call.
  `[fact-doc]` ESP-NOW frames are 802.11 vendor-specific action frames; `[fact-doc]` 802.11b/g timing
  constants below.
- **Explains symptoms:** **S1, S5**
- **Mechanism:** The 16-byte payload is a small fraction of the frame, so **airtime scales with packet
  count, not payload bytes** — which is why C5's "the payload is trivial" reasoning is correct but
  irrelevant. Frame size on air:

  | Component | Bytes |
  |---|---|
  | 802.11 MAC header | 24 |
  | Action frame (category + OUI + random) | 8 |
  | Vendor-specific IE (elem id, len, OUI, type, version) | 7 |
  | ESP-NOW payload | 16 |
  | FCS | 4 |
  | **Total** | **~59** |

  **Case A — 1 Mbps (802.11b long preamble), the likely IDF default for action frames:**
  - Preamble + PLCP header: 192 µs
  - Data: 59 B × 8 = 472 bits ÷ 1 Mbps = 472 µs
  - SIFS + ACK: 10 + (192 + 14×8/1) = 314 µs
  - DIFS + mean backoff (CW 15, 20 µs slots, 7.5 slots avg): 50 + 150 = 200 µs
  - **Total ≈ 1,178 µs per packet**
  - 15 nodes × 32 Hz = 480 pkt/s → **565 ms/s = 57% utilisation**
  - 15 nodes × 60 Hz = 900 pkt/s → **1,060 ms/s = 106% — oversubscribed**
  - 17 nodes × 60 Hz = 1,020 pkt/s → **1,202 ms/s = 120%**

  **Case B — 6 Mbps (802.11g OFDM):**
  - Preamble + signal: 20 µs; data: 472 ÷ 6 = 79 µs; SIFS + ACK ≈ 10 + 39 = 49 µs;
    DIFS + backoff ≈ 28 + 68 = 96 µs → **≈ 244 µs per packet**
  - 15 × 32 Hz → **12% utilisation**; 15 × 60 Hz → **22%**. Comfortable.

  CSMA/CA throughput collapses well before 100% — contention, backoff and retries degrade sharply
  above roughly 50–60% offered load `[fact-doc]`. Case A therefore puts the *current* 32 Hz
  configuration right at the knee, and makes 60 Hz impossible. Case B makes the radio a non-issue.
  **The same code exhibits both behaviours depending on one unconfigured register.**
- **Falsifying measurement:** Determine the actual PHY rate. Two routes: (a) capture frames with a
  monitor-mode sniffer on channel 1 and read the rate field directly; (b) call
  `esp_wifi_config_espnow_rate()` to force 6 Mbps on all nodes and re-run the per-node rate
  measurement (I1). If forcing 6 Mbps does not improve aggregate throughput at 15 nodes, airtime is
  not the binding constraint and this finding is wrong.
- **Proposed change:** Only after measurement — either set an explicit higher PHY rate, or batch
  (NET-04), or both.
- **Cost:** 1–2 lines for the rate call; measurement is the real work.
- **Risk of the fix:** Higher PHY rates have lower receiver sensitivity, so range and body-penetration
  margin drop. This trades robustness for airtime and must be validated worn, not just on a bench.

### NET-02 — Unicast retries produce discrete rate steps, not smooth degradation
- **Severity:** PERFORMANCE
- **Confidence:** medium
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:9`, `:452`, `:649`
- **Evidence:** `[fact-code]` the destination is a specific MAC, not `FF:FF:FF:FF:FF:FF`.
  `[fact-code]` the send-status callback at `:195-220` distinguishes success from failure and counts
  consecutive failures in `dccount`, which is only meaningful if ACKs exist. `[fact-doc]` unicast
  802.11 frames are acknowledged and retried by hardware; broadcast frames are not.
- **Explains symptoms:** **S2**, contributes to S1
- **Mechanism:** Each unicast frame costs its own airtime **plus** an ACK, and a collision triggers a
  hardware retry with an exponentially doubled contention window. As offered load approaches the
  knee, retries add load, which causes more collisions — a positive feedback that settles into
  distinct operating points rather than degrading linearly. That is precisely the "oscillates between
  specific values" character of S2, and it is a better fit than gradual saturation.
- **Falsifying measurement:** Log `dccount` / send-failure rate per node per second alongside I1. If
  failures stay near zero while the aggregate rate oscillates, retries are not the mechanism.
- **Proposed change:** After measurement, consider broadcast for telemetry (no ACK, no retry, roughly
  40% less airtime per frame) — accepting that delivery becomes unconfirmed and `dccount`'s
  auto-shutdown logic must be removed.
- **Cost:** ~10 lines, but see risk.
- **Risk of the fix:** Broadcast loses per-frame delivery feedback entirely, and the pods' no-link
  shutdown (`:216-219`) depends on it. Sequence numbers already exist for loss detection, so this is
  viable — but it is a real trade, not a free win.

### NET-03 — Nodes reboot together and therefore transmit in phase
- **Severity:** PERFORMANCE
- **Confidence:** medium
- **Location:** `Device code/Dongle_Binary/Dongle_Binary.ino:333-339`, `:346-355`;
  `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:634-652`
- **Evidence:** `[fact-code]` `sendReset()` broadcasts a reboot to every registered peer in a tight
  loop `:349-353`. `[fact-code]` each pod's transmit gate is driven from its own `millis()`, which
  restarts at zero on reboot `Pod_Watch_Binary.ino:634`.
- **Explains symptoms:** S1, S2, and the S3/S4 alternation
- **Mechanism:** After a fleet-wide reset every pod's `millis()` origin is aligned to within the
  spread of their boot times, and each then transmits on the same 31 ms period. Fifteen transmitters
  sharing a phase collide far more than fifteen with randomised offsets — the worst case for CSMA.
  Nothing in the firmware dithers the transmit phase. Because the alignment decays only slowly through
  crystal drift, a session can stay in a bad regime for minutes and then improve, which matches the
  intermittency of S4.
- **Falsifying measurement:** With I1 running, trigger `sendReset()` and watch whether aggregate
  per-node rate drops immediately afterwards and recovers over the following minutes. No change
  falsifies it.
- **Proposed change:** After measurement, add a per-node random phase offset (derived from `sendID`
  or the MAC) to the transmit gate.
- **Cost:** ~3 lines.
- **Risk of the fix:** None significant; it only dithers phase, not rate.

### NET-04 — No batching; batching is the cheapest available airtime win
- **Severity:** PERFORMANCE
- **Confidence:** high
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:634-652`
- **Evidence:** `[fact-code]` one `esp_now_send` per sample, 16-byte payload, no accumulation buffer.
- **Explains symptoms:** offers relief for S1, S5
- **Mechanism:** With ~43 bytes of framing overhead per 16-byte payload, four samples batched into one
  frame carry 4× the data for roughly 1.6× the airtime. Using the Case A numbers: a 4-sample frame is
  59 + 3×12 = 95 bytes on air (the sync bytes and id need not repeat), giving ~1,362 µs versus
  4 × 1,178 = 4,712 µs — a **71% airtime reduction** for the same sample rate. The cost is latency: at
  32 Hz, buffering four samples adds ~94 ms before the first is sent.
- **Falsifying measurement:** Implement batching on a subset of nodes (experiment E7) and compare
  per-node delivered sample rate via I1 against unbatched nodes in the same session. No improvement
  falsifies the airtime hypothesis generally.
- **Proposed change:** After measurement, batch 2–4 samples per frame. The existing `count` field
  already provides the sequence base, so the format extension is small.
- **Cost:** ~30 lines across pod, hub and browser parser.
- **Risk of the fix:** **Added latency is the whole trade.** If the target application is interactive,
  94 ms may be unacceptable; if it is recording, it is free. This depends on owner question 7, which
  is unanswered — do not implement before that is settled.

### NET-05 — RSSI is available at the hub and discarded
- **Severity:** QUALITY
- **Confidence:** high
- **Location:** `Device code/Dongle_Binary/Dongle_Binary.ino:179-224`
- **Evidence:** `[fact-code]` the callback signature provides `const esp_now_recv_info_t *recv_info`
  `:179`, and only `recv_info->src_addr` is used `:180`. `[fact-doc]` `recv_info->rx_ctrl->rssi`
  carries per-packet RSSI.
- **Explains symptoms:** none — its absence makes **C7 untestable**
- **Mechanism:** Link quality for a body-worn node is a function of the wearer's pose, and which nodes
  are shadowed changes as the person turns. Without per-packet RSSI logged as a data channel, that
  hypothesis cannot be tested at all; with it, dropouts become plottable against pose. It is free at
  the receiver — no extra airtime, no pod-side change.
- **Falsifying measurement:** n/a — this is an absence. The remedy is instrumentation I2.
- **Proposed change:** Read `recv_info->rx_ctrl->rssi` in `OnDataRecv` and include it in the per-id
  status output (alongside I1). Do **not** add it to the 16-byte pod packet — it is measured at the
  hub, so it costs nothing there.
- **Cost:** ~5 lines.
- **Risk of the fix:** None.

### NET-06 — Channel 1 is hardcoded, with the hub's own soft-AP beaconing on it
- **Severity:** RISK
- **Confidence:** medium
- **Location:** `Device code/Dongle_Binary/Dongle_Binary.ino:46`, `:293`, `:299`;
  `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:73-83`, `:476`
- **Evidence:** `[fact-code]` `#define ESPNOW_WIFI_CHANNEL 1` on both sides, with the pod firmware's
  own comment acknowledging the trade-off: "Channel 1 is a safe default but if your client site has a
  heavy 2.4 GHz AP on ch 1, you can move to 6 or 11". `[fact-code]` the hub additionally runs a soft-AP
  on that same channel `:293`.
- **Explains symptoms:** **S4**, and the reported Tempe→Boston regression
- **Mechanism:** Channels 1, 6 and 11 are the non-overlapping 2.4 GHz set and channel 1 is typically
  the most congested in an institutional building. Co-channel traffic does not collide destructively so
  much as consume airtime through CSMA deference — every neighbouring AP's frames are airtime the pods
  cannot use. That load varies by hour and by day, which is exactly the profile of "sometimes all 15
  connect and run fine". The hub's own soft-AP beacons (typically every 102 ms) add to this, for a
  service that may have no clients at all.
- **Falsifying measurement:** Experiment E6 — run the identical session at 2 pm and at 10 pm in the
  same room with I1 logging, and separately do a channel survey (any Wi-Fi analyser) to rank channel
  occupancy. If per-node rates are unchanged between the two times, congestion is not the driver.
- **Proposed change:** After measurement, move both sides to the least-occupied of 1/6/11. This is a
  one-line change on each side but **both must change together** or the link fails entirely.
- **Cost:** 2 lines.
- **Risk of the fix:** A mismatch between pod and hub channel constants breaks all communication —
  which the firmware comments identify as a past failure mode. Flash order matters.

### NET-07 — Max TX power on 15 co-located transmitters is not obviously correct
- **Severity:** RISK
- **Confidence:** low
- **Location:** `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:478`;
  `Device code/Dongle_Binary/Dongle_Binary.ino:301`
- **Evidence:** `[fact-code]` `esp_wifi_set_max_tx_power(80)` (20 dBm) on both, with the pod comment
  reasoning "the dongle 1-3 m away in a busy mocap volume, every dB helps".
- **Explains symptoms:** possible minor contributor to S1
- **Mechanism:** At 1–3 m the link budget is generous, so extra power buys little wanted signal.
  Meanwhile every node transmitting at maximum raises the received power of *interferers* at every
  other node, widening each node's CSMA deference radius and, in the near field, risking receiver
  desensitisation at the hub. Raising everyone's power in a dense co-located group does not improve
  relative SNR — it is a common-mode change. `[inference]` from standard CSMA behaviour; the effect
  size here is genuinely uncertain, hence low confidence.
- **Falsifying measurement:** Sweep TX power (e.g. 80 / 60 / 40) across a full-suit session with I1 and
  I2 (RSSI) logging. If aggregate delivered rate is flat or improves as power drops, the maximum is
  not optimal. If it degrades, this finding is wrong and max power is correct.
- **Proposed change:** None until measured. This is a cheap sweep, not a code change.
- **Cost:** measurement only.
- **Risk of the fix:** Reducing power reduces worn-body margin; must be tested worn (E5).

## 5. Symptom attribution

| Symptom | Contributes? | Evidence |
|---|---|---|
| **S1** degrades with node count | **Yes — primary scaling term** | NET-01: airtime scales with packet count × node count. Under Case A the knee sits at the current operating point. |
| **S2** oscillates between values | **Yes** | NET-02 retry feedback and NET-03 phase alignment both produce discrete operating points rather than smooth decay. |
| **S3** only ~4 nodes connect | Weak | Airtime saturation degrades all nodes together; it does not select four. NET-03 worsens it, but the primary S3 mechanism is node-side (NODE-04) and hub-side (HUB-03). |
| **S4** sometimes all 15 fine | **Yes** | NET-06: channel-1 congestion varies by time of day; NET-03 phase alignment decays over a session. |
| **S5** 60 Hz fails with many nodes | **Yes** | NET-01 Case A: 60 Hz × 15 nodes is 106% of airtime — impossible. Note SENS-01 independently caps the DMP at ~55 Hz, so both ceilings bind. |
| **S6** yaw drift | No evidence | Transport does not affect orientation estimation, except that dropped samples reach A5 as gaps. |

## 6. What I could not determine

- `[unknown]` **The actual ESP-NOW PHY rate.** No configuration call exists, so it is the IDF default,
  and that default depends on the IDF version — which is itself unknown (A1/A2). **This is the single
  most consequential unknown in this report**: it swings channel utilisation between 12% and 57% at
  the current operating point. Resolved by a monitor-mode capture or by forcing the rate and
  re-measuring.
- `[unknown]` **Actual retry counts.** ESP-NOW does not surface the hardware retry count through the
  Arduino API; only success/failure per send. Resolved only by sniffer capture.
- `[unknown]` **Real channel occupancy at the deployment site.** Requires a survey (E6).
- `[unknown]` **Whether the hub's soft-AP has clients during capture.** If not, its beacons are pure
  airtime cost. Resolved by I7.
- `[unverified]` The 802.11b/g timing constants and the ~59-byte ESP-NOW frame size are from published
  standards and ESP-NOW documentation; the exact frame overhead should be confirmed by capture.

## 7. Measurements I need

1. **The PHY rate**, by sniffer capture on channel 1 or by forcing it and re-measuring. Everything in
   NET-01 hinges on this. **Highest priority in this report.**
2. **I1 — per-node packets per second at the hub.** Distinguishes uniform airtime sag (all nodes drop
   together) from node-specific collapse (body shadowing) from queue effects (a cliff after a delay).
3. **I2 — per-packet RSSI at the hub**, free to obtain (NET-05).
4. **Send-failure rate per node**, from the existing `dccount` path. Gates NET-02.
5. **E5 — bench control:** 15 nodes on a table, hub 1 m away, no human, same session length. If the
   problem disappears the cause is body absorption; if it persists it is airtime or configuration.
   This single experiment separates C7 from NET-01 and should be run before any RF conclusion.
6. **E6 — time-of-day repeat** plus a channel survey. Gates NET-06.
7. **E7 — batching trial** on a subset of nodes. Gates NET-04, but only after owner question 7
   (latency budget) is answered.
