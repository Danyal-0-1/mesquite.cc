# LEDGER — Phase 1 findings carried forward

Status: `open` → `measured` → `fixed` → `verified` → `closed`, or `refuted` / `deferred` / `wontfix`.
**Phase 1 IDs are never renumbered.** A finding shown wrong is marked `refuted` with its evidence,
not deleted.

## Status changed this session

| ID | Phase 1 | Now | Why |
|---|---|---|---|
| `WEB-02` | open, BLOCKER, high | **measured — severity reduced to CORRECTNESS** | F1: latch self-clears in ~34 ms fleet-wide. No longer explains S3. |
| `HUB-02` | open, CORRECTNESS, high | **measured — confirmed, scope widened** | F2: destroys the JSON line as well as the packet. |
| `PHN-05` | open, RISK, medium | **measured — confirmed, quantified** | D1: up to 23.9% of frames near gimbal; reference 0.0%. |
| `PHN-03` | open, RISK, medium | **refuted (for existing captures)** | D2: no teleport signature; large jumps are the reference's trimmed frame 0. |
| `NODE-05` | open, QUALITY | **fixed, unverified** | Per-node build system; `#error` if `MESQ_POD_ID` undefined. Needs a real build to verify. |
| `SYNC-03` | open, CORRECTNESS, high | **fixed, unverified on hardware** | I5 implemented + fixture-verified (F4). Needs a live session. |
| `SYNC-07` / `WEB-03` / `EST-05` | open, CORRECTNESS, high | **fixed, unverified** | I12: measured frame interval + wall-clock + provenance line. |
| `INT-03` | open | **amended** | Three latching modes → **two** (`NODE-04`, `HUB-03`). |
| `PHN-01` | open, CORRECTNESS, high | **instrumented (W1b), not fixed** | Counter added; behaviour deliberately unchanged pending Rule 1. |
| `KIN-01` | open, CORRECTNESS, high | **measured — feasible** | D3: stance runs up to 3.3 s in every capture. ZUPT available; B8 scope survives. |
| `EST-02` | open, high | **weakened, not refuted** | D4: per-frame optimal yaw removes only 24–32% of root-relative error, stable across alignments. |
| `INT-04` | open | **widened** | D4: contaminates *new* analyses too. A ±1 s alignment change swings yaw across 1,176°. |

## Instrumented this session, awaiting hardware

All `open`, all with code written and none executed on a device:

`SENS-01` (I9 Quat6/s) · `SENS-02` (I9b) · `SENS-03` (N2) · `NODE-01` (N1) · `NODE-02` (N3) ·
`NODE-03` (I4) · `NODE-04` (I8) · `HUB-03` (H1) · `HUB-04` (I11) · `HUB-05` (I1) · `HUB-07` (H2) ·
`NET-05` (I2) · `NET-06` (I7) · `PWR-01` (H3) · `SYNC-02` (N3) · `SYNC-05` (I6)

## Untouched — blocked on hardware or on inputs

| ID | Blocked on |
|---|---|
| `NET-01`, `NET-02`, `NET-03`, `NET-04` | **U1** — the PHY rate. Needs a sniffer or a fleet reflash. |
| `EST-01`, `EST-02`, `EST-04` | **U3 / M4** — the static drift session. |
| `KIN-01` | **feasibility DONE (D3)**; the fix itself needs optical ground truth |
| `KIN-03`, `KIN-05`, `SYNC-08` | W5 |
| `PHN-02`, `PHN-04` | Phone source; `PHN-02` partly covered by F2 |
| `SYNC-01`, `SYNC-06` | W4 protocol change |
| `WEB-01`, `WEB-05` | Needs a DevTools profile on real traffic |
| `SYNC-10` | Flash partition enumeration (needs a device) |
| `HUB-01`, `HUB-06`, `HUB-08`, `KIN-02`, `KIN-04`, `KIN-06`, `EST-03`, `WEB-06`, `WEB-07`, `SYNC-09` | Phase 1 negative results — no action, retained for the record |

## New Phase 2 findings

| ID | Severity | Title |
|---|---|---|
| `P2-B10-02` | CORRECTNESS | **The benchmark maps no arm joints** — 14 of 22, all torso and legs. Silent about the fastest-moving half of the body. |
| `P2-B10-01` | BLOCKER (eval) | `INT-04` contaminates new analyses; no further quantitative work on existing captures |
| `P2-B7-01` | BLOCKER (B7) | U3 not resolvable offline; **M4 is the only instrument that works** |
| `P2-B8-02` | RISK | Foot vertical excursion implausibly small (1–4 units) — unrepresentative data or under-articulated legs |
| `P2-B1-01` | RISK (averted) | I10 as specified would have tripled I²C on the sample task; redesigned as hub-side H3 |
| `P2-B1-02` | BLOCKER (W1 gate) | Rule 2 unsatisfied — the 1 Hz print's own cost is not even accumulated |
| `P2-B2-01` | RISK | I3 cannot see packets lost to a stall; must be read against I5 |
| `P2-B3-02` | BLOCKER (averted) | Hub status frames would have mis-framed the pod stream without an explicit `0xFE` branch |
| `P2-B0-01` | BLOCKER | Git unusable — §4.1 cannot be executed |
| `P2-B0-02` | RISK | Nothing was compiled |

## Gate status

| Wave | Gate | State |
|---|---|---|
| **W0** | firmware provenance confirmed; U2 resolved; per-node build; replay fixture | **PARTIAL** — build system and replay fixture **done**; provenance code written but **U2 unresolved** (never run). **Gate CLOSED.** |
| **W1** | I1–I12 implemented; instrument cost measured; single-node pilot passed | **PARTIAL** — code complete; **Rule 2 unsatisfied** (cost never measured); no pilot possible. **Gate CLOSED.** |
| **W2** | baseline exists; U1 and U3 resolved | **NOT STARTED** — needs hardware |
| **W3**–**W6** | — | **BLOCKED** by Rule 1 |

**No gate is open.** W3+ fixes were therefore not written, which is the correct outcome under Rule 1
rather than a shortfall.
