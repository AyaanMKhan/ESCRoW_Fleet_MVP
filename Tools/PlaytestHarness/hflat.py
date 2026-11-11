exec(open('h_common.py').read())
R={'refPath':HB}
for n,ty in [('Cruise','float'),('ActiveFault','int'),('FaultStage','int'),('FaultTime','float'),('bFaultDone2','bool'),('bFaultDone4','bool'),('bCanTakeFault','bool'),('FaultTarget','Vector'),('GatherCount','int'),('RouteDoneTime2','float'),('LastHarnessLog','float'),('bDriveOn','bool')]:
    print(n,b.bp('add_variable',{'blueprint':R,'name':n,'type_name':ty})[:60])
print(b.bp('add_object_variable',{'blueprint':R,'name':'FaultVehicle','object_class':{'refPath':FV}})[:60])
O='editor_toolset.toolsets.object.ObjectTools'
for g in ['Escrow_HarnessSetLeg','Escrow_HarnessChain','Escrow_HarnessWaypoint','Escrow_HarnessAck','Escrow_HarnessCheckFaults','Escrow_HarnessGatherCount','Escrow_HarnessScenarioFlat']:
    b.bp('add_function_graph',{'blueprint':R,'graph_name':g})
def P(g,n,t,inp=True): print(g,n,b.bp('add_function_param',{'graph':{'refPath':f'{HB}:{g}'},'param_name':n,'param_type':t,'input_param':inp})[:70])
P('Escrow_HarnessSetLeg','X','float');P('Escrow_HarnessSetLeg','Y','float')
P('Escrow_HarnessWaypoint','Id','int');P('Escrow_HarnessWaypoint','X','float');P('Escrow_HarnessWaypoint','Y','float')
P('Escrow_HarnessAck','Sub','string');P('Escrow_HarnessAck','bFound','bool',False)
print('compile0',b.bp('compile_blueprint',{'blueprint':R,'warnings_as_errors':False})[:200])
for g,f in [('Escrow_HarnessSetLeg','f_leg'),('Escrow_HarnessChain','f_chain'),('Escrow_HarnessWaypoint','f_wp'),('Escrow_HarnessAck','f_ack'),('Escrow_HarnessGatherCount','f_gather'),('Escrow_HarnessCheckFaults','f_faults'),('Escrow_HarnessDrive','h_drive'),('Escrow_HarnessScenarioFlat','f_flat'),('Escrow_HarnessScenario','h_scen')]:
    r=b.bp('write_graph_dsl',{'graph':{'refPath':f'{HB}:{g}'},'code':open('HarnessDsl/'+f+'.dsl').read()}); print(g,r[:400])
print('compile',b.bp('compile_blueprint',{'blueprint':R,'warnings_as_errors':True})[:500])
