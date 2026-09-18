# FINDINGS — Master Ledger

All findings, deduplicated, ranked by **(impact × confidence) ÷ cost**. `CORRECTNESS` outranks
`PERFORMANCE` at equal score, per §8.

Full detail for each ID is in the owning agent's report.

## Tier 1 — Do these first (high impact, near-zero cost)

| Rank | ID | Severity | Conf. | Title | Cost | Explains |
|---|---|---|---|---|---|---|
| 1 | `SYNC-03` | CORRECTNESS | high | Sequence numbers parsed but gaps never detected — loss is invisible | **~10 lines, browser only** | masks S1–S5 |
| 2 | `HUB-05` | PERFORMANCE | high | Hub has no per-node receive counter (instrumentation I1) | ~20 lines | masks S1, S3 |
| 3 | `SENS-01` | **BLOCKER** | high | DMP ceiling is ~55 Hz — 60 Hz is unreachable by construction | measurement only | **S5** |
| 4 | `SENS-02` | PERFORMANCE | high | Unused raw accel/gyro inflate every FIFO read | **2 lines** | S1, S5 |
| 5 | `NET-05` | QUALITY | high | RSSI available free at the hub, discarded (I2) | ~5 lines | makes C7 testable |
| 6 | `PHN-01` | CORRECTNESS | high | Position guard is a truthiness test — a zero coordinate drops the frame | **1 line** | root freezes |
| 7 | `SENS-03` | CORRECTNESS | medium | Unclamped `sqrt` yields NaN → degenerate zero quaternion | **1 line** | frame corruption |

## Tier 2 — Correctness defects that produce plausible-looking wrong data

| Rank | ID | Severity | Conf. | Title | Cost | Explains |
|---|---|---|---|---|---|---|
| 8 | `WEB-02` / `PHN-02` | **BLOCKER** | high | Unterminated JSON line permanently halts all binary parsing | ~10 browser / ~40 hub | **S3, S4** |
| 9 | `HUB-02` | CORRECTNESS | high | Two task contexts write one unframed serial stream, no mutex | ~40 lines | S1, S2 |
| 10 | `SYNC-01` | **BLOCKER** | high | 15 independent `millis()` origins — timestamps not mutually comparable | see `SYNC-06` | silent |
| 11 | `SYNC-07` / `WEB-03` / `EST-05` | CORRECTNESS | high | Export hardcodes 1/30 s over ~32 Hz data → ~7% progressive dilation | ~10 lines | corrupts all evidence |
| 12 | `NODE-01` / `EST-06` | CORRECTNESS | high | Quaternion shared across cores with no synchronisation → torn reads | ~15 lines | S6 |
| 13 | `NODE-02` / `SYNC-02` | CORRECTNESS | high | `ms_lo` stamps transmit time, not sample time | ~5 lines | S6 |
| 14 | `SYNC-08` | CORRECTNESS | medium | Missing samples silently hold last value, recorded as real data | ~15 lines | S6, judder |
| 15 | `SYNC-05` | CORRECTNESS | medium | Arrival-stamping converts jitter to **3–6° of joint error** | see `SYNC-06` | S6 |
| 16 | `EST-01` | CORRECTNESS | high | Yaw unobservable; observed **71–99°** error is initialisation, not drift | measurement first | **S6** |
| 17 | `PHN-04` | CORRECTNESS | medium | WebXR world frame and IMU heading frame never reconciled | deferred | S6 |

## Tier 3 — Latching failures behind the intermittency (S3/S4)

| Rank | ID | Severity | Conf. | Title | Cost | Explains |
|---|---|---|---|---|---|---|
| 18 | `NODE-04` | RISK | high | Pod hangs forever (`while(1);`) if IMU or DMP init fails | ~10 lines | **S3, S4** |
| 19 | `HUB-03` | RISK | high | **Any** inbound serial byte reboots the entire suit | ~5 lines | **S3, S4** |
| 20 | `NODE-05` | QUALITY | high | Per-node identity by comment-toggling; duplicate `sendID` is silent | 0 (use existing check) | S3 |
| 21 | `HUB-07` | RISK | medium | `esp_now_add_peer` return unchecked; `peerMacsInit` set before the call | 3 lines | reset path only |
| 22 | `HUB-04` | RISK | medium | `ws.cleanupClients()` never called — leaks over a session | **1 line** | long-session decay |

## Tier 4 — Performance

| Rank | ID | Severity | Conf. | Title | Cost | Explains |
|---|---|---|---|---|---|---|
| 23 | `WEB-01` | PERFORMANCE | high | Per-packet `moment.js` + `innerHTML` + jQuery — ~480/s on the main thread | ~30 lines | **S1, S2, S5** |
| 24 | `NET-01` | PERFORMANCE | medium | Airtime **57%** at 32 Hz / **106%** at 60 Hz *if* PHY rate is 1 Mbps | 1–2 lines + measurement | **S1, S5** |
| 25 | `NODE-03` | PERFORMANCE | high | Transmit gate integer-truncated to 31 ms, then tick-quantised | ~5 lines | **S2** |
| 26 | `NET-02` | PERFORMANCE | medium | Unicast retry feedback produces discrete rate steps | ~10 lines | **S2** |
| 27 | `NET-03` | PERFORMANCE | medium | Fleet-wide reset leaves all nodes transmitting in phase | **~3 lines** | S1, S2, S4 |
| 28 | `NET-04` | PERFORMANCE | high | No batching; 4-sample batching saves ~71% airtime | ~30 lines | S1, S5 |
| 29 | `PWR-01` | PERFORMANCE | medium | Battery read + display redraw every 3 s on the sample task | ~10 lines | S2 |
| 30 | `PWR-02` | PERFORMANCE | medium | Backlight on and touch polled during capture | ~5 lines | S3/S4 via sag |
| 31 | `WEB-05` | PERFORMANCE | medium | `_rxBuf` reallocated and copied on every read | ~30 lines | S1 (minor) |
| 32 | `NET-06` | RISK | medium | Channel 1 hardcoded, hub's own soft-AP beaconing on it | 2 lines | **S4** |

