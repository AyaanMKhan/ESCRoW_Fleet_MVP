exec(open('h_common.py').read())
g={'refPath':HB+':Escrow_HarnessScenario'}
for f in ['GetAllActorsWithTag','AcquireTakeoverLock','ReleaseTakeoverLock','AcknowledgeMissionReport','GetMissionReportCount','GetMissionReportByIndex','BreakESCROWMissionReport','GetCurrentPhaseName','GetESCROWScenarioDirectorSubsystem','String|Contains','Contains']:
    t=b.bp('find_node_types',{'graph':g,'type_id_filter':f,'context_pins':[]})
    try: v=json.loads(t)['returnValue']
    except: v=t
    print(f,[x for x in v if 'Cast' not in x][:5] if isinstance(v,list) else v[:160])
r=json.loads(b.bp('get_node_type_pins',{'graph':g,'type_id':'Utilities|Struct|BreakESCROWMissionReport'}))['returnValue']; print([x['name'] for x in r['output_pins']])
