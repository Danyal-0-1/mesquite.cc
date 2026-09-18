#!/usr/bin/env python3
"""Minimal BVH parser + forward kinematics. Shared by the Phase 2 offline analyses."""
import math

class Joint:
    __slots__=('name','parent','offset','channels','cidx','children')
    def __init__(s,name,parent):
        s.name=name; s.parent=parent; s.offset=(0.,0.,0.)
        s.channels=[]; s.cidx=[]; s.children=[]

def parse(path):
    lines=open(path,errors='replace').read().splitlines()
    joints=[]; stack=[]; cur=None; ci=0; i=0; ft=None; nf=None
    while i<len(lines):
        t=lines[i].strip(); p=t.split()
        if p and p[0] in ('ROOT','JOINT'):
            j=Joint(p[1], stack[-1] if stack else None)
            joints.append(j)
            if stack is not None and stack: joints[stack[-1]].children.append(len(joints)-1)
            stack.append(len(joints)-1); cur=j
        elif p and p[0]=='End':
            stack.append(None)
        elif p and p[0]=='OFFSET' and stack and stack[-1] is not None:
            joints[stack[-1]].offset=(float(p[1]),float(p[2]),float(p[3]))
        elif p and p[0]=='CHANNELS' and stack and stack[-1] is not None:
            n=int(p[1]); ch=p[2:2+n]
            j=joints[stack[-1]]; j.channels=ch
            j.cidx=list(range(ci,ci+n)); ci+=n
        elif t=='}':
            if stack: stack.pop()
        elif t.startswith('Frames:'): nf=int(t.split(':')[1])
        elif t.startswith('Frame Time:'):
            ft=float(t.split(':')[1]); i+=1; break
        i+=1
    motion=[list(map(float,l.split())) for l in lines[i:] if l.strip() and not l.strip().startswith(';')]
    return joints, motion, ft, ci

def _rot(axis,deg):
    r=math.radians(deg); c,s=math.cos(r),math.sin(r)
    if axis=='X': return ((1,0,0),(0,c,-s),(0,s,c))
    if axis=='Y': return ((c,0,s),(0,1,0),(-s,0,c))
    return ((c,-s,0),(s,c,0),(0,0,1))

def _mm(a,b):
    return tuple(tuple(sum(a[i][k]*b[k][j] for k in range(3)) for j in range(3)) for i in range(3))

def _mv(m,v):
    return tuple(sum(m[i][k]*v[k] for k in range(3)) for i in range(3))

def fk(joints, frame):
    """Global positions and rotation matrices per joint. Honours each joint's
    DECLARED channel order, as compare_bvh_suits.py does."""
    P=[None]*len(joints); R=[None]*len(joints)
    for idx,j in enumerate(joints):
        t=list(j.offset); rot=((1,0,0),(0,1,0),(0,0,1))
        haspos=any(c.lower().endswith('position') for c in j.channels)
        if j.parent is None and haspos: t=[0.,0.,0.]
        for ch,k in zip(j.channels,j.cidx):
            v=frame[k]; ax=ch[0].upper()
            if ch.lower().endswith('position'):
                m={'X':0,'Y':1,'Z':2}[ax]
                if j.parent is None and haspos: t[m]=v
                else: t[m]+=v
            else:
                rot=_mm(rot,_rot(ax,v))
        if j.parent is None:
            P[idx]=tuple(t); R[idx]=rot
        else:
            pp,pr=P[j.parent],R[j.parent]
            off=_mv(pr,tuple(t))
            P[idx]=(pp[0]+off[0],pp[1]+off[1],pp[2]+off[2])
            R[idx]=_mm(pr,rot)
    return P,R
