# B3 — Browser Instrumentation & Export — Phase 2

## 1. Scope
New: `js/mesq_instr.js` (196 lines). Changed: `js/webserialnative.js` (hooks + `0xFE` status
branch), `js/bvh_converter.js` (I12), `js/custom_icm.js` (W1b counter only — **no behaviour
change**), `index.html` (one script tag). Verified with `tools/replay_harness.js`, 23/23 passing.

## 2. Summary
I5 is implemented and **verified by execution**, including the counter wrap and the pod-reboot guard
that would otherwise report 65,535 phantom losses. I12 is landed early as instructed, so any future
capture records a *measured* frame interval plus wall-clock instead of asserting 1/30 s. The
fixture produced the session's most consequential result: **`WEB-02` does not latch permanently** —
it self-clears in ~34 ms across a 15-pod fleet, which removes it as an explanation for S3. The new
`0xFE` hub status frame required an explicit parser branch; without it the 16-byte reader would have
consumed status frames as pod packets and mis-framed everything after them.

## 3. Implemented

| Instrument | Location | Verified |
|---|---|---|
| **I5** sequence-gap per bone | `js/mesq_instr.js:onPacket` | **Yes** — F4/T1,T3,T4,T5 |
| **I6** arrival timestamps, inter-arrival stats, `_jsonLine` length, byte/frame counters | `js/mesq_instr.js` + hooks | Partly — counters exercised by fixture |
| **I12** measured frame time + ISO wall-clock + provenance line | `js/bvh_converter.js:157-186` | **No** — needs a real recording |
| **W1b** position-guard failure counter | `js/custom_icm.js:922-930` | **No** — needs phone data |
| `0xFE` status frame | `js/webserialnative.js:210-222` | **Yes** — F5/T9 incl. chunk splits |

### I5 — the reboot guard matters more than the gap arithmetic
`gap = (count - lastCount + 65536) % 65536 - 1`. A pod reboot resets `count` to 0, which the modulo
alone reports as ~65535 lost. `RESYNC_THRESHOLD = 1000` (`js/mesq_instr.js:26`) classifies that as a
resync, not loss. Verified: T5 gives 40 received, **0 lost, 1 resync**. Without the guard, one
reboot would have destroyed the loss statistic for the whole session — and `HUB-03` means reboots
are not rare.

Console entry points: `MesqInstr.report()`, `.start(1000)`, `.exportJSON()`.

### I12 — landed early, deliberately
`frameTime` now derives from `window.mesqFrameTiming` (`{startMs, endMs, frames, startISO, endISO}`)
when the recorder supplies it, falling back to `1/30`. The file records which:
`; MESQ_FRAME_TIME_SOURCE measured` or `ASSUMED_1_30_UNTRUSTWORTHY`, plus
`; MESQ_CAPTURE_START/END` in ISO-8601 — the first absolute time any export has carried, and the
anchor §13 needs to join the field log.

**Gap:** the recording path must populate `window.mesqFrameTiming`. That is a `js/custom_icm.js`
recording-path change I did **not** make, because the recording buffer's location was not
established in Phase 1 (`WEB-04`'s open `[unknown]`). **Until it is wired, `frameTime` still falls
back to 1/30 — but the file now says so.** That is the honest interim state; the header line makes
it self-documenting rather than silently wrong.

## 4. Findings

### P2-B3-01 — `WEB-02` self-clears; severity reduced
- **Severity:** CORRECTNESS (was BLOCKER) · **Confidence:** high
- **Evidence:** `[fact-fixture]` T6 — see `../MEASUREMENTS.md` F1.
- **Mechanism:** Any `0x0A` byte terminates a stuck JSON line, and `0x0A` occurs freely in binary
  payloads (`count`, `ms_lo`, quaternion bytes) at ~6% per 16-byte packet. Mean 16.3 packets to
  clear ≈ 511 ms for one pod, ≈ 34 ms across 15. A permanent latch **is** reproducible when payload
  bytes avoid `0x0A` (T6b: 0 of 50 decoded, `_jsonLine` at 823 chars), so the failure is real but
  rare and bounded.
- **Explains symptoms:** contributes to S1/S2 as repeated bounded loss. **No longer explains S3.**
- **Falsifying measurement:** log `_jsonLine.length` per second in a live phone-active session
  (I6). Sustained growth past a few hundred characters would show a real latch in the field.
- **Proposed change:** still cap `_jsonLine` (~10 lines) — but as loss reduction, not hang
  prevention. **Deferred to W3 under Rule 1.**
- **Cost:** ~10 lines. **Risk:** a cap could truncate a legitimately long phone message; set it well
  above the observed maximum, which I6 will supply.

### P2-B3-02 — Hub status frames would have corrupted pod framing
- **Severity:** BLOCKER (averted) · **Confidence:** high
- **Location:** `js/webserialnative.js:210-222`
- **Evidence:** `[fact-code]` the pre-existing binary branch, on seeing `0xAA 0x55`, unconditionally
  consumed 16 bytes. `[fact-fixture]` T9 confirms the new branch keeps framing intact, including
  across chunk boundaries.
- **Mechanism:** A status frame is `[0xAA][0x55][0xFE][len][payload]`. Without an explicit branch
  the reader would treat it as a pod packet with id `0xFE`, fail the `BONE_NAMES` lookup, return
  null — **and still advance `i` by 16**, landing mid-payload and mis-framing the remainder.
- **Falsifying measurement:** T9, run. Also: any live session where `window._hubStatus` fills while
  `window._podRx` keeps climbing normally.
- **Proposed change:** done, as part of enabling I1's transport.
- **Cost:** 12 lines. **Risk:** the `0xFE` marker must stay outside the bone range; it does (0–16
  valid, `0xFF` is the control marker).

## 5. Not done, deliberately
`WEB-01` (per-packet `moment.js` + `innerHTML`, ~480/s) — W4, and it needs the DevTools profile
first. `WEB-05` (`_rxBuf` reallocation) — smaller than `WEB-01`; fix that first.
`PHN-01` — **counter only**; the truthiness guard is deliberately unchanged, per Rule 1.
`smoothOrientation` left commented out, as instructed.

## 6. Measurements I need
A DevTools Performance profile of a live 15-node session (`WEB-01`); a phone-active session with I6
logging `_jsonLine.length` (`WEB-02` in the field); one recording that exercises I12 end-to-end; and
the recording-path change that populates `window.mesqFrameTiming`.
