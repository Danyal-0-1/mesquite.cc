# B2 — Hub Instrumentation — Phase 2

## 1. Scope
Changed: `Device code/Dongle_Binary/Dongle_Binary.ino` — 355 → 510 lines, all behind
`#if MESQ_INSTR`. **Nothing compiled, nothing run.**

## 2. Summary
The routing decision §4.4 demanded is made: status output uses a **distinct framed message type**
(`0xAA 0x55 0xFE len payload`) on the existing USB stream, and the browser branch that decodes it is
implemented and fixture-verified. I1 — per-id receive counts plus drop-reason counters — is in
place, emitted from the existing 1 Hz `podTimeoutTask` and never from the ESP-NOW callback, because
measuring whether that callback blocks by adding writes to it would be circular. RSSI and per-node
battery are both obtained free at the receiver, so no pod-side change was needed for either.

## 3. The routing decision (§4.4)

| Option | Verdict |
|---|---|
| Raw text on the binary stream | **Rejected.** `HUB-02`, and `[fact-fixture]` F1 shows the browser's JSON branch swallows following binary bytes. |
| WebSocket | **Rejected.** That is the phone's path and shares the corruption problem being measured. §4.4 names this explicitly as an unsafe default. |
| Second UART | **Rejected.** Needs a second physical cable to every deployment. |
| **Distinct framed message type** | **CHOSEN.** |

Frame: `[0xAA][0x55][0xFE][len][payload≤250]`. Marker `0xFE` is outside the valid bone range (0–16)
and distinct from the `0xFF` control marker, so it cannot collide with pod data or the reboot packet.

**The trade, stated:** status now shares airtime-free but *bandwidth*-consuming space on the USB
link. Two ~200-byte lines per second is ~400 B/s against 16,320 B/s at the 60 Hz worst case — under
2.5%, and `HUB-08` establishes the link runs at ~18% utilisation. Acceptable.

**The hazard this created, and its fix:** a `0xAA 0x55` prefix means the *existing* browser reader
would have consumed a status frame as a 16-byte pod packet and mis-framed everything after it. The
explicit `0xFE` branch in `js/webserialnative.js:210-222` was mandatory, not optional, and is
verified by fixture test T9 including chunk-split delivery.

## 4. Implemented (all `#if MESQ_INSTR`)

| ID | What | Resolves |
|---|---|---|
| **I1** | per-id `mesq_rx[]`; drop counters at both previously-silent `return`s (bad length / bad sync / unknown id) | `HUB-05` — **the key measurement** |
| **I2** | `recv_info->rx_ctrl->rssi` per packet, per-id mean | `NET-05`, C7 |
| **I3** | `Serial.write` duration at `:201`, **max tracked** | the blocking-callback question |
| **I7** | `WiFi.softAPgetStationNum()` | `HUB-01`, `NET-06` |
| **I11** | free heap + `ws.count()` | `HUB-04` |
| **H1** | `sendReset()` invocation counter | `HUB-03` |
| **H2** | `esp_now_add_peer` failure counter | `HUB-07` |
| **H3** | `batt` byte decoded per id | `PWR-01` — free, no pod change |

Two lines per second:
```
I1 rx=32,31,32,... drop=0/0/0 wr_us(mean/max)=6/38 sta=1 heap=181204 ws=1 rst=0 peerFail=0
I2 rssi=-51,-58,-47,... batt=87,91,84,...
```

`I1`'s per-id vector is the measurement Phase 1 called the single most discriminating in the
assessment: uniform sag ⇒ shared airtime; specific ids collapsing ⇒ those pods or their body
position; a cliff after a delay ⇒ a queue filling.

## 5. Findings

### P2-B2-01 — I3 is deliberately partial and cannot see the worst case
- **Severity:** RISK · **Confidence:** medium
- **Location:** `Dongle_Binary.ino:~201`
- **Evidence:** `[fact-code]` I3 times `Serial.write` **inside** `OnDataRecv`, accumulating into
  volatile counters, and the report is emitted from `podTimeoutTask`. No print occurs in the callback.
- **Mechanism:** This measures how long `Serial.write` takes, which is the question. But if it
  blocks hard enough to stall the ESP-NOW receive path, the *lost* packets are by definition never
  counted — `mesq_rx[]` only counts packets that arrived. So I3 can show a large max duration but
  cannot directly show the resulting loss. The loss appears instead at the **pod** side, as
  send failures, and at the **browser** side, as I5 gaps. **Reading I3 alone will understate the
  problem; it must be read against I5.**
- **Falsifying measurement:** correlate I3's per-second max against I5's per-bone loss in the same
  session. Loss spikes coinciding with duration spikes confirm the stall.
- **Proposed change:** none yet; this is a reading instruction, not a defect.
- **Cost:** 0. **Risk:** misreading I3 in isolation.

### P2-B2-02 — Status frames are emitted before ESP-NOW init at boot, safely; after init, only framed
- **Severity:** QUALITY · **Confidence:** high
- **Evidence:** `[fact-code]` the boot banner uses plain `Serial.println` and runs **before**
  `esp_now_init()`, so no binary frame can interleave with it. All post-init status uses
  `mesq_emitStatus()`.
- **Mechanism:** The pre-existing `Serial.printf` calls in the WebSocket connect/disconnect handlers
  (`:240`, `:244`) are **still raw text and still a hazard** — they fire at arbitrary times after
  init. I did not change them, because that is `HUB-02`'s fix and belongs to W4 under Rule 1.
  **They remain a live source of the corruption this instrumentation is meant to measure**, which
  is worth knowing when reading the first session's data.
- **Falsifying measurement:** correlate `ws=` count changes in the I1 line against I5 loss spikes.
- **Proposed change:** W4, with the rest of `HUB-02`.
- **Cost:** deferred. **Risk:** first-session data will contain this artifact; expect it.

## 6. Not done, deliberately
`HUB-03`'s command token, `HUB-04`'s `cleanupClients()`, `HUB-02`'s write serialisation. All W3/W4.

## 7. Measurements I need
One dongle plus ≥4 pods, ten minutes, `MESQ_INSTR=1` both ends. That yields I1's per-node curve, I2's
RSSI baseline, I3's blocking answer, and H1's verdict on whether `sendReset()` fires unbidden.
