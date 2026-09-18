# Architecture changes worth making

Beyond drift and calibration. Each cites the Phase 1/2 finding it closes.

## 1. Validate at every boundary
`01_BROWSER_STATE_POISONING.md` shows one unvalidated quaternion permanently corrupts a bone. The
general lesson: **every stage trusts its input completely.**

| Boundary | Today | Should |
|---|---|---|
| DMP → firmware | `sqrt` unclamped → NaN (`SENS-03`) | clamp the radicand |
| firmware → wire | NaN becomes `(0,0,0,0)`, looks valid | send a validity flag, or skip the frame |
| wire → browser (binary) | **no check** | norm test — catches zero *and* torn quaternions (`NODE-01`) |
| browser → rig | slerped into persistent state | reject, then reset the bone if already bad |

The norm test `0.9 < x²+y²+z²+w² < 1.1` is one line and catches three distinct defects.

## 2. Give the pod a sample→send queue
`[fact-code]` `TaskReadIMU` (core 1) writes a plain global `quat`; `TaskWifi` (core 0) reads it. No
mutex, no atomic — a packet can carry x,y from one sample and z,w from the next (`NODE-01`). A
FreeRTOS queue fixes the tearing **and** lets the sample be stamped when it is *taken* rather than
when it is *sent* (`NODE-02`/`SYNC-02`), which is a prerequisite for the time-sync work.

## 3. Land one protocol change, not three
`SYNC-06` (sync beacon), `HUB-02` (framing the phone stream) and `NET-04` (batching) all change the
wire format. Ship them together with a **version byte** — absent today, so a pod on old firmware and
a hub on new firmware fail undiagnosably. Adding it now is trivial; adding it later is impossible.

## 4. Move per-packet work off the packet
`[fact-code]` `js/custom_icm.js:519-560` runs `moment.js .fromNow()`, an `innerHTML` rebuild and a
jQuery selector **per packet** — ~480/s at 15 pods, ~900/s at 60 Hz, all on the main thread
(`WEB-01`). Accumulate counters per bone; repaint on a timer at 4–10 Hz. The data path needs none of
this work. Same for the 120 KB/frame `Float32Array` reallocation in `updateTrackingLine()`.

## 5. Reconsider the two "wasted" DMP streams
`SENS-02` recommends deleting `RAW_GYROSCOPE` and `RAW_ACCELEROMETER` (`:348-349`) as unused FIFO
cost. **Decide this after the drift work, not before** — ZARU (`02_DRIFT_ALGORITHMS.md` §B) needs
exactly that gyro stream, and it is already crossing the bus. Measure I9/I9b first: if the FIFO cost
is small, keep them and use them.

## 6. Close the two remaining latching failures
`[fact-data]` Phase 2 removed `WEB-02` as a permanent hang. Two remain, both needing a power cycle:
- **`NODE-04`** — `while(1);` if IMU/DMP init fails (`:377-384`). Bound the retry, then restart.
- **`HUB-03`** — *any* inbound serial byte reboots the whole fleet (`Dongle_Binary.ino:333-339`),
  bytes discarded unread. Require a command token.

These are the surviving explanation for S3/S4. ~15 lines together.

## 7. Extract the parser so it is testable
`tools/replay_harness.js` holds a hand-synced **copy** of `feedSerialBytes`. If the real one changes
and the copy does not, the tests validate dead code. Extract it into a module both import.

## 8. Fix the rotation order in BVH export
`[fact-data]` Mesquite's `XYZ` puts **yaw** in the gimbal-critical middle slot; up to **23.9%** of
frames land within 10° of the singularity. Rokoko's `YXZ` puts pitch there and scores **0.0%**
(`PHN-05`). Changing the declared order affects every consumer, so batch it with the export fixes.
