# Mesquite MoCap: Audited Literature Review and Publication Roadmap

**Prepared:** 15 September 2026  
**Project:** Mesquite MoCap and the proposed *Post-Processing Error Correction for Magnetometer-Free Inertial Motion Capture* extension  
**Scope:** project code, the Mesquite preprint, System Assessments 1–3, benchmark artifacts, and authoritative peer-reviewed literature

## Contents

- [Executive verdict](#executive-verdict)
- [Reviewed sources and selection policy](#what-was-reviewed)
- [Project/assessment claim audit and local evidence](#project-and-assessment-claim-audit)
- [Problem map and assessment crosswalk](#problem-to-literature-map)
- [Prioritized reading method and list](#prioritized-reading-list)
- [Detailed paper reviews](#detailed-paper-reviews)
- [Which problems the literature can fix](#what-the-selected-literature-canand-cannotfix)
- [Recommended research design](#recommended-mesquite-research-design)
- [Evaluation, architecture, and reproducibility](#evaluation-protocol-reviewers-can-trust)
- [Venue strategy](#venue-strategy-as-of-15-september-2026)
- [Manuscript rewrite plan](#how-to-rewrite-the-mesquite-manuscript)

## Executive verdict

Mesquite currently contains three related but distinct research artifacts:

1. **The public systems preprint/manuscript:** the older 15-pod Wi-Fi/router/Raspberry Pi design and its low-cost, body-worn, web-native motion-capture claim.
2. **The materially revised current implementation:** a 17-ID ESP-NOW/ESP32-S3/USB-WebSerial architecture. It must either be presented as a revision of the preprint or given an explicit systems contribution delta.
3. **The proposed ML paper:** offline quaternion integrity filtering, missing-sample reconstruction, relative-heading correction from body constraints and motion priors, and calibration refinement.

The strongest immediate paper is **not yet a learned yaw-correction result**. The current repository does not contain a trained correction model, and the available benchmark cannot support the proposal's numerical baseline. The defensible research sequence is:

1. make every sample identifiable, valid, and time-aligned;
2. collect simultaneous optical ground truth with the current architecture;
3. separate orientation, articulation, scale, root translation, and trajectory errors;
4. establish classical constraint/filter baselines;
5. only then train and evaluate a motion-prior correction model.

The proposal's most important conceptual change should be this:

> A motion prior can regularize or infer **relative segment headings** and select a plausible solution, but it cannot make absolute global yaw observable from accelerometer and gyroscope data alone. Absolute world heading remains a gauge freedom unless Mesquite supplies an initial/world reference such as a known T-pose direction, a reconciled WebXR frame, a trusted visual reference, or carefully gated magnetometer information. A known initial direction fixes initial yaw only; continuing drift still needs bias estimation, constraints, or continuing heading aid.

For the **eventual ML correction contribution**, the best topical fit is the [ACM SIGGRAPH/Eurographics Symposium on Computer Animation (SCA)](https://computeranimation.org/instructions.html). For a **networked wearable platform contribution**, [ACM SenSys](https://sensys.acm.org/2026/cfp.html) is the clearest conference fit and [IEEE Internet of Things Journal](https://ieee-iotj.org/guidelines-for-authors/) is a plausible archival-journal fit. The project is currently at pre-submission research readiness: it has neither a learned model nor a valid accuracy baseline. Venue-specific recommendations and overlap constraints appear later.

## What was reviewed

### Local material

- [Project `README.md`](../README.md) and the bill of materials.
- [`ML_PROJECT_PROPOSAL.md`](../ML_PROJECT_PROPOSAL.md).
- Current pod, dongle, browser, calibration, rendering, recording, and BVH-export code.
- [System Assessment 1 index](../system_assessment/README.md), [consolidated findings](../system_assessment/FINDINGS.md), and its component reports.
- [System Assessment 2 index](../system_assessment_2/README.md), [evidence ledger](../system_assessment_2/LEDGER.md), measurements, retractions, open questions, and integration gates.
- [System Assessment 3 index](../system_assessment_3/README.md) plus its browser poisoning, drift, calibration, and architecture analyses.
- [`Mesquite_benchmarks/`](../Mesquite_benchmarks/), including the comparison script and stored metrics.

### Associated manuscript

The related manuscript is:

> Poojan Vanani, Darsh Patel, Danyal Khorami, Siva Munaganuru, Pavan Reddy, Varun Reddy, Bhargav Raghunath, Ishrat Lallmamode, Romir Patel, Assegid Kidané, and Tejaswi Gowda. **“Mesquite MoCap: Democratizing Real-Time Motion Capture with Affordable, Bodyworn IoT Sensors and WebXR SLAM.”** arXiv:2512.22690v2, revised 10 January 2026. [Abstract](https://arxiv.org/abs/2512.22690) · [PDF](https://arxiv.org/pdf/2512.22690) · [HTML](https://arxiv.org/html/2512.22690v2)

The arXiv record says it was **submitted to** IEEE Internet of Things Journal. It should not be described as accepted or published there.

### Selection and reliability policy

“Most cited” is not a stable or reproducible ordering unless a database, query, version-merging policy, and date are fixed. Google Scholar, Scopus, Web of Science, OpenAlex, and publisher pages count differently. This review therefore prioritizes:

- direct match to a Mesquite failure or proposed method;
- peer review at a strong venue or journal;
- methodological transparency and meaningful validation;
- open code/data where possible;
- sustained community influence, formal awards, or publisher-reported uptake.

Useful impact signals visible during this review include: Mahony et al. (2008) at 1,672 Scopus citations on its university record; Seel et al. (2014) at 777 citations on its publisher page; DIP at 331 citations on its ACM page; VQF at 69 citations on its publisher page; FTSP and SMPL receiving Test-of-Time awards; SIP receiving the Eurographics Best Paper award; and IMUPoser receiving a CHI Best Paper Honorable Mention. These are **September 2026 snapshots from different sources and are not comparable rankings**. Citation count is an influence signal, not proof of correctness.

## Project and assessment claim audit

The assessments found many real code risks, but several of their strongest numerical interpretations are themselves incorrect. A new paper should use the following audited version.

| Claim in paper/proposal/assessments | What the repository actually supports | Required action |
|---|---|---|
| The current platform is the paper's 15-pod Wi-Fi/router/Raspberry Pi architecture. | The checked-in implementation uses a 16-byte ESP-NOW pod packet, an ESP32-S3 dongle/hub, USB WebSerial, and a mixed phone JSON path. Firmware/parser enumerate pod IDs 0–16. The rig initializes phone `Hips` plus pod IDs 0–14 (including `HipsAlt`), but IDs 15–16 (`LeftShoulder`/`RightShoulder`) can reach a dereference before their state exists. | Version the architecture, redraw the system diagram from current code, state the exact functional sensor count, and test every configured ID end to end. Distinguish checked-in Assessment-2 instrumentation from firmware actually compiled/flashed during each capture. |
| The preprint's 2–5° error, <15 ms latency, and ≥99.7% delivery are established by artifacts here. | Those numbers are claims in the preprint, but the corresponding OptiTrack data, protocol, sample size, scripts, and uncertainty analysis are not present in this checkout. Local captures use a Rokoko inertial reference. | Do not reuse the headline numbers until the original evidence is recovered and independently rerun, or replace them with a new synchronized optical study. |
| `global_joint_rmse_mean` ≈42–43 and `pose_joint_rmse_mean` ≈15–16 are angular errors in degrees. | The comparison code computes Euclidean distances between joint positions and labels the output **BVH units**. The assessments and ML proposal mislabel these values as degrees. | Retract the degree symbol and all claims derived from it; recompute actual joint-rotation geodesic errors in degrees and position errors in calibrated centimetres. |
| The ≈3× gap proves heading is the dominant error. | Neither value is raw world error: the script first latency-shifts/trims, subtracts each initial root origin, and applies a fitted trajectory yaw. The “pose” metric additionally subtracts the root per frame, applies fitted scale, and excludes the pelvis. The ratio therefore mixes trajectory translation, scale/retargeting, articulation, and different joint sets. | Compare like with like. Report unaligned world MPJPE, translation-aligned MPJPE, explicitly scale-aligned MPJPE, root orientation, segment-relative rotation, and trajectory errors separately. |
| −71° and +99° are measured DMP/IMU yaw errors. | `estimate_yaw()` aligns **root position trajectories in the horizontal plane**. It does not compare sensor orientation. The result also conflates WebXR and reference coordinate frames. | Measure root/pod orientation against synchronized optical segment frames. Use the old values only as evidence that coordinate-frame alignment is unresolved, not as IMU drift magnitude. |
| A hardcoded 30 Hz BVH declaration over ≈32 Hz input proves ≈7% progressive time dilation. | Recording samples the current rendered rig with `setInterval(..., 1000/30)`; it does not append one frame per arriving pod packet. The exporter can read `window.mesqFrameTiming`, but no writer exists in this checkout, so it falls back to 1/30. Timer jitter and asynchronous/stale sensor state remain serious, but input-rate mismatch alone does not establish 7% dilation or cause the weak correlation by itself. | Wire per-frame monotonic timing into recording; record acquisition, send, hub receive, browser receive, composition, and export times. Derive export timing from actual recorded frames and measure skew/jitter empirically. Correct the stale “measured 7%” comments in the exporter. |
| Assessment-2's I12 timing/provenance patch makes a trustworthy BVH. | I12 is unwired. It also unconditionally writes a semicolon-prefixed provenance line immediately after `Frame Time:`. BVH has no universally supported comment syntax, and both the primary benchmark parser and `tools/offline_analysis.py` attempt to parse every subsequent non-empty line as floats. Current I12 exports therefore fail those two float-only consumers; only `tools/bvh_kin.py` explicitly skips semicolon lines. | Put provenance/timestamps in a sidecar or a tested extension, wire the recorder, and add exporter→every-consumer round-trip tests. Do not call I12 complete until a generated capture parses through the primary comparator. |
| The system has no host-side filter/smoothing. | The browser performs stateful per-bone quaternion SLERP (factors about 0.11–0.40) and hip-position LERP (0.1). | Treat this as an explicit filter. Measure its delay and attenuation versus update rate; make coefficients time-constant based rather than packet-rate dependent. |
| The benchmark covers whole-body motion and arms need only aliases. | Stored Rokoko reference BVHs contain no arm joints. Mesquite arms therefore cannot be validated from those captures. | Re-capture a reference export that includes arms; no alias-only patch can manufacture missing reference joints. |
| Phase 3 proved the cache-clearing symptom's cause. | A negative reconstruction radicand makes reconstructed `qw` NaN, but the component quantizer generally maps only `qw` to zero while finite `qx/qy/qz` remain with squared norm greater than one; the browser later normalizes mapped targets. This supports a non-unit/large-rotation corruption path, not the assessment's exact all-zero, permanent-poisoning chain. Persistent JSON/calibration poisoning remains plausible but unproven. No live Phase-2 run establishes frequency or the cache symptom's cause. | Validate at source and binary/JSON browser boundaries, deliberately inject each corruption class, test calibration capture, and measure recovery. Reject grossly invalid samples; normalize only near-valid ones. Do not claim a confirmed cause until reproduced on hardware. |
| Phase 2 produced live hardware measurements that close the assessment. | The Phase-2 directory contains instrumentation patches and offline/replay analyses, but its own records say firmware/browser hooks were not compiled, flashed, or run as a complete hardware campaign. WEB-02/HUB-02 evidence comes from replay fixtures, so it establishes code-path possibility rather than live incidence. | Run the staged measurement campaign, archive raw outputs/build identifiers, and keep `fact-code`, `replay`, `measured-hardware`, and `inference` labels distinct. |
| The Euler singularity count proves exported rotations are corrupt. | The offline tool counts proximity of the middle Euler angle and raw first/third-coordinate jumps. It does not perform quaternion round-trip error, unwrap angles, or demonstrate a corrupted pose. The reported 23.9% is an exposure/risk indicator only. | Add quaternion→Euler→quaternion round-trip geodesic error, continuity-aware angle handling, and downstream importer tests before claiming actual damage. Prefer rotation-native storage for research data. |
| Current root calibration is a stable position offset. | `initialPosition` aliases the live hip `Vector3`, which is later mutated by LERP. Subtracting that moving object can create feedback/scale distortion. The five-second T-pose flow takes one final `last` sample rather than averaging five seconds, and whether it ran for the stored captures is open. | Clone/freeze the initial vector, make calibration an explicit averaged/robust window with validity checks, log whether/when it ran, and test known translations and repeated calibration. |
| The active system already fuses phone/WebXR heading continuously. | While a pelvis pod (`HipsAlt`) is active, its orientation is preferred and incoming phone `Hips` orientation is ignored; phone position is handled on a separate path. The phone orientation is used only after the pelvis pod has been absent for more than one second. The position guard also drops otherwise valid frames when any coordinate is exactly zero. | Treat continuous visual world-heading fusion as a **new estimator path**, not an existing factor. First fix the zero-coordinate guard and live-reference root bug, then define phone-to-pelvis extrinsics, tracking confidence, reset handling, and an explicit orientation-fusion ablation. |
| A static ten-minute run fully distinguishes initialization from drift. | It can distinguish an arbitrary initial yaw from a constant residual bias under static conditions, but not dynamic bias, scale/misalignment, acceleration sensitivity, or motion-dependent DMP behavior. | Run static, controlled rotation, and representative dynamic trials against a reference. |
| Phone/WebXR supplies absolute heading. | It can supply a shared locally consistent frame, but an unanchored SLAM world has yaw gauge freedom and may reset/relocalize. | Define the WebXR reference-space lifecycle, tracking quality, reset events, and a calibrated transform between phone, pelvis, IMUs, and optical world. |
| Phase 2 rules out phone relocalization jumps. | No jump was found in the saved Mesquite BVH root trajectory, which is useful evidence for those captures. However, that trajectory is downstream of browser root LERP, and the raw WebXR pose/events were not retained, so smoothing could attenuate a discontinuity. | Report this as “not observed in the smoothed exported captures,” retain raw phone pose/tracking events next time, and inject measured resets into the full pipeline. |
| Phase-2 D4 measures per-segment yaw drift. | Its per-frame yaw is a positional alignment surrogate over root-centred joints. A ±1 s assumed timing shift changed the mean by 1,176° and reversed the slope; even an optimal per-frame yaw reduced the still-contaminated positional error by only 24–32%. | Call D4 inconclusive, not a drift measurement. It is weak evidence against heading being the sole dominant residual and a strong reason to measure synchronized segment/joint orientations directly. |
| Double-stance closure uniquely determines yaw correction. | Closure is an excellent inconsistency metric, but correction allocation can be underdetermined without contact, joint, root, and confidence assumptions. | Use it first as a quality signal; demonstrate observability and correction allocation before claiming a unique solution. |
| Fixed segment length alone detects sensor slip. | If positions come only from FK with fixed rig lengths, length consistency is tautological. | Combine fixed length with independent positions, contacts, loop closure, or optical observations. |
| The paper's `q_raw · q_tpose^-1 · q_box` calibration equation matches current code. | Calls pass `bc` as a third argument to Three.js `multiplyQuaternions(a,b)`, so that third argument is ignored. A newer mounting-offset operation is applied separately, so this is a paper/code mismatch, not proof that all calibration fails. | Unit-test multiplication order and frames, then document the equation actually executed. |

### What remains well supported

- Magnetometer-free 6-axis attitude estimation does not observe absolute yaw from gravity alone.
- The current packet includes a per-node counter and a low 16-bit send-time clock, but lacks a full acquisition-time model and end-to-end validity semantics.
- Pod quaternion state is shared across two FreeRTOS tasks without an explicit atomic snapshot or queue.
- Invalid/non-unit samples are not rejected at the active binary browser boundary.
- Current calibration, coordinate-frame composition, smoothing, and export behavior need direct tests.
- Existing hardware rates, loss distributions, latency, long-run failure frequency, and drift magnitude remain unmeasured in Phase 2.
- Existing Assessment-2 instrumentation in the working tree is not evidence that the same code was deployed during the stored captures.
- No learned correction pipeline, trained model, or ML evaluation is present in the repository.

### Local evidence trail

These links are an audit trail into the reviewed checkout, not claims that the code was deployed on any particular capture. Line numbers describe the 15 September 2026 working tree and may move after edits.

| Audit point | Primary local evidence | Interpretation |
|---|---|---|
| Current packet and 17-ID range | [pod packet definition](<../Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino#L211>) and [ID range/table](<../Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino#L55>); [browser length/ID map](../js/webserialnative.js#L27) | The active wire record is 16 bytes and IDs 0–16 exist; this is not the preprint's described string/Wi-Fi/Raspberry-Pi data path. |
| Possible non-coherent firmware record | [shared `Quat`](<../Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino#L308>), [two pinned tasks](<../Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino#L620>), [radio reads components](<../Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino#L720>), [IMU writes components](<../Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino#L952>) | Source inspection supports a race-risk hypothesis; only live stress/instrumentation can establish incidence or effect. |
| Quaternion-corruption path and its limit | [`q_to_i16`](<../Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino#L246>), [unchecked square root](<../Device code/Pod_Watch_Binary/Pod_Watch_Binary.ino#L917>), [binary unpack](../js/webserialnative.js#L83), [browser target mapping and normalization](../js/custom_icm.js#L439) | A negative radicand can make `qw` NaN; quantization makes that component zero while the finite vector components remain. This can become a bad/large rotation, but it does not prove an all-zero quaternion or permanent cache poisoning. |
| Shoulder initialization defect | [initialization ends without shoulder entries](../js/custom_icm.js#L95), [dereference before guard](../js/custom_icm.js#L500), [late fallback](../js/custom_icm.js#L911) | IDs 15–16 are parsed, but their `mac2Bones` state is not safely initialized before use. |
| Phone orientation is fallback-only | [phone-versus-pelvis selection](../js/custom_icm.js#L581) | `HipsAlt` wins while fresh; phone orientation does not continuously anchor pelvis heading in the active implementation. |
| Root-position bugs | [live `initialPosition` alias](../js/custom_icm.js#L227), [truthiness guard and root LERP](../js/custom_icm.js#L920), [single delayed calibration call](../js/custom_icm.js#L1278) | Exact-zero WebXR coordinates are dropped; the calibration reference is mutable; the five-second flow samples once at the end rather than averaging a window. |
| Active presentation smoothing | [per-bone SLERP factors](../index.html#L138), [orientation SLERP example](../js/custom_icm.js#L581), [root LERP](../js/custom_icm.js#L946) | Raw estimator, corrected pose, and displayed/exported pose must be logged separately; smoothing has rate-dependent delay today. |
| Recorder/export timing | [30 Hz recorder timer](../index.html#L368), [unwired timing fallback and stale causal comment](../js/bvh_converter.js#L157) | Input is not recorded once per received packet, and no writer for `window.mesqFrameTiming` was found; a 7% progressive dilation is therefore not established. |
| Exporter/parser incompatibility | [semicolon metadata after `Frame Time`](../js/bvh_converter.js#L181), [primary comparator parses all rows as floats](../Mesquite_benchmarks/scripts/compare_bvh_suits.py#L288), [`offline_analysis.py` does likewise](../tools/offline_analysis.py#L23), while [`bvh_kin.py` skips semicolons](../tools/bvh_kin.py#L35) | I12 is unwired, and its current output is format-incompatible with both float-only analysis parsers. Use a sidecar or update and round-trip-test every consumer. |
| What the benchmark's “yaw” is | [`estimate_yaw` on horizontal positions](../Mesquite_benchmarks/scripts/compare_bvh_suits.py#L570) and [application to root-relative trajectories](../Mesquite_benchmarks/scripts/compare_bvh_suits.py#L930) | This is trajectory-coordinate alignment, not a measurement of an IMU's heading error. |
| What the two RMSEs are | [global/root-relative position loops](../Mesquite_benchmarks/scripts/compare_bvh_suits.py#L976) and [output units](../Mesquite_benchmarks/scripts/compare_bvh_suits.py#L1299) | Both are Euclidean joint-position statistics in BVH units; pose mode also root-centres, scales, and excludes the pelvis. Neither is angular RMSE. |
| Stored values and missing arms | [session 01 values](../Mesquite_benchmarks/outputs/bvh_verification/01_MMcap_bvh_2026-6-8-13-20-59_vs_MesBench1_2_ue5/metrics.json#L25), [session 02 values](../Mesquite_benchmarks/outputs/bvh_verification/02_MMcap_bvh_2026-6-8-13-22-52_vs_MesBench1_3_ue5/metrics.json#L25), and [reference-only/missing mappings](../Mesquite_benchmarks/outputs/bvh_verification/01_MMcap_bvh_2026-6-8-13-20-59_vs_MesBench1_2_ue5/metrics.json#L103) | The files contain the quoted 42/15 values and missing-arm mappings; they do not turn those values into degrees or optical truth. |
| Phase-2 evidence strength | [explicit statement that no hardware was available](../system_assessment_2/MEASUREMENTS.md#L3), [open deployment/timing questions](../system_assessment_2/OPEN_QUESTIONS.md#L3), and [retraction registry](../system_assessment_2/RETRACTIONS.md#L1) | Preserve `fact-code`, `fact-data`, `fact-fixture`, `fact-measured`, and inference as distinct evidence levels. Some conclusions in `RETRACTIONS.md` are themselves superseded by the audit above. |
| Inconclusive yaw surrogate | [Assessment-2 D4 sensitivity sweep](../system_assessment_2/MEASUREMENTS.md#L111) | Its sign/magnitude collapses under plausible timing shifts; the surviving 24–32% best-case reduction only weakens heading-dominance, and remains contaminated. |
| Proposal claims requiring correction | [baseline table and interpretation](../ML_PROJECT_PROPOSAL.md#L13) | The degree units, exact heading values, 7% cause, all-zero quaternion story, and “heading becomes recoverable” wording should not enter a submission unchanged. |

## Problem-to-literature map

| Mesquite need | Assessment/project section | Best papers to start with | What the literature can actually deliver |
|---|---|---|---|
| Establish observability and sensor-error model | Proposal P1; Assessment 3 drift | Kok et al.; Mahony et al.; VQF | Correct mathematical framing and strong filter/bias baselines; no absolute yaw from 6-axis data alone. |
| Relative-heading and joint constraints | Proposal Stage 3; Assessment 3 C/F/G | Seel et al.; Laidig et al. 2019/2022; Lehmann et al.; Teufl et al. | Direct relative-heading/joint-angle methods under explicit joint models and excitation; not absolute root heading. |
| Sensor-to-segment and intrinsic calibration | Proposal Stage 4; Assessment 3 calibration | Tedaldi et al.; Miezal et al.; Taetz et al.; Pacher et al. | Intrinsic calibration and joint/mounting estimators; online strap-slip correction still requires careful observability and gating. |
| Packet loss, invalid rotations, and reconstruction | Proposal Stages 1–2; Assessments 1/3 | BRITS; Shoemake; QuaterNet; Zhou et al.; Huynh | Mask-aware sequence modeling and rotation-safe interpolation/losses; only after timestamps and validity masks exist. |
| Distributed timing and coherent records | Proposal P3; Assessment 1 sync; Assessment 3 architecture | Chen et al.; FTSP; RBS; Herlihy & Wing | Acquisition timestamps, affine clock mapping, receiver/broadcast synchronization concepts, and atomic-record reasoning. |
| Wearable-system reliability and evaluation | Assessments 1–2 | Mercury; Chen et al.; Saltzer et al. | Evaluation dimensions, end-to-end validation, link/data coverage, energy/latency, and explicit failure accounting. |
| Learned body/motion prior | Proposal Stage 3 | SIP; DIP; TransPose; PIP; TIP; PNP; DynaIP; ProbIP; DiffusionPoser; GlobalPose | Strong design/comparison family for ambiguity, temporal consistency, contacts, physics, real-data transfer, and uncertainty, **conditional on compatible inputs**. They do not automatically solve Mesquite's clock or global-yaw gauge. |
| Synthetic data and body model | Proposal data section | SMPL; AMASS; DIP; DynaIP | A standardized body representation and large motion corpus; strong warning that synthetic-only training has a real-domain gap. |
| Phone/IMU fusion | Existing WebXR design; Assessment 3 E | EgoLocate | Closest architecture comparator and a model for jointly calibrated camera/IMU feedback. |
| Visible jitter versus interactive latency | Active browser smoothing; Assessments 1–2 host path | Casiez et al. (1€ Filter) | A transparent speed-adaptive jitter–lag baseline and tuning procedure; it must be adapted on SO(3), evaluated against the current SLERP, and cannot repair sensing bias. |
| Commodity-device/HCI framing | Affordability/accessibility claim | IMUPoser; OpenCap | Examples of strong accessibility framing backed by datasets, user/application motivation, and transparent validation. |
| Valid ground-truth study and statistics | Paper evaluation; Assessment 2 retractions | Robert-Lachaine et al.; OpenSense; Bland & Altman; Huynh | Simultaneous optical reference, separation of technological/model error, long-duration trials, agreement statistics, and rotation metrics. |

### Assessment-to-literature crosswalk

The assessment identifiers below are retained so a repair can be traced back to the local reports. Papers justify principles and comparison methods; they do **not** substitute for testing the Mesquite implementation.

| Assessment finding family | Local issue | Literature to use | Concrete response |
|---|---|---|---|
| [`NODE-01`](../system_assessment/01_node_firmware/REPORT.md) / [`EST-06`](../system_assessment/05_estimation/REPORT.md) | Non-atomic quaternion producer/consumer snapshot | Herlihy & Wing; Saltzer et al. | Define one immutable acquired sample and measure the chosen ESP32 handoff under stress. Linearizability is an analogy for coherent publication, not proof that a torn read occurs. |
| [`SENS-03`](../system_assessment/01_node_firmware/REPORT.md) and [Assessment-3 browser-state hypothesis](../system_assessment_3/01_BROWSER_STATE_POISONING.md) | Quaternion reconstruction/quantization, missing validity checks, recovery, and possible persistent state | Huynh; Shoemake; Saltzer et al. | Validate before/after quantization, inject each fault class, and measure recovery. The precise cache-poisoning mechanism remains unproven. |
| [`SYNC-01`, `NODE-02`/`SYNC-02`, `SYNC-03`, `SYNC-05`, `SYNC-08`](../system_assessment/08_sync_integrity/REPORT.md) | Send-time rather than acquisition-time semantics, truncated/unrelated clocks, invisible loss, and asynchronous/stale body assembly | Chen et al.; FTSP; RBS; Mercury | Timestamp acquisition, retain hub receive time, estimate affine clock maps, common-time resample, and report skew/residual/sample-age/loss-burst distributions. |
| [`NET-01`–`NET-06`](../system_assessment/03_network/REPORT.md) | Fleet delivery, PHY/rate assumptions, congestion, queues, and end-to-end latency | Mercury; FTSP; RBS; Chen et al.; Saltzer et al. | Measure loss-burst CDFs, worst-pod delivery, airtime, queueing, energy, and physical-event-to-display tails on the full fleet. |
| [`WEB-01`–`WEB-04`](../system_assessment/04_webapp/REPORT.md) | Parser state, mixed binary/JSON paths, DOM work, stale composition, and unwired export timing | Saltzer et al.; Casiez et al.; Shoemake | Isolate parsing from rendering, preserve sample age/validity, test replay against production code, measure smoothing delay, and round-trip exported files through every consumer. |
| [`EST-01`](../system_assessment/05_estimation/REPORT.md) | Absolute versus relative heading, initialization versus continuing drift, and weak observability | Kok et al.; VQF; Seel et al.; Laidig et al.; Lehmann et al. | State the yaw gauge, establish raw-data-compatible filter baselines, and use joint/ROM constraints only under their excitation and joint-model assumptions. |
| [`EST-04`](../system_assessment/05_estimation/REPORT.md) | Fifteen chips treated as intrinsically identical | Tedaldi et al.; VQF; Kok et al. | Estimate/version per-device gyro/accelerometer bias, scale and cross-axis parameters separately from per-session sensor-to-segment mounting. |
| [`KIN-01`, `KIN-05`, `KIN-06`](../system_assessment/06_kinematics/REPORT.md) | Missing kinematic correction, absent joint limits, and skeleton-proportion/retargeting contamination | Skog et al.; Seel et al.; Laidig et al.; Lehmann et al.; OpenSense; Robert-Lachaine et al. | Evaluate contact/joint constraints inside a declared estimator, add an explicit range-of-motion baseline, document skeleton proportions and retargeting, and separate sensor, biomechanical-model, scale, and retargeting error. |
| [`PHN-03`, `PHN-04`, `PHN-05`](../system_assessment/07_phone_slam/REPORT.md) | WebXR reference lifecycle, root trajectory, and Euler/BVH exposure | EgoLocate; Pons-Moll et al.; Zhou et al.; Huynh | Log raw phone pose/confidence/reset events, calibrate the phone–body frame, and keep rotation-native research records. |
| [`P2-B10-01`, `P2-B10-02`](../system_assessment_2/10_evaluation/REPORT.md) | Timing-contaminated evaluation and an arm-free reference mapping | Robert-Lachaine et al.; OpenSense; Bland & Altman; Huynh | Stop drawing accuracy conclusions from the current pair, re-capture synchronized optical data with all evaluated limbs, and predefine frame-aware rotation/position metrics. |

Several findings—including the shoulder initialization order, the zero-coordinate truthiness guard (`PHN-01`), unsafe/missing DOM elements, and the live `initialPosition` alias—are direct engineering defects. Fix them with unit/integration tests; citing a paper does not turn them into research contributions.

## Prioritized reading list

### Tier A — read before changing the research claim

1. **Kok, Hol & Schön (2017)** — observability, inertial error models, filtering and smoothing.
2. **Chen et al. (2010)** — the most directly matched paper on synchronization and calibration errors in an inertial body sensor network.
3. **Robert-Lachaine et al. (2017)** — how to validate a whole-body inertial system against optical capture without mixing model and technology error.
4. **OpenSense (2022)** — simultaneously recorded, post-hoc-aligned long-duration IMU/optical data, biomechanical constraints, drift assessment, and an open implementation.
5. **Seel, Raisch & Schauer (2014)** — functional joint-axis calibration and magnetometer-free relative joint angles.
6. **Laidig, Weygers & Seel (2022)** — self-calibrating, magnetometer-free relative-heading estimation for 2-DoF joints.
7. **VQF (2023)** — a strong reproducible fusion/bias/magnetic-gating baseline, with the important raw-data caveat.
8. **DIP (2018), TransPose (2021), PIP (2022), and TIP (2022)** — the minimum modern learned-inertial baseline family.
9. **DynaIP (2024)** — the clearest warning and remedy for synthetic-to-real training mismatch.
10. **Zhou et al. (2019), Huynh (2009), and QuaterNet (2018)** — rotation representation, geodesic evaluation, and differentiable-FK loss.
11. **Transformer IMU Calibrator (2025)** — the closest learned paper for dynamic sensor drift and sensor-to-body offset calibration.
12. **DiffusionPoser (2024) and GlobalPose (2025)** — generative reconstruction from arbitrary subsets of 13 predefined sensor sites and the newest contact/physics-based global-motion baseline.

### Tier B — read while designing the method

- Taetz et al. (2016), Miezal et al. (2016), Tedaldi et al. (2014), Pacher et al. (2020), Lehmann et al. (2020), and Skog et al. (2010).
- SIP (2017), PNP (2024), ProbIP (2025), EgoLocate (2023), IMUPoser (2023), and MagShield (2025).
- Loose Inertial Poser (2024) for attachment motion; MobilePoser (2024) for commodity subsets; RobustCap (2023) and DiffCap (2025) if phone images become a real input.
- BRITS (2018), FTSP (2004), RBS (2002), Mercury (2009), and the 1€ Filter (2012).

### Tier C — cite or consult for foundations

- Mahony et al. (2008), Roetenberg et al. (2005), Shoemake (1985), SMPL (2015), AMASS (2019), Bland & Altman (1986), Saltzer et al. (1984), Herlihy & Wing (1990), and OpenCap (2023).

## How to read efficiently

For every empirical paper, use the same pass:

1. **Five-minute pass:** abstract, contribution bullets, figures, tables, limitations.
2. **Assumption pass:** write down sensors, sampling rate, synchronization, coordinate frames, calibration motion, magnetic assumptions, joint model, and whether the method estimates relative or absolute heading.
3. **Evaluation pass:** record participants, duration, motion types, ground truth, train/test split, alignment transforms, units, metrics, uncertainty, and failure cases.
4. **Transfer pass:** label each idea `directly reusable`, `baseline only`, `needs adaptation`, or `not identifiable in Mesquite`.
5. **Reproduction pass:** run released code first on its provided example, then on one carefully instrumented Mesquite sequence before changing architecture.

Do not begin by copying network architecture diagrams. Begin with each paper's coordinate-frame definitions, assumptions, loss/constraint, and evaluation transform; those are where apparently similar inertial methods differ.

---

## Detailed paper reviews

The reviews below explicitly answer: why the paper was selected, what Mesquite section it supports, whether it offers a real solution, what to take into the manuscript, and how much to read.

### A. Foundations, timing, and defensible evaluation

#### A1. Using Inertial Sensors for Position and Orientation Estimation

**Citation.** Manon Kok, Jeroen D. Hol, and Thomas B. Schön. *Foundations and Trends in Signal Processing*, 11(1–2), 1–153, 2017. [DOI](https://doi.org/10.1561/2000000094) · [Open manuscript](https://arxiv.org/abs/1704.06053)

**Why selected.** This is the best single tutorial for replacing informal “drift” language with sensor models, state definitions, observability, filtering, and smoothing. It directly supports the proposal's P1/P3 framing and prevents the paper from implying that a neural prior turns an unobservable global gauge into a physical measurement.

**Does it solve Mesquite?** **Framework and baseline, not a drop-in solution.** It explains why aiding information is required and how bias/noise propagate. Mesquite still must choose and validate its aiding signals.

**Use in this project.** Define gyro/accelerometer error models, distinguish bias-driven drift from arbitrary initialization, state the yaw gauge explicitly, and design an offline smoother baseline before claiming ML improvement.

**Read recommendation.** **Must read.** Read the sensor-model, orientation-estimation, filtering/smoothing, calibration, and observability material closely; skim position-only examples that do not match the phone-root design. Make a one-page table of each state, observation, frame, and unobservable degree of freedom.

#### A2. Characterizing and Minimizing Synchronization and Calibration Errors in Inertial Body Sensor Networks

**Citation.** Shanshan Chen, Jeff Brantley, Taeyoung Kim, and John Lach. *5th International ICST Conference on Body Area Networks (BodyNets 2010)*, 138–144, 2010. [DOI and full record](https://eudl.eu/doi/10.1145/2221924.2221951). The EUDL page's 2012 date is online-publication metadata; the conference was in 2010.

**Why selected.** It matches Mesquite unusually closely: multiple body-worn inertial nodes, node-to-node synchronization, sensor and mounting calibration, joint-angle output, and optical ground truth. It quantifies error sources instead of treating all disagreement as filter drift.

**Does it solve Mesquite?** **Partly and directly.** Its experimental decomposition and mitigation logic transfer; its particular hardware/protocol does not.

**Use in this project.** Rebuild the error budget around clock offset/skew, sampling timestamp, intrinsic calibration, mounting variation, and application accuracy. Perturb each factor independently in synthetic and hardware experiments.

**Read recommendation.** **Must read in full** because it is short and directly matched. Recreate its error-source table for Mesquite and add packet loss, asynchronous composition, browser smoothing, and WebXR frame registration.

#### A3. Validation of Inertial Measurement Units with an Optoelectronic System for Whole-Body Motion Analysis

**Citation.** Xavier Robert-Lachaine, Hakim Mecheri, Christian Larue, and André Plamondon. *Medical & Biological Engineering & Computing*, 55(4), 609–619, 2017. [DOI](https://doi.org/10.1007/s11517-016-1537-2) · [PubMed record](https://pubmed.ncbi.nlm.nih.gov/27379397/)

**Why selected.** This study separates **technological orientation error** from differences caused by the biomechanical model and tests both short/simple and long/complex tasks. That separation is exactly what the current 42-versus-15 benchmark fails to do.

**Does it solve Mesquite?** **It solves the validation-design problem, not the estimator.** Its rigid optical marker clusters on the IMUs isolate sensor technology error; anatomical modeling then exposes model/calibration error.

**Use in this project.** Use simultaneous optical capture, rigid clusters attached to pods, anatomical markers, short and long tasks, and separate technology/model analyses. Do not call a commercial inertial suit “ground truth.”

**Read recommendation.** **Must read.** Focus on instrumentation, coordinate-system definitions, technological-versus-model decomposition, duration/task effects, and limitations. Copy the logic of the study design, not its numerical thresholds.

#### A4. OpenSense: An Open-Source Toolbox for IMU-Based Measurement of Lower-Extremity Kinematics over Long Durations

**Citation.** Mazen Al Borno, Johanna O'Day, Vanessa Ibarra, James Dunne, Ajay Seth, Ayman Habib, Carmichael Ong, Jennifer Hicks, Scott Uhlrich, and Scott Delp. *Journal of NeuroEngineering and Rehabilitation*, 19, Article 22, 2022. [DOI/full text](https://doi.org/10.1186/s12984-022-01001-x) · [OpenSense software](https://mobilize.stanford.edu/software/opensense/)

**Why selected.** It offers an open, reproducible workflow, simultaneously recorded IMU/optical data aligned post hoc by maximizing cross-correlation, ten-minute trials, biomechanical inverse kinematics, drift analysis, and comparisons across filters. It is a much stronger template for Mesquite's “open and affordable” evaluation story than a two-file BVH comparison.

**Does it solve Mesquite?** **Partly.** Its model-constrained kinematics and validation workflow can be reused or benchmarked. Its lower-body setup and sensor hardware differ; the study used magnetometer-containing orientation estimates and excluded/downweighted problematic sensors, so its drift result is not evidence for a pure 6-axis Mesquite design. Cross-correlation should not replace true timestamps when Mesquite controls the protocol.

**Use in this project.** Use OpenSense/OpenSim as an independent constrained-kinematics baseline and consider publishing the new synchronized Mesquite dataset in a compatible form.

**Read recommendation.** **Must read.** Study data collection, synchronization, sensor fusion, calibration, inverse kinematics, long-duration protocol, drift-slope analysis, and supplementary tables. Then run one exported Mesquite trial through OpenSense if the coordinate data can be mapped correctly.

#### A5. Statistical Methods for Assessing Agreement Between Two Methods of Clinical Measurement

**Citation.** J. Martin Bland and Douglas G. Altman. *The Lancet*, 327(8476), 307–310, 1986. [DOI/publisher page](https://doi.org/10.1016/S0140-6736(86)90837-8)

**Why selected.** The current benchmark uses root-speed correlation to estimate lag, then reports errors without a complete agreement/repeated-measures analysis. This seminal paper explains why correlation is not agreement and introduces bias and limits of agreement.

**Does it solve Mesquite?** **Yes, for one part of statistical reporting.** It does not handle every repeated-measures complication by itself, but it corrects the present interpretation error.

**Use in this project.** Predefine acceptable errors, report bias and 95% limits of agreement with confidence intervals, and account for repeated frames nested within participants rather than treating thousands of frames as independent subjects.

**Read recommendation.** **Read the entire four-page paper.** It is short. Then read A5b for the project's multiple-observations-per-participant design.

#### A5b. Agreement Between Methods of Measurement with Multiple Observations per Individual

**Citation.** J. Martin Bland and Douglas G. Altman. *Journal of Biopharmaceutical Statistics*, 17(4), 571–582, 2007. [DOI](https://doi.org/10.1080/10543400701329422)

**Why selected.** Mesquite produces many frames and repeated trials from each person; the 1986 introductory paper alone does not specify how to handle that clustering. This follow-up treats multiple measurements per individual.

**Does it solve Mesquite?** **It supplies a statistical starting point, not the full analysis plan.** Modern hierarchical/participant-bootstrap confidence intervals may still be preferable for the final design.

**Use in this project.** Estimate method bias and limits of agreement without treating thousands of correlated frames as thousands of independent participants. Define the biological/engineering unit and repeated-measure structure explicitly.

**Read recommendation.** **Read the worked cases and assumptions.** Consult a statistician before freezing inference if trials, joints, activities, sessions, and participants form multiple nested levels.

#### A6. Metrics for 3D Rotations: Comparison and Analysis

**Citation.** Du Q. Huynh. *Journal of Mathematical Imaging and Vision*, 35, 155–164, 2009. [DOI](https://doi.org/10.1007/s10851-009-0161-2)

**Why selected.** The project needs a mathematically defined rotational error in degrees. The current “joint RMSE” is Euclidean position error in BVH units, while Euler-coordinate differences are representation dependent.

**Does it solve Mesquite?** **Yes, for choosing and explaining an SO(3) error metric.** It does not solve alignment or ground truth.

**Use in this project.** Report the geodesic angle of the relative rotation, specify whether comparison is world, root-relative, sensor, segment, or joint-local, and handle quaternion sign equivalence.

**Read recommendation.** **Must read selectively.** Read the definitions, invariance analysis, and geometric comparison. Implement tests such as identity, sign-flipped quaternion, 180° boundary, and known axis-angle offsets before applying the metric to data.

#### A7. The Flooding Time Synchronization Protocol (FTSP)

**Citation.** Miklós Maróti, Branislav Kusy, Gyula Simon, and Ákos Lédeczi. *Proceedings of the 2nd ACM Conference on Embedded Networked Sensor Systems (SenSys)*, 39–49, 2004. [DOI](https://doi.org/10.1145/1031495.1031501) · [Official SenSys program](https://sensys.acm.org/2004/program.html) · [SenSys Test-of-Time recognition](https://sensys.acm.org/tot/)

**Why selected.** FTSP is a foundational, Test-of-Time sensor-network synchronization paper. It combines timestamping near the communication boundary, repeated synchronization, clock-skew estimation, and robustness—precisely the concepts missing from a 16-bit send-time field with unrelated node origins.

**Does it solve Mesquite?** **It is an implementation model, not a copy-paste protocol.** ESP-NOW and the S3 hub have different timestamp access and topology.

**Use in this project.** Fit an affine mapping from every pod clock to a hub timebase, timestamp as near acquisition and radio receive as possible, retain residuals, and report offset/skew error. A one-byte 0–255 ms offset cannot encode a one-second beacon period.

**Read recommendation.** **Must read for the firmware/network author.** Focus on MAC-layer timestamping, linear regression/skew, outlier rejection, resynchronization, and the evaluation method.

#### A8. Fine-Grained Network Time Synchronization Using Reference Broadcasts

**Citation.** Jeremy Elson, Lewis Girod, and Deborah Estrin. *5th USENIX Symposium on Operating Systems Design and Implementation (OSDI)*, 2002. [Official USENIX page and paper](https://www.usenix.org/conference/osdi-02/fine-grained-network-time-synchronization-using-reference-broadcasts)

**Why selected.** RBS shows why receiver-to-receiver comparison can remove sender-side nondeterminism. This is valuable for thinking about simultaneous multi-pod events and broadcast sync in a star body network.

**Does it solve Mesquite?** **Conceptually, partly.** ESP-NOW receiver timestamps, body shadowing, callbacks, and clock APIs must be measured. Mesquite may ultimately choose a simpler hub-beacon/affine-clock scheme.

**Use in this project.** Compare RBS and FTSP concepts in a small prototype; quantify timestamp uncertainty at sensor sample, radio, hub, and browser stages instead of reporting a single unexplained “latency.”

**Read recommendation.** **Targeted read.** Read the delay decomposition, receiver/receiver idea, multi-hop discussion only for context, and experimental method.

#### A9. Mercury: A Wearable Sensor Network Platform for High-Fidelity Motion Analysis

**Citation.** Konrad Lorincz, Bor-rong Chen, Geoffrey Werner Challen, Atanu Roy Chowdhury, Shyamal Patel, Paolo Bonato, and Matt Welsh. *7th ACM Conference on Embedded Networked Sensor Systems (SenSys)*, 183–196, 2009. [DOI](https://doi.org/10.1145/1644038.1644057) · [Author/hosted paper](https://projects.csail.mit.edu/wiki/pub/Evodesign/EEGSensorNetworkArchitectures/mercury-sensys09.pdf)

**Why selected.** Mercury is a classic wearable sensing platform paper that treats data fidelity, coverage, radio conditions, latency, computation, energy, and long-duration deployment as coupled system outcomes.

**Does it solve Mesquite?** **No direct estimator solution.** It provides a strong systems architecture and evaluation precedent.

**Use in this project.** Add per-stage data coverage, link quality, battery/runtime, adaptive policy or local buffering rationale, long-session tests, and failure accounting. This is especially valuable for a SenSys/IoT-J version of Mesquite.

**Read recommendation.** **Read architecture and evaluation closely; skim clinical specifics.** Notice how every platform claim is connected to a measured system tradeoff.

#### A10. End-to-End Arguments in System Design

**Citation.** Jerome H. Saltzer, David P. Reed, and David D. Clark. *ACM Transactions on Computer Systems*, 2(4), 277–288, 1984. [DOI](https://doi.org/10.1145/357401.357402) · [MIT author copy](https://groups.csail.mit.edu/ana/Publications/PubPDFs/End-to-End%20Arguments%20in%20System%20Design.pdf)

**Why selected.** It supports validation of the result at the final consumer even when useful lower-layer checks exist: a source-side check does not guarantee that the complete delivered record is correct.

**Does it solve Mesquite?** **No; it is a design principle.** The finite/norm checks, sequence semantics, framing, and recovery policy remain Mesquite-specific.

**Use in this project.** Justify end-to-end quaternion validity, continuity, sequence, timestamp, and frame checks even after firmware-side safeguards are added.

**Read recommendation.** **Read the argument and examples, not every historical detail.** Use it sparingly in the systems discussion; regression tests and measured failure recovery are stronger than a theoretical citation alone.

#### A11. Linearizability: A Correctness Condition for Concurrent Objects

**Citation.** Maurice P. Herlihy and Jeannette M. Wing. *ACM Transactions on Programming Languages and Systems*, 12(3), 463–492, 1990. [DOI](https://doi.org/10.1145/78969.78972) · [Author copy](https://www.cs.cmu.edu/~wing/publications/HerlihyWing90.pdf)

**Why selected.** It gives rigorous language for the desired property of a quaternion/time/sequence record shared across tasks: the transfer should appear as one atomic event. Source inspection shows a plausible unsynchronized-read risk, but this paper is not evidence that tearing actually occurs on this ESP32 build.

**Does it solve Mesquite?** **No direct FreeRTOS implementation.** A queue, seqlock, critical-section snapshot, or double buffer must be selected and tested using platform documentation.

**Use in this project.** Define the unit of atomicity as `{quaternion, acquisition timestamp, sequence, validity}` and transfer it as one record. The instrumentation must use the same coherent record rather than rereading globals.

**Read recommendation.** **Skim the definition and intuitive examples.** Use official ESP-IDF/FreeRTOS queue documentation for implementation; cite this paper only for the correctness concept.

### B. Heading, observability, contacts, and calibration

**Input-compatibility warning.** VQF, Seel's gyro/acceleration constraints, the rotation-rate form of Laidig's 2-DoF method, Tedaldi's intrinsic calibration, and foot-mounted navigation/ZUPT require raw inertial channels. They cannot be reconstructed from Mesquite's present quaternion-only packet. Laidig et al. (2022) also give an orientation-only variant for on-chip-fused orientations, and Lehmann's ROM objective can operate on fused relative orientations, but those compatible variants may be weaker and must be identified and benchmarked explicitly.

#### B1. VQF: Highly Accurate IMU Orientation Estimation with Bias Estimation and Magnetic Disturbance Rejection

**Citation.** Daniel Laidig and Thomas Seel. *Information Fusion*, 91, 187–204, 2023. [DOI/publisher page](https://doi.org/10.1016/j.inffus.2022.10.014) · [Open manuscript](https://arxiv.org/abs/2203.17024) · [Code](https://github.com/dlaidig/vqf)

**Why selected.** VQF is a recent, extensively validated, open-source quaternion fusion method with online/offline variants, gyro-bias estimation at rest and in motion, and decoupled magnetic-disturbance rejection. The publisher page reports 69 citations and comparison against eight prior algorithms on six datasets.

**Does it solve Mesquite?** **Direct baseline, conditional implementation.** It requires raw gyroscope/accelerometer data, and magnetometer data for its 9D path. It cannot simply correct an opaque DMP quaternion after the fact, nor can its 6D mode recover absolute yaw.

**Use in this project.** Log raw data and run VQF beside the DMP. Compare DMP, VQF-6D, VQF offline, and optionally gated VQF-9D. This determines whether replacing fusion is useful rather than assuming it is or is not the bottleneck.

**Read recommendation.** **Must read.** Study the basic filter, bias/rest detector, magnetic gating, offline variant, and evaluation. Run the released implementation on a synchronized single-pod reference trial before whole-body integration.

#### B2. Nonlinear Complementary Filters on the Special Orthogonal Group

**Citation.** Robert Mahony, Tarek Hamel, and Jean-Michel Pflimlin. *IEEE Transactions on Automatic Control*, 53(5), 1203–1218, 2008. [DOI](https://doi.org/10.1109/TAC.2008.923738) · [University record](https://researchportalplus.anu.edu.au/en/publications/nonlinear-complementary-filters-on-the-special-orthogonal-group/)

**Why selected.** This is a foundational SO(3) observer and gyro-bias reference; its university record reports 1,672 Scopus citations. It is useful both mathematically and as a strong classical baseline.

**Does it solve Mesquite?** **Partly.** It supplies geometry-respecting attitude and bias observers, but without an independent heading vector it cannot bound absolute 6D yaw. It also cannot be applied correctly without raw measurements.

**Use in this project.** Cite it when defining attitude and bias estimation on SO(3), and use it to distinguish “better filtering” from “new information that changes observability.”

**Read recommendation.** **Targeted read.** Work through the observer/bias equations and assumptions; understand the stability result at a high level; inspect experiments. Do not spend implementation time reproducing proofs unless the paper's contribution becomes estimation theory.

#### B3. Compensation of Magnetic Disturbances Improves Inertial and Magnetic Sensing of Human Body Segment Orientation

**Citation.** Daniel Roetenberg, Henk J. Luinge, Chris T. M. Baten, and Peter H. Veltink. *IEEE Transactions on Neural Systems and Rehabilitation Engineering*, 13(3), 395–405, 2005. [DOI](https://doi.org/10.1109/TNSRE.2005.847353) · [PubMed record](https://pubmed.ncbi.nlm.nih.gov/16200762/)

**Why selected.** It is direct evidence that naive magnetometer use is unsafe near ferromagnetic disturbances and that modeling the disturbance can materially improve body-segment orientation against Vicon.

**Does it solve Mesquite?** **It offers a real alternative path, not the proposal's magnetometer-off path.** Its complementary Kalman filter is older, and Mesquite's radio/steel environment must be measured.

**Use in this project.** Keep it as the primary “gated/robust magnetometer” comparator. A magnetometer-off paper should still compare against a competent disturbed-magnetic baseline rather than a naive 9-axis toggle.

**Read recommendation.** **Read the disturbance model, filter states, experiment, and recovery behavior.** Skim general inertial background already covered by Kok/VQF.

#### B4. IMU-Based Joint Angle Measurement for Gait Analysis

**Citation.** Thomas Seel, Jörg Raisch, and Thomas Schauer. *Sensors*, 14(4), 6891–6909, 2014. [DOI/full text](https://doi.org/10.3390/s140406891)

**Why selected.** This highly influential paper—777 citations shown by the publisher during this review—identifies joint axes and positions from arbitrary motion and derives magnetometer-free relative joint angles from gyroscope and accelerometer constraints. It is the direct foundation for Assessment 3's hinge-axis suggestion.

**Does it solve Mesquite?** **Directly for selected relative joint angles and functional calibration.** It assumes an appropriate joint model and informative motion; knees/elbows are only approximate hinges. It does not supply absolute whole-body heading.

**Use in this project.** Implement it as a classical baseline for knees and elbows and as a source of constraint residuals/confidence. Compare learned correction against this method rather than describing classical methods generically.

**Read recommendation.** **Must read.** Focus on joint-axis/position identification, magnetometer-free angle calculation, experiments, soft-tissue limitations, and the distinction between sensor and anatomical frames. Reproduce on one two-pod mechanical or rigid test before a human trial.

#### B5. Magnetometer-Free Realtime Inertial Motion Tracking by Exploitation of Kinematic Constraints in 2-DoF Joints

**Citation.** Daniel Laidig, Dustin Lehmann, Marc-André Bégin, and Thomas Seel. *41st IEEE Engineering in Medicine and Biology Conference (EMBC)*, 1233–1238, 2019. [DOI](https://doi.org/10.1109/EMBC.2019.8857535) · [PubMed record](https://pubmed.ncbi.nlm.nih.gov/31946115/)

**Why selected.** It directly estimates relative heading when individual magnetometer-free orientation estimates can drift severely, using constraints in a 2-DoF joint and explicitly discussing singular conditions.

**Does it solve Mesquite?** **Yes for relative heading across modeled 2-DoF joints.** It requires known axes/calibration and sufficient excitation, and it cannot determine absolute root yaw.

**Use in this project.** Use its windowed objective and observability/confidence conditions as a baseline and as a sanity check for any learned per-joint corrections.

**Read recommendation.** **Must read for Stage 3 design.** Study the constraint, cost function, window, singularity handling, and evaluation; write down exactly which Mesquite joint pairs meet its assumptions.

#### B6. Self-Calibrating Magnetometer-Free Inertial Motion Tracking of 2-DoF Joints

**Citation.** Daniel Laidig, Ive Weygers, and Thomas Seel. *Sensors*, 22(24), 9850, 2022. [DOI/full text](https://doi.org/10.3390/s22249850)

**Why selected.** This is perhaps the closest direct algorithmic match to the proposed relative-heading plus calibration problem. It simultaneously identifies joint axes and heading offset from arbitrary motion and validates elbow angles against optical motion capture.

**Does it solve Mesquite?** **Directly for 2-DoF joint pairs, with limitations.** It does not cover a full unconstrained shoulder/hip model or global heading, and it needs an informative motion window.

**Use in this project.** Make it a required classical baseline and consider using its estimated axes/headings as pseudo-labels, initialization, or differentiable constraints—not just as a citation.

**Read recommendation.** **Must read in full.** Pay particular attention to both constraints, simultaneous parameter estimation, the reported short excitation windows, optical protocol, outliers, and failure/observability conditions. Human natural-motion validation was small, so do not extrapolate its result to every whole-body joint.

#### B7. Magnetometer-Free Inertial Motion Tracking of Arbitrary Joints with Range-of-Motion Constraints

**Citation.** Dustin Lehmann, Daniel Laidig, Raphael Deimel, and Thomas Seel. *IFAC-PapersOnLine*, 53(2), 16016–16022, 2020. [DOI](https://doi.org/10.1016/j.ifacol.2020.12.401) · [Open manuscript](https://arxiv.org/abs/2002.00639)

**Why selected.** It extends magnetometer-free relative orientation beyond ideal hinges by using joint range-of-motion constraints in a nonlinear windowed objective.

**Does it solve Mesquite?** **A direct partial solution.** It can stabilize relative heading for a mechanically constrained joint, but its validation includes a mechanical joint and its assumptions may not transfer unchanged to soft-tissue human hips/shoulders.

**Use in this project.** Add range-of-motion penalties and compare them with learned motion priors. This provides a clean ablation: simple anatomical range constraints versus a learned prior.

**Read recommendation.** **Read method, identifiability assumptions, experiments, and limitations.** Test first on synthetic AMASS rotations with known perturbations, then on optical human data.

#### B8. Towards Self-Calibrating Inertial Body Motion Capture

**Citation.** Bertram Taetz, Gabriele Bleser, and Markus Miezal. *19th International Conference on Information Fusion (FUSION)*, 1751–1759, 2016. [IEEE record](https://ieeexplore.ieee.org/document/7528096/) · [Open manuscript](https://arxiv.org/abs/1606.03754)

**Why selected.** It jointly estimates body motion and sensor-to-segment calibration in a constrained sliding-window weighted least-squares formulation, using biomechanical and body-shape priors and no magnetometer after initialization.

**Does it solve Mesquite?** **Close research prototype, not a ready whole-body fix.** It is computationally heavier and validation is limited, but the formulation is a strong baseline for Stage 4.

**Use in this project.** Compare joint estimation versus a two-stage “calibrate then correct” pipeline. Its regularization terms can inform a slow calibration observer and synthetic perturbation experiments.

**Read recommendation.** **Read the state variables, priors, objective, windowing, experiments, and stated underconstraints.** Implement a small lower-body version before promising continuous 15/17-sensor calibration.

#### B9. On Inertial Body Tracking in the Presence of Model Calibration Errors

**Citation.** Markus Miezal, Bertram Taetz, and Gabriele Bleser. *Sensors*, 16(7), 1132, 2016. [DOI/full text](https://doi.org/10.3390/s16071132)

**Why selected.** It isolates sensor-to-segment orientation, sensor position, and segment-length errors and compares EKF/optimization formulations with and without magnetometers. It directly tests the importance of the calibration errors Mesquite currently conflates.

**Does it solve Mesquite?** **Mostly diagnostic.** It shows which calibration errors propagate and which estimators are robust; it does not prove that Mesquite straps slip or provide a complete online slip detector.

**Use in this project.** Design controlled calibration perturbations, prioritize orientation offsets, and report sensitivity curves instead of only aggregate accuracy.

**Read recommendation.** **Read the error-injection setup, comparative tables, and discussion.** Use its tested error ranges as context, but measure Mesquite-specific ranges.

#### B10. A Robust and Easy to Implement Method for IMU Calibration without External Equipments

**Citation.** David Tedaldi, Alberto Pretto, and Emanuele Menegatti. *IEEE International Conference on Robotics and Automation (ICRA)*, 3042–3049, 2014. [DOI](https://doi.org/10.1109/ICRA.2014.6907297)

**Why selected.** It estimates accelerometer and gyroscope biases, scale factors, and axis misalignment through a practical multi-position procedure. That is the correct literature for Assessment 3's per-pod EEPROM idea.

**Does it solve Mesquite?** **Yes for intrinsic IMU calibration; no for wearing/mounting calibration.** Chip properties and sensor-to-bone orientation must remain separate states.

**Use in this project.** Characterize every pod once, store versioned intrinsic parameters with uncertainty, and retain per-session mounting calibration in the capture application.

**Read recommendation.** **Targeted read.** Study the sensor model, static interval detector, collection procedure, optimizer, and validation. Re-run calibration after temperature changes to determine whether “stable for months” is justified.

#### B11. Sensor-to-Segment Calibration Methodologies for Lower-Body Kinematic Analysis with Inertial Sensors: A Systematic Review

**Citation.** Léonie Pacher, Christian Chatellier, Rodolphe Vauzelle, and Laëtitia Fradet. *Sensors*, 20(11), 3322, 2020. [DOI/full text](https://doi.org/10.3390/s20113322) · [PubMed Central](https://pmc.ncbi.nlm.nih.gov/articles/PMC7309059/)

**Why selected.** This systematic review classifies static, functional, manual, and anatomical calibration methods across 55 studies and documents heterogeneous validation. It prevents selecting a calibration method based on one favorable paper.

**Does it solve Mesquite?** **No single algorithm; it supplies a taxonomy and evidence checklist.**

**Use in this project.** Justify T-pose versus functional calibration, identify participant burden, and design a repeatability/redonning study. It is especially useful for the Related Work and limitations sections.

**Read recommendation.** **Read the taxonomy/table, quality comparison, and discussion/conclusion.** Use the cited primary paper—not only the review—when implementing a specific method.

#### B12. Zero-Velocity Detection—An Algorithm Evaluation

**Citation.** Isaac Skog, Peter Händel, John-Olof Nilsson, and Jouni Rantakokko. *IEEE Transactions on Biomedical Engineering*, 57(11), 2657–2666, 2010. [DOI](https://doi.org/10.1109/TBME.2010.2060723)

**Why selected.** Assessment 3 proposes ZARU, stance, and double-stance updates, but Phase 2's percentile threshold only identifies low-speed clusters in smoothed BVH. This paper evaluates principled zero-velocity detectors and threshold tradeoffs.

**Does it solve Mesquite?** **It solves detection methodology, not heading.** ZUPT constrains velocity; a zero-angular-rate update can estimate gyro bias. Neither alone provides absolute yaw.

**Use in this project.** Establish detector ROC/precision/recall against annotated contact, test across walking/running/standing/jumps, and propagate detector confidence into—not as a hard switch for—constraint updates.

**Read recommendation.** **Read detector derivation and threshold evaluation.** Validate against force plates or optical foot velocity before using “stance” as a correction event.

#### B13. Validity, Test-Retest Reliability and Long-Term Stability of Magnetometer Free Inertial Sensor Based 3D Joint Kinematics

**Citation.** Wolfgang Teufl, Markus Miezal, Bertram Taetz, Michael Fröhlich, and Gabriele Bleser. *Sensors*, 18(7), 1980, 2018. [DOI/full text](https://doi.org/10.3390/s18071980) · [PubMed Central](https://pmc.ncbi.nlm.nih.gov/articles/PMC6068643/)

**Why selected.** It evaluates magnetometer-free lower-body joint kinematics over a six-minute walk with 28 participants, simultaneous OptiTrack, repeatability, long-term stability, and explicit soft-tissue effects.

**Does it solve Mesquite?** **It validates one lower-body magnetometer-free pipeline in repeated six-minute walking with 28 healthy participants and supplies an experimental template.** It does not validate the whole method family or generalize automatically to other activities, joints, populations, or Mesquite hardware.

**Use in this project.** Add repeat donning/calibration, participant-level test-retest, time-dependent error, and marker-on-sensor versus anatomical-marker analyses.

**Read recommendation.** **Must read for evaluation planning.** Focus on participant/trial design, the magnetometer-free method assumptions, Bland–Altman/ICC/RMSE reporting, soft-tissue analysis, and what “no error growth” actually means.

#### B14. Transformer IMU Calibrator: Dynamic On-body IMU Calibration for Inertial Motion Capture

**Citation.** Chengxu Zuo, Jiawei Huang, Xiao Jiang, Yuan Yao, Xiangren Shi, Rui Cao, Xinyu Yi, Feng Xu, Shihui Guo, and Yipeng Qin. *ACM Transactions on Graphics*, 44(4), Article 45, 45:1–45:14, 2025; presented in the SIGGRAPH 2025 Journal track. SIGGRAPH 2025 Best Paper Award. [DOI](https://doi.org/10.1145/3730937) · [Open accepted paper](https://orca.cardiff.ac.uk/id/eprint/177840/1/TIC_camera_ready.pdf) · [Open manuscript](https://arxiv.org/abs/2506.10580) · [Official code/data](https://github.com/ZuoCX1996/TIC) · [Official award announcement](https://blog.siggraph.org/2025/06/siggraph-2025-technical-papers-awards-best-papers-honorable-mentions-and-test-of-time.html/)

**Why selected.** This is the closest recent paper to Mesquite's proposed online calibration claim. TIC estimates time-varying drift between IMU and target coordinates and sensor-to-body measurement offsets from body motion, removing the conventional requirement that those quantities stay fixed for an entire capture. Its focus on free-pose, motion-driven calibration and long-run strap/drift changes directly addresses the fragile single-sample T-pose and proposed slip refinement.

**Does it solve Mesquite?** **A major method candidate, not a drop-in or an absolute-heading oracle.** It requires coherent orientation and acceleration readings from six canonical sites; those inputs are deliberately uncalibrated with respect to the coordinate-drift and sensor-to-body transforms it estimates. It assumes those calibration quantities change negligibly within a short window and that motion is sufficiently diverse. TIC is trained on synthetically perturbed sequences derived from AMASS/DIP and evaluated on a dedicated five-participant optical-plus-IMU dataset; it does not repair Mesquite's packet, clocks, browser state, or phone frame. Its learned calibration can select time-varying relative coordinate corrections from motion, but without a continuing directional observation the whole system still retains a common global-yaw gauge.

**Use in this project.** Reproduce TIC before inventing a new calibration network; compare static T-pose, classical offline calibration, TIC-style dynamic calibration, and the proposed residual corrector under controlled strap rotations and redonning. Keep intrinsic chip calibration, sensor-to-segment offset, estimator-frame drift, and world-heading alignment as four separately named quantities. If only quaternion packets are retained, document exactly which TIC inputs are missing rather than claiming a faithful baseline.

**Read recommendation.** **Must read in full before defining Stage 4.** Read the coordinate notation first, then the calibration target, window/transformer design, motion-diversity trigger, synthetic perturbations, five-subject optical dataset, initialization/free-pose assumptions, observability/failure cases, ablations, and long-duration experiment. Run the released evaluation and inspect the dataset fields before adapting the model.

### C. Learned inertial pose, motion priors, physics, and body models

These papers are the closest intellectual neighborhood for the proposed ML extension. They are strong evidence that learned motion priors can resolve *pose ambiguity* and improve temporal or physical plausibility. They are not evidence that corrupted packets, missing acquisition timestamps, an unknown sensor-to-body transform, or global yaw gauge freedom can be learned away.

**Input-compatibility warning.** Most DIP-family methods assume canonical sparse placements, coherent calibrated/root-normalized orientations **and accelerations**, and SMPL-compatible targets. The current 16-byte Mesquite packet carries only quaternion, counter, and low send-time bits. Until Mesquite logs raw acceleration/gyro in named frames and collects synchronized optical/SMPL targets, these papers are related-work and design baselines—not directly runnable correction baselines. The project must choose between an orientation-only performance protocol and a larger raw-IMU research protocol; it cannot silently claim both.

#### C1. Sparse Inertial Poser: Automatic 3D Human Pose Estimation from Sparse IMUs

**Citation.** Timo von Marcard, Bodo Rosenhahn, Michael J. Black, and Gerard Pons-Moll. *Computer Graphics Forum*, 36(2), 349–360; Proceedings of Eurographics 2017. [DOI](https://doi.org/10.1111/cgf.13131) · [Official MPI page and paper](https://is.mpg.de/ncs/en/publications/sip) · [Open manuscript](https://arxiv.org/abs/1703.08014)

**Why selected.** SIP is a foundational sparse-inertial pose paper and received the Eurographics 2017 Best Paper award. It combines a statistical body model, anthropometric constraints, inertial observations, and optimization over multiple frames. This is the clearest classical predecessor to Mesquite's proposed “body-aware motion prior.”

**Does it solve Mesquite?** **It solves a related underconstrained pose problem, not Mesquite's data-integrity or absolute-heading problem.** It uses six IMUs to recover plausible pose, but its assumptions and optimization target do not make a 6-axis global heading observable.

**Use in this project.** Implement SIP or a faithful approximation as a non-neural/full-sequence baseline. Compare its statistical prior against simple anatomical range constraints and the proposed learned corrector. Its multi-frame objective is also a good model for an offline smoother.

**Read recommendation.** **Must read.** Read the observation model, body model, multi-frame objective, initialization, TNT15 evaluation, outdoor examples, and limitations. Track exactly which global transform is aligned during evaluation.

#### C2. Deep Inertial Poser: Learning to Reconstruct Human Pose from Sparse Inertial Measurements in Real Time

**Citation.** Yinghao Huang, Manuel Kaufmann, Emre Aksan, Michael J. Black, Otmar Hilliges, and Gerard Pons-Moll. *ACM Transactions on Graphics*, 37(6), Article 185, 2018; presented at SIGGRAPH Asia 2018. [DOI/ACM record](https://doi.org/10.1145/3272127.3275108) · [Open paper](https://virtualhumans.mpi-inf.mpg.de/papers/DIPSiggraphAsia18/DIP.pdf) · [Open manuscript](https://arxiv.org/abs/1810.04703)

**Why selected.** DIP is the canonical learned sparse-IMU baseline. It introduced a bidirectional recurrent pose prior, synthetic IMU generation, and DIP-IMU: 10 subjects, 64 sequences, and about 330,000 time instants recorded with 17 IMUs. The ACM record reports substantial uptake, but its importance here is its direct problem match.

**Does it solve Mesquite?** **Partly.** It reconstructs full-body pose from six clean, calibrated IMU streams and explicitly addresses pose ambiguity. It does not repair Mesquite's radio protocol, timestamp model, WebXR registration, or unobservable absolute yaw.

**Use in this project.** Use it as the minimum learned baseline and as a template for synthesizing orientation/acceleration from mocap. Compare both its six-sensor setting and a sensor-rich Mesquite setting; more sensors should be tested rather than assumed to help.

**Read recommendation.** **Must read in full.** Pay special attention to preprocessing/reference frames, synthetic-to-real training, bidirectional inference latency, DIP-IMU splits, evaluation alignments, and failure examples. Reproduce the released model on DIP-IMU before adapting it.

#### C3. TransPose: Real-Time 3D Human Translation and Pose Estimation with Six Inertial Sensors

**Citation.** Xinyu Yi, Yuxiao Zhou, and Feng Xu. *ACM Transactions on Graphics*, 40(4), Article 86, 2021; presented at SIGGRAPH 2021. [Official project, paper, and code](https://xinyu-yi.github.io/TransPose/) · [DOI](https://doi.org/10.1145/3450626.3459786)

**Why selected.** TransPose separates the task into leaf-joint positions, full-joint positions, pose, and translation, then fuses support-foot and learned translation estimates by confidence. It is directly relevant to the proposal's kinematic-chain and contact ideas.

**Does it solve Mesquite?** **A strong baseline and partial solution.** It estimates pose and translation from six inertial sensors, but support-foot logic constrains translation rather than absolute yaw, and it assumes correctly synchronized and calibrated input.

**Use in this project.** Borrow the staged prediction idea, explicit contact confidence, and translation ablations. Compare Mesquite's phone/WebXR root with TransPose-style inertial translation and with a fused version; do not silently substitute one for another.

**Read recommendation.** **Must read.** Study the coordinate normalization, intermediate targets, supporting-foot detector, confidence fusion, real-time protocol, metrics, and per-motion failures. Check whether its offline and online variants use future frames before comparing latency.

#### C4. Physical Inertial Poser (PIP): Physics-Aware Real-Time Human Motion Tracking from Sparse Inertial Sensors

**Citation.** Xinyu Yi, Yuxiao Zhou, Marc Habermann, Soshi Shimada, Vladislav Golyanik, Christian Theobalt, and Feng Xu. *IEEE/CVF Conference on Computer Vision and Pattern Recognition (CVPR)*, 13167–13178, 2022. Best Paper Finalist. [Official CVF paper](https://openaccess.thecvf.com/content/CVPR2022/html/Yi_Physical_Inertial_Poser_PIP_Physics-Aware_Real-Time_Human_Motion_Tracking_From_CVPR_2022_paper.html) · [Project and code](https://xinyu-yi.github.io/PIP/) · [DOI](https://doi.org/10.1109/CVPR52688.2022.01282)

**Why selected.** PIP combines a learned kinematics estimator with a physics-based optimizer and explicitly targets contacts, temporal stability, physical plausibility, global motion, joint torques, and ground-reaction forces. It is a more rigorous version of several constraints suggested in Assessment 3.

**Does it solve Mesquite?** **Partly.** It offers a mature architecture for plausible pose and contact-aware translation. It still depends on valid inertial observations and does not turn a local inertial frame into absolute world heading.

**Use in this project.** Treat PIP as a high-quality physics-aware baseline. If full reproduction is too expensive, implement a transparent constrained optimizer with joint limits, contact, non-penetration, smoothness, and confidence weights, and state exactly which parts differ.

**Read recommendation.** **Must read for method design.** Focus on the interface between network and optimizer, learned targets, contact treatment, physical state, latency definition, evaluation metrics, and ablations. Read the supplement before claiming “physics-aware.”

#### C5. Transformer Inertial Poser: Real-Time Human Motion Reconstruction from Sparse IMUs with Simultaneous Terrain Generation

**Citation.** Yifeng Jiang, Yuting Ye, Deepak Gopinath, Jungdam Won, Alexander W. Winkler, and C. Karen Liu. *SIGGRAPH Asia 2022 Conference Papers*, Article 3, 1–9. [DOI](https://doi.org/10.1145/3550469.3555428) · [Open manuscript](https://arxiv.org/abs/2203.15720) · [Official code](https://github.com/jyf588/transformer-inertial-poser)

**Why selected.** TIP introduces a causal Transformer, predicts stationary body points, and uses analytic routines to reduce joint/global drift while simultaneously estimating terrain. It is a direct source for Assessment 3's stationary-contact and motion-prior ideas.

**Does it solve Mesquite?** **A strong partial solution for contact-conditioned motion reconstruction.** Stationary points can constrain drift, but contact predictions can be wrong and do not uniquely anchor global yaw without an external directional reference.

**Use in this project.** Use stationary-body-point probability as a soft observation with uncertainty rather than a binary reset. Compare against optical contact labels, and ablate the learned contact target separately from heading correction.

**Read recommendation.** **Must read.** Study its causal conditioning, stationary-body-point representation, analytic correction, terrain update, real-versus-synthetic tests, latency, and failure cases. Reproduce the public code before borrowing only the headline idea.

#### C6. Physical Non-Inertial Poser (PNP): Modeling Non-Inertial Effects in Sparse-Inertial Human Motion Capture

**Citation.** Xinyu Yi, Yuxiao Zhou, and Feng Xu. *SIGGRAPH 2024 Conference Papers*, Article 50, 1–11, 2024. [DOI](https://doi.org/10.1145/3641519.3657436) · [Official project, paper, code, raw-IMU synthesis, and ESKF](https://xinyu-yi.github.io/PNP/) · [Open manuscript](https://arxiv.org/abs/2404.19619)

**Why selected.** PNP points out that a rotating/accelerating root frame is non-inertial, models fictitious forces, and provides hardware-aware raw-IMU synthesis and an error-state Kalman filter. It is unusually relevant to Mesquite because the proposal currently treats processed quaternions and synthetic rotations more carefully than raw accelerations and frame dynamics.

**Does it solve Mesquite?** **Partly.** It improves sparse-inertial pose modeling and synthetic measurement realism; it does not solve packet timing, browser corruption, or global heading.

**Use in this project.** Use its simulator as a reference when injecting bias, noise, calibration error, and acceleration effects. Compare raw-IMU-plus-filter training with quaternion-only training instead of assuming processed orientations are sufficient.

**Read recommendation.** **Read after DIP/PIP.** Focus on the non-inertial derivation, raw measurement synthesis, hardware fitting, ESKF interface, and calibration-error experiments. It is especially useful for revising the proposal's synthetic corruption section.

#### C7. Dynamic Inertial Poser (DynaIP): Part-Based Motion Dynamics Learning for Enhanced Human Pose Estimation with Sparse Inertial Sensors

**Citation.** Yu Zhang, Songpengcheng Xia, Lei Chu, Jiarui Yang, Qi Wu, and Ling Pei. *IEEE/CVF Conference on Computer Vision and Pattern Recognition (CVPR)*, 1889–1899, 2024. [DOI](https://doi.org/10.1109/CVPR52733.2024.00185) · [Official CVF paper](https://openaccess.thecvf.com/content/CVPR2024/html/Zhang_Dynamic_Inertial_Poser_DynaIP_Part-Based_Motion_Dynamics_Learning_for_Enhanced_CVPR_2024_paper.html) · [Official code](https://github.com/dx118/dynaip)

**Why selected.** DynaIP directly addresses dependence on synthetic data, learns from diverse real inertial datasets and skeleton formats, predicts pseudo velocity, and divides the body into regions. Its reported cross-dataset improvement makes it one of the most relevant recent baselines for Mesquite's domain gap.

**Does it solve Mesquite?** **It offers a strong pose-model baseline, not an end-to-end system repair.** It can improve real-data generalization, but only once Mesquite has trustworthy streams and ground truth.

**Use in this project.** Use part-wise modeling as an ablation, especially for pelvis/legs versus arms, and train on multiple public real-IMU sources where licenses permit. Test an explicit Mesquite-hardware/session holdout.

**Read recommendation.** **Must read before finalizing data strategy.** Read dataset harmonization, pseudo-velocity target, part partition, splits, cross-dataset experiments, and supplementary implementation details. Its central lesson is that synthetic-only validation is inadequate.

#### C8. Probabilistic Inertial Poser (ProbIP): Uncertainty-Aware Human Motion Modeling from Sparse Inertial Sensors

**Citation.** Min Kim, Younho Jeon, and Sungho Jo. *IEEE/CVF International Conference on Computer Vision (ICCV)*, 25893–25902, 2025. [Official CVF paper](https://openaccess.thecvf.com/content/ICCV2025/html/Kim_Probabilistic_Inertial_Poser_ProbIP_Uncertainty-aware_Human_Motion_Modeling_from_Sparse_ICCV_2025_paper.html) · [DOI](https://doi.org/10.1109/ICCV51701.2025.02402) · [Official code](https://github.com/MinKim14/ProbIP-ICCV2025)

**Why selected.** ProbIP predicts a matrix Fisher distribution on rotations through RU-Mamba blocks and Progressive Distribution Narrowing, including experiments with six and fewer sensors. It is the best recent pointer for turning Assessment 3's confidence flags into uncertainty-aware motion estimates. It is included for topical recency, not because it has the long citation history of the foundational papers.

**Does it solve Mesquite?** **Partly.** It represents pose ambiguity and uncertainty more honestly than a single deterministic quaternion. It does not model Mesquite's packet-age uncertainty, clocks, browser failure state, or external world frame unless those are added explicitly.

**Use in this project.** Predict a rotation distribution or calibrated confidence, condition it on sensor-validity masks and sample age, and evaluate calibration as well as mean error. Compare selective correction—abstaining when uncertain—with always-on correction.

**Read recommendation.** **Read after a deterministic baseline works.** Focus on the matrix Fisher output, uncertainty parameterization, distribution-narrowing schedule, reduced-sensor tests, and uncertainty evaluation. Avoid adopting a sophisticated distribution before establishing a trustworthy deterministic pipeline.

#### C9. EgoLocate: Real-Time Motion Capture, Localization, and Mapping with Sparse Body-Mounted Sensors

**Citation.** Xinyu Yi, Yuxiao Zhou, Marc Habermann, Vladislav Golyanik, Shaohua Pan, Christian Theobalt, and Feng Xu. *ACM Transactions on Graphics*, 42(4), Article 76, 2023; presented at SIGGRAPH 2023. [Official project, paper, and code](https://xinyu-yi.github.io/EgoLocate/) · [DOI](https://doi.org/10.1145/3592099)

**Why selected.** EgoLocate is the closest high-quality comparator to Mesquite's architectural idea: sparse body IMUs plus a monocular phone camera/SLAM signal. It performs bidirectional fusion—the camera corrects global motion while inertial motion helps camera tracking—and explicitly uses confidence, mapping, and loop closure.

**Does it solve Mesquite?** **It solves a closely related fusion problem but is not a drop-in WebXR transform.** It uses images and a jointly designed SLAM/mocap optimizer, whereas Mesquite currently consumes an external WebXR pose stream whose world-frame lifecycle is not documented.

**Use in this project.** Establish camera-to-body extrinsics, model tracking confidence and relocalization, compare one-way versus two-way fusion, and evaluate local trajectory with ATE/RPE. This is the paper to cite when arguing that phone/IMU fusion can be scientifically substantive rather than merely a display feature.

**Read recommendation.** **Must read if the phone remains part of the contribution.** Study the frame graph, initialization, camera constraints, confidence weighting, loop closure, map evaluation, and failure cases. Diagram Mesquite's frames beside EgoLocate's before coding.

#### C10. IMUPoser: Full-Body Pose Estimation Using IMUs in Phones, Watches, and Earbuds

**Citation.** Vimal Mollyn, Riku Arakawa, Mayank Goel, Chris Harrison, and Karan Ahuja. *ACM CHI Conference on Human Factors in Computing Systems*, Article 529, 1–12, 2023. Best Paper Honorable Mention. [DOI](https://doi.org/10.1145/3544548.3581392) · [Official project page](https://www.figlab.com/research/2023/imuposer) · [Code](https://github.com/FIGLAB/IMUPoser)

**Why selected.** IMUPoser shows how a sparse-inertial engineering contribution becomes a strong CHI paper: it starts from real user/device constraints, accepts a changing subset of commodity devices, releases data, and evaluates ten participants across activity contexts. It also speaks directly to missing pods.

**Does it solve Mesquite?** **Partly for variable sensor availability; no for the present full-suit protocol and heading claims.** Its model outputs a best-guess pose from available consumer-device streams, which is useful but not equivalent to measurement correction.

**Use in this project.** Add sensor-mask conditioning and graceful-degradation experiments. For a CHI version, formulate an interaction or access question—for example, whether fast self-calibration and transparent uncertainty enable creators to complete real animation tasks—not merely “is RMSE lower?”

**Read recommendation.** **Read method, dataset, device-subset experiments, applications, and study framing.** Then compare the actual Mesquite question with the paper's user-centered motivation. A user study is not automatically required at CHI, but a direct HCI contribution is.

#### C11. SMPL: A Skinned Multi-Person Linear Model

**Citation.** Matthew Loper, Naureen Mahmood, Javier Romero, Gerard Pons-Moll, and Michael J. Black. *ACM Transactions on Graphics*, 34(6), Article 248, 1–16, 2015; presented at SIGGRAPH Asia 2015. [DOI](https://doi.org/10.1145/2816795.2818013) · [Official model page](https://smpl.is.tue.mpg.de/) · [Author paper](https://virtualhumans.mpi-inf.mpg.de/papers/SMPL15/SMPL15.pdf)

**Why selected.** SMPL is the standard differentiable body model behind SIP, DIP, AMASS, and much subsequent inertial-pose work. It provides explicit pose, shape, joints, and a mesh instead of an undocumented BVH skeleton. It has also received long-term field recognition.

**Does it solve Mesquite?** **It solves representation and anthropometric modeling, not sensing.** It cannot correct a bad orientation by itself.

**Use in this project.** Use SMPL or a clearly documented skeleton as the canonical training/evaluation body, estimate subject shape from calibration data, and keep sensor-to-segment transforms separate. Retarget to the browser avatar only after evaluation.

**Read recommendation.** **Targeted must-read.** Read parameterization, joint regression, blend skinning, pose/shape terms, limitations, and license. Do not spend time on mesh-rendering details unless surface accuracy becomes an output.

#### C12. AMASS: Archive of Motion Capture as Surface Shapes

**Citation.** Naureen Mahmood, Nima Ghorbani, Nikolaus F. Troje, Gerard Pons-Moll, and Michael J. Black. *IEEE/CVF International Conference on Computer Vision (ICCV)*, 2019. [Official CVF paper](https://openaccess.thecvf.com/content_ICCV_2019/html/Mahmood_AMASS_Archive_of_Motion_Capture_As_Surface_Shapes_ICCV_2019_paper.html) · [DOI](https://doi.org/10.1109/ICCV.2019.00554) · [Open manuscript](https://arxiv.org/abs/1904.03278)

**Why selected.** AMASS unifies 15 optical-mocap sources in SMPL form and reports more than 40 hours, more than 300 subjects, and more than 11,000 motions. It is the most practical foundation for synthetic inertial training and motion-prior pretraining.

**Does it solve Mesquite?** **It supplies motion data, not Mesquite-domain sensor data.** Derived accelerations, sensor orientations, strap motion, clock errors, DMP behavior, radio loss, and WebXR events must be simulated or recorded. Dataset license terms also matter.

**Use in this project.** Pretrain on AMASS with participant/source-aware splits, then calibrate the corruption model using training-session Mesquite hardware logs and fine-tune on real synchronized captures. Reserve whole people, sessions, devices, and motion classes for testing.

**Read recommendation.** **Must read for data planning.** Read source composition, MoSh++ fitting, representation, dataset bias, licensing, and splits. Audit whether the intended AMASS subsets permit the planned use and redistribution of derivatives.

#### C13. QuaterNet: A Quaternion-Based Recurrent Model for Human Motion

**Citation.** Dario Pavllo, David Grangier, and Michael Auli. *British Machine Vision Conference (BMVC)*, Paper 188, 2018. [Official Meta research page](https://ai.meta.com/research/publications/quaternet-a-quaternion-based-recurrent-model-for-human-motion/) · [Open manuscript](https://arxiv.org/abs/1805.06485) · [Code](https://github.com/facebookresearch/QuaterNet)

**Why selected.** QuaterNet connects quaternion sequence prediction with a differentiable forward-kinematics position loss. It directly supports the proposal's idea of penalizing downstream joint positions while predicting rotations.

**Does it solve Mesquite?** **It offers reusable representation/loss machinery, not an inertial corrector.** Its task is motion prediction/generation, and quaternion outputs still require sign handling and normalization.

**Use in this project.** Add differentiable FK loss alongside local-rotation geodesic loss, but report both separately: FK can hide errors near the root or overweight distal joints. Unit-test quaternion normalization and antipodal equivalence.

**Read recommendation.** **Read the rotation representation, FK loss, training protocol, and ablations.** Skim generation-specific evaluation. Contrast it with Zhou's 6D representation before choosing the network output.

#### C14. On the Continuity of Rotation Representations in Neural Networks

**Citation.** Yi Zhou, Connelly Barnes, Jingwan Lu, Jimei Yang, and Hao Li. *IEEE/CVF Conference on Computer Vision and Pattern Recognition (CVPR)*, 5745–5753, 2019. [DOI](https://doi.org/10.1109/CVPR.2019.00589) · [Official CVF paper](https://openaccess.thecvf.com/content_CVPR_2019/html/Zhou_On_the_Continuity_of_Rotation_Representations_in_Neural_Networks_CVPR_2019_paper.html)

**Why selected.** This paper explains why Euler angles and quaternions are discontinuous as unconstrained Euclidean regression targets over all of SO(3), and proposes continuous 5D/6D representations. It directly addresses a hidden risk in training a quaternion corrector with ordinary component-wise loss.

**Does it solve Mesquite?** **Yes for a network-output design decision, not for sensing or observability.** A 6D representation improves optimization behavior but does not add information.

**Use in this project.** Predict 6D rotations (or a distribution on SO(3)), map them to valid rotation matrices, train/evaluate with geodesic loss, and convert to normalized quaternions only at interfaces that require them.

**Read recommendation.** **Must read selectively.** Read the continuity definition, SO(3) construction, 6D conversion, experiments, and caveats. Implement round-trip and near-degenerate-vector tests.

#### C15. DiffusionPoser: Real-time Human Motion Reconstruction From Arbitrary Sparse Sensors Using Autoregressive Diffusion

**Citation.** Tom Van Wouwe, Seunghwan Lee, Antoine Falisse, Scott Delp, and C. Karen Liu. *IEEE/CVF Conference on Computer Vision and Pattern Recognition (CVPR)*, 2513–2523, 2024. [Official CVF paper](https://openaccess.thecvf.com/content/CVPR2024/html/Van_Wouwe_DiffusionPoser_Real-time_Human_Motion_Reconstruction_From_Arbitrary_Sparse_Sensors_Using_CVPR_2024_paper.html) · [DOI](https://doi.org/10.1109/CVPR52733.2024.00243) · [Open manuscript](https://arxiv.org/abs/2308.16682) · [Project](https://diffusionposer.github.io/)

**Why selected.** It directly targets arbitrary combinations of IMUs and pressure insoles rather than one fixed six-sensor layout, using an autoregressive diffusion model to generate plausible unmeasured degrees of freedom. That makes it a much better domain-specific comparator than a generic imputation network when Mesquite studies pod loss, sensor subsets, or graceful degradation.

**Does it solve Mesquite?** **Strong partial solution for variable sparse sensing, not measurement recovery.** “Arbitrary” means a subset of 13 predefined body-segment locations, not a sensor mounted anywhere, and the model consumes calibrated global orientations and world-frame accelerations (plus optional insoles). A generative posterior can supply plausible motion when information is absent, but it cannot know the actor's unique unobserved action or repair clocks/global-yaw gauge by itself.

**Use in this project.** Compare deterministic mask conditioning, DiffusionPoser-style arbitrary subsets, and the proposed corrector at matched sensor layouts. Train/test with contiguous measured loss bursts and report sample diversity, best-of-one—not oracle best-of-many—accuracy, uncertainty/coverage, temporal consistency, and how often a plausible sample is measurably wrong.

**Read recommendation.** **Read after DIP and before choosing the missing-data model.** Focus on sensor representation/location encoding, diffusion objective, autoregressive schedule and latency, pressure/contact inputs, arbitrary-subset training, evaluation protocol, and failure cases. Determine whether its “real-time” hardware and look-ahead budget match Mesquite's intended path.

#### C16. Improving Global Motion Estimation in Sparse IMU-based Motion Capture with Physics (GlobalPose)

**Citation.** Xinyu Yi, Shaohua Pan, and Feng Xu. *ACM Transactions on Graphics*, 44(4), Article 142, 142:1–142:16, 2025; presented in the SIGGRAPH 2025 Journal track. [DOI](https://doi.org/10.1145/3730822) · [Official project and paper](https://xinyu-yi.github.io/GlobalPose/) · [Open manuscript](https://arxiv.org/abs/2505.05010) · [Official code](https://github.com/Xinyu-Yi/GlobalPose)

**Why selected.** GlobalPose is the newest direct contact/physics reference in this review. It jointly improves local pose and full 3D translation from six IMUs, optimizes multiple contacts, and estimates physically meaningful quantities such as contact forces, joint torques, and proxy interaction surfaces. It is the mature comparator for Assessment 3's stance, closure, gravity, and constrained-smoother ideas.

**Does it solve Mesquite?** **A strong global-motion baseline, not a source of absolute heading.** It assumes coherent raw-derived six-IMU input and an explicit setup: the wearer stands straight and takes a standard step forward; the method uses integrated raw acceleration with a terminal ZUPT, estimates sensor-to-bone rotations, and aligns five sensors' **relative** headings to the sixth. Gravity constrains tilt, while contact and dynamics improve translation and plausibility. In the absence of a mapped directional/environment observation, rotating the entire state around gravity leaves the basic physics equivalent, so a motion/physics prior alone still cannot certify geographic or optical-world yaw.

**Use in this project.** Compare simple ZUPT/contact constraints, PIP/TIP, and GlobalPose-style multi-contact physics under the same six-sensor subset and matched calibration motion. Report local pose and root trajectory separately, include activities without stationary contacts and non-flat/3D contacts, and ablate phone/visual aiding so the source of world-space accuracy is visible.

**Read recommendation.** **Must read for any global-translation claim.** Study the local/global state, contact representation, gravity term, physics optimizer, interacting-surface assumptions, runtime, evaluation alignment, ablations, and failure examples. Reproduce the released model before simplifying its physics into a single “foot planted” heuristic; note that the published repository documents an unusually specific Windows/Python/precompiled-physics environment.

#### C17. MagShield: Towards Better Robustness in Sparse Inertial Motion Capture Under Magnetic Disturbances

**Citation.** Yunzhe Shao, Xinyu Yi, Lu Yin, Shihui Guo, Junhai Yong, and Feng Xu. *IEEE/CVF International Conference on Computer Vision (ICCV)*, 29021–29030, 2025. [Official CVF paper](https://openaccess.thecvf.com/content/ICCV2025/html/Shao_MagShield_Towards_Better_Robustness_in_Sparse_Inertial_Motion_Capture_Under_ICCV_2025_paper.html) · [DOI](https://doi.org/10.1109/ICCV51701.2025.02695) · [Open manuscript](https://arxiv.org/abs/2506.22907) · [Official code and dataset](https://github.com/YZ-Shiao/MagShield)

**Why selected.** MagShield is the most directly relevant recent alternative to Mesquite's magnetometer-free premise. It detects magnetic disturbances jointly across six body IMUs and then corrects yaw-related orientation error with a human-motion prior; its interface is demonstrated with PNP and DynaIP.

**Does it solve Mesquite?** **Only if Mesquite adopts a raw 9-axis path.** MagShield explicitly consumes sensor-local accelerometer, gyroscope, and magnetometer data. It is therefore not a quaternion-only or magnetometer-free method and cannot be applied to the present packet. It also assumes fixed sensor–body alignment/mean shape, and heading can still drift during prolonged disturbance. It is valuable because it tests whether carefully gated magnetic information outperforms throwing that directional observation away.

**Use in this project.** If the hardware exposes magnetometers, log raw 9-axis data and add naive 9D fusion, VQF magnetic gating, and MagShield as alternative baselines under mapped disturbance trials. Compare clean fields, local steel/electronics, transient disturbances, recovery, and never-magnetometer mode. If magnetometers are unavailable, cite it as excluded-by-input and avoid claiming state of the art against magnetic disturbance.

**Read recommendation.** **Read if the design decision “magnetometers off” remains open.** Focus on the spatial detector, ESKF interface, neural yaw-error corrector, disturbance-generation protocol, PNP/DynaIP integration, detector errors, and limitations. Do not cite it as evidence that a 6-axis motion prior obtains absolute yaw.

### D. Missing data, rotation interpolation, and hybrid reconstruction

#### D1. BRITS: Bidirectional Recurrent Imputation for Time Series

**Citation.** Wei Cao, Dong Wang, Jian Li, Hao Zhou, Lei Li, and Yitan Li. *Advances in Neural Information Processing Systems 31 (NeurIPS)*, 2018. [Official proceedings paper](https://proceedings.neurips.cc/paper/2018/hash/734e6bfcd358e25ac1db0a4241b95651-Abstract.html) · [Open manuscript](https://arxiv.org/abs/1805.10572)

**Why selected.** BRITS is an influential mask-aware bidirectional imputation method that makes missing values variables in a recurrent graph and exploits correlations between channels. It is a defensible reference for the proposal's offline reconstruction stage.

**Does it solve Mesquite?** **Only as a generic modeling pattern.** It was not designed for rotations or asynchronous body networks; direct component-wise quaternion imputation can create invalid paths and antipodal discontinuities.

**Use in this project.** Feed explicit validity masks and time gaps, impute a continuous SO(3) representation or tangent-space increments, and compare with hold-last, bounded SLERP, a Kalman/smoother baseline, and no-imputation masking.

**Read recommendation.** **Targeted read.** Read temporal decay, feature-based estimates, bidirectional consistency, masking, and artificial-missingness evaluation. Skip application-specific classifiers. Evaluate contiguous bursts, not only random missing points.

#### D2. Animating Rotation with Quaternion Curves

**Citation.** Ken Shoemake. *Proceedings of SIGGRAPH 1985*, 245–254; also *ACM SIGGRAPH Computer Graphics*, 19(3). [Conference DOI](https://doi.org/10.1145/325334.325242) · [Journal DOI](https://doi.org/10.1145/325165.325242)

**Why selected.** This is the classic source for spherical interpolation of unit quaternions and remains heavily cited. It is the correct foundation for short-gap rotational interpolation instead of linearly blending quaternion components.

**Does it solve Mesquite?** **Yes for short, bounded gaps; no for long loss bursts, outliers, unknown timing, or drift.** SLERP connects two valid endpoints along a rotation arc and cannot recover unobserved human intent.

**Use in this project.** Make sign-consistent SLERP the transparent baseline, gate it by elapsed time and endpoint validity, and preserve a missingness/confidence flag after filling. Never evaluate only on the filled trajectory without also reporting the original loss pattern.

**Read recommendation.** **Read the quaternion-curve and interpolation portions.** Then test identity, antipodal representations, nearly equal endpoints, and rotations near 180 degrees.

#### D3. Multisensor-Fusion for 3D Full-Body Human Motion Capture

**Citation.** Gerard Pons-Moll, Andreas Baak, Thomas Helten, Meinard Müller, Hans-Peter Seidel, and Bodo Rosenhahn. *IEEE Conference on Computer Vision and Pattern Recognition (CVPR)*, 663–670, 2010. [Official MPI record](https://is.mpg.de/ps/publications/ponscvpr2010) · [Author paper](https://virtualhumans.mpi-inf.mpg.de/papers/ponsmollCVPR2010/ponsmollCVPR2010.pdf) · [DOI](https://doi.org/10.1109/CVPR.2010.5540153)

**Why selected.** This early hybrid work fuses video position evidence with inertial limb orientations to handle each modality's weaknesses. It is a useful historical bridge between Mesquite's phone idea and EgoLocate.

**Does it solve Mesquite?** **Conceptually, not directly.** It assumes calibrated video and extended inertial sensing, while Mesquite supplies an opaque WebXR pose rather than optimizing image measurements itself.

**Use in this project.** Frame the phone and IMUs as complementary measurements with explicit covariance/confidence. Include a camera-off, IMU-off, one-way-fusion, and full-fusion ablation.

**Read recommendation.** **Targeted read.** Focus on the fusion objective, calibration, failure complementarity, and evaluation; use EgoLocate for a more modern implementation comparison.

#### D4. OpenCap: Human Movement Dynamics from Smartphone Videos

**Citation.** Scott D. Uhlrich, Antoine Falisse, Łukasz Kidziński, Julie Muccini, Michael Ko, Akshay S. Chaudhari, Jennifer L. Hicks, and Scott L. Delp. *PLOS Computational Biology*, 19(10), e1011462, 2023. [DOI/full text](https://doi.org/10.1371/journal.pcbi.1011462) · [Project](https://www.opencap.ai/)

**Why selected.** OpenCap is a high-quality example of making motion analysis accessible through phones and the web while still validating against laboratory motion capture/force plates, publishing code/data, and demonstrating a 100-subject field use case.

**Does it solve Mesquite?** **No direct inertial correction.** It offers an evaluation, openness, and impact model—and potentially an independent comparison modality—not a replacement for synchronized optical ground truth in the core estimator study.

**Use in this project.** Replace vague “democratization” rhetoric with measured setup time, cost, expertise, failure rate, and task utility. If used as a comparator, keep its video/model error distinct from laboratory ground truth.

**Read recommendation.** **Read abstract, system pipeline, laboratory validation, field study, data/code availability, and limitations.** The openly published peer-review history is also valuable for seeing what reviewers demanded of a low-cost motion platform.

#### D5. 1 € Filter: A Simple Speed-based Low-pass Filter for Noisy Input in Interactive Systems

**Citation.** Géry Casiez, Nicolas Roussel, and Daniel Vogel. *Proceedings of the SIGCHI Conference on Human Factors in Computing Systems (CHI '12)*, 2527–2530, 2012. [DOI](https://doi.org/10.1145/2207676.2208639) · [Official author project, paper, code, and tuning guide](https://gery.casiez.net/1euro/)

**Why selected.** The active Mesquite renderer already trades jitter for lag through fixed-factor SLERP/LERP, but its coefficients change their physical meaning when update rate changes. The 1 € filter is a widely used interactive-input baseline that increases cutoff with motion speed to suppress slow jitter while reducing fast-motion lag, and it provides a simple, reproducible tuning procedure.

**Does it solve Mesquite?** **It addresses presentation smoothing only.** It cannot correct IMU bias, packet corruption, missing motion, calibration, or unobservable yaw. The original filter is Euclidean; independently filtering quaternion components would be geometrically wrong.

**Use in this project.** First derive ordinary exponential smoothing from elapsed time, `alpha = 1 - exp(-delta_t/tau)`, so the time constant is rate invariant. Then implement a rotation-aware 1 € analogue using geodesic angular speed and SLERP or an SO(3) log/exp update. Log raw estimator and displayed pose separately; compare fixed-factor smoothing, fixed physical time constant, rotation-aware 1 €, and no smoothing.

**Read recommendation.** **Short, high-value read.** Read the jitter–lag motivation, derivative filter, cutoff rule, evaluation, and two-step tuning procedure. Then measure step response, frequency response/amplitude attenuation, group delay, and visible jitter at multiple packet rates; tune only on development trials and freeze parameters before the optical test.

#### D6. Loose Inertial Poser: Motion Capture with IMU-Attached Loose-Wear Jacket

**Citation.** Chengxu Zuo, Yiming Wang, Lishuang Zhan, Shihui Guo, Xinyu Yi, Feng Xu, and Yipeng Qin. *IEEE/CVF Conference on Computer Vision and Pattern Recognition (CVPR)*, 2209–2219, 2024. [Official CVF paper](https://openaccess.thecvf.com/content/CVPR2024/html/Zuo_Loose_Inertial_Poser_Motion_Capture_with_IMU-attached_Loose-Wear_Jacket_CVPR_2024_paper.html) · [DOI](https://doi.org/10.1109/CVPR52733.2024.00215) · [Official code](https://github.com/ZuoCX1996/Loose-Inertial-Poser)

**Why selected.** LIP models secondary motion between four jacket-mounted IMUs and the body with a Secondary Motion AutoEncoder, then augments a pose network with synthesized loose-wear signals. It is the closest paper here to Mesquite's mounting/strap-motion concern and shows that attachment motion is a structured domain shift, not merely white sensor noise.

**Does it solve Mesquite?** **A useful corruption/data-model idea, not a slip calibrator.** The study uses one garment, two wearing styles, a five-second T-pose, and a specialized sparse layout; twisting and unzipped configurations remain difficult. It does not estimate the exact time-varying transform of each Mesquite pod or solve heading/timing.

**Use in this project.** Record controlled translations/rotations of tight and loose pod mounts, fit the corruption distribution only on training hardware, and compare measured augmentation with simple white-noise augmentation. Retain an explicit mounting-transform state so the network is not asked to hide all calibration error inside a pose prior.

**Read recommendation.** **Read the sensor setup, SeMo-AE, synthetic-versus-real data construction, wearing-style tests, and limitations.** Use it to design measured strap-perturbation augmentation, but compare against TIC and explicit calibration-state estimation rather than treating all mounting error as learnable noise.

#### D7. MobilePoser: Real-Time Full-Body Pose Estimation and 3D Human Translation from IMUs in Mobile Consumer Devices

**Citation.** Vasco Xu, Chenfeng Gao, Henry Hoffmann, and Karan Ahuja. *37th Annual ACM Symposium on User Interface Software and Technology (UIST)*, Article 70, 70:1–70:11, 2024. [DOI/ACM record](https://doi.org/10.1145/3654777.3676461) · [Official project and open paper](https://spice-lab.org/projects/MobilePoser/) · [Official code](https://github.com/SPICExLAB/MobilePoser)

**Why selected.** MobilePoser accepts available subsets of phone, watch, and earbud IMUs, predicts pose in stages, estimates translation from contact/direct velocity, and applies a physics optimizer. It complements IMUPoser with a newer global-motion and physics design and offers a strong UIST example of framing low-instrumentation sensing as enabling technology.

**Does it solve Mesquite?** **Partial architecture and graceful-degradation comparator.** It targets a few commodity-device placements, not asynchronous 17-ID suit packets, per-pod calibration, or absolute yaw. Zero-masking an unavailable device is also not enough unless Mesquite supplies explicit masks/age and prevents a real zero from being confused with missingness.

**Use in this project.** Add a sensor-subset curriculum and explicit availability/age masks, evaluate accuracy and uncertainty for every practically relevant missing-pod pattern, and compare a lightweight causal model with the full offline corrector. Use its applications only as inspiration until equivalent Mesquite tasks are evaluated.

**Read recommendation.** **Read if the project emphasizes affordability, pod subsets, or UIST/CHI positioning.** Focus on device normalization, subset training, staged pose/translation, physics optimizer, runtime, applications, and evaluation. Contrast its information budget with Mesquite's before making accuracy claims.

#### D8. Fusing Monocular Images and Sparse IMU Signals for Real-Time Human Motion Capture (RobustCap)

**Citation.** Shaohua Pan, Qi Ma, Xinyu Yi, Weifeng Hu, Xiong Wang, Xingkang Zhou, Jijunnan Li, and Feng Xu. *SIGGRAPH Asia 2023 Conference Papers*, 1–11, 2023. [DOI](https://doi.org/10.1145/3610548.3618145) · [Official project and paper](https://shaohua-pan.github.io/robustcap-page/) · [Official code](https://github.com/shaohua-pan/RobustCap) · [Open manuscript](https://arxiv.org/abs/2309.00310)

**Why selected.** RobustCap uses dual camera- and root-coordinate branches plus recurrent feedback to combine a monocular camera with six IMUs under occlusion, low light, fast motion, and out-of-view intervals. It is a direct modern reference for a substantive visual–inertial Mesquite variant.

**Does it solve Mesquite?** **A close alternative architecture, not a WebXR-pose plug-in.** It consumes and calibrates image evidence and requires valid IMU signals; Mesquite currently receives an external phone pose, does not continuously fuse phone heading, and does not retain images. Its reported global improvements therefore cannot be transferred to the current code path.

**Use in this project.** If images can be retained ethically, compare phone pose only, image features only, IMU only, one-way fusion, and bidirectional fusion with explicit extrinsics/confidence. If images are unavailable, use RobustCap to state the missing information rather than presenting the current fallback switch as equivalent fusion.

**Read recommendation.** **Must read if visual aiding becomes central.** Study camera/IMU calibration, the two coordinate branches, feedback, modality-dropout tests, alignment, live setup, and failure cases. Compare against EgoLocate: RobustCap focuses pose fusion, whereas EgoLocate also treats localization/mapping and loop closure.

#### D9. DiffCap: Diffusion-Based Real-Time Human Motion Capture Using Sparse IMUs and a Monocular Camera

**Citation.** Shaohua Pan, Xinyu Yi, Yan Zhou, Weihua Jian, Yuan Zhang, Pengfei Wan, and Feng Xu. *IEEE Transactions on Visualization and Computer Graphics*, 31(12), 10272–10283, 2025. [DOI](https://doi.org/10.1109/TVCG.2025.3596403) · [Official author project, paper, and code](https://shaohua-pan.github.io/diffcap-page/) · [Open manuscript](https://arxiv.org/abs/2508.06139)

**Why selected.** DiffCap uses a diffusion motion prior to fuse sequential visual features with frame-wise sparse-IMU observations and explicitly considers occlusion or leaving the camera view. It is the most recent learned visual–inertial alternative in this review and tests the proposal's intuition that a generative prior can bridge modality gaps.

**Does it solve Mesquite?** **A strong future baseline for an image-enabled design, not the present system.** It assumes synchronized camera and sparse-IMU inputs and generates a plausible body solution; it does not repair radio timestamps or prove the unique true motion during long missing intervals. A WebXR pose stream contains much less information than the images/features it uses.

**Use in this project.** Use it only in an image-enabled branch with matched timestamps and camera calibration. Compare its generative fusion with RobustCap and a deterministic factor/optimizer, report one-sample accuracy plus uncertainty/diversity, and retain modality-loss masks rather than hiding visual failure in a sequence embedding.

**Read recommendation.** **Read after RobustCap/EgoLocate if planning a hybrid paper.** Focus on diffusion conditioning, visual-window handling, IMU representation, causal/runtime definition, modality degradation, evaluation alignment, and comparisons. Do not adopt diffusion solely because it is newer; first test whether calibrated deterministic fusion already solves the measured failure.

---

## What the selected literature can—and cannot—fix

| Proposed or assessed problem | Best-supported response | Papers that justify it | Is the problem solved? |
|---|---|---|---|
| Quaternion reconstruction/quantization can create an invalid or large-rotation sample; persistent corruption is only a hypothesis | Reject malformed/non-finite source records; validate full norm/range before and after quantization; preserve last valid state plus an invalid flag; test calibration/storage recovery intentionally. | Huynh; Shoemake; Saltzer et al.; Herlihy & Wing | **Engineering safeguards are available; the reported cache symptom's mechanism is not established.** The binary path carries finite integers and normalizes mapped targets, so hardware fault injection must determine incidence and persistence. |
| Torn read between sampling and radio tasks | Publish one immutable sample record through a queue/double buffer/critical section; timestamp it at acquisition. | Herlihy & Wing; FTSP; Chen et al. | **Engineering solution available.** Benchmark the chosen primitive on the ESP32. |
| Per-pod clocks are unrelated and the timestamp is truncated send time | Keep acquisition sequence/time, infer per-pod affine clock-to-hub mappings, preserve hub receive time, and quantify residual clock error. | FTSP; RBS; Chen et al. | **Well-studied solution class.** Exact ESP-NOW timestamp precision must be measured. |
| Packet loss and asynchronous stale-body composition | Expose validity, age, and loss bursts; resample to a common timeline; use hold/SLERP only for short gaps and a mask-aware model for longer gaps. | BRITS; Shoemake; Chen et al. | **Partial.** No paper can reconstruct arbitrary unobserved motion without uncertainty. |
| Initial per-pod yaw differs | Test whether a known-direction T-pose estimates a per-pod initial yaw offset and sensor-to-segment transform. | Kok et al.; Seel et al.; Pacher et al. | **Likely fix for initialization if the reference direction is truly known.** It does not prevent later bias drift. |
| Relative yaw drifts across connected segments | Use gyro-bias estimation plus joint-axis/ROM/kinematic constraints, with excitation and confidence checks. | VQF; Seel et al.; Laidig et al.; Lehmann et al.; Teufl et al. | **Solved for restricted joints/conditions, not universally for every human joint.** |
| Absolute whole-body yaw is unobservable from 6-axis IMUs | Declare the gauge and add a directional observation: calibrated phone/camera world, trusted visual landmark, carefully gated magnetometer, UWB/other heading aid, or known repeated reference. | Kok et al.; EgoLocate; Pons-Moll et al.; Roetenberg et al. | **Not solvable by a motion prior alone.** A prior chooses plausible states; it is not an absolute measurement. |
| Foot/root translation drifts | Detect contacts probabilistically and use support/stationary-point/physics constraints inside a declared pose/root state estimator. A classical ZUPT additionally requires raw foot inertial data and an integrated navigation/velocity state, neither of which exists downstream today. | Skog et al.; TransPose; TIP; PIP; GlobalPose | **Strong partial solution family, not a drop-in ZUPT.** Contact errors, flight, moving support surfaces, and nonstationary activities remain. |
| Calibration changes after donning or strap movement | Estimate intrinsic IMU parameters separately; monitor kinematic residuals; update sensor-to-segment calibration slowly and only when observable. | Tedaldi et al.; Taetz et al.; Miezal et al.; Pacher et al. | **Partial.** Online arbitrary-joint slip remains a research contribution and needs ground truth. |
| Sparse observations permit many poses | Fit a body/motion prior, preferably uncertainty-aware and contact/physics constrained. | SIP; DIP; TransPose; PIP; TIP; DynaIP; ProbIP | **Strong solution family for plausible pose reconstruction.** “Plausible” must not be reported as measured truth. |
| Synthetic training does not match Mesquite hardware | Fit a raw-IMU/error simulator from training hardware logs and fine-tune/test on real, subject/session/device-held-out data. | DIP; AMASS; PNP; DynaIP | **Mitigable, not eliminated.** A real synchronized test set is mandatory. |
| Current benchmark conflates errors | Re-capture simultaneous optical data; report rotation, joint pose, root trajectory, scale/retargeting, and timing separately. | Robert-Lachaine et al.; OpenSense; Huynh; Bland & Altman | **Evaluation solution available.** Existing Rokoko files cannot answer the central accuracy question. |

## Recommended Mesquite research design

### Research claim to pursue

A defensible ML claim would be:

> Given valid, acquisition-timestamped, magnetometer-free body IMU streams and an explicit initial/world-frame convention, a constraint- and motion-prior-based offline method reduces **relative segment-heading and joint-pose error** under calibrated packet loss, timing uncertainty, and mounting perturbations, while emitting calibrated uncertainty and preserving the global-yaw gauge.

This is narrower than “solves yaw drift,” but much stronger scientifically because every noun is observable and testable. If the phone supplies continuous visual heading, add a separate hypothesis about **world-heading fusion**; do not hide that information inside the motion prior.

Suggested hypotheses:

- **H1 — integrity:** validity-aware acquisition/parsing and coherent sample publication reduce injected corruption's error and bound recovery time; a separate reproduction test determines whether any corruption class explains the reported browser-cache symptom.
- **H2 — time:** acquisition-time resampling reduces rotation and joint-position error relative to arrival-order composition.
- **H3 — constraints:** classical joint/contact constraints reduce relative-heading error versus no correction.
- **H4 — prior:** a learned motion prior improves over the classical constraint baseline on held-out people and motions, especially at weakly constrained joints.
- **H5 — uncertainty:** predicted confidence identifies corrupted, missing, or out-of-distribution segments and improves selective risk/error.
- **H6 — external heading:** calibrated phone/camera aiding reduces absolute root-yaw and trajectory error; without it, only gauge-invariant metrics are claimed.

### Stage 0 — freeze claims and provenance before new modeling

1. Tag the exact firmware, dongle, browser, tree configuration, model, and benchmark commit used for every capture.
2. Record build toolchains, Arduino/ESP-IDF versions, DMP configuration, requested sensor rate, radio channel/rate, pod mapping, and browser version.
3. Mark the existing 2–5°, less-than-15-ms, and 99.7%-plus numbers **unverified in this checkout** until the originating OptiTrack artifacts and scripts are recovered.
4. Withdraw the proposal's degree labels on the 42/15 values, exact −71/+99 sensor-yaw interpretation, and asserted 7% dilation. Preserve those old results only as diagnostic history.
5. Determine the status and overlap of the IEEE IoT Journal submission before submitting overlapping platform work elsewhere.

**Gate:** no headline claim remains whose raw inputs, code revision, unit, alignment, sample size, and uncertainty cannot be named.

### Stage 1 — make a sample a coherent scientific record

Use one schema from sensor to stored dataset, for example:

    protocol_version, sensor_id, boot_id, sample_seq,
    sensor_sample_time, hub_receive_time,
    quaternion_or_raw_imu, battery, validity_bits, calibration_version

Choose and name one of two evidence protocols rather than treating them as interchangeable:

- **Deployment/performance protocol:** retain the compact orientation packet, then evaluate integrity, timing, calibration, relative-orientation correction, rendering, and export under exactly that information budget. Raw-data filter, ZUPT, and acceleration-based learned baselines are out of scope.
- **Research/estimator protocol:** add or separately log acquisition-timestamped raw gyroscope and accelerometer channels—and magnetometer/temperature if studied—alongside the DMP quaternion. This is required for faithful VQF/Mahony/Tedaldi/Seel raw-constraint, navigation-ZUPT, and most DIP-family comparisons. Measure the bandwidth/rate/energy cost rather than assuming the richer packet is deployable.

Required changes, derived from Assessments 1 and 3:

- The IMU task should create the quaternion/raw-IMU values, acquisition timestamp, validity, and sequence as one snapshot. Pass that immutable snapshot to the radio task through a queue, critical-section-protected double buffer, or proven atomic handoff.
- Replace ambiguous/truncated timing with enough bits plus boot identity to unwrap it safely. Preserve send time only if it answers a separate question.
- Add message type, protocol version, payload length, and an integrity check. A sync word helps framing but is not a checksum.
- Validate finite values, component bounds, and full quaternion norm before quantization, at the hub, and again in the browser. Reject and count failures; do not silently replace them with identity orientation.
- Normalize a near-valid quaternion after validation. Normalization is not a substitute for rejecting zero, NaN, grossly non-unit, or implausibly discontinuous input.
- Initialize every supported bone before dereference, generate the ID table from one source, and run an automated 0–16 end-to-end ID test. The current shoulder IDs need special attention.
- Move display/DOM work away from the packet path; preallocate tracking buffers; make reset/recovery visible and local to the affected pod/session.
- Replace packet-count-dependent smoothing with elapsed-time smoothing, deriving alpha from the measured update interval for a fixed physical time constant, for example `alpha = 1 - exp(-delta_t/tau)`, and log both raw and displayed states.

**Literature anchors:** Herlihy & Wing for coherent shared state; FTSP/RBS/Chen for time; Saltzer et al. for end-to-end validation; Huynh/Shoemake for rotations; Casiez et al. for the interactive jitter–lag baseline.

**Gate:** injected malformed/truncated/out-of-order/duplicate/lost records are detected, counted, bounded in effect, and recover without page storage deletion or a full-suit reboot.

### Stage 2 — perform the measurement campaign that the assessments could not

The repository contains instrumentation hooks and plans, but not the hardware results required to close the open questions. Measure, do not infer:

- DMP output rate, application sample rate, send rate, receive rate, browser composition rate, and export rate.
- Per-pod packet delivery, duplicates, reorder, loss-burst length, RSSI, PHY rate, channel occupancy, and scaling from 1 to the complete pod count.
- Sensor-sample age at transmission, hub arrival, browser processing, render, and recording.
- Clock offset, skew, mapping residual, re-lock time, 16-bit-wrap behavior, and reboot behavior.
- End-to-end latency distribution—not a single correlation peak—including median, 95th, and 99th percentile. Use a shared physical event or instrumented LED/TTL/high-speed-camera method with stated uncertainty.
- CPU/task time, queue depth, serial blocking, browser long tasks/garbage collection, memory, thermal behavior, battery runtime, and energy per useful sample.
- Quaternion norm failures, negative reconstruction radicands, browser rejects, pod reset count, WebXR tracking loss/relocalization, and recovery time.
- Raw-estimator versus rendered-pose jitter and filter response: controlled angular steps/sinusoids at several rates, amplitude attenuation, phase/group delay, settling time, and sensitivity to packet-rate changes. Compare no smoothing, current fixed-factor SLERP/LERP, elapsed-time fixed-constant smoothing, and a rotation-aware 1 € baseline.

Run at least static, controlled single-axis rotation, normal whole-body motion, high-dynamic motion, radio-body-shadowing, controlled burst loss, pod reboot, and a 30–60 minute stability trial. Repeat across relevant rates and pod counts.

**Literature anchors:** Mercury and Chen et al. for wearable-system fidelity; FTSP/RBS for clock evaluation; Saltzer et al. for physical-event end-to-end latency.

**Gate:** every architecture bottleneck and failure claim is supported by a measured distribution with hardware/software versions, not a nominal constant or hypothetical PHY mode.

### Stage 3 — collect valid simultaneous reference data

The existing Rokoko comparison is useful as a pipeline smoke test, but it is not ground truth for the same inertial-heading failure mode and contains no reference arms. Record a new dataset with:

- rigid optical marker clusters attached to the Mesquite pods to isolate sensor/technology orientation error;
- anatomical markers or a validated segment model to measure the additional biomechanical/model error;
- a hardware-visible synchronization event and common time mapping, not post-hoc correlation alone;
- current Mesquite raw IMU, fused quaternion, packet metadata, phone/WebXR pose and events, displayed pose, and optical data retained separately;
- multiple participants and body shapes, repeated removal/redonning, cold boots, multiple hardware units, multiple sessions/days, and controlled mounting perturbations;
- static holds, known rotations, gait, turns, sit/stand, stairs if safe, upper-body articulation, dance/high dynamics, contacts and no-contact intervals, and long trials;
- reference exports that actually contain every evaluated limb, including both arms.

**Literature anchors:** Robert-Lachaine et al. for technology-versus-model separation; OpenSense and Teufl et al. for simultaneous long-duration validation and repeatability; Pacher et al. for donning/calibration design.

**Gate:** an independent script can reconstruct the frame graph, time map, calibration, units, and evaluation windows from released metadata without manual guesses.

### Stage 4 — establish transparent baselines before ML

Evaluate these in increasing complexity:

1. raw current pipeline, with all legacy transforms documented;
2. integrity/time repair only;
3. short-gap hold and sign-consistent SLERP;
4. current DMP and a raw-data VQF/complementary-filter baseline, only if raw gyro/accelerometer streams are recorded;
5. initial T-pose/mounting calibration only;
6. Seel/rotation-rate Laidig joint-axis or relative-heading constraints where raw channels and joint assumptions hold; otherwise label and test the weaker fused-orientation-compatible Laidig/Lehmann variants;
7. range-of-motion and contact-constrained windowed optimization; use “ZUPT” only when a raw foot-IMU navigation/velocity state is actually integrated, otherwise call it a contact or stationary-point constraint;
8. OpenSense or another independently implemented biomechanical IK baseline;
9. SIP/DIP/TransPose/PIP/TIP/DynaIP/DiffusionPoser/GlobalPose baselines as appropriate to the exact input/output task;
10. proposed method, with each new component ablated.

If an external implementation cannot consume Mesquite's 15/17-sensor layout, use its published six-sensor subset and label the comparison. Do not quietly give the proposed method more sensors, future context, calibration data, or a different alignment.

**Gate:** the proposed method beats meaningful simple/classical and current learned baselines under the same information, alignment, split, and latency budget.

### Stage 5 — train a motion prior only after Stages 1–4 pass

Recommended model inputs:

- time-normalized orientation or raw gyro/acceleration in explicitly named frames;
- per-sensor validity mask, elapsed time, sample age, and loss-gap length;
- sensor placement/calibration metadata and optional body-shape parameters;
- phone/visual observation and confidence as a separate optional channel;
- contact/stationary-point probabilities rather than assumed labels at inference.

Recommended outputs and objectives:

- output local/relative rotations in Zhou et al.'s 6D representation or a well-defined SO(3) distribution;
- use geodesic local-rotation loss, differentiable-FK joint-position loss, temporal velocity/acceleration loss, joint-limit/ROM terms, contact velocity/foot-skate loss, and correction-magnitude regularization;
- preserve a free global-yaw variable unless a world-heading observation is present;
- predict uncertainty or an abstention score; penalize confident wrong corrections;
- constrain online and offline variants separately. A bidirectional model must not be described as live without its look-ahead delay.

Synthetic corruption should be fitted from training-hardware measurements and include bursty—not merely independent—loss, asynchronous clocks and skew, sampling jitter, bias, scale/misalignment, sensor-to-segment error, strap perturbations, quantization, non-unit/invalid records, and WebXR dropout/reset. Never tune these distributions on the held-out optical test set.

**Literature anchors:** AMASS/SMPL for motion/body data; DIP/PNP/DynaIP for synthesis and domain gap; Zhou/QuaterNet for rotation outputs and FK loss; BRITS for masks/time gaps; ProbIP for uncertainty; PIP/TIP for physics/contact.

**Gate:** results survive person-, motion-, session-, and hardware-held-out tests, and uncertainty worsens appropriately under unseen corruption rather than remaining overconfident.

## Evaluation protocol reviewers can trust

### Report outcomes by layer

| Layer | Required metrics | Key reporting rule |
|---|---|---|
| Record validity | non-finite count, zero/near-zero norm, norm distribution, discontinuity/outlier count, detected versus injected corruptions, false rejection | Give counts per pod-hour and show recovery duration; do not collapse into “no crash.” |
| Network/time | actual sample/send/receive/render/record rates; delivery ratio per pod; duplicate/reorder; loss-burst CDF; clock offset/skew/residual; sample age; queue depth | Report distributions and worst pod, not only fleet averages. State timestamp location and precision. |
| Latency | physical-event-to-raw-pose and physical-event-to-displayed-pose median/p95/p99; jitter; filter group delay | Separate algorithm look-ahead, transport, rendering, and smoothing. Correlation is diagnostic, not ground-truth latency without a unique synchronized event. |
| Orientation | SO(3) geodesic error in degrees for pod/sensor frame, optical segment frame, root orientation, and parent-relative joint rotation | State every coordinate frame and calibration. Split tilt and heading where scientifically meaningful. |
| Articulation | joint-angle error by degree of freedom; local rotation; joint-position error from FK | Do not infer angular accuracy from Euclidean joint distances. Report per joint and per activity. |
| Position | MPJPE in calibrated centimetres in raw world, root-translation-aligned, root-orientation-aligned, and scale-aligned conditions | Treat each alignment as a different question. Do not use Procrustes/scale-aligned error as the only headline for a metric-scale system. |
| Root trajectory | ATE, RPE versus interval/distance, final drift, drift per metre/minute, heading error, scale error, WebXR reset/relocalization failures | State whether alignment is SE(3), yaw-only, Sim(3), or none. Local SLAM coordinates are not geographic truth. |
| Motion quality | velocity/acceleration/jerk error, foot skating, penetration, contact precision/recall, joint-limit violations | Pair plausibility metrics with measurement errors; a smoother but wrong motion should not “win.” |
| Calibration/reliability | initial calibration error, repeat-donning spread, time-to-calibrate, controlled-slip detection delay/false alarm, test-retest ICC/agreement | Keep intrinsic sensor calibration separate from sensor-to-segment calibration. |
| Resources | CPU, memory, energy, battery life, radio airtime, browser long tasks/GC, throughput, thermal behavior | Measure on the actual pod, hub, phone/browser, and target computer configuration. |
| Uncertainty | negative log-likelihood or distribution score where valid, calibration curve, expected calibration error, coverage-versus-width, risk-coverage/selective-error | Verify that confidence responds to missing sensors, long gaps, unseen motions, and bad calibration. |

### Alignment and metric rules

1. Choose a primary metric and alignment **before** examining final test results.
2. For relative-heading correction, primary orientation error should be parent-relative joint/segment geodesic error; report absolute root yaw only when a world-heading observation exists.
3. Compute quaternion error with antipodal equivalence; never subtract quaternion components or Euler triplets and call the result an angle.
4. Convert all position data to metres/centimetres from documented skeleton scale. “BVH units” are not a physical unit.
5. Report raw/no-alignment performance first for a claimed world-space system. Provide root/yaw/scale-aligned variants as diagnostic decompositions.
6. Estimate synchronization uncertainty and propagate it through a sensitivity analysis: deliberately shift the reference by plausible residual timing errors and show how rotation/position metrics change.
7. Keep raw fused orientations, corrected orientations, browser-smoothed orientations, and exported Euler/BVH values separate. Otherwise export and smoothing artifacts will be attributed to the estimator.

### Experimental design and statistics

- Use the participant—not frames—as the primary independent unit for general human-motion claims. Frames are repeated observations nested within trials, sessions, and participants.
- Choose participant count from an a priori power/precision analysis based on a meaningful improvement and participant-level variance. Do not copy another paper's sample size mechanically.
- Use subject-held-out evaluation; also hold out sessions, hardware units, and at least selected motion families. Fit normalization, corruption distributions, calibration thresholds, and early stopping on training/validation only.
- Report participant-level mean/median, dispersion, confidence intervals, and per-activity/per-joint results. A hierarchical model or participant-level bootstrap is preferable to pretending every frame is independent.
- For method agreement, report Bland–Altman bias and limits of agreement with a repeated-measures/hierarchical treatment. Correlation can be additional, never a replacement for agreement.
- Use paired comparisons because each trial can be processed by all algorithms. Report effect sizes and interval estimates, not only p-values.
- Pre-register or freeze primary hypotheses, metrics, alignments, exclusions, and thresholds before unlocking the final optical test set.
- Include failure rate, worst-case tails, and the fraction of frames/trials the correction **worsens**. An average improvement can conceal destructive corrections.

### Minimum ablation matrix

| Ablation | Scientific question answered |
|---|---|
| current pipeline vs integrity/time repair | Were gains caused by the proposed model or by fixing invalid input and timestamps? |
| no prior vs joint/ROM constraints vs learned prior | Does learning add value beyond known biomechanics? |
| no contact vs oracle contact vs predicted contact | Are gains limited by the detector or by the correction method? |
| no phone vs phone translation only vs phone pose/heading | Which degrees of freedom come from WebXR, and at what tracking quality? |
| quaternion vs 6D rotation output | Is optimization stability representation dependent? |
| geodesic only vs geodesic + FK vs + physics/contact | Which objective creates each improvement or artifact? |
| synthetic only vs real fine-tuning vs multi-real-dataset training | How large is the Mesquite domain gap? |
| random loss vs measured burst loss/asynchrony | Does the model work under the system's real failure process? |
| fixed calibration vs offline joint estimation vs slow online update | Is Stage 4 correcting mounting drift or merely absorbing pose error? |
| deterministic vs uncertainty-aware/selective correction | Can the system know when not to “fix” a valid unusual motion? |
| causal vs bidirectional | What accuracy is purchased by future context and delay? |
| six-sensor subset vs full functional pod set | Is the hardware density useful after accounting for rate, loss, and calibration burden? |

### Negative controls and stress tests

- Feed a valid but unusual held-out motion and test whether the prior wrongly pulls it toward common training motions.
- Apply a common global-yaw rotation to every sensor. Gauge-invariant relative pose should remain unchanged unless an external heading observation is enabled.
- Inject a known relative yaw into exactly one segment and check whether correction is localized rather than redistributed arbitrarily across the chain.
- Inject the same corruption at different timestamps, packet rates, and loss contexts to expose rate-dependent filtering.
- Remove the phone or trigger WebXR tracking loss/relocalization. The method should lower confidence and maintain a documented local frame rather than jump silently.
- Repeat a capture after removal/redonning and across sensor swaps. This detects leakage of person-, session-, or unit-specific calibration.

## Suggested system architecture for the ML paper

The following decomposition keeps engineering correctness and research novelty separable:

    IMU acquisition
        -> immutable timestamped sample + validity
        -> radio/hub transport with measured receive time
        -> clock mapping and common-time resampling
        -> short-gap transparent interpolation / long-gap mask
        -> intrinsic + sensor-to-segment calibration
        -> classical relative-heading/contact constraint baseline
        -> learned residual motion prior with uncertainty
        -> optional explicit phone/visual world-heading factor
        -> constrained smoother
        -> raw, corrected, confidence, and provenance outputs

Why a **residual** corrector is preferable initially:

- it can be initialized to no change and constrained not to damage valid measurements;
- correction magnitude and location are interpretable;
- classical filter/constraint output remains a strong fallback;
- uncertainty can gate the correction;
- the global-yaw gauge can be preserved explicitly rather than accidentally learned from dataset alignment.

Start offline, because bidirectional context and a complete integrity audit are easier to validate. Only claim real time after creating a causal version and measuring acquisition-to-display delay on the target system.

## Dataset and reproducibility plan

### Split policy

Use a nested, leakage-resistant split:

- **test people:** never used for training, normalization, calibration hyperparameters, corruption fitting, or early stopping;
- **test sessions/days:** captures after redonning and cold boot;
- **test devices:** at least some physical pods or pod assignments withheld/swapped;
- **test motions:** selected activity families absent from training;
- **test corruptions:** measured conditions or severity ranges not used to tune thresholds.

Report both in-distribution and out-of-distribution results. A random frame split is invalid because adjacent windows from one motion are nearly duplicates.

### Public artifact contents

- immutable raw pod/hub/browser/phone/optical logs with monotonic and wall-clock anchors;
- data dictionary, packet schema, units, coordinate-frame graph, calibration files, skeleton definition, and consent/license statement;
- exact train/validation/test manifests and hash list;
- preprocessing, clock synchronization, calibration, training, evaluation, and figure-generation code;
- dependency lockfile/container, trained checkpoints, configuration files, random seeds, and compute/runtime report;
- fault-injection harness plus small non-identifiable example capture;
- one command that regenerates every headline table/figure from released or appropriately controlled data.

For identifiable full-body motion, complete the applicable human-subjects/ethics process **before collection** and obtain explicit permission for the intended data/video release. Provide a restricted-data path if public release is not permitted.

## Venue strategy as of 15 September 2026

The named **2026 conference-cycle deadlines** below have passed, so those calls are scope evidence rather than live conference targets. Journal schedules differ: IMWUT advertised a 1 November 2026 journal cycle, and IEEE IoT-J accepts regular submissions on a rolling basis. The official pages were checked on 15 September 2026; SCA's page is unversioned and may be overwritten. Re-check the eventual call, page limits, tracks, policies, and dates before acting. Fit means that the topic belongs there; it is not an estimate of acceptance probability, and the project is not ready for the remaining journal opportunities in its current state.

| Venue | Best Mesquite paper identity | Topical fit | Readiness now | What must be true before submission |
|---|---|---:|---:|---|
| [ACM SIGGRAPH/Eurographics Symposium on Computer Animation (SCA)](https://computeranimation.org/instructions.html) | Learned relative-heading/reconstruction/calibration method; alternatively a rigorous MoCap benchmark | **High; best eventual ML fit** | **Low** | **Method branch:** a real algorithm, valid reference data, modern baselines, ablations, failures, and reproducible results. **Benchmark branch:** SCA 2026 explicitly allowed experimental/benchmark evaluations without algorithmic novelty, but still required strong benchmark design, methodology, insight, and community benefit. That waiver did not automatically cover every dataset/system/hardware submission and may be year-specific. Full papers publish in *Computer Graphics Forum*. |
| [ACM SenSys](https://sensys.acm.org/2026/cfp.html) | Current ESP-NOW/ESP32-S3 wearable sensing platform, synchronization/reliability, and possibly embedded inference | **Very high for the system** | **Low; potentially medium after the measurement campaign** | A distinct contribution beyond the public preprint, full-fleet delivery/loss-burst and clock results, physical-event latency tails, energy/runtime, scalability, and recovery. SenSys 2026 was a special SenSys/IPSN/IoTDI merger, so its exact next-cycle structure may differ. |
| [ACM IMWUT / UbiComp](https://www.ubicomp.org/ubicomp-iswc-2026/imwut-papers/) | Wearable/pervasive platform design, development, deployment, or human experience | **High** | **Low** | A rigorous ubiquitous/wearable technical evaluation; field deployment, usability, or lived-use evidence is required when Mesquite makes those corresponding claims, not for every technical paper by default. Main UbiComp technical work is published first in *IMWUT*, not as an ordinary conference-proceedings paper. |
| [ACM ISWC Notes or Briefs](https://www.ubicomp.org/ubicomp-iswc-2026/iswc-cfp/) | A narrow wearable subsystem/result | **High for a tightly scoped result** | **Brief: conditional after cleanup; Note: low** | A distinct narrow contribution. A Brief can have limited evaluation; a Note must still be robust and is not an outlet for a preliminary/under-developed study. Neither is a loophole for repeating the preprint or publishing a bug list. |
| [SIGGRAPH Technical Papers—Conference track and TOG Journal track](https://s2026.siggraph.org/program/technical-papers/) | New learned graphics/animation MoCap method | **High topically** | **Conference: very low; Journal: aspirational** | Conference-track work still must advance graphics/interactive techniques, though 2026 allowed less-comprehensive evidence than the Journal track. The TOG track expects a mature, novel, well-validated and comprehensively described result. In 2026 these were choices in one integrated submission process, not a conference-then-journal sequence; an expanded version of prior conference work goes through direct TOG policy. Affordability or integration alone is insufficient for either. |
| [ACM CHI Papers](https://chi2026.acm.org/authors/papers/) and [subcommittee scopes](https://chi2026.acm.org/authors/papers/selecting-a-subcommittee/) | Embodiment, accessibility, creative practice, calibration experience, or a new enabling interaction | **Conditional** | **Very low under the current accuracy-only framing** | A direct HCI contribution and matching evidence. CHI can accept enabling systems, novel tracking hardware, and computational interaction without a user study in every case, but a lower RMSE or radio migration alone is not an HCI contribution. |
| [IEEE VR](https://ieeevr.org/2026/contribute/papers/) | XR avatar tracking, body ownership/agency, embodiment, or XR infrastructure | **High only when XR is central** | **Very low** | The question and evaluation must engage VR/AR/MR/3DUI literature and outcomes. Merely using WebXR as a root sensor is insufficient; the 2026 call used unified conference/TVCG outcomes. |
| [IEEE ISMAR](https://www.ieeeismar.net/2026/call-for-papers/) | XR registration/tracking, calibration, sensor fusion, real-time avatars | **High only with an explicit significant XR connection** | **Very low** | Make spatial registration or XR experience the contribution and evaluate it accordingly. The 2026 call explicitly required a clear, significant AR/MR/VR connection and used one review process with conference or TVCG outcomes. |
| [IEEE Internet of Things Journal](https://ieee-iotj.org/guidelines-for-authors/) | Archival end-to-end IoT architecture, networked sensing, edge processing, or testbed | **Very high for the systems story; weak for standalone animation ML** | **Depends on the reported submission status** | Resolve the preprint's stated IoT-J submission, establish a novel extension, and measure the current architecture end to end. The current guide also imposes mandatory over-length charges after eight published pages; check current cost/policy before planning. |

### Venue decision

- **If the contribution is a learned pose/calibration/relative-heading method:** target **SCA** first after optical validation; treat the SIGGRAPH Conference track as a stretch and the SIGGRAPH TOG track as the higher-completeness option in its integrated submission process, not as an automatic later upgrade path.
- **If the contribution is the revised networked suit:** target **SenSys**; consider **IoT-J** for an archival systems/testbed article, **IMWUT/UbiComp** for a full wearable/pervasive-computing paper, or an **ISWC Brief/Note** for a genuinely narrow contribution under that format's distinct requirements.
- **If the contribution is phone–IMU spatial tracking for avatars:** consider **IEEE VR or ISMAR**, but only after explicit calibrated fusion and XR-centered evaluation exist.
- **If the contribution is access, creator workflow, self-calibration, or embodiment:** consider **CHI**, but first formulate the human/interaction question and evidence rather than adding a token user study to an estimator paper.
- **Current overall verdict:** no version is submission-ready from this checkout. The systems path is closer than the ML path because code exists, but it still lacks the required live measurements and a clear novelty delta from the public manuscript.

### Publication and ethics constraints

- Substantially overlapping concurrent submissions are prohibited. The public arXiv posting alone is not the immediate concurrency blocker at venues that permit preprints; the possibly active IoT-J review is. Verify its status before sending the same systems contribution elsewhere, and disclose the preprint and related manuscripts exactly as each venue requires.
- Preserve four distinct potential papers: the old-architecture systems preprint/revision; a genuinely novel current-platform extension; the new estimator/data/evaluation paper; and, only with a distinct question, a later HCI/XR study. Shared hardware or data can be legitimate, but contributions and reported results must not be recycled deceptively.
- Public arXiv/repository/video material can affect double-blind anonymity even where preprints are permitted. Follow the target's anonymized supplement and disclosure workflow.
- Obtain applicable human-participant/ethics approval **before** optical, wearable, or XR data collection and consent explicitly for any motion/video release.
- Re-check the target's generative-AI disclosure policy when preparing the submission; policies differ and change by cycle.

## How to rewrite the Mesquite manuscript

### First decide which paper is being written

There are at least three distinct artifacts:

1. **The public systems preprint:** the older 15-pod Wi-Fi/router/Raspberry Pi description.
2. **The current platform revision:** the 17-ID ESP-NOW → ESP32-S3 → USB WebSerial implementation in this checkout. This may be a revision/extension of the systems work, but its delta must be explicit.
3. **The proposed ML correction paper:** a new estimator, dataset/protocol, baselines, ablations, and conclusions.

An optional fourth paper is an XR/HCI study, but only if it asks a real interaction/embodiment/creator-access question. Architecture modernization cannot be presented as ML novelty, and the ML paper must cite/disclose the systems preprint and explain non-overlap according to the target venue's policy.

### Recommended paper structure for the ML contribution

1. **Abstract.** State input sensors, offline/causal status, exact correction target, external heading availability, dataset size, primary metric/alignment, baseline, and uncertainty. Add numbers only after the frozen test.
2. **Introduction.** Motivate low-cost outdoor mocap; distinguish transport corruption, calibration, relative heading, absolute heading, pose ambiguity, and root translation; state 3–4 testable contributions.
3. **Related work.** Organize by observability/fusion, joint/calibration constraints, learned inertial pose, contact/physics, missing data, hybrid visual-inertial mocap, and validation—not as a chronological paper list.
4. **Mesquite measurement model.** Give the actual current architecture, frame graph, timing model, packet schema, calibration transforms, smoothing, and the global-yaw gauge.
5. **Method.** Define data validity/resampling, baseline estimator, residual corrector, rotation representation, losses, uncertainty, phone factor, and causal/offline window precisely.
6. **Dataset/protocol.** Participants, ethics, hardware and versions, optical setup, rigid/anatomical markers, synchronization, tasks, splits, corruption fitting, and release terms.
7. **Experiments.** Same-information baselines, primary result, per-joint/activity results, timing/loss stress, calibration/redonning, ablations, uncertainty, runtime/latency, failures.
8. **Discussion/limitations.** Global gauge, unusual motions, soft tissue, contact errors, WebXR resets, privacy, domain shift, and when the method must abstain.
9. **Artifact statement.** Exactly what code/data/models reproduce the paper.

### Contribution language to use—and avoid

Prefer:

- “relative-heading correction under stated joint/motion assumptions”;
- “world-heading aided by a calibrated visual observation”;
- “reconstructs a plausible estimate with calibrated uncertainty”;
- “root-relative MPJPE in centimetres after translation-only alignment”;
- “measured end-to-end latency p50/p95/p99.”

Avoid unless directly proven:

- “eliminates yaw drift”;
- “ground truth” for Rokoko;
- “42 degrees global error” or “15 degrees pose error” from the current script;
- “7% time dilation” from nominal 32-versus-30 rates alone;
- “absolute heading” from an unanchored WebXR frame;
- “recovers missing motion” without masks, uncertainty, gap limits, and optical evaluation;
- “online calibration” when the parameter is unobservable in the active motion;
- “real time” based only on network inference time rather than full physical-event-to-display latency.

### Suggested working title

**Mesquite-Correct: Constraint- and Motion-Prior-Based Relative Heading Correction for Magnetometer-Free Inertial Motion Capture**

Use this only if relative-heading correction becomes the measured central result. If the strongest contribution is the repaired wearable data path, use a systems title; if phone/IMU co-estimation becomes central, use a hybrid visual-inertial title.
