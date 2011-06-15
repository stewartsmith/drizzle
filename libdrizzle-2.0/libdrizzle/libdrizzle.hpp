#pragma once

#include <libdrizzle/libdrizzle.h>

namespace drizzle {

class bad_query : public std::runtime_error
{
};

class drizzle_c
{
public:
  drizzle_c()
  {
    drizzle_create(&b_);
  }

  ~drizzle_c()
  {
    drizzle_free(&b_);
  }

  drizzle_st b_;
};

class result_c
{
public:
  result_c()
  {
    memset(&b_, 0, sizeof(b_));
  }

  ~result_c()
  {
    drizzle_result_free(&b_);
  }

  uint16_t column_count()
  {
    return drizzle_result_column_count(&b_);    
  }

  uint64_t row_count()
  {
    return drizzle_result_row_count(&b_);
  }

  drizzle_row_t row_next()
  {
    return drizzle_row_next(&b_);
  }

  drizzle_result_st b_;
};

class connection_c
{
public:
  explicit connection_c(drizzle_c& drizzle)
  {
    drizzle_con_create(&drizzle.b_, &b_);
  }

  ~connection_c()
  {
    drizzle_con_free(&b_);
  }

  drizzle_return_t query(result_c& result, const char* query)
  {
    drizzle_return_t ret;
    drizzle_query_str(&b_, &result.b_, query, &ret);
    if (ret == DRIZZLE_RETURN_OK)
      ret = drizzle_result_buffer(&result.b_);
    return ret;
  }

  drizzle_con_st b_;
};

}
