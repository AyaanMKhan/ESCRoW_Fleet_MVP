(fn Escrow_HarnessWaypoint (Id X Y)
  (Class|BPESCROWConvoyTelemetryManager|EscrowApplyNavOrder :self (Variables|Default|GetConvoyMgr) :FollowerID Id :OrderType 4 :TargetVehicleID 0 :TargetLocation (Math|Vector|MakeVector X Y 120.0) :BreadcrumbIndex 0 :Reason "TestHarness"))
