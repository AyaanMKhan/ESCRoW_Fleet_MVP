import json,sys,importlib.util,pathlib,os,subprocess,time
p=pathlib.Path(__file__).parent; os.chdir(p); sys.path.insert(0,str(p))
sp=importlib.util.spec_from_file_location('mcp',p/'mcp.py'); m=importlib.util.module_from_spec(sp); sp.loader.exec_module(m)
def call(ts,tool,args,t=170):
    r=m.call('call_tool',{'toolset_name':ts,'tool_name':tool,'arguments':args},t); res=r.get('result',r)
    return ''.join(c.get('text','') for c in res.get('content',[])) or json.dumps(r)
AT='editor_toolset.toolsets.asset.AssetTools'; Sc='editor_toolset.toolsets.scene.SceneTools'; O='editor_toolset.toolsets.object.ObjectTools'
ROOT='/Users/ayaankhan/UnrealProjects/ESCRoW_Fleet_MVP'
FOL='/Game/ESCROW/Blueprints/Followers/BP_ESCROW_FollowerVehicle'; HAR='/Game/ESCROW/Blueprints/Test/BP_ESCROW_TestHarness'; GMP='/Game/ESCROW/Blueprints/Core/BP_ESCROW_GameMode'
def dirty(a):
    r=call(AT,'is_dirty',{'asset_path':a}); return 'true' in r.lower()
d=json.loads(''.join(c.get('text','') for c in m.call('describe_toolset',{'toolset_name':AT})['result']['content']))
for x in d.get('tools',d):
    if x['name'].endswith('is_dirty'): print(json.dumps(x['inputSchema'])[:200]); key=list(x['inputSchema']['properties'].keys())[0]
def dirty(a):
    r=call(AT,'is_dirty',{key:a if 'path' in key else {'refPath':a+'.'+a.rsplit('/',1)[1]}}); return r
pie=call('EditorToolset.EditorAppToolset','IsPIERunning',{}); print('PIE',pie)
if 'false' not in pie.lower():
    print('PIE RUNNING: not restarting'); sys.exit(5)
s={a:dirty(a) for a in (FOL,HAR,GMP)}; print(s)
if 'true' in s[FOL].lower():
    print('FOLLOWER DIRTY: not restarting'); sys.exit(3)
print('switch',call(O,'get_properties',{'instance':{'refPath':GMP+'.Default__BP_ESCROW_GameMode_C'},'properties':['bRunTestHarness']}))
print('save',call(AT,'save_assets',{'asset_paths':[HAR,GMP]}))
print('menu',call(Sc,'load_level',{'level_path':'/Game/ESCROW/Maps/MAP_ESCROW_MAIN_MENU'})[:90])
for a in ('/Game/ESCROW/Maps/MAP_ESCROW_DESERT_INTEGRATION_TEST','/Game/ESCROW/Maps/MAP_ESCROW_MAIN_MENU',GMP,HAR,FOL): print(a.rsplit('/',1)[1],dirty(a)[:50])
subprocess.run(['osascript','-e','tell application "UnrealEditor" to quit'],timeout=60)
t=time.time()
while time.time()-t<90:
    if subprocess.run(['pgrep','-f','UnrealEditor.app'],capture_output=True).returncode!=0: break
    time.sleep(3)
else:
    print('EDITOR DID NOT QUIT in 90s; not forcing'); sys.exit(4)
print('quit ok',round(time.time()-t))
print(subprocess.run(['git','-C',ROOT,'status','--short','--','Content/ESCROW/Blueprints/Core','Content/ESCROW/Blueprints/Test','Content/ESCROW/Maps'],capture_output=True,text=True).stdout)
subprocess.run(['git','-C',ROOT,'checkout','--','Content/ESCROW/Blueprints/Core/BP_ESCROW_GameMode.uasset'])
try: os.remove('mcp_session.txt')
except FileNotFoundError: pass
subprocess.run(['open','-n','/Applications/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app','--args',ROOT+'/ESCRoW_Fleet_MVP.uproject','-ExecCmds=ModelContextProtocol.StartServer'])
t=time.time()
while time.time()-t<420:
    r=subprocess.run(['curl','-s','-m','3','-o','/dev/null','-w','%{http_code}','http://127.0.0.1:8000/mcp'],capture_output=True,text=True).stdout
    if r not in('000',''): print('mcp up',r,round(time.time()-t)); break
    time.sleep(5)
else:
    print('MCP DID NOT COME UP'); sys.exit(5)
time.sleep(15)
print('level',call(Sc,'get_current_level',{}))
print('menu',call(Sc,'load_level',{'level_path':'/Game/ESCROW/Maps/MAP_ESCROW_MAIN_MENU'})[:90])
