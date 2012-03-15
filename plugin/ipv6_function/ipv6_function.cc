/*
  Original copyright header listed below. This comes via rsync.
  Any additional changes are provided via the same license as the original.

  Copyright (C) 2011 Muhammad Umair

*/
/*
 * Copyright (C) 1996-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>

#include <drizzled/error.h>
#include <drizzled/charset.h>
#include <drizzled/function/str/strfunc.h>
#include <drizzled/item/func.h>
#include <drizzled/plugin/function.h>

#include <drizzled/type/ipv6.h>

namespace plugin {
namespace ipv6 {

class Generate: public drizzled::Item_str_func
{
public:
  Generate(): drizzled::Item_str_func() {}
  void fix_length_and_dec()
  {
    max_length= (drizzled::type::IPv6::IPV6_BUFFER_LENGTH) * drizzled::system_charset_info->mbmaxlen;
  }
  const char *func_name() const{ return "ipv6"; }
  drizzled::String *val_str(drizzled::String *);
};


drizzled::String *Generate::val_str(drizzled::String *str)
{
  drizzled::String _result;
  drizzled::String *result= val_str(&_result);
  drizzled::type::IPv6 ptr_address;

  if (not ptr_address.inet_pton(result->c_str()))
  {
    drizzled::my_error(drizzled::ER_INVALID_IPV6_VALUE, MYF(ME_FATALERROR));
     str->length(0);

    return str;
  }

  char *ipv6_string;
  str->realloc(drizzled::type::IPv6::IPV6_BUFFER_LENGTH);
  str->length(drizzled::type::IPv6::IPV6_BUFFER_LENGTH);
  str->set_charset(drizzled::system_charset_info);
  ipv6_string= (char *) str->ptr();

  if (not ptr_address.inet_ntop(ipv6_string))
  {
    drizzled::my_error(drizzled::ER_INVALID_IPV6_VALUE, MYF(ME_FATALERROR));
    str->length(0);

    return str;
  }
  str->length(max_length);

  return str;
}

} // namespace ipv6
} // namespace plugin

static int initialize(drizzled::module::Context &context)
{
  context.add(new drizzled::plugin::Create_function<plugin::ipv6::Generate>("ipv6"));

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "ipv6",
  "1.0",
  "Muhammad Umair",
  N_("IPV6 function"),
  drizzled::PLUGIN_LICENSE_GPL,
  initialize,
  NULL,
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
