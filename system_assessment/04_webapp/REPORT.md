# A4 — Webapp & Real-Time Data Path — Assessment

## 1. Scope and files read

| File | Size | Read |
|---|---|---|
| `js/webserialnative.js` | 16 KB / ~430 lines | **In full** |
| `js/custom_icm.js` | 60 KB | Data path, calibration and handler paths read; three.js scene helpers skimmed |
| `js/bvh_converter.js` | 5 KB | **In full** |
| `js/threejsscene.js` | 26 KB | Render loop and animation coupling |
| `index.html` | 23.6 KB | Script loading, inline logic |
| `js/mappings.js` | 1 KB | Bone mapping |

Skipped and why: `js/three.min.js`, `js/jsm/`, `js/loaders/`, `js/shaders/`, `js/postprocessing/`,
`node_modules/`, `build/`, `build-static/` — vendored three.js, not project code.
`Mesquite_benchmarks/pod_watch__1_/code/mesquite.cc/` — stale duplicate of this webapp.

## 2. Summary

The byte-stream parser is **correct** — it keeps a persistent carry buffer across reads and handles
frames straddling chunk boundaries, so the chunk-loss hypothesis is disproved. The real defect is in
the handler: `handleWSMessage` performs a `moment.js` relative-time format, an `innerHTML` rebuild and
a jQuery selector **per packet**, roughly 480 times a second at 15 nodes, all on the main thread. The
parser's JSON branch consumes every byte once a line is open, so pod packets interleaved into phone
JSON by the hub (HUB-02) are silently eaten as text. The BVH exporter hardcodes `frameTime = 1/30`
while the firmware samples at 32 Hz, giving every exported capture a ~7% time-base dilation.
Rendering is properly decoupled via `requestAnimationFrame`, so the render loop is not throttling
ingest.

## 3. How this subsystem actually works

- Input is **USB serial via WebSerial**, not a WebSocket `[fact-code]` `js/webserialnative.js:307`
  `navigator.serial.requestPort()`. The port opens at `baudRate: 115200` `[fact-code]` `:319`, while
  the hub declares 921600 — harmless under native USB CDC (see HUB-08) but the accompanying comment
  `:313-318` justifying 115200 is stale relative to the current dongle.
- The stream is **dual-mode** `[fact-code]` `:16-25`: 16-byte binary pod frames identified by
  `0xAA 0x55`, and JSON text lines from the phone forwarded verbatim by the dongle's WebSocket
  handler.
- Parser state is module-scoped and **survives across reads** `[fact-code]` `:97-99`:
  `let _rxBuf = new Uint8Array(0); let _jsonLine = "";` with the explicit comment "a single 16-byte
  frame can straddle two reads". `feedSerialBytes` appends to `_rxBuf` `:201-204` and, after the parse
  loop, keeps the unconsumed tail: `_rxBuf = _rxBuf.subarray(i)` `:301`.
- Branch A (binary) `[fact-code]` `:210-232`: requires `b === SYNC0 && _jsonLine.length === 0`, checks
  the second sync byte, waits for a full 16 bytes, then `unpackPodPacket` `:68-95` and
  `handleWSMessage`.
- Branch B (JSON) `[fact-code]` `:234-296`: triggered by `{` **or by `_jsonLine.length > 0`**,
  accumulating characters until `\n`, with a two-stage parse and a legacy repair pass `:178-190`.
- `count` and `ms_lo` are both unpacked `[fact-code]` `:88-93`.
- Rendering is decoupled: `animate()` drives `requestAnimationFrame(animate)` `[fact-code]`
  `js/threejsscene.js:589-592`, independent of packet arrival.
- Sensor-to-segment calibration exists `[fact-code]` `js/custom_icm.js:12-18`: `mountingOffsets` and
  `tposeOffsets`, the latter loaded from `trees/meta.json` `:98-123`.
- BVH export writes `frameTime = 1/30` `[fact-code]` `js/bvh_converter.js:157`, with channel order
  `Xrotation Yrotation Zrotation` `:124,127` and Euler extraction in `"XYZ"` order `:8`.

## 4. Findings

### WEB-01 — Per-packet DOM and moment.js work on the main thread
- **Severity:** PERFORMANCE
- **Confidence:** high
- **Location:** `js/custom_icm.js:519-560`
- **Evidence:** `[fact-code]` inside `handleWSMessage`, executed once per received packet:
  `moment(new Date().getTime() - millis)` and `t.fromNow(true)` `:527-528`; a multi-hundred-character
  template-string `innerHTML` assignment `:558`; `$("#" + ... + "Status").addClass("connected")` `:560`;
  and `statsObjs[lowerFirstLetter(bone)].update()` `:517`.
