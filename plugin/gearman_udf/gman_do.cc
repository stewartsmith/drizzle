/* Copyright (C) 2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <config.h>
#include "gman_do.h"
#include "function_map.h"

using namespace std;
using namespace drizzled;

extern "C"
{
  static void *_do_malloc(size_t size, void *arg)
  {
    Item_func_gman_do *item= (Item_func_gman_do *)arg;
    return item->realloc(size);
  }
}

Item_func_gman_do::~Item_func_gman_do()
{
  if (options & GMAN_DO_OPTIONS_CLIENT)
    gearman_client_free(&client);
}

String *Item_func_gman_do::val_str(String *str)
{
  String *function;
  String *res;
  const char *unique;
  const char *workload;
  size_t workload_size;
  size_t result_size;
  gearman_return_t ret;
  char job_handle[GEARMAN_JOB_HANDLE_SIZE];

  if (arg_count < 1 || arg_count > 3 || !(function= args[0]->val_str(str)))
  {
    null_value= 1;
    return NULL;
  }

  if (arg_count > 1 && (res= args[1]->val_str(str)) != NULL)
  {
    workload= res->ptr();
    workload_size= res->length();
  }
  else
  {
    workload= NULL;
    workload_size= 0;
  }

  if (arg_count == 3 && (res= args[2]->val_str(str)) != NULL)
    unique= res->ptr();
  else
    unique= NULL;

  if (!(options & GMAN_DO_OPTIONS_CLIENT))
  {
    if (!GetFunctionMap().get(string(function->ptr()), &client))
    {
      null_value= 1;
      return NULL;
    }

    gearman_client_set_workload_malloc_fn(&client, _do_malloc, this);
    options= (gman_do_options_t)(options | GMAN_DO_OPTIONS_CLIENT);
  }

  if (options & GMAN_DO_OPTIONS_BACKGROUND)
  {
    if (options & GMAN_DO_OPTIONS_HIGH)
    {
      ret= gearman_client_do_high_background(&client, function->ptr(), unique,
                                             workload, workload_size,
                                             job_handle);
    }
    else if (options & GMAN_DO_OPTIONS_LOW)
    {
      ret= gearman_client_do_low_background(&client, function->ptr(), unique,
                                            workload, workload_size,
                                            job_handle);
    }
    else
    {
      ret= gearman_client_do_background(&client, function->ptr(), unique,
                                        workload, workload_size, job_handle);
    }

    if (ret == GEARMAN_SUCCESS)
    {
      result_size= strlen(job_handle);
      buffer.realloc(result_size);
      buffer.length(result_size);
      memcpy(buffer.ptr(), job_handle, result_size);
    }
  }
  else
  {
    if (options & GMAN_DO_OPTIONS_HIGH)
    {
      (void) gearman_client_do_high(&client, function->ptr(), unique, workload,
                                    workload_size, &result_size, &ret);
    }
    else if (options & GMAN_DO_OPTIONS_LOW)
    {
      (void) gearman_client_do_low(&client, function->ptr(), unique, workload,
                                   workload_size, &result_size, &ret);
    }
    else
    {
      (void) gearman_client_do(&client, function->ptr(), unique, workload,
                               workload_size, &result_size, &ret);
    }
  }

  if (ret != GEARMAN_SUCCESS)
  {
    null_value= 1;
    return NULL;
  }

  null_value= 0;
  return &buffer;
}

void *Item_func_gman_do::realloc(size_t size)
{
  buffer.realloc(size);
  buffer.length(size);
  return buffer.ptr();
}
