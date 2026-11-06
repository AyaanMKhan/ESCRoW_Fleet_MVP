(fn Escrow_RecoveryGoalActive ()
  (bind _loc (Transformation|GetActorLocation))
  (bind _goal (Variables|Default|GetRecoveryGoal))
  (return (and (< (Utilities|Time|GetGameTimeInSeconds) (Variables|Default|GetRecoveryGoalUntil)) (> (Math|Vector|Distance(Vector) _loc _goal) 300.0))))
