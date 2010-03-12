/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL
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

#ifndef DRIZZLED_INTERNAL_AIO_RESULT_H
#define DRIZZLED_INTERNAL_AIO_RESULT_H

#ifdef HAVE_AIOWAIT
#include <sys/asynch.h>      /* Used by record-cache */

namespace drizzled
{
namespace internal
{

typedef struct my_aio_result {
  aio_result_t result;
  int         pending;
} my_aio_result;

} /* namespace internal */
} /* namespace drizzled */

#endif

#endif /* DRIZZLED_INTERNAL_AIO_RESULT_H */
