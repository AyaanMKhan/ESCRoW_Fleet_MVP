import re,sys,json,os,glob,collections,math
tx=open('last_run.log').read().splitlines()
pat=re.compile(r'DIAG F(\d) t=([\d.]+) loc=X=(-?[\d.]+) Y=(-?[\d.]+) Z=(-?[\d.]+) yaw=(-?[\d.]+) final=X=(-?[\d.]+) Y=(-?[\d.]+) Z=-?[\d.]+ pursuit=X=(-?[\d.]+) Y=(-?[\d.]+) Z=-?[\d.]+ selAng=(-?[\d.]+) selDist=(-?[\d.]+) selCost=(-?[\d.]+) speed=(-?[\d.]+) hold=(\w+) rev=(\w+) path=(\d+) idx=(\d+)')
D=collections.defaultdict(list)
for l in tx:
    m=pat.search(l)
    if m:
        g=m.groups(); D[int(g[0])].append(dict(t=float(g[1]),x=float(g[2]),y=float(g[3]),yaw=float(g[5]),fx=float(g[6]),fy=float(g[7]),px=float(g[8]),py=float(g[9]),ang=float(g[10]),dist=float(g[11]),cost=float(g[12]),v=float(g[13]),hold=g[14]=='true',rev=g[15]=='true',path=int(g[16]),idx=int(g[17])))
print({k:len(v) for k,v in D.items()}, [l[-120:] for l in tx if 'DIAG' in l][:1])
R=max(glob.glob(os.path.expanduser('~/Documents/ESCRoW_Data/*/')),key=os.path.getmtime); print(os.path.basename(R[:-1])[:44])
c=collections.Counter()
for l in open(R+'scenario_events.jsonl'):
    r=json.loads(l); t=r['EventType']
    if t.startswith('Autonomy') or t in('VehicleCollisionStart','TakeoverRequested','AlertPosted'): c[(t,r['AffectedFollowerID'])]+=1
for k,v in sorted(c.items()): print(v,k)
for f,rows in sorted(D.items()):
    drv=[r for r in rows if not r['hold']]
    lock=[r for r in drv if abs(r['ang'])>=30]
    # total turned
    turn=0
    for a,b in zip(drv,drv[1:]):
        d=(b['yaw']-a['yaw']+180)%360-180; turn+=d
    print(f"F{f} samples {len(rows)} driving {len(drv)} |ang|>=30:{len(lock)} net-turn {turn:.0f} deg, last {rows[-1]['x']:.0f},{rows[-1]['y']:.0f} v{rows[-1]['v']:.0f}")
json.dump({str(k):v for k,v in D.items()},open('diag.json','w'))
