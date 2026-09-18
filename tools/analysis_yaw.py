#!/usr/bin/env python3
"""EST-01 / EST-02 / KIN-02 falsifying test, run OFFLINE.

Phase 1 claimed the dominant error is a whole-body heading failure, and Phase 2
lists U3 (drift vs initialisation) as needing a hardware static session (M4).
This computes the PER-FRAME optimal yaw alignment between Mesquite and Rokoko.
  - flat, nonzero  -> uncorrected initial heading (initialisation)
  - steady ramp    -> drift
  - collapse of global error toward pose error after alignment -> EST-02 confirmed
"""
import sys, os, math, json
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from bvh_kin import parse, fk

MAP=[("Hips","pelvis"),("Spine","spine_01"),("Spine1","spine_02"),("Spine2","spine_03"),
     ("Neck","neck_01"),("Head","head"),
     ("RightUpLeg","thigh_r"),("RightLeg","calf_r"),("RightFoot","foot_r"),("RightToeBase","ball_r"),
     ("LeftUpLeg","thigh_l"),("LeftLeg","calf_l"),("LeftFoot","foot_l"),("LeftToeBase","ball_l")]

SESS=[("Mesquite_benchmarks/data/mesquite_smooth/may8th2026/MMcap_bvh_2026-6-8-13-20-59.bvh",
       "Mesquite_benchmarks/data/rokoko/may8th2026/MesBench1_2_ue5.bvh",
       "01", -9),
      ("Mesquite_benchmarks/data/mesquite_smooth/may8th2026/MMcap_bvh_2026-6-8-13-22-52.bvh",
       "Mesquite_benchmarks/data/rokoko/may8th2026/MesBench1_3_ue5.bvh",
       "02", -66)]

def series(path, wanted):
    j,m,ft,_=parse(path); names=[x.name for x in j]
    idx=[names.index(w) if w in names else None for w in wanted]
    out=[]
    for f in m:
        P,_=fk(j,f)
        out.append([P[i] if i is not None else None for i in idx])
    return out, ft, [w for w,i in zip(wanted,idx) if i is None]

print("="*78)
print("EST-01/EST-02 PER-FRAME YAW ALIGNMENT  (offline surrogate for U3)")
print("="*78)

for mp, rp, tag, lag in SESS:
    M,ftm,missM = series(mp,[a for a,_ in MAP])
    R,ftr,missR = series(rp,[b for _,b in MAP])
    if missM or missR:
        print("  missing joints:",missM,missR)
    TRIM=5
    n=min(len(M)-TRIM, len(R)-TRIM-abs(lag))
    yaws=[]; errs_raw=[]; errs_al=[]
    for k in range(n):
        a=M[k+TRIM]; b=R[k+TRIM+max(0,-lag)]
        if a[0] is None or b[0] is None: continue
        ax,ay,az=a[0]; bx,by,bz=b[0]
        pm=[(p[0]-ax,p[1]-ay,p[2]-az) for p in a if p]
        pr=[(p[0]-bx,p[1]-by,p[2]-bz) for p in b if p]
        if len(pm)!=len(pr): continue
        # uniform scale from Phase 1 metrics
        s=1.1022657457679133
        pr=[(x/s,y/s,z/s) for x,y,z in pr]
        A=sum(m[0]*r[0]+m[2]*r[2] for m,r in zip(pm,pr))
        B=sum(m[2]*r[0]-m[0]*r[2] for m,r in zip(pm,pr))
        th=math.atan2(B,A)
        yaws.append(math.degrees(th))
        c,si=math.cos(th),math.sin(th)
        e0=math.sqrt(sum((m[0]-r[0])**2+(m[1]-r[1])**2+(m[2]-r[2])**2 for m,r in zip(pm,pr))/len(pm))
        e1=0.0
        for m,r in zip(pm,pr):
            rx=m[0]*c+m[2]*si; rz=-m[0]*si+m[2]*c
            e1+=(rx-r[0])**2+(m[1]-r[1])**2+(rz-r[2])**2
        e1=math.sqrt(e1/len(pm))
        errs_raw.append(e0); errs_al.append(e1)

    if not yaws: print("  session",tag,": no usable frames"); continue
    # unwrap
    uw=[yaws[0]]
    for v in yaws[1:]:
        d=v-uw[-1]
        while d>180: v-=360; d=v-uw[-1]
        while d<-180: v+=360; d=v-uw[-1]
        uw.append(v)
    N=len(uw); dur=N*ftm
    mean=sum(uw)/N
    var=sum((v-mean)**2 for v in uw)/N
    # least-squares slope (deg/s)
    tm=(N-1)/2.0
    num=sum((i-tm)*(uw[i]-mean) for i in range(N)); den=sum((i-tm)**2 for i in range(N))
    slope=(num/den)/ftm if den else 0.0
    first=sum(uw[:N//10])/max(1,N//10); last=sum(uw[-N//10:])/max(1,N//10)
    print("\n--- session %s   frames=%d  duration=%.1f s ---"%(tag,N,dur))
    print("  per-frame optimal yaw : mean=%8.2f deg   sd=%6.2f   range=[%.1f, %.1f]"
          %(mean,math.sqrt(var),min(uw),max(uw)))
    print("  first 10%% mean=%8.2f   last 10%% mean=%8.2f   change=%+7.2f deg over %.0f s"
          %(first,last,last-first,dur))
    print("  drift slope           : %+.4f deg/s   (=> %+.1f deg over 10 min)"%(slope,slope*600))
    print("  RMS joint error       : before yaw alignment %7.2f   after %7.2f   (%.0f%% reduction)"
          %(sum(errs_raw)/len(errs_raw), sum(errs_al)/len(errs_al),
            100*(1-(sum(errs_al)/len(errs_al))/(sum(errs_raw)/len(errs_raw)))))
