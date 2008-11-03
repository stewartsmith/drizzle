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

#if defined(_FORTEC_)
# if defined(HAVE_IEEEFP_H)
#  include <ieeefp.h>
# endif
# include CMATH_H

# if defined(__cplusplus) 

#  if defined(NEED_ISNAN)
int isnan (double a);
#  endif /* defined(NEED_ISNAN) */

#  if defined(NEED_ISINF)
int isinf(double a);
#  endif /* defined(NEED_ISINF) */

#  if defined(NEED_ISFINITE)
int isfinite(double a);
#  endif /* defined(NEED_ISFINITE) */

# endif /* defined(__cplusplus) */
#endif /* __FORTEC__ */


#endif /* DRIZZLE_SERVER_UTIL_MATH */