- **Explains symptoms:** **S1, S2**
- **Mechanism:** At 15 nodes × 32 Hz this runs ~480 times per second; at the 60 Hz target it would be
  ~900. `moment().fromNow()` is a well-known expensive call, and assigning `innerHTML` forces the
  browser to reparse HTML and invalidate layout for that element — per packet, per bone. All of it is
  on the main thread, competing with the WebSerial read loop and `requestAnimationFrame`. When the
  main thread saturates, the serial reader is starved, the OS buffer backs up, and throughput drops in
  steps as the event loop falls behind — presenting exactly as a "network" problem. This is the
  bottleneck the master prompt anticipated for A4, and it is real.
- **Falsifying measurement:** In DevTools Performance, record 30 s of a 15-node session and read the
  main-thread self-time of `handleWSMessage`, `moment`, and recalculate-style. Independently, gate the
  status-UI block behind a flag that updates at most 4 Hz per bone and compare `window._rxMode.binary`
  throughput before and after. If throughput is unchanged, this finding is wrong.
- **Proposed change:** After measurement, decouple the status UI from packet arrival — accumulate
  counters per bone and repaint on a timer (4–10 Hz) or inside the existing `requestAnimationFrame`
  loop. The data path itself needs none of this work.
- **Cost:** ~30 lines.
- **Risk of the fix:** Status chips update less smoothly; visually irrelevant.

### WEB-02 — An unterminated JSON line permanently blocks all binary parsing
- **Severity:** BLOCKER
- **Confidence:** high
- **Location:** `js/webserialnative.js:212`, `:234-236`, `:294-296`
- **Evidence:** `[fact-code]` Branch A is guarded by `if (b === SYNC0 && _jsonLine.length === 0)`
  `:212`. Branch B is entered by `if (b === 0x7B /* '{' */ || _jsonLine.length > 0)` `:234` and
  `_jsonLine` is only cleared when a `\n` is seen `:247`. There is no length cap, no timeout, and no
  resynchronisation path out of an open JSON line.
- **Explains symptoms:** **S3, S4**
- **Mechanism:** If a `{` arrives and its terminating newline never does — a truncated WebSocket
  message, a dropped byte, a hub reset mid-line — `_jsonLine` stays non-empty forever. From that
  moment Branch A can never fire, so **every subsequent binary pod packet is consumed one byte at a
  time as JSON text** and silently discarded. All pods appear to stop at once, and only a page reload
  recovers it. `_jsonLine` also grows without bound. Combined with HUB-02, where the hub interleaves
  writes from two task contexts with no mutual exclusion, a pod packet's bytes landing inside a JSON
  line are guaranteed to be eaten — and a 16-byte binary packet containing a `\n` (0x0A) byte will
  terminate the JSON line at an arbitrary point, leaving the remainder to be misparsed.
- **Falsifying measurement:** During a session with the phone active, poll `window._rxMode.binary` and
  `window._rxLines` once a second. If `binary` ever plateaus while `window._rxBytes` keeps climbing,
  this failure has occurred. Also log `_jsonLine.length` — a value that grows monotonically past a few
  hundred characters is the smoking gun.
- **Proposed change:** After measurement, cap `_jsonLine` length (a JSON pose line is well under
  512 bytes) and abandon the line on overflow; additionally allow Branch A to fire on a valid
  `0xAA 0x55` pair even mid-line. Properly, fix the framing at the hub (HUB-02) so the two streams are
  distinguishable.
- **Cost:** ~10 lines in the browser; ~40 to fix framing properly at the hub.
- **Risk of the fix:** A length cap could truncate a legitimately long phone message; set it well
  above the observed maximum.

### WEB-03 — BVH export asserts 30 Hz for data captured at ~32 Hz
- **Severity:** CORRECTNESS
- **Confidence:** high
- **Location:** `js/bvh_converter.js:157,161`
- **Evidence:** `[fact-code]` `frameTime = 1/30;` then `bvhContent += \`Frame Time: ${frameTime}\n\``.
  `[fact-data]` every exported capture carries `Frame Time: 0.03333333333333333` —
  `Mesquite_benchmarks/data/mesquite_smooth/may8th2026/MMcap_bvh_2026-6-8-13-20-59.bvh` and
  `-13-22-52.bvh`, and all three files in `Mesquite_benchmarks/Mocab may 20th/`.
  `[fact-code]` the node's nominal transmit rate is `fps = 32`
  (`Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:222`), and the gate uses integer division
  `1000/32 = 31` ms → ~32.26 Hz.
