// =========================================================================
//  MESQUITE PHASE 2 INSTRUMENTATION  (browser side)
//  Observational only. Adds no behaviour to the data path.
//
//  Instruments implemented here:
//    I5  - per-bone sequence-gap detection (SYNC-03, INT-02)
//    I6  - arrival timestamps, inter-arrival histograms, stream health
//    W1b - position-guard failure counter (PHN-01)
//
//  Loaded before js/webserialnative.js. If this file is absent every hook
//  degrades to a no-op, so the parser still runs unmodified.
//
//  Console entry points:
//    MesqInstr.report()    -> one-shot snapshot, console.table + object
//    MesqInstr.start(1000) -> begin 1 Hz sampling into MesqInstr.history
//    MesqInstr.stop()
//    MesqInstr.exportJSON()-> string, for saving alongside a session
// =========================================================================
(function (root) {
  'use strict';

  // A pod reboot resets `count` to 0, which the modulo below would otherwise
  // report as ~65535 lost packets. Any apparent gap larger than this is
  // treated as a resynchronisation event, not loss. At 32 Hz this is ~30 s
  // of genuinely missed packets, which is far beyond any real dropout.
  var RESYNC_THRESHOLD = 1000;

  var bones = {};        // per-bone I5 + I6 state
  var stream = { bytes: 0, binary: 0, json: 0, jsonLineLen: 0, jsonLineMax: 0 };
  var guards = { posRejected: 0, posAccepted: 0, nanFrames: 0 };
  var timer = null;
  var history = [];
  var t0 = null;

  function now() {
    return (typeof performance !== 'undefined' && performance.now)
      ? performance.now() : Date.now();
  }

  function bone(name) {
    if (!bones[name]) {
      bones[name] = {
        received: 0, lost: 0, resyncs: 0, lastCount: null,
        lastArrival: null, intervals: [], dup: 0, reorder: 0
      };
    }
    return bones[name];
  }

  // ---- I5: sequence-gap detection -------------------------------------
  // gap = (count - lastCount + 65536) % 65536 - 1
  //   0        -> consecutive, nothing lost
  //   1..N     -> N packets lost
  //   huge     -> pod rebooted (counter reset) OR very long dropout
  //   -1 (dup) -> same count seen twice
  function onPacket(name, count, tArrival) {
    var b = bone(name);
    var t = (tArrival === undefined) ? now() : tArrival;

    if (b.lastCount !== null) {
      var raw = (count - b.lastCount + 65536) % 65536;
      var gap = raw - 1;
      if (raw === 0) {
        b.dup++;                       // identical count repeated
      } else if (gap > RESYNC_THRESHOLD) {
        b.resyncs++;                   // reboot or counter reset, not loss
      } else if (gap > 0) {
        b.lost += gap;
      }
      // raw > 32768 would indicate a backwards jump; the resync branch
      // already absorbs those, so no separate reorder path is needed for
      // the unicast link. Counter retained for future transports.
    }
    b.lastCount = count;
    b.received++;

    if (b.lastArrival !== null) {
      var dt = t - b.lastArrival;
      if (b.intervals.length < 20000) b.intervals.push(dt);
    }
    b.lastArrival = t;
    if (t0 === null) t0 = t;
  }

  // ---- I6: stream health ----------------------------------------------
  function onBytes(n)        { stream.bytes += n; }
  function onBinaryFrame()   { stream.binary++; }
  function onJsonLine()      { stream.json++; }
  function onJsonLineLen(n)  {
    stream.jsonLineLen = n;
    if (n > stream.jsonLineMax) stream.jsonLineMax = n;
  }

  // ---- W1b: position guard (PHN-01) -----------------------------------
  function onPositionGuard(accepted) {
    if (accepted) guards.posAccepted++; else guards.posRejected++;
  }
  function onNanFrame() { guards.nanFrames++; }

  // ---- summary ---------------------------------------------------------
  function pct(a, b) { return (a + b) === 0 ? 0 : (100 * a / (a + b)); }

  function stats(arr) {
    if (!arr.length) return null;
    var s = arr.slice().sort(function (x, y) { return x - y; });
    var sum = 0;
    for (var i = 0; i < s.length; i++) sum += s[i];
    return {
      n: s.length,
      min: +s[0].toFixed(2),
      median: +s[s.length >> 1].toFixed(2),
      mean: +(sum / s.length).toFixed(2),
      p95: +s[Math.floor(s.length * 0.95)].toFixed(2),
      max: +s[s.length - 1].toFixed(2)
    };
  }

  function snapshot() {
    var out = { t: now(), bones: {}, stream: {}, guards: {} };
    var totR = 0, totL = 0;
    for (var k in bones) {
      var b = bones[k];
      totR += b.received; totL += b.lost;
      out.bones[k] = {
        received: b.received,
        lost: b.lost,
        lossPct: +pct(b.lost, b.received).toFixed(2),
        resyncs: b.resyncs,
        dup: b.dup,
        interval: stats(b.intervals)
      };
    }
    out.totals = {
      received: totR, lost: totL,
      lossPct: +pct(totL, totR).toFixed(2),
      bonesSeen: Object.keys(bones).length
    };
    out.stream = {
      bytes: stream.bytes, binaryFrames: stream.binary, jsonLines: stream.json,
      jsonLineLen: stream.jsonLineLen, jsonLineMax: stream.jsonLineMax
    };
    out.guards = {
      positionRejected: guards.posRejected,
      positionAccepted: guards.posAccepted,
      nanFrames: guards.nanFrames
    };
    return out;
  }

  function report() {
    var s = snapshot();
    if (typeof console !== 'undefined' && console.table) {
      var rows = {};
      for (var k in s.bones) {
        rows[k] = {
          rx: s.bones[k].received, lost: s.bones[k].lost,
          'loss%': s.bones[k].lossPct, resync: s.bones[k].resyncs,
          'dt_med': s.bones[k].interval ? s.bones[k].interval.median : '-',
          'dt_p95': s.bones[k].interval ? s.bones[k].interval.p95 : '-'
        };
      }
      console.table(rows);
      console.log('totals', s.totals, 'stream', s.stream, 'guards', s.guards);
    }
    return s;
  }

  function start(periodMs) {
    stop();
    timer = setInterval(function () { history.push(snapshot()); },
                        periodMs || 1000);
    return 'MesqInstr sampling every ' + (periodMs || 1000) + ' ms';
  }
  function stop() { if (timer) { clearInterval(timer); timer = null; } }
  function reset() {
    bones = {}; history = []; t0 = null;
    stream = { bytes: 0, binary: 0, json: 0, jsonLineLen: 0, jsonLineMax: 0 };
    guards = { posRejected: 0, posAccepted: 0, nanFrames: 0 };
  }
  function exportJSON() {
    return JSON.stringify({ final: snapshot(), history: history }, null, 2);
  }

  var api = {
    onPacket: onPacket, onBytes: onBytes, onBinaryFrame: onBinaryFrame,
    onJsonLine: onJsonLine, onJsonLineLen: onJsonLineLen,
    onPositionGuard: onPositionGuard, onNanFrame: onNanFrame,
    snapshot: snapshot, report: report, start: start, stop: stop,
    reset: reset, exportJSON: exportJSON,
    get history() { return history; },
    RESYNC_THRESHOLD: RESYNC_THRESHOLD
  };

  root.MesqInstr = api;
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
})(typeof window !== 'undefined' ? window : globalThis);
