import json,glob,os,csv,sys,math
R=max(glob.glob(os.path.expanduser('~/Documents/ESCRoW_Data/*/')),key=os.path.getmtime); print(os.path.basename(R[:-1])[:34])
for l in open(R+'scenario_events.jsonl'):
    r=json.loads(l); t=r['EventType']
    if (t.startswith('Autonomy') and 'Hold' not in t) or t in('VehicleCollisionStart','TakeoverRequested') or (t=='AlertPosted' and 'REPORT' not in r['EventLabel']):
        pl=''
        try:
            p=json.loads(r['PayloadJSON']); pl=f"{p.get('LocX',0):.0f},{p.get('LocY',0):.0f} v{p.get('ForwardSpeedCms',p.get('VehicleSpeedCms',0)):.0f}"
        except: pass
        print(f"{r['ScenarioTimeSeconds']:6.1f} F{r['AffectedFollowerID']} {t[:24]:24} {pl} {r['EventLabel'][:26] if t=='AlertPosted' else ''}")
rows=list(csv.DictReader(open(R+'fleet_heartbeat.csv')))
f=sys.argv[1] if len(sys.argv)>1 else 'F3'
for r in rows[::4]:
    try: x=json.loads(r['PayloadJSON'])['FleetTelemetry'][f]
    except: continue
    l=r[f+'Location'].split(','); c=x['Control']
    print(f"{float(r['ScenarioTimeSeconds']):6.1f} {float(l[0]):7.0f},{float(l[1]):7.0f} v{float(r[f+'SpeedCms']):4.0f} T{c['Throttle']:.2f} S{c['Steering']:+.2f} {x['AutonomyMode'][:6]}")
