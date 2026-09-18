# OPEN QUESTIONS — Phase 2

## Blocking, and blocking everything

| # | Question | Owner | Blocks |
|---|---|---|---|
| **B1** | **Git is unusable** — `sudo xcodebuild -license` has never been accepted, so every `git` command fails. | **Owner (needs sudo)** | §4.1 entirely: no branch, no tag, no revert point. **Do not flash a fleet until this is cleared.** |
| **B2** | **No hardware attached.** | Owner | W0 provenance, W1 Rule 2 + pilot, all of W2, and via Rule 1 all of W3–W6 |
| **B3** | **Does the firmware compile?** No toolchain here; ~24 instrumentation sites are untested against a compiler. | Owner | Every flash |
| **U1** | **ESP-NOW PHY rate.** Nothing calls `esp_wifi_config_espnow_rate()`. | M3 | Swings channel utilisation **12%↔57%**; the whole bottleneck verdict |
| **U2** | **`configTICK_RATE_HZ`.** Banner written, never run. | one boot | Real transmit rate ~32 Hz or ~25 Hz; `NODE-03`; the S2 explanation |
| **U3** | **Yaw error: drift or initialisation?** | M4 / E11 | Two completely different fixes; all of `B7` |

## New this session

| # | Question | Why it matters |
|---|---|---|
| **N1** | **Does the recording path populate `window.mesqFrameTiming`?** I12 is landed but unwired — the recording buffer's location was never established (Phase 1 `WEB-04` `[unknown]`). | **M1 must not be recorded until this is closed**, or the frozen baseline inherits the same time dilation that ruined the existing captures. The export currently self-labels `ASSUMED_1_30_UNTRUSTWORTHY`. |
| **N2** | **What does the 1 Hz `Serial.printf` cost on the pod?** ~200 chars at 115200 ≈ 17 ms against a 31 ms send interval, on core 0. | Rule 2. If it blocks, the instrument perturbs `I4`, the thing it measures. |
| **N3** | **Is `MESQ_INSTR=0` genuinely behaviourally identical?** Asserted by inspection only. | If not, the baseline measures something other than the system it claims to. |
| **N4** | **Does `WEB-02` ever latch in the field?** F1 shows it self-clears in ~34 ms under ordinary traffic, but a permanent latch is reproducible when payload bytes avoid `0x0A`. | Decides whether the `_jsonLine` cap is urgent or routine. I6 logs `_jsonLine.length`. |
| **N5** | **Is the replay harness's copy of the parser drifting from the real one?** It is kept in sync by hand. | If `feedSerialBytes` changes and the harness does not, the tests validate dead code. Extracting a shared module is the fix (W4). |

## Carried forward from Phase 1, still open

Owner questions **4** (room layout), **6** (**is there an OptiTrack pairing?** — §1.5 puts this on the
publication critical path), **7** (**latency budget** — blocks `NET-04` batching entirely), **8**
(**is 60 Hz a hard requirement or a proxy for choppiness?** — `SENS-01` shows the DMP tops out near
55 Hz, so this now determines whether an acquisition redesign is even in scope), **9** (battery life,
mid-capture resets), **10** (does deployed firmware match this workspace — `INT-01`).

Also open: whether a T-pose is actually performed in normal operation (reopens `KIN-02` if not);
whether the benchmark captures used the current firmware; which of the eight `trees/` variants was
used; the DMP's frame convention; and the full ten-question phone list in
`system_assessment/07_phone_slam/REPORT.md` §7.

## Answered this session

| Question | Answer |
|---|---|
| Does `WEB-02` halt parsing permanently? | **No** — self-clears in ~16 packets (~34 ms fleet-wide). Permanent only when payload avoids `0x0A`. `[fact-fixture]` |
| Does `HUB-02` interleaving actually destroy data? | **Yes, and worse than stated** — both the pod packet *and* the JSON line are lost. `[fact-fixture]` |
| Is the parser's chunk handling correct? | **Yes** — 50/50 across 7-byte chunks. `[fact-fixture]` |
| Are there relocalization jumps in the root? | **No** — no discontinuity in ~7,100 frames. The large jumps are the reference's trimmed frame 0. `[fact-data]` |
| How exposed is the export to gimbal lock? | **Up to 23.9% of frames**, because `XYZ` puts yaw in the middle slot; the reference's `YXZ` scores 0.0%. `[fact-data]` |
| Does the I5 reboot guard work? | **Yes** — a counter reset gives 0 lost, 1 resync instead of 65,535 phantom losses. `[fact-fixture]` |
