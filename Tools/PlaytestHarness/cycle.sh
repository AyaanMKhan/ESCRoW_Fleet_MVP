#!/bin/bash
# cycle.sh <name> : one scenario 3 run and report
python3 -I runtest.py MAP_ESCROW_DESERT_INTEGRATION_TEST 140 3 | tail -1 | cut -c1-60 && python3 -I analyze.py >/dev/null && python3 -I rock_report.py && cp last_run.log rock_$1.log && cp diag.json rock_$1.json
