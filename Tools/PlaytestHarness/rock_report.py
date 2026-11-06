import json,glob,os,math,sys
p=os.path.dirname(os.path.abspath(__file__))
D=json.load(open(p+'/diag.json'))
R=max(glob.glob(os.path.expanduser('~/Documents/ESCRoW_Data/*/')),key=os.path.getmtime)
print(os.path.basename(R[:-1])[:32])
import collections
c=collections.Counter(); hits=[]
for l in open(R+'scenario_events.jsonl'):
    r=json.loads(l); t=r['EventType']
    if t.startswith('Autonomy') and 'Hold' not in t: c[(t[8:],r['AffectedFollowerID'])]+=1
    if t=='VehicleCollisionStart':
        pl=json.loads(r['PayloadJSON']); hits.append((round(r['ScenarioTimeSeconds'],1),r['AffectedFollowerID'],round(pl.get('VehicleSpeedCms',0))))
print('events',dict(c)); print('hits t/F/speed',hits)
for f in ('2','3','4'):
    rows=D.get(f,[])
    if not rows: print('F'+f,'no diag'); continue
    z=max(abs(r['v']) for r in rows); last=rows[-1]
    print(f"F{f} n{len(rows)} maxV {z:.0f} end {last['x']:.0f},{last['y']:.0f} toFinal {math.hypot(last['fx']-last['x'],last['fy']-last['y'])/100:.1f}m endV {last['v']:.0f} rev {sum(r['rev'] for r in rows)} maxAng {max(abs(r['ang']) for r in rows):.0f} blockedCost {sum(1 for r in rows if r['cost']>=1e5)}")
