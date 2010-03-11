/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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
 *
 *  Implementation drawn from visibility.texi in gnulib.
 */

/**
 * @file
 * @brief Visibility Control Macros
 */

#ifndef DRIZZLED_VISIBILITY_H
#define DRIZZLED_VISIBILITY_H

/**
 *
 * DRIZZLE_API is used for the public API symbols. It either DLL imports or
 * DLL exports (or does nothing for static build).
 *
 * DRIZZLE_LOCAL is used for non-api symbols.
 */

#if defined(BUILDING_DRIZZLE)
# if defined(HAVE_VISIBILITY)
#  define DRIZZLE_API __attribute__ ((visibility("default")))
#  define DRIZZLE_LOCAL  __attribute__ ((visibility("hidden")))
# elif defined (__SUNPRO_C) && (__SUNPRO_C >= 0x550)
#  define DRIZZLE_API __global
#  define DRIZZLE_API __hidden
# elif defined(_MSC_VER)
#  define DRIZZLE_API extern __declspec(dllexport) 
#  define DRIZZLE_LOCAL
# endif /* defined(HAVE_VISIBILITY) */
#else  /* defined(BUILDING_DRIZZLE) */
# if defined(_MSC_VER)
#  define DRIZZLE_API extern __declspec(dllimport) 
#  define DRIZZLE_LOCAL
# else
#  define DRIZZLE_API
#  define DRIZZLE_LOCAL
# endif /* defined(_MSC_VER) */
#endif /* defined(BUILDING_DRIZZLE) */

#endif /* DRIZZLED_VISIBILITY_H */
