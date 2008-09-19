/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLE_SERVER_LOCALE_H
#define DRIZZLE_SERVER_LOCALE_H

typedef struct my_locale_st
{
  uint  number;
  const char *name;
  const char *description;
  const bool is_ascii;
  TYPELIB *month_names;
  TYPELIB *ab_month_names;
  TYPELIB *day_names;
  TYPELIB *ab_day_names;
#ifdef __cplusplus 
  my_locale_st(uint number_par,
               const char *name_par, const char *descr_par, bool is_ascii_par,
               TYPELIB *month_names_par, TYPELIB *ab_month_names_par,
               TYPELIB *day_names_par, TYPELIB *ab_day_names_par) : 
    number(number_par),
    name(name_par), description(descr_par), is_ascii(is_ascii_par),
    month_names(month_names_par), ab_month_names(ab_month_names_par),
    day_names(day_names_par), ab_day_names(ab_day_names_par)
  {}
#endif
} MY_LOCALE;

extern MY_LOCALE my_locale_en_US;
extern MY_LOCALE *my_locales[];
extern MY_LOCALE *my_default_lc_time_names;

MY_LOCALE *my_locale_by_name(const char *name);
MY_LOCALE *my_locale_by_number(uint number);

#endif /* DRIZZLE_SERVER_LOCALE_H */
