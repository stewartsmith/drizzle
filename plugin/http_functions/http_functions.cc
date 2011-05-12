/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Stewart Smith
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>

#include <drizzled/plugin/function.h>
#include <drizzled/function/str/strfunc.h>
#include <drizzled/charset.h>
#include <drizzled/error.h>

#include <curl/curl.h>

using namespace drizzled;

class HttpGetFunction :public Item_str_func
{
  String result;
public:
  HttpGetFunction() :Item_str_func() {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "http_get"; }

  bool check_argument_count(int n)
  {
    return n == 1;
  }
};

extern "C" size_t
http_get_result_cb(void *ptr, size_t size, size_t nmemb, void *data);

extern "C" size_t
http_get_result_cb(void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t realsize= size * nmemb;
  String *result= (String *)data;

  result->reserve(realsize + 1);
  result->append((const char*)ptr, realsize);

  return realsize;
}


String *HttpGetFunction::val_str(String *str)
{
  assert(fixed == 1);
  String *url = args[0]->val_str(str);
  CURL *curl;
  CURLcode retref;

  if ((null_value=args[0]->null_value))
    return NULL;

  curl= curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url->c_ptr_safe());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_get_result_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&result);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "drizzle-http-functions/1.0");
  retref= curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (retref != 0)
    my_error(ER_GET_ERRMSG, MYF(0), retref, curl_easy_strerror(retref),
             "http_get");

  return &result;
}

void HttpGetFunction::fix_length_and_dec()
{
  collation.set(args[0]->collation);
  max_length = ~0;
}

class HttpPostFunction :public Item_str_func
{
  String result;
public:
  HttpPostFunction() :Item_str_func() {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "http_post"; }

  bool check_argument_count(int n)
  {
    return n == 2;
  }
};

class HttpPostData
{
private:
  String *data;
  size_t progress;

public:
  HttpPostData(String* d) : data(d), progress(0) {}

  size_t length() { return data->length(); }

  size_t write(void* dest, size_t size)
  {
    size_t to_write= size;

    if ((data->length() - progress) < to_write)
      to_write= data->length() - progress;

    memcpy(dest, data->ptr() + progress, to_write);

    progress+= to_write;

    return to_write;
  }
};

extern "C" size_t
http_post_readfunc(void *ptr, size_t size, size_t nmemb, void *data);

extern "C" size_t
http_post_readfunc(void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t realsize= size * nmemb;
  HttpPostData *post_data= (HttpPostData *)data;

  return post_data->write(ptr, realsize);
}

String *HttpPostFunction::val_str(String *str)
{
  assert(fixed == 1);
  String *url = args[0]->val_str(str);
  CURL *curl;
  CURLcode retref;
  String post_storage;
  HttpPostData post_data(args[1]->val_str(&post_storage));

  if ((null_value=args[0]->null_value))
    return NULL;

  curl= curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url->c_ptr_safe());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_get_result_cb);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post_data.length());
  curl_easy_setopt(curl, CURLOPT_READDATA, &post_data);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, http_post_readfunc);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&result);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "drizzle-http-functions/1.0");
  retref= curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  return &result;
}

void HttpPostFunction::fix_length_and_dec()
{
  collation.set(args[0]->collation);
  max_length = ~0;
}

static int initialize(drizzled::module::Context &context)
{
  curl_global_init(CURL_GLOBAL_ALL);
  context.add(new plugin::Create_function<HttpGetFunction>("http_get"));
  context.add(new plugin::Create_function<HttpPostFunction>("http_post"));
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "http_functions",
  "1.0",
  "Stewart Smith",
  "HTTP functions",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  NULL,   /* depends */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
