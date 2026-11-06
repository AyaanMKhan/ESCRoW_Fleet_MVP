import json,sys,urllib.request,pathlib
URL='http://127.0.0.1:8000/mcp'; SF=pathlib.Path(__file__).with_name('mcp_session.txt')
def post(body,sid=None,timeout=120):
    h={'Content-Type':'application/json','Accept':'application/json, text/event-stream'}
    if sid: h['Mcp-Session-Id']=sid
    r=urllib.request.urlopen(urllib.request.Request(URL,json.dumps(body).encode(),h),timeout=timeout)
    t=r.read().decode(); sid2=r.headers.get('Mcp-Session-Id')
    if 'data:' in t[:200] or t.startswith('event'):
        t=[l[5:] for l in t.splitlines() if l.startswith('data:')][-1]
    return (json.loads(t) if t.strip() else None),sid2
def session():
    if SF.exists(): return SF.read_text().strip() or None
    r,sid=post({'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2025-03-26','capabilities':{},'clientInfo':{'name':'claude-code-curl','version':'1'}}})
    SF.write_text(sid or '')
    try: post({'jsonrpc':'2.0','method':'notifications/initialized'},sid)
    except Exception: pass
    print('init',json.dumps(r)[:300],file=sys.stderr); return sid
def call(name,args,timeout=120):
    sid=session()
    r,_=post({'jsonrpc':'2.0','id':2,'method':'tools/call','params':{'name':name,'arguments':args}},sid,timeout)
    return r
if __name__=='__main__':
    name=sys.argv[1]; args=json.loads(sys.argv[2]) if len(sys.argv)>2 else {}
    r=call(name,args)
    res=r.get('result',r)
    for c in res.get('content',[]) if isinstance(res,dict) else []:
        print(c.get('text',c))
    if not (isinstance(res,dict) and res.get('content')): print(json.dumps(r)[:3000])
