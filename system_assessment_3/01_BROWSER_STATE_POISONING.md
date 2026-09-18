# S7 — "The browser needs cache clearing or the pods keep moving"

**This is a real defect with a specific mechanism, and your instinct about accumulation is half
right.** It is not gradual junk build-up. It is **one bad value permanently poisoning persistent
state, with no recovery path in the code.** That distinction matters, because it changes the fix
entirely: you do not need periodic flushing, you need input validation plus a reset path.

## The mechanism, end to end

### Step 1 — the firmware can emit an invalid quaternion
`[fact-code]` `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino:~795`

```cpp
double q0 = sqrt(1.0 - ((q1 * q1) + (q2 * q2) + (q3 * q3)));
```

The radicand is never clamped. When DMP noise or drift makes the sum of squares exceed 1, this is
`sqrt(negative)` → **NaN**. This is Phase 1's `SENS-03`.

`[fact-code]` `:174-180` — `q_to_i16()` maps NaN to **0**:
```cpp
if (isnan(v)) v = 0.0f;
```

So a NaN quaternion is transmitted as `(0, 0, 0, 0)` — a **zero quaternion**, which is not a rotation
at all. It looks like perfectly valid data on the wire: correct sync bytes, correct length, valid
bone id, plausible sequence number.

`[fact-code]` The webapp's own comments confirm this happens in the field —
`js/webserialnative.js:168-172` describes firmware emitting `nan` literals "when the IMU's
`sqrt(1 - q1^2 - q2^2 - q3^2)` underflows".

### Step 2 — the binary path does not validate, though the legacy path does
This is the asymmetry that makes the bug live today.

`[fact-code]` The **legacy JSON path** checks explicitly (`js/webserialnative.js:270-282`):
```js
const _isBadNum = function (v) {
  return v === null || v === undefined || (typeof v === 'number' && !isFinite(v));
};
if (_isBadNum(j.x) || _isBadNum(j.y) || _isBadNum(j.z) || _isBadNum(j.w)) {
  window._nanFrameDropped = (window._nanFrameDropped || 0) + 1;
} else { ... handleWSMessage(j); }
```
with the comment "a NaN quaternion would propagate through every dot product downstream."

`[fact-code]` The **binary path — the one your current firmware actually uses** — has no check
(`js/webserialnative.js:242-250`):
```js
const obj = unpackPodPacket(_rxBuf.subarray(i, i + POD_PACKET_LEN));
if (obj) { ... handleWSMessage(obj); }
```

`[fact-code]` `handleWSMessage` stores it unconditionally (`js/custom_icm.js:512-515`):
```js
mac2Bones[bone].last.x = parseFloat(obj.x);   // no validity test
```

**Someone correctly identified this hazard and guarded the old path. When the wire format moved to
binary, the guard did not move with it.**

### Step 3 — the poison lands in *persistent* state and never leaves
`[fact-code]` `js/custom_icm.js:583-900` — every bone is updated as:
```js
x.quaternion.slerp(someCorrection, slerpDict[bone] || slerpFactor);
```
where `x = model.getObjectByName(rigPrefix + bone)` — the actual three.js bone object.

**`x.quaternion` is both the input and the output.** It is a feedback loop that runs every frame for
the life of the page.

- Slerping toward a zero quaternion **shrinks** the bone's quaternion toward zero.
- Once any component becomes NaN, NaN propagates through every subsequent slerp forever.
- `[fact-code]` there are **17 slerp calls and zero `normalize()` after any of them**.
- A degenerate quaternion applied to a bone produces scaling, shearing, and erratic rotation — which
  is exactly "the pods keep moving".

### Step 4 — there is no recovery path
`[fact-code]` `js/custom_icm.js:93-96`:
```js
var flag = true;
function initGlobalLocalLast() {
  flag = false;      // one-shot
```
`[fact-code]` `:502` — `if (flag) { initGlobalLocalLast(); }` runs **once per page load**.

Nothing re-initialises `mac2Bones` or the rig's bone quaternions during a session. **Reloading the
page is the only reset that exists** — which is precisely why clearing the cache "fixes" it. The
cache is incidental; the reload is the cure.

## Why it looks like gradual accumulation

Each pod has an independent chance of emitting a bad frame. With 15–17 pods running for tens of
minutes, the probability that *at least one* has been poisoned rises steadily with session length.
Bones degrade one at a time, so the suit appears to decay gradually — but each individual bone fails
instantly and permanently. **The gradualness is a population effect, not an accumulation effect.**

## A second, independent contributor: allocation churn

`[fact-code]` `js/custom_icm.js:416-431` — `updateTrackingLine()` runs every frame and does:
```js
new THREE.BufferAttribute(new Float32Array(line_tracker), 3)
```
`line_tracker` is capped at 10,000 vertices = 30,000 floats = **120 KB reallocated and copied every
frame**. At 60 fps that is ~7 MB/s of garbage on the main thread, on top of `WEB-01`'s per-packet
`moment.js` and `innerHTML` work.

This does not corrupt anything, but it produces exactly the "gets worse the longer it runs" feel,
and a reload clears it. **It is probably why the symptom reads as accumulation even though the
corruption mechanism is instantaneous.**

## The fixes, in order

### F1 — Validate at the boundary *(the actual fix, ~10 lines)*
Apply the same guard the JSON path already has, in the binary path. Reject any packet where the
quaternion is not finite **or** whose norm is not near 1:

```
n = x*x + y*y + z*z + w*w
reject if !isFinite(n) || n < 0.9 || n > 1.1
```

The norm test is the important half: it catches the zero quaternion, *and* it catches the torn
cross-core reads from `NODE-01` (a quaternion assembled from two different samples is not unit).
Count rejections per bone and surface it — silent rejection would hide a failing pod.

### F2 — Clamp the radicand in firmware *(1 line, stops it at source)*
```cpp
double rad = 1.0 - ((q1*q1) + (q2*q2) + (q3*q3));
if (rad < 0.0) rad = 0.0;
double q0 = sqrt(rad);
```
Phase 2 already instrumented this as counter `N2`, so you can measure how often it fires before and
after.

### F3 — Normalise after slerp *(17 one-word additions)*
`x.quaternion.slerp(target, f).normalize();`
Three.js r124's `slerp` only normalises in its near-degenerate branch, not the general path. On a
feedback loop running 100k+ times per session, make normalisation explicit rather than assumed.

### F4 — Add a recovery path *(~15 lines)*
A "Reset skeleton" control that re-runs `initGlobalLocalLast()`, resets every bone quaternion to the
rig's rest pose, and clears `mac2Bones`. Today the only reset is a page reload, which also drops the
serial connection and any unsaved recording. **This alone would remove the operational pain even
before the root cause is fixed.**

Consider also an automatic version: if a bone's quaternion norm leaves `[0.9, 1.1]`, reset that bone
and log it. Self-healing beats a manual button.

### F5 — Stop reallocating the tracking line *(~10 lines)*
Allocate the `Float32Array` once at maximum size, write into it in place, and use `setDrawRange`.
The buffer is already length-capped; only the reallocation needs removing.

## What to measure first
Phase 2 already built the counters. Before changing behaviour, run one long session and read:
- `N2` (firmware negative-radicand count) — is `SENS-03` firing?
- `N1` (firmware unit-norm violations) — are torn reads reaching the wire?
- `window._nanFrameDropped` — the JSON path's existing counter
- a new binary-path rejection counter from F1

If `N2` and `N1` are both zero over a long session, the poison is entering somewhere else and this
diagnosis needs revisiting. **That is the falsifying test, and it costs one session.**
