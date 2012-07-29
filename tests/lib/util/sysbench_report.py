def getSysbenchReport(run,fetch):
    """returns the report of the last sysbench test executed"""

    report="""==============================
  SYSBENCH REGRESSION REPORT
==============================
CONCURRENCY    :  %d
ITERATIONS     :  %d
MODE           :  %s
TPS            :  %f      %f
MIN_REQ_LAT_MS :  %f      %f
MAX_REQ_LAT_MS :  %f      %f
AVG_REQ_LAT_MS :  %f      %f
95P_REQ_LAT_MS :  %f      %f
RWREQPS        :  %f      %f
DEADLOCKSPS    :  %f      %f
=============================
          """ % (fetch['concurrency'],
                 fetch['iteration']+1,
                 run['mode'],
                 run['tps'],           run['tps']-fetch['tps'],
                 run['min_req_lat_ms'],run['min_req_lat_ms']-fetch['min_req_lat_ms'],
                 run['max_req_lat_ms'],run['max_req_lat_ms']-fetch['max_req_lat_ms'],
                 run['avg_req_lat_ms'],run['avg_req_lat_ms']-fetch['avg_req_lat_ms'],
                 run['95p_req_lat_ms'],run['95p_req_lat_ms']-fetch['95p_req_lat_ms'],
                 run['rwreqps'],       run['rwreqps']-fetch['rwreqps'],
                 run['deadlocksps'],   run['deadlocksps']-fetch['deadlocksps']
                )
    return report
