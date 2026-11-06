import json,sys,importlib.util,pathlib,os,time,re
p=pathlib.Path(__file__).parent; os.chdir(p); sys.path.insert(0,str(p))
sp=importlib.util.spec_from_file_location('mcp',p/'mcp.py'); m=importlib.util.module_from_spec(sp); sp.loader.exec_module(m)
def call(ts,tool,args,t=120):
    r=m.call('call_tool',{'toolset_name':ts,'tool_name':tool,'arguments':args},t); res=r.get('result',r)
    return ''.join(c.get('text','') for c in res.get('content',[])) or json.dumps(r)
A='EditorToolset.EditorAppToolset'; O='editor_toolset.toolsets.object.ObjectTools'; Sc='editor_toolset.toolsets.scene.SceneTools'
LOG=os.path.expanduser('~/Library/Logs/Unreal Engine/ESCRoW_Fleet_MVPEditor/ESCRoW_Fleet_MVP.log')
MAP=sys.argv[1]; LIMIT=float(sys.argv[2]); SCEN=int(sys.argv[3]) if len(sys.argv)>3 else 0
cdo={'refPath':'/Game/ESCROW/Blueprints/Core/BP_ESCROW_GameMode.Default__BP_ESCROW_GameMode_C'}
print('switch on',call(O,'set_properties',{'instance':cdo,'values':json.dumps({'bRunTestHarness':True})})[:200])
hc={'refPath':'/Game/ESCROW/Blueprints/Test/BP_ESCROW_TestHarness.Default__BP_ESCROW_TestHarness_C'}
print('scen',call(O,'set_properties',{'instance':hc,'values':json.dumps({'ScenarioId':SCEN})})[:100])
if MAP not in call(Sc,'get_current_level',{}):
    print('load',call(Sc,'load_level',{'level_path':'/Game/ESCROW/Maps/'+MAP},180)[:200])
size0=os.path.getsize(LOG)
print('start',call(A,'StartPIE',{'options':{'warmupSeconds':2}},120)[:200])
t0=time.time(); status='timeout'
try:
    while time.time()-t0<LIMIT:
        time.sleep(4)
        with open(LOG,'rb') as f:
            f.seek(size0); tx=f.read().decode('utf8','ignore')
        if 'HARNESS DONE' in tx: status='done'; break
        if 'Fatal' in tx[-3000:]: status='fatal'; break
        if 'IsPIERunning' and time.time()-t0>10 and 'false' in call(A,'IsPIERunning',{}): status='pie-ended'; break
finally:
    print('status',status,round(time.time()-t0)); print('stop',call(A,'StopPIE',{},60)[:100])
    call(O,'set_properties',{'instance':hc,'values':json.dumps({'ScenarioId':0})})
    print('switch off',call(O,'set_properties',{'instance':cdo,'values':json.dumps({'bRunTestHarness':False})})[:120])
with open(LOG,'rb') as f:
    f.seek(size0); tx=f.read().decode('utf8','ignore')
open('last_run.log','w').write(tx)
print('harness lines:'); [print(' ',l[30:130]) for l in tx.splitlines() if 'HARNESS' in l][:0]
for l in [l for l in tx.splitlines() if 'HARNESS' in l][:24]: print(' ',l[:24],l.split(']')[-1][:80])
print('errors:',len(re.findall(r'Accessed None',tx)),[l[-140:] for l in tx.splitlines() if 'Accessed None' in l][:3])
