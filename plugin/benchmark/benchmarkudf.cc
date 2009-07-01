/* 
 -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 */

#include <drizzled/server_includes.h>
#include <drizzled/error.h>
#include <drizzled/session.h>

using namespace std;

class Item_func_benchmark :public Item_int_func
{
public:
//  Item_func_benchmark(Item *count_expr, Item *expr) :Item_int_func(count_expr, expr)  {}
  Item_func_benchmark() :Item_int_func() {}
  int64_t val_int();
  virtual void print(String *str, enum_query_type query_type);

  const char *func_name() const      { return "benchmark"; }
  void fix_length_and_dec()          { max_length=1; maybe_null=0; }
  bool check_argument_count(int n)   { return (n==2); }
};


/* This function is just used to test speed of different functions */
int64_t Item_func_benchmark::val_int()
{
  assert(fixed == 1);
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff), &my_charset_bin);
  my_decimal tmp_decimal;
  Session *session=current_session;
  uint64_t loop_count;

  loop_count= (uint64_t) args[0]->val_int();

  if (args[0]->null_value ||
      (!args[0]->unsigned_flag && (((int64_t) loop_count) < 0)))
  {
    if (!args[0]->null_value)
    {
      llstr(((int64_t) loop_count), buff);
      push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                          ER_WRONG_VALUE_FOR_TYPE, ER(ER_WRONG_VALUE_FOR_TYPE),
                          "count", buff, "benchmark");
    }

    null_value= 1;
    return 0;
  }

  null_value=0;
  for (uint64_t loop=0 ; loop < loop_count && !session->killed; loop++)
  {
    switch (args[1]->result_type()) {
    case REAL_RESULT:
      (void) args[1]->val_real();
      break;
    case INT_RESULT:
      (void) args[1]->val_int();
      break;
    case STRING_RESULT:
      (void) args[1]->val_str(&tmp);
      break;
    case DECIMAL_RESULT:
      (void) args[1]->val_decimal(&tmp_decimal);
      break;
    case ROW_RESULT:
    default:
      // This case should never be chosen
      assert(0);
      return 0;
    }
  }
  return 0;
}

void Item_func_benchmark::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("benchmark("));
  args[0]->print(str, query_type);
  str->append(',');
  args[1]->print(str, query_type);
  str->append(')');
}

Create_function<Item_func_benchmark> benchmarkudf(string("benchmark"));

static int initialize(PluginRegistry &registry)
{
  registry.add(&benchmarkudf);
  return 0;
}

drizzle_declare_plugin(benchmark)
{
  "benchmark",
  "0.1",
  "Devananda van der Veen",
  "Measure time for repeated calls to a function.",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  NULL,   /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
