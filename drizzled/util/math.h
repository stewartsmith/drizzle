/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef DRIZZLE_SERVER_UTIL_MATH
#define DRIZZLE_SERVER_UTIL_MATH

/**
 * This is all to work around what I _think_ is a bug in Sun Studio headers
 * But for now, it just gets things compiling while I get the Sun Studio 
 * guys to look at it.
 */

#ifdef (__FORTEC__)
#if defined(HAVE_IEEEFP_H)
# include <ieeefp.h>
#endif

#if defined(__cplusplus) 
# if !defined(HAVE_ISNAN)

inline int isnan(double a)
{
  return isnand(a);
}

inline int isnan(float a)
{
  return isnanf(a);
}
#endif

#if !defined(__builtin_isnan)
inline int __builtin_isnan (double a)
{
  return isnand(a);
}

inline int __builting_isnan (float a)
{
  return isnanf(a);
}
#endif

#if !defined(__builtin_isinf)
int __builtin_isinf(double a)
{
  fpclass_t fc= fpclass(a);
  return ((fc==FP_NINF)||(fc==FP_PINF)) ? 0 : 1;
}
#endif

#endif
#endif /* __FORTEC__ */
#endif
