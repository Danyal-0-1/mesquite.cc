# Shared Briefing — Mesquite Phase 1 Assessment
READ THIS FIRST. It is verified ground truth and it CORRECTS the master prompt.

## Workspace root
`/Users/danyalkhorami/Desktop/project/PhD/Semester 1/Mesquite/mesquite.cc`
All repos are here. There is no separate `pod-mcu-code` repo; node firmware is under `Device code/`.

## File map (verified, with line counts)
| Path | Role | Lines |
|---|---|---|
| `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino` | **Node firmware, CURRENT** | 814 |
| `Device code/Pod_Watch_JSON/Pod_Watch_JSON.ino` | Node firmware, legacy | 678 |
| `Device code/Dongle_Binary/Dongle_Binary.ino` | **Hub firmware, CURRENT** | 355 |
| `Device code/Dongle__JSON/Dongle__JSON.ino` | Hub firmware, legacy | 224 |
| `Mesquite_benchmarks/pod_watch__1_/pod_watch__1_.ino` | Older node snapshot | 328 |
| `Mesquite_benchmarks/pod_watch__1_/code/dongle_combined/dongle_combined.ino` | Older hub snapshot | — |
| `index.html` | Webapp shell + inline logic | 23.6 KB |
| `js/custom_icm.js` | **Main ICM data path / skeleton logic** | 60 KB |
| `js/threejsscene.js`, `js/threejssceneplayer.js` | three.js scene | 26 KB / 13 KB |
| `js/webserialnative.js`, `js/webserial.js` | **WebSerial transport** | 16 KB / 6.5 KB |
| `js/bvh_converter.js`, `js/mappings.js` | BVH export, bone mapping | 5 KB / 1 KB |
| `Mesquite_benchmarks/` | Captures, BVH, comparison scripts, outputs | — |

Ignore `build/`, `build-static/`, `node_modules/`, `js/jsm/`, `js/loaders/`, `js/shaders/`
(vendored three.js). Also ignore `Mesquite_benchmarks/pod_watch__1_/code/mesquite.cc/` — it is a
stale duplicate copy of the webapp; note its existence but do not analyze it as current.

## CORRECTIONS TO THE MASTER PROMPT — all [fact-code], verify yourself before citing

**1. Transport is ESP-NOW, not Wi-Fi association.**
Nodes run `WiFi.mode(WIFI_STA)` and never associate. They broadcast ESP-NOW frames on a
hard-pinned channel. `Pod_Watch_Binary.ino:464,476-478` — `esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL,...)`,
`esp_wifi_set_ps(WIFI_PS_NONE)`, `esp_wifi_set_max_tx_power(80)`.
Hub: `Dongle_Binary.ino:280,293,299-301` — `WIFI_AP_STA`, `softAP(..., ESPNOW_WIFI_CHANNEL, 0)`, channel 1.
**Consequence: the §4.1 `max_connection` / four-node theory does NOT gate the nodes.** Nodes are not
AP clients. The softAP serves the browser/phone, not the pods. §4.1 must be re-scoped, not deleted —
determine what the softAP and its `AsyncWebSocket` are actually for, and whether ESP-NOW has its own
peer cap (`esp_now_add_peer`, ESP-NOW max peer limits) that produces the 4-node symptom instead.

**2. Hub→browser is USB CDC serial, not WebSocket.**
`Dongle_Binary.ino:277` `Serial.begin(921600)`; `:201` `Serial.write(incomingData, POD_PACKET_LEN)`.
Browser consumes via WebSerial (`navigator.serial`, `js/webserialnative.js:307`).
A WebSocket server exists (`Dongle_Binary.ino:127-128,255`) — determine its real role.
**921600 baud is a hard downstream ceiling. Do the arithmetic.**

**3. The DMP IS ENABLED.** The master prompt's "DMP may be sitting unused" hypothesis is wrong.
`Pod_Watch_Binary.ino:325` `initializeDMP()`; `:345` `enableDMPSensor(INV_ICM20948_SENSOR_GAME_ROTATION_VECTOR)`
(6-axis quaternion); `:348-349` RAW_GYROSCOPE and RAW_ACCELEROMETER **also enabled**;
`:357` `setDMPODRrate(DMP_ODR_Reg_Quat6, 0)` (max rate); `:365` `enableFIFO()`; `:368` `enableDMP()`.
Magnetometer line `:350` is commented out → 6-axis, yaw unobservable.
**Open question worth pursuing: why are RAW gyro+accel enabled alongside Quat6? That multiplies
FIFO bytes per DMP cycle for data that may never be transmitted.**

