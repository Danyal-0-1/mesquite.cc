#!/usr/bin/env python3
"""KIN-01 FEASIBILITY: are foot stance phases detectable in existing captures?
If not, ZUPT is unavailable and B8's scope collapses. B8 item 1."""
import sys, math, glob, os
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from bvh_kin import parse, fk

print("="*76); print("KIN-01 FEASIBILITY - foot stance detection in existing captures"); print("="*76)

for path in sorted(glob.glob('Mesquite_benchmarks/data/mesquite_smooth/may8th2026/*.bvh')
                 + glob.glob('Mesquite_benchmarks/Mocab may 20th/*.bvh')):
    joints,motion,ft,_=parse(path)
    names=[j.name for j in joints]
    feet=[n for n in names if n in ('LeftFoot','RightFoot','LeftToeBase','RightToeBase')]
    if not feet: print("  no foot joints in", os.path.basename(path)); continue
    idx={n:names.index(n) for n in feet}
    traj={n:[] for n in feet}
    for f in motion:
        P,_=fk(joints,f)
        for n in feet: traj[n].append(P[idx[n]])
    print("\n"+os.path.basename(path)+"   frames=%d  dt=%.5f"%(len(motion),ft))
    for n in ('LeftFoot','RightFoot'):
        if n not in traj: continue
        p=traj[n]
        sp=[math.dist(p[i],p[i-1])/ft for i in range(1,len(p))]
        vy=[abs(p[i][1]-p[i-1][1])/ft for i in range(1,len(p))]
        ss=sorted(sp)
        # stance = speed below the 25th percentile of the session
        thr=ss[int(len(ss)*0.25)]
        # count contiguous runs below threshold lasting >=5 frames (~165 ms)
        runs=[];cur=0
        for v in sp:
            if v<thr: cur+=1
            else:
                if cur>=5: runs.append(cur)
                cur=0
        if cur>=5: runs.append(cur)
        hy=sorted(q[1] for q in p)
        print("   %-10s speed p25=%6.1f med=%6.1f p95=%7.1f u/s | height min=%6.1f med=%6.1f"
              %(n,thr,ss[len(ss)//2],ss[int(len(ss)*0.95)],hy[0],hy[len(hy)//2]))
        if runs:
            tot=sum(runs)
            print("              stance-like runs>=5f: %3d  total %4d frames (%.1f%% of session)  longest %d f (%.2f s)"
                  %(len(runs),tot,100*tot/len(sp),max(runs),max(runs)*ft))
        else:
            print("              NO stance-like runs >= 5 frames")
