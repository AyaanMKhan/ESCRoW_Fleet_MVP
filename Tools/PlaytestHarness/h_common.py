import json,sys,importlib.util,pathlib,os
p=pathlib.Path(__file__).parent; os.chdir(p); sys.path.insert(0,str(p))
sp=importlib.util.spec_from_file_location('bpm',p/'bp.py'); b=importlib.util.module_from_spec(sp); sp.loader.exec_module(b)
HB='/Game/ESCROW/Blueprints/Test/BP_ESCROW_TestHarness.BP_ESCROW_TestHarness'
GM='/Game/ESCROW/Blueprints/Core/BP_ESCROW_GameMode.BP_ESCROW_GameMode'
FV='/Game/ESCROW/Blueprints/Followers/BP_ESCROW_FollowerVehicle.BP_ESCROW_FollowerVehicle_C'
CV='/Game/ESCROW/Blueprints/Vehicles/BP_ESCROW_CommandVehicle.BP_ESCROW_CommandVehicle_C'
CT='/Game/ESCROW/Blueprints/Scenario/BP_ESCROW_ConvoyTelemetryManager.BP_ESCROW_ConvoyTelemetryManager_C'
