/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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


#pragma once

#include <drizzled/common_fwd.h>

/**
 *
 * DRIZZLED_API is used for the public API symbols. It either DLL imports or
 * DLL exports (or does nothing for static build).
 *
 * DRIZZLED_LOCAL is used for non-api symbols.
 */

#if defined(BUILDING_DRIZZLED) && defined(HAVE_VISIBILITY)
# if defined(__GNUC__)
#  define DRIZZLED_API __attribute__ ((visibility("default")))
#  define DRIZZLED_INTERNAL_API __attribute__ ((visibility("hidden")))
#  define DRIZZLED_API_DEPRECATED __attribute__ ((deprecated,visibility("default")))
#  define DRIZZLED_LOCAL  __attribute__ ((visibility("hidden")))
# elif (defined(__SUNPRO_C) && (__SUNPRO_C >= 0x550)) || (defined(__SUNPRO_CC) && (__SUNPRO_CC >= 0x550))
#  define DRIZZLED_API __global
#  define DRIZZLED_INTERNAL_API __hidden
#  define DRIZZLED_API_DEPRECATED __global
#  define DRIZZLED_LOCAL __hidden
# elif defined(_MSC_VER)
#  define DRIZZLED_API extern __declspec(dllexport)
#  define DRIZZLED_INTERNAL_API extern __declspec(dllexport)
#  define DRIZZLED_DEPRECATED_API extern __declspec(dllexport)
#  define DRIZZLED_LOCAL
# endif
#else  /* defined(BUILDING_DRIZZLED) && defined(HAVE_VISIBILITY) */
# if defined(_MSC_VER)
#  define DRIZZLED_API extern __declspec(dllimport)
#  define DRIZZLED_INTERNAL_API extern __declspec(dllimport)
#  define DRIZZLED_API_DEPRECATED extern __declspec(dllimport)
#  define DRIZZLED_LOCAL
# else
#  define DRIZZLED_API
#  define DRIZZLED_INTERNAL_API
#  define DRIZZLED_API_DEPRECATED
#  define DRIZZLED_LOCAL
# endif /* defined(_MSC_VER) */
#endif  /* defined(BUILDING_DRIZZLED) && defined(HAVE_VISIBILITY) */