**4. IMU is on I²C, shared bus, 400 kHz.**
`Pod_Watch_Binary.ino:298` `Wire.begin(21, 22)`; `:301` `Wire.setClock(400000)`; `:90` `ICM_20948_I2C myICM`.
Pins 21/22 are the T-Watch sensor bus shared with PCF8563 + BMA423 + AXP192 (§C8 is live).
Not 100 kHz — E4 is already answered; the remaining C8 question is contention and blocking cost.

**5. Packets already carry a sequence number and a node-side timestamp.**
16-byte packed `pod_packet_t` (`Pod_Watch_Binary.ino:158-171`, `Dongle_Binary.ino:104-117`):
`sync0=0xAA, sync1=0x55, id, batt, qx,qy,qz,qw (int16 quantized), count (uint16), ms_lo (uint16)`.
So I5 (sequence numbers) and part of C2 (node timestamping) partly EXIST. The real questions are
whether the browser *uses* `count` and `ms_lo`, what `ms_lo` wrapping at 65.536 s does, and whether
15 independent `millis()` domains are ever reconciled. `batt` exists → part of I10 exists.

**6. `NUM_PODS 17`, not 15** (`Dongle_Binary.ino:60`). Reconcile with the 15-node claim.

## Standing rules (from the master prompt — non-negotiable)
- **CHANGE ZERO CODE.** Diagnosis only. Do not edit, refactor, or "fix" anything outside
  `system_assessment/`. Do not run builds or install anything.
- **Tag every factual statement**: `[fact-code]` (must cite `path/file.ext:LINE`), `[fact-doc]`
  (cite the doc), `[fact-data]` (cite the file + what you measured), `[inference]` (state which
  facts it rests on), `[unverified]`, `[unknown]` (one-line reason why).
  An untagged claim is a bug. If you cannot tag it, delete it.
- **The referee rule**: no proposed fix without a falsifying measurement. No exceptions.
- Do not recommend 5 GHz (hardware cannot do it). Do not propose hardware purchases.
- Do not trust the paper over the code. Where they differ, the code wins and the divergence is a finding.
- **Never conflate the seven rates (§C1).** State which one you mean, every time.
- Read the `.ino` files line by line. They are small. Do not skim.

## Required REPORT.md structure
```
# <Agent name> — Assessment
## 1. Scope and files read      (every file, with line counts; files skipped and why)
## 2. Summary                   (five sentences MAX, most important finding first)
## 3. How this subsystem actually works   (from source, not assumption; every claim tagged)
## 4. Findings                  (one block per finding, schema below)
## 5. Symptom attribution       (S1-S6: does this subsystem contribute? evidence or "no evidence")
## 6. What I could not determine (every [unknown], reason, what would resolve it)
## 7. Measurements I need
```

## Finding schema (exact)
```
### <ID> — <one-line title>
- **Severity:** BLOCKER | CORRECTNESS | PERFORMANCE | QUALITY | RISK
- **Confidence:** high | medium | low
- **Location:** `path/to/file:LINE-LINE`
- **Evidence:** [fact-code]/[fact-doc]/[fact-data] with citation
- **Explains symptoms:** S1, S3 (or "none")
- **Mechanism:** why this causes what it causes, 2-3 sentences
- **Falsifying measurement:** REQUIRED. The specific measurement that would prove this wrong.
- **Proposed change:** only after the measurement confirms it
- **Cost:** lines of code / hours / hardware
- **Risk of the fix:** what it could break
```
ID prefixes: `NODE-`, `SENS-`, `HUB-`, `NET-`, `WEB-`, `EST-`, `KIN-`, `PHN-`, `SYNC-`, `PWR-`, `INT-`.
Severity: BLOCKER = cannot work correctly until fixed. CORRECTNESS = wrong data that looks plausible
(outranks PERFORMANCE). PERFORMANCE = correct but too slow/lossy. QUALITY = fragile. RISK = will fail
under a foreseeable condition.

## Symptoms every theory must explain
- **S1** frame rate degrades as node count rises
- **S2** frame rate *oscillates* between specific values, not smooth degradation
- **S3** sometimes only ~4 nodes connect at all
- **S4** sometimes all 15 connect and run fine — same code
- **S5** 60 Hz works with few nodes, fails with many
- **S6** yaw/heading drift over a session