- **Explains symptoms:** none directly — it **corrupts the evidence** used to judge everything else
- **Mechanism:** Frames are appended as they arrive and then labelled as though they were 33.33 ms
  apart. If the true mean interval is ~31 ms, the exported file stretches the motion by roughly 7%:
  `[fact-data]` session 01 claims 2508 frames × 0.03333 s = 83.6 s, but 2508 frames at 32.26 Hz is
  77.7 s — a ~6 s discrepancy over 83 s, accumulating linearly. Because the dilation is progressive
  rather than a constant offset, cross-correlation against a ground-truth capture cannot find a single
  consistent lag, which is precisely what the benchmark shows: `[fact-data]`
  `Mesquite_benchmarks/outputs/bvh_verification/metrics_all.json` reports `latency_ms` of **-300 ms**
  for session 01 and **-2200 ms** for session 02, with correlations of only 0.593 and 0.608. Those are
  not two measurements of one latency; they are a correlator failing to lock.
- **Falsifying measurement:** Record wall-clock time at recording start and stop in the browser, divide
  elapsed seconds by the frame count, and compare to 1/30. If the measured mean interval is
  33.33 ms ±1%, this finding is wrong.
- **Proposed change:** After measurement, write the **measured** mean frame interval into the header,
  and separately record per-frame arrival timestamps so non-uniformity is visible rather than assumed.
- **Cost:** ~10 lines.
- **Risk of the fix:** Existing exported files remain wrong; any published numbers derived from them
  need revisiting. That is a finding in itself, not a reason to avoid the fix.

### WEB-04 — `ms_lo` is misinterpreted as an age, so the latency UI is meaningless
- **Severity:** QUALITY
- **Confidence:** high
- **Location:** `js/custom_icm.js:526-531`; `js/webserialnative.js:89-93`
- **Evidence:** `[fact-code]` `var t = moment(new Date().getTime() - millis);` treats `millis` as
  "milliseconds ago". `[fact-code]` the parser's own comment at `js/webserialnative.js:89-92` states
  "ms_lo wraps every ~65 s. The display code in custom_icm.js treats millis as 'ms ago' and the
  original JSON path was already broken in the same way, so we preserve behaviour". `[fact-code]` the
  firmware writes `myData.ms_lo = (uint16_t)millis()` — an absolute wrapping counter, not an age
  (`Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:647`).
- **Explains symptoms:** none — it **hides** latency rather than causing it
- **Mechanism:** The displayed figure is `now − (node uptime mod 65536)`, which is not a latency, a
  delay, or anything physically meaningful. It sweeps through a 65-second ramp regardless of actual
  transport delay. The team therefore has no working end-to-end latency readout, which is why primary
  question 5 (the latency budget) cannot currently be answered from the running system.
- **Falsifying measurement:** n/a — established by reading; the code comments concede it.
- **Proposed change:** Requires the sync scheme in SYNC-04 before a true latency can be shown. As an
  interim, display arrival-interval statistics per bone, which are meaningful and cheap.
- **Cost:** ~10 lines interim.
- **Risk of the fix:** None.

### WEB-05 — `_rxBuf` is reallocated and copied on every read
- **Severity:** PERFORMANCE
- **Confidence:** medium
- **Location:** `js/webserialnative.js:201-204`
- **Evidence:** `[fact-code]` `_appendBytes` allocates `new Uint8Array(_rxBuf.length + chunk.length)`
  and performs two `set()` copies on every call, and `feedSerialBytes` calls it for every chunk `:207`.
- **Explains symptoms:** minor contributor to S1
- **Mechanism:** Every serial read allocates a fresh buffer and copies both the retained tail and the
  new chunk. `_rxBuf = _rxBuf.subarray(i)` `:301` returns a **view**, not a copy, so the underlying
  ArrayBuffer of the original chunk is retained until the next append — creating steady garbage
  pressure at hundreds of reads per second. It is far smaller than WEB-01 but sits on the same
  starved main thread.
- **Falsifying measurement:** DevTools Memory allocation-sampling over 30 s of streaming; check whether
  `_appendBytes` appears as a significant allocation site and whether minor-GC frequency is material.
- **Proposed change:** After measurement, use a fixed-capacity ring buffer with read/write offsets.
- **Cost:** ~30 lines.
- **Risk of the fix:** Ring-buffer index bugs are easy to introduce; only worth it if measurement
  justifies it. Fix WEB-01 first — it is far larger.

### WEB-06 — The Kalman smoothing path is dead code
- **Severity:** QUALITY
- **Confidence:** high
- **Location:** `js/bvh_converter.js:8-10`, `:15-31`; `index.html:20`
- **Evidence:** `[fact-code]` `index.html:20` loads `kalmanjs@1.1.0`. `js/bvh_converter.js:15-31`
  defines `smoothOrientation` using three per-axis `KalmanFilter` instances, but its only call site is
  **commented out** at `:10`: `// euler = smoothOrientation(euler, bone);`.
- **Explains symptoms:** none
- **Mechanism:** A dependency is loaded and a filter implemented, but no filtering occurs on the export
  path. This matters for A5's error budget: it means **no host-side smoothing is active**, so all
  observed drift and noise is attributable to the DMP and the pipeline, not to a browser filter. It
  also means the "mesquite_smooth" directory name in the benchmark data may be misleading.
