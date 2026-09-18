# A7 — Phone / WebXR SLAM — Assessment

**Status: partially unblocked.** The phone *source* is still unavailable, but the receiving boundary
is fully readable and the captured data settles the most important question: **the phone path is live
and producing real global position**, not vestigial. This report documents everything establishable
from this side and lists precisely what the source must answer.

## 1. Scope and files read

`js/custom_icm.js` (phone ingestion, Hips/HipsAlt arbitration), `js/webserialnative.js` (JSON branch,
in full), `Device code/Dongle_Binary/Dongle_Binary.ino:127-128,227-258` (WebSocket → serial bridge),
`index.html` (dependency and session setup), and as evidence the root channels of
`Mesquite_benchmarks/data/mesquite_smooth/may8th2026/MMcap_bvh_2026-6-8-13-20-59.bvh` plus
`metrics_all.json` and the root-trajectory analyses under
`Mesquite_benchmarks/outputs/bvh_verification/`.

Skipped: the phone application itself — **not present in this workspace**.

## 2. Summary

Phone data reaches the browser by a genuinely surprising route: phone → dongle WebSocket → **`Serial.println`
into the same USB stream as the binary IMU packets** → browser dual-mode parser. It arrives as JSON
under the bone name `Hips`, carrying both position (`px, py, pz`) and orientation, while the hip-worn
IMU pod is `HipsAlt`; the webapp prefers the pod for orientation and falls back to the phone after a
1-second timeout. The captured BVH root translates substantially (X from 7.5 to 84.3 units), so SLAM
was live during the benchmark sessions. The position guard is a truthiness test on `px/py/pz`, so a
coordinate of exactly zero — which is precisely where a WebXR session's origin sits — silently
discards the frame. Sharing one unframed serial stream with the binary path is the root of the
framing hazard documented as HUB-02 and WEB-02.

## 3. How this subsystem actually works

- **Transport into the browser is the USB serial stream, not a direct WebSocket.** `[fact-code]` the
  dongle's `handleWebSocketMessage` writes inbound WebSocket text straight to serial:
  `Serial.println((char *)data)` — `Device code/Dongle_Binary/Dongle_Binary.ino:232`. The WebSocket
  server runs on the dongle's soft-AP at `/ws` `:127-128,255-258,291`.
