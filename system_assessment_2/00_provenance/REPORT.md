# B0 — Provenance & Build Control — Phase 2

## 1. Scope
Changed: `Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino` (identity block, provenance banner),
`Device code/Dongle_Binary/Dongle_Binary.ino` (provenance banner).
New: `tools/build_pods.sh`, `tools/replay_harness.js`, `tools/offline_analysis.py`, `../ROLLOUT.md`.
Sessions run: **none — no hardware available.**

## 2. Summary
The per-node build system is delivered and the comment-toggle identity block is gone, so
`NODE-05` can no longer silently produce a duplicate `sendID`. Provenance banners are written for
both pod and dongle but **have never executed**, so **U2 remains unresolved**. The replay fixture is
delivered and immediately productive — it re-characterised `WEB-02` and confirmed `HUB-02` on its
first run. Firmware provenance (does the fleet match this source?) **could not be checked at all**,
which leaves `INT-01`'s standing risk open. Git is unusable on this machine, which blocks §4.1
entirely.

## 3. Deliverables

### 3.1 Per-node build system — DELIVERED (§4.2 blocker)
`Pod_Watch_Binary.ino:33-79` replaces 17 mutually exclusive comment lines with:
- `#error` if `MESQ_POD_ID` is undefined — a **build** failure instead of a **silent runtime** one
- range check `0..16`
- `POD_BG[]` / `POD_FG[]` transcribed verbatim, so the on-watch appearance is unchanged
- `const int sendID = MESQ_POD_ID;`

The unused `String bone` declaration was removed after confirming (grep) it had no references — it
was only ever declared.

`tools/build_pods.sh` builds all 17, writes `build/pods/MANIFEST.txt` with a SHA-256 per image, and
**fails if two ids yield identical binaries** — the signature of the build flag not taking effect.

**Not executed:** `arduino-cli` is absent here. The script exits 127 with a clear message if that
holds on the target machine.

### 3.2 Provenance banner — WRITTEN, NOT RUN
Pod (`:485-510`) and dongle (`:285-300`) print at boot: build date/time, pod id, Arduino core
version, IDF version, **`configTICK_RATE_HZ`**, `portTICK_PERIOD_MS`, `esp_reset_reason()`, CPU
frequency, free heap/PSRAM, nominal `fps` and the computed gate interval.

`#include <esp_system.h>` added to both for `esp_reset_reason()` / `esp_get_idf_version()`.

**U2 is not resolved.** One boot with a serial monitor attached resolves it. The tick rate decides
whether `vTaskDelay(1)` is 1 ms or 10 ms and therefore whether the real transmit rate is ~32 Hz or
~25 Hz — `NODE-03`, and the S2 explanation, both hang on it.

### 3.3 Replay fixture — DELIVERED and already productive
`tools/replay_harness.js`, 23 assertions, all passing. It mirrors the real parser and supports fault
injection. First run produced two results Phase 1 could not have obtained by reading:
- **`WEB-02` re-characterised** — the latch self-clears (see `../MEASUREMENTS.md` F1)
- **`HUB-02` confirmed** — both payloads destroyed by one interleave (F2)

It also verified the I5 arithmetic including the counter wrap and the reboot guard (F4), and proved
the new `0xFE` status frame does not disturb pod framing even when split across chunks (F5).

§4.5 estimated ~60 lines; the delivered fixture is ~200 including tests, and it converted three
"does this happen in the field?" questions into deterministic tests.

**Note:** the harness holds a *copy* of the parser loop, kept in sync by hand. That is a real
maintenance hazard — if `feedSerialBytes` changes and the harness does not, the tests validate dead
code. Extracting the parser into a shared module is the correct fix and is a W4 refactor, out of
scope here. Flagged rather than done.

### 3.4 Firmware provenance — NOT DONE
The most important W0 item is unaddressable without a device. `INT-01` notes that **all four**
Phase 1 code-based refutations assume the workspace source is what runs on the fleet. That
assumption is still unverified. The banner makes it checkable in one boot: flash, read
`FW_BUILD`, compare.

### 3.5 Also not done (all need hardware)
- Native USB CDC vs UART bridge per dongle (`HUB-08` — a UART bridge would impose a real
  11,520 B/s ceiling at 115200 that the 60 Hz case exceeds)
- Flash partition enumeration (gates `SYNC-10`)

## 4. Findings

### P2-B0-01 — Git unusable; §4.1 cannot be executed
- **Severity:** BLOCKER · **Confidence:** high
- **Evidence:** `[fact-code]` every `git` invocation returns "You have not agreed to the Xcode
  license agreements."
- **Mechanism:** No branch, no tag, no revert point. §4.1 requires a tag before every fleet flash;
  without one, a bad wave means 17 devices with no reachable previous state.
- **Falsifying measurement:** run `git status` after `sudo xcodebuild -license`.
- **Proposed change:** owner runs `sudo xcodebuild -license` (needs sudo; I cannot).
- **Cost:** one command. **Risk:** none.

### P2-B0-02 — Nothing was compiled
- **Severity:** RISK · **Confidence:** high
- **Evidence:** no `arduino-cli`, no toolchain in this environment.
- **Mechanism:** Every firmware edit — identity block, banners, ~13 `MESQ_INSTR` sites on the pod,
  ~11 on the dongle — is untested against a compiler. Type errors, missing includes and `printf`
  format mismatches would all survive to the first build attempt.
- **Falsifying measurement:** `tools/build_pods.sh 1` and a dongle build. Both must succeed with
  `MESQ_INSTR` unset **and** set.
- **Proposed change:** compile before flashing anything. **Cost:** minutes. **Risk:** none.

### P2-B0-03 — `MESQ_INSTR=0` equivalence unverified
- **Severity:** RISK · **Confidence:** medium
- **Evidence:** `[fact-code]` all instruments are inside `#if MESQ_INSTR`, default `0`.
- **Mechanism:** The claim that an instrumentation-off build is behaviourally identical to Phase 1
  rests on inspection alone. One misplaced `#endif` would silently change the default build — and
  the baseline would then measure something other than the system it claims to.
- **Falsifying measurement:** build both, diff the `.bin` against a pre-change build, or at minimum
  confirm identical size and a matching provenance banner.
- **Cost:** one build. **Risk:** none.

## 5. What I could not determine
`[unknown]` **U2** — tick rate, core and IDF version. Code written, never run.
`[unknown]` Whether deployed firmware matches this workspace (`INT-01`).
`[unknown]` Whether any dongle uses a UART bridge (`HUB-08`).
`[unknown]` Flash partition layout (`SYNC-10`).
`[unknown]` Whether the firmware even compiles.

## 6. Measurements I need
One pod and one dongle on USB, flashed with the current source, with a serial monitor at 115200
(pod) and 921600 (dongle). **That single boot resolves U2, confirms the banner, and validates the
build system** — the whole W0 gate, in about ten minutes with hardware present.
