import MySQLdb

def results_db_fetch(cursor,query):
    """used to fetch data from database
       and return a dictionary containing the required values

    """

    cursor.execute(query)
    data=cursor.fetchone()
    fetch={
           'tps':data[1],
           'min_req_lat_ms':data[2],
           'max_req_lat_ms':data[3],
           'avg_req_lat_ms':data[4],
           '95p_req_lat_ms':data[5],
           'rwreqps':data[6],
           'deadlocksps':data[7]
          }
    return fetch
    

def results_db_connect(dsn_string,operation,query):
    """used to establish a database connection
    
    """

    #getting the connection parameters
    connect_param=dsn_string.split(":")

    #establishing the connection
    connection=MySQLdb.connect(host=connect_param[0],user=connect_param[1],passwd=connect_param[2],db=connect_param[3],port=int(connect_param[4]))
    cursor=connection.cursor()
    sql=query

    #select operation - selects tests results from database and returns a dictionary
    if operation=="select":

        # returns a fetch value for the concurrency-iteration value which is not yet recorded
        try:
            fetch=results_db_fetch(cursor,query)
        except TypeError:
            fetch={
                   'tps':0.0,
                   'min_req_lat_ms':0.0,
                   'max_req_lat_ms':0.0,
                   'avg_req_lat_ms':0.0,
                   '95p_req_lat_ms':0.0,
                   'rwreqps':0.0,
                   'deadlocksps':0.0
                  }
        cursor.close()
        connection.close()
        return fetch

    #update operation - updates the database with the new test result
    elif operation=="insert" or operation=="delete":
        cursor.execute(query)
        connection.autocommit(True)
        cursor.close()
        connection.close()