## Tier 5 — Quality, and negative results worth recording

| ID | Severity | Title |
|---|---|---|
| `KIN-01` | CORRECTNESS | **No kinematic-constraint drift correction** — the one fix class that addresses yaw without a magnetometer. Highest-value Phase 2 direction. |
| `EST-04` | CORRECTNESS | 15 chips treated as identical; no per-sensor calibration exists |
| `WEB-04` | QUALITY | `ms_lo` misread as an age — no working latency readout exists |
| `KIN-05` | QUALITY | No joint limits; benchmark shows spine ROM **46.2° vs 12.0°** reference |
| `PHN-03` | RISK | No relocalization handling; a SLAM jump goes straight to the root |
| `PHN-05` | RISK | Root Euler in XYZ order passes within **5.4°** of the gimbal singularity |
| `KIN-06` | RISK | Skeletons differ in proportion (pelvis 55.5 vs 95.9), one uniform scale applied |
| `SYNC-10` | QUALITY | Decoupled logging viable (528 KB per 10-min session) but blocked on `SYNC-06` |
| `NODE-06` | QUALITY | Dead Euler computation on every sample |
| `KIN-03` | QUALITY | Hierarchy hand-unrolled across ~18 blocks |
| `KIN-04` | QUALITY | Two calibration helpers are dead code, one mathematically invalid |
| `WEB-06` | QUALITY | Kalman smoothing path commented out — **no host-side filtering is active** |
| `HUB-06` | QUALITY | Two of three FreeRTOS tasks are empty no-ops |
| `WEB-07` | RISK | Rotation order XYZ vs reference YXZ — correctly declared, but a portability trap |
| `SYNC-09` | QUALITY | **Negative result:** crystal drift ~12 ms / 10 min — second-order, subsumed by `SYNC-06` |
| `EST-03` | QUALITY | **Negative result:** int16 quantisation = **0.006°** — negligible, do not revisit |
| `HUB-08` | QUALITY | **Negative result:** serial bandwidth 18% utilised — **not a constraint** |
| `HUB-01` | QUALITY | **Negative result:** `max_connection` cannot gate pods — §4.1 refuted |
| `KIN-02` | QUALITY | **Negative result:** flat error profile exonerates per-limb calibration |

## Symptom coverage

Every symptom has at least one candidate explanation. None is unexplained.

| | Primary explanation | Supporting |
|---|---|---|
| **S1** | `NET-01` airtime (conditional) + `WEB-01` main-thread cost | `HUB-02`, `SENS-02`, `NET-03` |
| **S2** | `NODE-03` tick quantisation | `NET-02`, `WEB-01`, `HUB-02`, `PWR-01` |
| **S3** | `NODE-04` init hang + `HUB-03` reset + `WEB-02` parser hang — **three latching modes** | `NODE-05` |
| **S4** | Same three, all probabilistic and sticky | `NET-06`, `NET-03` |
| **S5** | `SENS-01` — **DMP cannot produce 60 Hz** | `NET-01`, `WEB-01` |
| **S6** | `EST-01` unreferenced heading (**71–99°**) | `PHN-04`, `KIN-01`, `SYNC-05`, `NODE-01` |

## The three headline conclusions

1. **60 Hz is unreachable without changing the acquisition architecture.** `SENS-01` — the DMP tops
   out near 55 Hz and the transmit task is capped at ~32 Hz. This is settled by code reading; no
   network or browser work can move it.
2. **The intermittency (S3/S4) is not a throughput problem.** It is three independent *latching*
   failures — a pod that hangs forever on init, a hub that reboots the fleet on any serial byte, and a
   browser parser that stops permanently on a truncated JSON line. Each needs a power cycle or reload
   to clear, which is why the same code behaves differently on different days.
3. **The accuracy problem is whole-body, not per-limb.** `[fact-data]` global joint RMSE is flat at
   42–54° across every joint including the root, while root-relative pose RMSE is only **15–16°**.
   The articulated skeleton performs respectably; the system does not know which way the body faces
   or where it stands. **Do not replace the fusion filter** — it would address at most the smaller
   term.

## The cheapest path to knowing more

`SYNC-03` (10 lines, browser only, no wire-format change) plus `HUB-05`/I1 (20 lines at the hub)
convert every symptom in the log from an anecdote into a number, and together they discriminate
uniform airtime sag from node-specific collapse from queue-filling cliffs from parser stalls. **Every
other ranked fix should wait behind these two.**