- **The browser parses it as the JSON branch of a dual-mode stream** `[fact-code]`
  `js/webserialnative.js:16-25` documents exactly this: "Phone (Hips) data -> JSON lines forwarded
  verbatim from the dongle's WebSocket handler. Always start with '{' and end '\n'." Branch B handles
  it at `:234-296`.
- **The phone is `Hips`; the hip IMU pod is `HipsAlt`.** `[fact-code]` `js/custom_icm.js:922-923`:
  `if (bone == "Hips" && obj.px && obj.py && obj.pz) { obj.sensorPosition = {x: obj.px, y: obj.py, z: obj.pz}; }`
  — position arrives only under the `Hips` name. `[fact-code]` `HipsAlt` is bone id 2 in the binary
  pod table (`js/webserialnative.js:34`, `Device code/Dongle_Binary/Dongle_Binary.ino:72`).
- **Orientation arbitration is explicit and timeout-driven** `[fact-code]` `js/custom_icm.js:581`
  ("Hips orientation - use when HipsAlt is not available (timeout > 1 second)") and `:602-603`
  ("HipsAlt orientation - preferred for orientation when available"). So the phone supplies **position
  always and orientation only as a fallback** — which answers §C3's question directly.
- **SLAM was live in the benchmark captures** `[fact-data]`
  `Mesquite_benchmarks/data/mesquite_smooth/may8th2026/MMcap_bvh_2026-6-8-13-20-59.bvh`, root
  translation channels across 2508 frames: frame 0 `(7.500, 55.000, -1.500)`, frame 99
  `(21.682, 56.500, -4.000)`, frame 499 `(84.307, 54.908, -48.364)`, frame 999
  `(26.859, 61.340, -50.349)`. The subject moves tens of units in X and Z while Y stays near 55 —
  consistent with real locomotion over a floor plane, not a synthetic or static root.

## 4. Findings

### PHN-01 — Position is guarded by a truthiness test, so a zero coordinate drops the frame
- **Severity:** CORRECTNESS
- **Confidence:** high
- **Location:** `js/custom_icm.js:922-923`
- **Evidence:** `[fact-code]` `if (bone == "Hips" && obj.px && obj.py && obj.pz)`. In JavaScript the
  number `0` is falsy, so any frame in which **any one** of the three coordinates is exactly zero
  fails the guard and `obj.sensorPosition` is never set.
- **Explains symptoms:** none of S1–S6 — it corrupts global position, which the symptom log does not
  currently track
- **Mechanism:** A WebXR reference space begins at exactly `(0, 0, 0)`, so the opening frames of every
  session are the most likely to be discarded. Thereafter any axis crossing exactly zero drops that
  frame's position. The failure is silent: no counter, no warning, and the root simply holds its
  previous value, producing a brief freeze followed by a jump — which reads as tracking judder rather
  than as dropped data. The correct guard tests for `undefined`/`null`/`NaN`, not truthiness.
- **Falsifying measurement:** Count frames per session where the guard fails while `obj.px !== undefined`.
  If the count is zero over a full session, the coordinates never hit exact zero and the bug is latent
  rather than active.
- **Proposed change:** After measurement, replace with an explicit finiteness test (the codebase
  already has `_isBadNum` at `js/webserialnative.js:270-272` doing exactly this for quaternions).
- **Cost:** 1 line.
- **Risk of the fix:** More frames pass through, including any genuinely bad ones the truthiness test
  was accidentally filtering — pair it with a finiteness check.

### PHN-02 — Phone JSON and binary IMU packets share one unframed serial stream
- **Severity:** BLOCKER
- **Confidence:** high
- **Location:** `Device code/Dongle_Binary/Dongle_Binary.ino:201` and `:232`;
  `js/webserialnative.js:212`, `:234`
- **Evidence:** `[fact-code]` the hub writes binary pod frames with `Serial.write(incomingData, 16)`
  from the ESP-NOW receive callback `:201`, and phone text with `Serial.println((char *)data)` from
  the WebSocket callback `:232` — **different task contexts, no mutual exclusion**. `[fact-code]` the
  browser's binary branch is gated on `_jsonLine.length === 0` `js/webserialnative.js:212`, and its
  JSON branch consumes every byte once a line is open `:234`.
- **Explains symptoms:** **S1, S2, S3, S4**
- **Mechanism:** This is the same defect A2 records as HUB-02 and A4 as WEB-02, seen from the boundary
  that creates it. Because the two producers are unsynchronised, a 16-byte binary packet can be
  written into the middle of a JSON line. The browser then consumes those 16 bytes as JSON text and
  loses the pod sample; worse, if the binary packet contains a `0x0A` byte it terminates the JSON line
  early, corrupting the phone frame as well. And if a JSON line's newline is ever lost, `_jsonLine`
  never clears and **all** binary parsing stops permanently. The failure rate scales with phone
  message frequency, so it is worst exactly when the system is most loaded. The phone is not merely a
  victim here — it is the mechanism.
- **Falsifying measurement:** Run two otherwise identical sessions, one with the phone WebSocket
  connected and one without, and compare per-bone binary frame counts (`window._podRx`) against the
  hub's per-id counts (instrumentation I1). If the delivered fraction is unchanged by phone activity,
  this finding is wrong.
- **Proposed change:** After measurement, give the phone payload its own binary framing with a distinct
  sync marker, and serialise all hub serial writes through a single queue. Do not attempt to fix this
  in the browser alone — a parser cannot reliably separate two interleaved unframed streams.
- **Cost:** ~40 lines at the hub plus ~20 in the browser, landing together.
- **Risk of the fix:** Wire-format change; hub and webapp must be deployed in lockstep.

### PHN-03 — No relocalization handling; a SLAM jump propagates straight into the root
- **Severity:** RISK
- **Confidence:** medium
- **Location:** `js/custom_icm.js:922-923`, and absence of any filtering around it
- **Evidence:** `[fact-code]` the ingestion path assigns `obj.sensorPosition` directly from the
  reported coordinates with no continuity check, no outlier rejection, and no velocity bound.
- **Explains symptoms:** none logged — but this is a plausible source of unreported artifacts
- **Mechanism:** `[fact-doc]` visual SLAM systems including ARCore correct accumulated drift by
  relocalizing against a map, which produces a **discontinuous** pose jump. With no continuity check,
  such a jump is written directly to the skeleton root and the entire body teleports in a single
  frame. Because the tester's field log records qualitative anomalies, this may already have been
  observed and attributed to something else. `[fact-data]` the benchmark's `root_error_max` of 109.60
  units against a `root_error_mean` of 38.41 (session 01, `metrics_all.json`) is consistent with
  occasional large excursions rather than uniform error — suggestive, not conclusive.
- **Falsifying measurement:** Compute per-frame root displacement over an existing capture and
  histogram it. Physically plausible human motion bounds displacement per 33 ms frame; count
  excursions beyond that bound. Zero outliers falsifies this.
- **Proposed change:** None in Phase 1. If confirmed, a velocity-bounded rejection or a jump detector
  that flags rather than smooths.
- **Cost:** analysis first.
- **Risk of the fix:** Smoothing a genuine relocalization correction reintroduces the drift it fixed;
  flagging is safer than filtering.

### PHN-04 — Two independent drifting estimates of the same world, never reconciled
- **Severity:** CORRECTNESS
- **Confidence:** medium
- **Location:** `js/custom_icm.js:581`, `:602-603`, `:922-923`
- **Evidence:** `[fact-code]` the phone supplies position and fallback orientation; the pods supply
  orientation from a 6-axis DMP with no heading reference (A5's EST-01). `[fact-code]` no code
  aligns the WebXR world frame to the IMU heading frame at any point.
- **Explains symptoms:** **S6**, jointly with EST-01
- **Mechanism:** WebXR's reference space has its own arbitrary yaw origin, fixed when the session
  starts. The pods' Game Rotation Vector has a *separate* arbitrary yaw origin per sensor. Nothing
  establishes the rotation between these two frames, so the body's orientation and its direction of
  travel are expressed in unrelated headings — and both drift independently. `[fact-data]` this is
  precisely what the benchmark shows: `yaw_alignment_degrees` of **−71.19°** and **+99.16°** in two
  sessions minutes apart, an offset the comparison had to solve for post hoc because the system never
  established it. A person walking north would be rendered walking north while facing an arbitrary
  other direction.
- **Falsifying measurement:** Have the subject walk a straight line for 10 m facing forward. Compare
  the root's direction of travel against the torso's forward axis. A constant nonzero angle between
  them is the unreconciled frame offset; a near-zero angle falsifies this.
- **Proposed change:** None in Phase 1. The fix — aligning the IMU heading frame to the WebXR world
  frame at T-pose — is cheap and high-value, but it belongs after E11 establishes whether heading
  error is initialisation or drift, since that determines whether one alignment suffices or it must be
  maintained.
- **Cost:** deferred.
- **Risk of the fix:** A one-time alignment is invalidated by subsequent drift in either estimate.

### PHN-05 — Root Euler extraction passes through gimbal-singular configurations
- **Severity:** RISK
- **Confidence:** medium
- **Location:** `js/bvh_converter.js:8`, `:124`
- **Evidence:** `[fact-code]` Euler angles are extracted with `setFromQuaternion(q, "XYZ")` and the
  root declares `CHANNELS 6 Xposition Yposition Zposition Xrotation Yrotation Zrotation`.
  `[fact-data]` in `MMcap_bvh_2026-6-8-13-20-59.bvh`, root rotation at frame 499 is
  `(-138.841, -84.620, -143.009)` — the middle (Y) angle is within 5.4° of the −90° singularity of
  XYZ-order Euler extraction.
- **Explains symptoms:** none logged
- **Mechanism:** In XYZ Euler order the second rotation approaching ±90° collapses the first and third
  axes onto each other. Near that configuration the X and Z angles become numerically unstable and can
  swing wildly between adjacent frames while representing nearly the same orientation — visible as a
  sudden spin of the root. A hip-worn root that yaws freely through a full turn will pass through this
  region routinely. The quaternion carries no such problem; the singularity is introduced purely by
  the BVH export's choice of order.
- **Falsifying measurement:** Scan every exported capture for frames where the root's Y rotation is
  within 10° of ±90°, and check whether adjacent-frame X/Z deltas spike there relative to the session
  baseline. No spikes falsifies it.
- **Proposed change:** None in Phase 1. If confirmed, a rotation order placing the freely-rotating
  yaw axis first (BVH's conventional ZXY exists for this reason) avoids the singularity in normal use.
- **Cost:** deferred; it is a format change affecting every consumer.
- **Risk of the fix:** Changing declared channel order breaks any consumer that assumed the old one.

## 5. Symptom attribution

| Symptom | Contributes? | Evidence |
|---|---|---|
| **S1** degrades with node count | **Yes** | PHN-02: phone traffic interleaving destroys binary packets, at a rate proportional to phone message frequency. |
| **S2** oscillates between values | **Yes** | PHN-02: interleaving is bursty and tied to WebSocket message timing, giving discrete loss rather than smooth decay. |
| **S3** only ~4 nodes connect | **Yes** | PHN-02: an unterminated JSON line halts binary parsing entirely and permanently. |
| **S4** sometimes all 15 fine | **Yes** | PHN-02 is a race — whether it fires depends on interleaving timing, which varies per session. |
| **S5** 60 Hz fails with many nodes | Indirect | More binary traffic means more opportunity to collide with phone writes. |
| **S6** yaw drift | **Yes** | PHN-04: the WebXR world frame and the IMU heading frame are never reconciled, and both drift. |

## 6. What I could not determine

Everything here needs the phone source.

- `[unknown]` **The phone's update rate.** Not recorded anywhere on this side; the JSON carries no
  rate field and arrival timestamps are not logged. `[fact-data]` all captures export at a fixed
  1/30 s, so the phone's true rate is unrecoverable from them.
- `[unknown]` **Units and handedness of `px/py/pz`.** WebXR is metres, right-handed, Y-up
  `[fact-doc]`; the BVH root spans tens of units with Y near 55, so a scale conversion happens
  somewhere that was not located. Whether handedness is converted is unverified.
- `[unknown]` **Which WebXR reference space is requested** (`local`, `local-floor`, `viewer`,
  `unbounded`). This determines the origin, whether Y is floor-referenced, and relocalization
  behaviour.
- `[unknown]` **Whether the phone runs its own WebXR session or streams raw ARCore poses.**
- `[unknown]` **Whether relocalization events are exposed at all** by the phone app.
- `[unknown]` **The phone's clock domain** — its timestamps are unrelated to any node's `millis()` and
  to the dongle's. Handed to A8.
- `[unknown]` **Camera mounting geometry and the camera-to-hip extrinsic.** A hip-mounted camera sees
  mostly floor and swinging limbs — close to a worst case for feature tracking — but no calibration
  for it exists on this side.
- `[unknown]` **Thermal behaviour over long sessions.** Sustained camera plus SLAM plus browser is a
  known throttling load `[fact-doc]`; the benchmark sessions are only 70–84 s, far too short to show it.

## 7. Question list for when the source arrives

1. Which `XRReferenceSpace` type, and what is the origin convention?
2. What is the pose update rate, and is it locked to the camera frame rate or to `requestAnimationFrame`?
3. Are `px/py/pz` metres? Where does the conversion to BVH units happen, and what is the scale factor?
4. Is handedness converted between WebXR (right-handed, Y-up) and the three.js scene?
5. Is orientation sent as a quaternion in the same JSON, and in which frame?
6. Are relocalization or tracking-state-change events detectable, and can they be forwarded as a flag?
7. What is `XRFrame`'s timestamp domain, and can it be included in the JSON for A8's sync scheme?
8. What happens on tracking loss — is the last pose held, is sending suspended, or is a zero sent?
   (This interacts directly with PHN-01.)
9. What is the WebSocket send rate and message size? Needed to quantify PHN-02's collision rate.
10. Is the phone screen on during capture, and what is the thermal and battery profile of a 10-minute
    session?

## 8. Measurements I need

1. **Phone-on versus phone-off session comparison** (PHN-02's falsifying test). This is the single
   most valuable measurement and it needs no phone source at all — only two sessions and I1.
2. **Root displacement histogram** over existing captures (PHN-03). Available now, offline.
3. **Straight-line walk test** (PHN-04). Available now; needs no source.
4. **Gimbal-region scan** of existing captures (PHN-05). Available now, offline.
5. **Position-guard failure counter** (PHN-01). One line of instrumentation.

Note that items 2–4 are all runnable **today** against existing data. The blocked status of this
subsystem has been treated as a reason to defer, but four of five measurements here do not need the
phone source.
