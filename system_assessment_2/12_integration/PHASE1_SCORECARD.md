# Phase 1 Scorecard — were the inferences right?

Phase 2 §B12.4 asks for this explicitly, and it is not bookkeeping: Phase 1 made a great deal of
reasoning-without-measurement, and the point is to calibrate how much weight that deserves.

**Scope caveat:** only findings testable **without hardware** could be scored. That is 6 of ~50.
The hardware-dependent majority — every airtime, timing and rate claim — remains unscored.

## Scored

| Phase 1 finding | Phase 1 claim | Verdict | Evidence |
|---|---|---|---|
| `WEB-02` | "**permanently** halts all binary parsing" — BLOCKER, high | **PARTLY WRONG** | F1: self-clears in ~16 packets (~34 ms fleet-wide). Permanent latch reproducible only when payload avoids `0x0A`. |
| `HUB-02` | interleaved writes destroy pod packets — CORRECTNESS, high | **RIGHT, and understated** | F2: both the packet *and* the JSON line are destroyed. |
| Parser chunk-straddling | "the parser is **correct**; chunk-loss hypothesis disproved" | **RIGHT** | F3: 50/50 across 7-byte chunks. |
| `PHN-05` | gimbal risk — RISK, medium, from one frame | **RIGHT, and understated** | D1: up to 23.9% of frames in the danger zone; reference scores 0.0%. |
| `PHN-03` | relocalization jumps — RISK, medium; `root_error_max` "suggestive" | **WRONG for these captures** | D2: no teleport signature; the large jumps are the reference's frame-0 rest pose, already trimmed. |
| `SYNC-03` guard | reboot would register 65535 phantom losses | **RIGHT** | F4/T5: confirmed, and the guard works. |

**Score on testable findings: 3 right, 2 right-but-understated, 1 partly wrong, 1 wrong.**

## What this says about Phase 1's method

**Where it was reliable:** claims resting on *reading control flow* — that the parser keeps a carry
buffer, that two task contexts share an unguarded stream, that a modulo needs a reboot guard. All
correct. Reading code to determine what code does worked.

**Where it failed:** claims about **how often** something happens, or **how long** it lasts.

- `WEB-02` traced the latch correctly but never asked what would *clear* it. The answer was in the
  data format Phase 1 itself documented — `0x0A` appears freely in binary payload bytes. A
  five-minute thought experiment would have caught it; instead it became a headline S3 explanation.
- `PHN-03` inferred a mechanism from a summary statistic (`root_error_max` 109.60) without looking
  at the underlying frames. The number was real; the interpretation was wrong; the actual cause was
  one rest-pose frame the analysis already discarded.

**The pattern: Phase 1 was accurate about mechanism and unreliable about magnitude and frequency.**
Both errors ran in the same direction — toward severity. `WEB-02` was promoted to BLOCKER, `PHN-03`
to a live suspicion, on reasoning that felt conclusive and was not.

**Calibration for the rest of Phase 2:** treat Phase 1's *"this can happen"* as trustworthy and its
*"this is what's happening"* as a hypothesis. Rule 1 already encodes this; the scorecard shows the
rule is earning its keep. The findings most in need of scepticism are the ones stated most
confidently about rates — `NET-01`'s 57% airtime, `NODE-03`'s tick quantisation, `SENS-01`'s 55 Hz
ceiling. All three are arithmetic or comment-derived, none is measured, and all three currently sit
at the top of the ledger.

## One correction that propagates

`INT-03` claimed **three** latching failures explain S3/S4. F1 shows `WEB-02` does not latch in
practice. **Two remain** — `NODE-04` (init hang, permanent until power cycle) and `HUB-03` (fleet
reboot). Both are still consistent with S3/S4; the account is narrower and, being narrower, easier
to test. M7 (10 cold boots, I8) now carries more weight than before, because it is the only
remaining candidate that is both permanent and probabilistic.
