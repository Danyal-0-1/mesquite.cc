#!/usr/bin/env python3
"""
Phase 2 offline analyses (B9 items 2-4, B10 diagnostics).
Runs against EXISTING captures only - no hardware required.
Produces [fact-data] results. Read-only; writes nothing into the repo.
"""
import sys, math, json, glob, os

def load_bvh(path):
    txt = open(path, errors='replace').read().splitlines()
    ft, nframes, mi = None, None, None
    joints, chan_names, stack = [], [], []
    for n, line in enumerate(txt):
        s = line.strip()
        if s.startswith('ROOT ') or s.startswith('JOINT '):
            stack.append(s.split(None, 1)[1]); joints.append(s.split(None, 1)[1])
        elif s.startswith('CHANNELS'):
            p = s.split()
            for c in p[2:2+int(p[1])]:
                chan_names.append((stack[-1] if stack else '?', c))
        elif s == '}' and stack:
            stack.pop()
        elif s.startswith('Frames:'):
            nframes = int(s.split(':')[1])
        elif s.startswith('Frame Time:'):
            ft = float(s.split(':')[1]); mi = n + 1; break
    motion = [list(map(float, l.split())) for l in txt[mi:] if l.strip()]
    return dict(path=path, frame_time=ft, frames=nframes, joints=joints,
                channels=chan_names, motion=motion)

def root_analysis(b):
    """B9-2: root displacement per frame; B9-4: gimbal proximity."""
    m = b['motion']; ft = b['frame_time']
    ch = b['channels']
    xi = [i for i,(j,c) in enumerate(ch[:6]) if c=='Xposition']
    if not xi: return None
    x0 = xi[0]
    # find root rotation channel offsets and order
    rot = [(i,c) for i,(j,c) in enumerate(ch[:6]) if c.endswith('rotation')]
    disp, gim, spikes = [], 0, 0
    prevXZ = None
    prev_rot = None
    for f in m:
        p = (f[x0], f[x0+1], f[x0+2])
        if prevXZ is not None:
            d = math.dist(p, prevXZ)
            disp.append(d)
        prevXZ = p
        # middle euler angle of the declared order = gimbal axis for that order
        rv = [f[i] for i,_ in rot]
        if len(rv) == 3:
            mid = rv[1]
            if abs(abs(mid) - 90.0) < 10.0:
                gim += 1
                if prev_rot is not None:
                    d13 = abs(rv[0]-prev_rot[0]) + abs(rv[2]-prev_rot[2])
                    if d13 > 90.0: spikes += 1
            prev_rot = rv
    disp.sort()
    n = len(disp)
    return dict(frames=len(m), frame_time=ft,
                disp_median=disp[n//2] if n else 0,
                disp_p99=disp[int(n*0.99)] if n else 0,
                disp_max=disp[-1] if n else 0,
                speed_p99_units_s=(disp[int(n*0.99)]/ft) if n and ft else 0,
                speed_max_units_s=(disp[-1]/ft) if n and ft else 0,
                gimbal_frames=gim, gimbal_pct=100.0*gim/max(1,len(m)),
                rot_order=''.join(c[0] for _,c in rot),
                xz_spikes_near_gimbal=spikes)

print("="*78)
print("B9 OFFLINE ANALYSES - existing captures, no hardware")
print("="*78)
files = sorted(glob.glob('Mesquite_benchmarks/data/mesquite_smooth/may8th2026/*.bvh')
             + glob.glob('Mesquite_benchmarks/Mocab may 20th/*.bvh'))
rok = sorted(glob.glob('Mesquite_benchmarks/data/rokoko/may8th2026/*.bvh'))
for p in files + rok:
    b = load_bvh(p)
    r = root_analysis(b)
    tag = 'ROKOKO' if 'rokoko' in p else 'MESQ  '
    print(f"\n[{tag}] {os.path.basename(p)}")
    print(f"   frames={r['frames']}  frame_time={r['frame_time']}  root_rot_order={r['rot_order']}")
    print(f"   root disp/frame: median={r['disp_median']:.3f}  p99={r['disp_p99']:.3f}  max={r['disp_max']:.3f} units")
    print(f"   implied root speed: p99={r['speed_p99_units_s']:.1f}  max={r['speed_max_units_s']:.1f} units/s")
    print(f"   PHN-05 gimbal: {r['gimbal_frames']} frames within 10deg of +-90 ({r['gimbal_pct']:.1f}%), "
          f"X/Z spikes there: {r['xz_spikes_near_gimbal']}")