- **Falsifying measurement:** n/a — read directly. Worth confirming with the owner whether the
  benchmark captures were produced with this line enabled at the time.
- **Proposed change:** None in Phase 1. Record that filtering is inactive.
- **Cost:** 0.
- **Risk of the fix:** n/a.

### WEB-07 — Rotation order differs from the reference system (correctly declared)
- **Severity:** RISK
- **Confidence:** high
- **Location:** `js/bvh_converter.js:8`, `:124`, `:127`
- **Evidence:** `[fact-code]` Euler extracted with `setFromQuaternion(q, "XYZ")` `:8` and channels
  declared `Xrotation Yrotation Zrotation` `:124,127` — self-consistent. `[fact-data]` the exported
  files declare `CHANNELS 3 Xrotation Yrotation Zrotation`, while the Rokoko reference declares
  `CHANNELS 3 Yrotation Xrotation Zrotation`.
- **Explains symptoms:** none — but it is a portability trap
- **Mechanism:** Mesquite's export is internally consistent, and `[fact-code]`
  `Mesquite_benchmarks/scripts/compare_bvh_suits.py:321-331` composes rotations by iterating each
  file's **declared** channel order, so the benchmark comparison is valid on this axis. The risk is
  external: many BVH importers assume a fixed ZXY or YXZ order and ignore the declared channels, which
  would silently mis-rotate every joint on import into another tool.
- **Falsifying measurement:** Import one exported BVH into a second tool (Blender, MotionBuilder) and
  compare the rendered pose against the webapp's own view at the same frame. Divergence indicates the
  importer is ignoring the declared order.
- **Proposed change:** None to code. Document the declared order alongside released captures.
- **Cost:** 0.
- **Risk of the fix:** n/a.

## 5. Symptom attribution

| Symptom | Contributes? | Evidence |
|---|---|---|
| **S1** degrades with node count | **Yes — major** | WEB-01: per-packet main-thread cost scales linearly with node count × rate. WEB-05 adds GC pressure. |
| **S2** oscillates between values | **Yes** | WEB-01: event-loop saturation produces step changes as the reader is starved and recovers, not smooth decay. |
| **S3** only ~4 nodes connect | **Yes** | WEB-02: once a JSON line hangs open, binary parsing stops permanently and pods appear to vanish together. |
| **S4** sometimes all 15 fine | **Yes** | WEB-02 is triggered by a race (phone traffic interleaving with pod packets), so it fires unpredictably. |
| **S5** 60 Hz fails with many nodes | **Yes** | WEB-01's cost is per packet; 900 packets/s roughly doubles the main-thread load versus 32 Hz. |
| **S6** yaw drift | No evidence | The browser applies calibration offsets but performs no orientation estimation; WEB-06 confirms filtering is inactive. |

## 6. What I could not determine

- `[unknown]` **Actual main-thread time spent in `handleWSMessage`.** Requires a DevTools profile
  during a live 15-node session; no capture of that exists. Gates WEB-01's severity.
- `[unknown]` **Whether WEB-02 has actually occurred in the field**, or is only latent. Requires the
  `_jsonLine.length` logging described above during a phone-active session.
- `[unknown]` **The real mean frame interval of recorded sessions.** The exported files assert 30 Hz
  and carry no arrival timestamps, so the true rate is unrecoverable from existing captures. This is
  why WEB-03 cannot be quantified retrospectively — only prospectively.
- `[unknown]` **Where recording accumulates frames and whether it is bounded.** `js/custom_icm.js:357`
  shows a `#recordButton`, but the accumulation buffer was not located in the read portions; a long
  session's memory growth is unassessed.
- `[unknown]` **Whether the benchmark captures were recorded with `smoothOrientation` enabled.** The
  call is commented out now; the directory name `mesquite_smooth` suggests it may not always have
  been. Owner question.
- `[unverified]` `moment.fromNow()`'s per-call cost is asserted from general knowledge, not measured
  here.

## 7. Measurements I need

1. **A DevTools Performance profile** of a 15-node session, 30 s, reporting main-thread self-time by
   function. Gates WEB-01 and WEB-05, and is the single most useful webapp measurement.
2. **I6 — arrival timestamps and per-second binary-frame counts.** `window._rxBytes` and
   `window._rxMode.binary` already exist (`js/webserialnative.js:105`, `:65`); they need only be
   sampled once a second and logged rather than inspected by hand.
3. **`_jsonLine.length` logged once a second.** Directly tests WEB-02.
4. **Wall-clock start/stop around recording**, plus per-frame arrival timestamps. Gates WEB-03 and is
   required before any further ground-truth comparison (see A8 and A9).
5. **Per-bone arrival-interval histograms** in the browser, to cross-check the hub's I1 and localise
   loss to transport versus parser.
