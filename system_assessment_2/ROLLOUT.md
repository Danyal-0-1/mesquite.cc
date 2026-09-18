# ROLLOUT — branches, tags, flashing, revert

## BLOCKER: git is unusable on this machine

```
$ git status
You have not agreed to the Xcode license agreements.
Please run 'sudo xcodebuild -license' from within a Terminal window.
```

Every `git` invocation fails. §4.1's entire discipline — one branch per wave, a tag before every
fleet flash, a documented revert path — **cannot be executed until this is cleared.**

**Fix (owner, one command, needs sudo which I cannot run):**
```
sudo xcodebuild -license
```
Then confirm with `git status`. Until then, **do not flash anything**: a fleet flash with no tag to
return to is exactly the unrecoverable situation §4.1 exists to prevent.

**Consequence for this session:** all edits were made directly on the working tree with no branch.
Backups of every modified file are in the session scratchpad, and every change is additive and
individually revertible (see below).

## Branch scheme (to create once git works)

```
main
 └── phase2/w0-provenance      build system, provenance prints, replay fixture
 └── phase2/w1-instrumentation  I1-I12 behind MESQ_INSTR
 └── phase2/w3-reliability      (not started - Rule 1)
 └── phase2/w4-protocol-v2      (not started - Rule 1)
```

Tag before **every** fleet flash: `fleet/YYYY-MM-DD-<wave>-<n>`, e.g. `fleet/2026-09-05-w1-1`.
The tag must be cut **before** the flash, not after, so the pre-flash state is always reachable.

## Files changed this session

| File | Change | Revert |
|---|---|---|
| `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino` | identity block → `MESQ_POD_ID`; provenance banner; I9/I4/I8/N1/N2/N3/I11 behind `MESQ_INSTR` | scratchpad `.orig`, or build with `MESQ_INSTR=0` for behavioural equivalence |
| `Device code/Dongle_Binary/Dongle_Binary.ino` | provenance banner; I1/I2/I3/I7/I11/H1/H2/H3 behind `MESQ_INSTR` | as above |
| `js/webserialnative.js` | instrumentation hooks; `0xFE` status-frame branch | scratchpad copy |
| `js/mesq_instr.js` | **new** — I5/I6/W1b | delete the file; hooks degrade to no-ops |
| `js/bvh_converter.js` | I12 measured frame time + wall-clock | scratchpad copy |
| `js/custom_icm.js` | W1b counter only, **no behaviour change** | scratchpad copy |
| `index.html` | loads `js/mesq_instr.js` | remove one line |
| `tools/build_pods.sh`, `tools/replay_harness.js`, `tools/offline_analysis.py` | **new** | delete |

**Behavioural equivalence:** every firmware instrument is inside `#if MESQ_INSTR`, default `0`. A
build without `-DMESQ_INSTR=1` should be behaviourally identical to Phase 1. **This has not been
verified by compilation** — no toolchain was available. Verify before trusting it.

## The one breaking workflow change

`Pod_Watch_Binary.ino` now **fails to compile** without `-DMESQ_POD_ID=<0..16>`:

```
#error "MESQ_POD_ID is not defined. Build with -DMESQ_POD_ID=<0..16> ..."
```

This is deliberate (§4.2 promotes `NODE-05` to BLOCKER). Opening the sketch in the Arduino IDE and
pressing Compile will now fail until an id is supplied. The trade: **a build error is recoverable in
seconds; a duplicate `sendID` is silent at runtime and costs a session.**

Build all 17:
```
tools/build_pods.sh              # all ids
tools/build_pods.sh 3 4 5        # selected ids
FQBN=esp32:esp32:twatch tools/build_pods.sh
```
It writes `build/pods/MANIFEST.txt` with a SHA-256 per image and **fails if two ids produce
identical binaries** — which is the signature of the build flag not taking effect, i.e. the exact
`NODE-05` failure returning by another route.

`tools/build_pods.sh` has **not been executed** — `arduino-cli` is not installed here. It will exit
127 with a clear message if that is still true on the target machine.

## Flash procedure (once git works)

1. `git tag fleet/<date>-<wave>-<n> && git push --tags`
2. `tools/build_pods.sh` — confirm 17 distinct SHA-256s in the manifest
3. **Rule 3 — one node before fifteen.** Flash **one** pod. Run a live session with it alongside
   ≥4 pods on the previous firmware for 10 minutes. Confirm via I1 that the modified pod's delivered
   rate is not worse than its peers.
4. Only then flash the remaining 16, and the dongle.
5. Preflight before every session: confirm all 17 expected ids appear in `window._podRxByteId`
   (`js/webserialnative.js:56`) — this catches a duplicate or missing id before capture starts.

## Revert

```
git checkout fleet/<previous-tag>
tools/build_pods.sh
# reflash all 17 pods and the dongle
```
There is no OTA path, so revert means physically reflashing 17 devices. **Budget for that before
each fleet flash** — it is the real cost of a bad wave, and it is why Rule 3 exists.

## Unresolved rollout risks

1. **Git unusable** — no tags, no revert point. Blocks all flashing.
2. **Nothing compiled** — no toolchain here. Every firmware edit is untested against a compiler.
3. **`MESQ_INSTR=0` equivalence unverified.**
4. **Instrument cost unmeasured** (Rule 2). The pod's 1 Hz `Serial.printf` is a real cost on core 0
   and `mesq_instrCostUs` exists to quantify it, but has never been read.
