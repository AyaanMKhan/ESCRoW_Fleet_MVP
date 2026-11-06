(fn Escrow_ApplyRecoverySteer ()
  (if (< (Utilities|Time|GetGameTimeInSeconds) (Variables|Default|GetRecoveryReverseUntil))
    (Game|Components|ChaosVehicleMovement|SetSteeringInput (Variables|Vehicle|GetVehicleMovementComponent) (Variables|Default|GetRecoverySteer))))
