import json,sys,importlib.util,pathlib
sp=importlib.util.spec_from_file_location('mcp',pathlib.Path(__file__).with_name('mcp.py')); m=importlib.util.module_from_spec(sp); sp.loader.exec_module(m)
T='editor_toolset.toolsets.blueprint.BlueprintTools'
def bp(tool,args,timeout=180):
    r=m.call('call_tool',{'toolset_name':T,'tool_name':tool,'arguments':args},timeout)
    res=r.get('result',r)
    out=''.join(c.get('text','') for c in res.get('content',[])) if isinstance(res,dict) else ''
    return out or json.dumps(r)
if __name__=='__main__':
    print(bp(sys.argv[1],json.loads(sys.argv[2])))
