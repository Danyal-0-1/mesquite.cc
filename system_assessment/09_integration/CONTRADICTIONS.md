# Contradictions and Referee Rulings

Where reports disagree, or where a report disagrees with the master prompt, both positions are stated
and a single resolving measurement is named.

---

## C-01 — Is the radio the bottleneck?

**A3 (Network):** airtime is the primary scaling term; at 1 Mbps, 15 nodes × 32 Hz consumes ~57% of
channel airtime and 60 Hz is oversubscribed at 106%.
**A1 (Node firmware):** the DMP cannot produce 60 Hz at all (~55 Hz ceiling), and the transmit task is
independently capped at ~32 Hz by tick quantisation. The radio is never asked for 60 Hz.
**A4 (Webapp):** per-packet main-thread cost (~480 DOM+moment.js operations/second) saturates the
event loop and starves the serial reader, which presents as a network problem.

**Ruling:** Not a genuine contradiction — three ceilings at different points, and A1's binds first.
A3's arithmetic stands but is conditional on an unmeasured PHY rate. **The radio is not declared the
bottleneck** (§C8's standing rule respected).

**Resolving measurement:** Capture the ESP-NOW PHY rate on channel 1 with a monitor-mode sniffer, or
force 6 Mbps via `esp_wifi_config_espnow_rate()` and re-measure per-node delivered rate (I1). If
forcing 6 Mbps does not improve aggregate throughput at 15 nodes, airtime is not binding and A1/A4's
ceilings are the whole story.

---

## C-02 — What causes S3, the four-node symptom?

**Master prompt (§4.1):** `WiFi.softAP`'s `max_connection` default of 4 throttles the fleet. Called
"the highest-value clue in the list" and assigned as A2's first task.
**A2 (Hub):** impossible. `[fact-code]` pods call `WiFi.mode(WIFI_STA)` and never `WiFi.begin()`;
they reach the hub over ESP-NOW, which is connectionless. No AP client cap can gate them.
**A1:** a pod whose IMU or DMP fails to initialise executes `while (1) ;` and hangs forever
(`Pod_Watch_Binary.ino:377-384`), never transmitting.
**A2 additionally:** any inbound serial byte triggers `sendReset()`, rebooting every pod
(`Dongle_Binary.ino:333-339`).
**A4/A7:** an unterminated JSON line leaves `_jsonLine` non-empty forever, permanently halting all
binary parsing (`js/webserialnative.js:212,234`).

**Ruling:** The master prompt's hypothesis is **refuted by code**. Three latching mechanisms replace
it, all consistent with S3 *and* S4 because each is sticky and each fires probabilistically.
Experiment E1 as written cannot produce the symptom and should be retired.

**Resolving measurement:** I1 (per-node packet counts at the hub) plus a boot-time init-success print
per pod. These separate the three: pods that never appear at the hub are init hangs; a simultaneous
fleet-wide dropout is a reset; pods arriving at the hub but absent in the browser is the parser hang.
**One measurement, three hypotheses discriminated.**

---

## C-03 — Is sensor-to-segment calibration missing?

**Master prompt (§A6):** "If ignored, this is likely a dominant error source."
**A6 (Kinematics):** it is not ignored. `[fact-code]` `js/custom_icm.js:268-298` solves
`offset = q_expectedBone · q_sensor⁻¹` from a T-pose, with axis-flip disambiguation, and applies it at
every bone (`:588-898`).
**A5 (Estimation):** the measured error pattern rules it out as dominant. `[fact-data]` global RMSE is
flat at 43.8–53.9° across every joint **including the root** (44.24), whereas a calibration error
compounds down each chain and would make distal joints markedly worse.

**Ruling:** Refuted. Calibration is implemented and the data exonerates it. Effort should go to the
whole-body heading/root problem instead.

**Resolving measurement:** Recompute per-joint error after per-frame optimal yaw alignment. If distal
joints then separate from proximal, a chain error is hiding under the frame error and this ruling is
incomplete. **Caveat:** A6 cannot confirm from source that a T-pose is actually *performed* in normal
operation — that is an owner question, and a negative answer would reopen this.

---

## C-04 — Where does the yaw error come from: drift or initialisation?

**Master prompt (§A5):** heading error is "a random walk that grows without bound" with the
magnetometer disabled.
**A5:** the observability argument is correct, but the arithmetic does not fit. `[fact-doc]` the
ICM-20948's 0.015 °/s/√Hz gives ~0.12° at 1 min and ~0.37° at 10 min. `[fact-data]` the observed
`yaw_alignment_degrees` is **−71.19°** and **+99.16°** in two sessions minutes apart. Random walk is
**two orders of magnitude too small** to explain it.

**Ruling:** The mechanism is misdiagnosed. The magnitude and the session-to-session sign change point
to an **uncorrected arbitrary initial heading** — each DMP initialises to its own yaw origin, and
nothing aligns them to each other or to the WebXR world frame (A7's PHN-04) — not to accumulated
drift. These demand different fixes: a per-session alignment versus bias estimation or a heading
reference.

**Resolving measurement:** **E11** — one node, stationary, 10 minutes, full-rate quaternion log. Tens
of degrees of drift implicates the random walk; a couple of degrees implicates initialisation. This is
the highest-value estimation measurement available and it needs one node and no code change.

---

## C-05 — Is the browser losing packets, or is the radio?

**A4:** the parser is **correct** — `_rxBuf` persists across reads and handles frames straddling
chunk boundaries (`js/webserialnative.js:97-99,201-204,301`). The chunk-loss hypothesis is disproved.
**A4/A7 nonetheless:** packets are lost in the browser by a different route — the JSON branch consumes
every byte once a line is open, so pod packets interleaved into phone JSON are eaten as text.
**A2:** the hub creates that interleaving by writing to `Serial` from two unsynchronised task contexts
(`Dongle_Binary.ino:201` and `:232`).

**Ruling:** Both are right about different things. Loss is real and is **created at the hub, realised
in the browser**. It is not a radio phenomenon and would not be fixed by any RF change.

**Resolving measurement:** Two otherwise identical sessions, phone WebSocket connected versus
disconnected, comparing hub per-id counts (I1) against browser per-bone counts (`window._podRx`,
already implemented). If the delivered fraction is unchanged by phone activity, this ruling is wrong.
**Requires no code changes on the browser side** — the counter already exists.

---

## C-06 — Is 30 Hz actually 30 Hz?

**Owner's framing:** "~30 Hz works; 60 Hz fails above a few nodes."
**A1:** the firmware requests **32** Hz (`fps = 32`), gated by integer division to a 31 ms period,
then quantised by `vTaskDelay(1)` to a possibly-40 ms tick multiple (25 Hz).
**A4/A8:** the export asserts exactly **30.0** Hz (`frameTime = 1/30`), confirmed in all five captures.
**A5:** the DMP produces ~55 Hz, so roughly 40% of generated samples are never transmitted.

**Ruling:** Three different rates in three places, none matching, and the "30 Hz" the owner refers to
is the *exported* figure — which is a hardcoded constant, not a measurement. The system's true
transmit rate has never been measured.

**Resolving measurement:** I4 (send-interval histogram at the node) plus wall-clock elapsed divided by
frame count at the browser. Together these give the real rate at both ends and quantify the dilation.

---

## C-07 — Does the timebase problem invalidate the existing benchmark?

**Master prompt (§C2):** unsynchronised data "poisons any comparison against OptiTrack ground truth,
making it a blocker for benchmark work."
**A8/A5:** confirmed, and the evidence is in the benchmark's own output. `[fact-data]`
`metrics_all.json` reports `latency_ms` of **−300.0** (session 01) and **−2200.0** (session 02) at
correlations of only **0.593** and **0.608**. Two recordings of the same activity minutes apart cannot
differ in latency by 1.9 seconds.
**A6 additionally:** the two skeletons have very different proportions (pelvis bone length 55.53 vs
95.92) reconciled by a single uniform scale of 1.102, and only 14 of 22 joints are compared.

**Ruling:** The master prompt is right, and the existing comparison is unsound in **two** independent
ways — progressive time dilation preventing correlation lock, and retargeting artifact contaminating
positional magnitudes. The error *pattern* (flat across joints, root-dominated) is robust to both and
can be relied on; the *magnitudes* cannot, and should not be quoted.

**Resolving measurement:** Fix the export first (record measured frame interval plus per-frame arrival
timestamps), then re-capture. Existing captures cannot be repaired — they carry no timing information
to recover. **Any resubmission using the current numbers is at risk.**

---

## C-08 — Is the DMP being wasted?

**Master prompt (§2.2):** "The DMP may be sitting unused… A1 must determine whether the DMP is
enabled, and if not, why not."
**A1:** it is enabled. `[fact-code]` `initializeDMP()` `:325`, `GAME_ROTATION_VECTOR` `:345`,
`enableFIFO()` `:365`, `enableDMP()` `:368`.
**A1 nonetheless finds a related waste:** `RAW_GYROSCOPE` and `RAW_ACCELEROMETER` are also enabled
(`:348-349`) with their ODRs left at full DMP rate, and **nothing reads them** — `:725` branches only
on Quat6.

**Ruling:** Hypothesis refuted, but the underlying instinct was sound — there *is* unnecessary sensor
work, just not the kind predicted. The fix is two commented-out lines rather than an architecture
change.

**Resolving measurement:** I9 — time `readDMPdataFromFIFO()`, then repeat with `:348-349` disabled. If
the duration does not drop materially, the waste is negligible.
