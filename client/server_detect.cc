#include "client/client_priv.h"
#include "client/server_detect.h"
#include <iostream>

ServerDetect::ServerDetect(drizzle_con_st *connection) :
  type(SERVER_UNKNOWN_FOUND),
  version("")
{
  version= drizzle_con_server_version(connection);
  
  const char *safe_query = "SHOW VARIABLES LIKE 'vc_%'";
  drizzle_result_st* result= new drizzle_result_st;
  drizzle_return_t ret_ptr;
  drizzle_query_str(connection, result, safe_query, &ret_ptr);
  uint64_t num_of_rows;

  if(ret_ptr == DRIZZLE_RETURN_OK)
  {
    ret_ptr = drizzle_result_buffer(result);
    num_of_rows = drizzle_result_row_count(result);
    if(num_of_rows > 0)
    {
      type = SERVER_DRIZZLE_FOUND;
    }
    else if(num_of_rows == 0)
    {
      type = SERVER_MYSQL_FOUND;
    }
  }
  else
  {
    std::cerr << "Server version not detectable. Assuming MySQL." << std::endl;
    type= SERVER_MYSQL_FOUND;
  }

  drizzle_result_free(result);    
}                              
