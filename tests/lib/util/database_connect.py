import MySQLdb

def results_db_fetch(cursor,query):
    """used to fetch data from database
       and return a dictionary containing the required values

    """

    cursor.execute(query)
    data=cursor.fetchone()
    fetch={'tps':data[1],
               'min_req_lat_ms':data[2],
               'max_req_lat_ms':data[3],
               'avg_req_lat_ms':data[4],
               '95p_req_lat_ms':data[5],
               'rwreqps':data[6],
               'deadlocksps':data[7]}
    return fetch
    

def results_db_connect(operation,query):
    """used to establish a database connection
    
    """

    connection=MySQLdb.connect(host='127.0.0.1',user='root',passwd="",db="results_db",port=3306)
    cursor=connection.cursor()
    sql=query

    #select operation - selects tests results from database and returns a dictionary
    if operation=="select":
        fetch=results_db_fetch(cursor,query)
        cursor.close()
        connection.close()
        return fetch

    #update operation - updates the database with the new test result
    elif operation=="update":
        cursor.execute(query)
        connection.autocommit(True)
        cursor.close()
        connection.close()
