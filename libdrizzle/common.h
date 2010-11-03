/*
 * Drizzle Client & Protocol Library
 *
 * Copyright (C) 2008 Eric Day (eday@oddments.org)
 * All rights reserved.
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING.BSD file in the root source directory for full text.
 */

/**
 * @file
 * @brief System Include Files
 */

#ifndef __DRIZZLE_COMMON_H
#define __DRIZZLE_COMMON_H

#include "config.h"

#define HAVE_VISIBILITY 1

#include "drizzle_client.h"
#include "drizzle_server.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
#include <signal.h>

#include "drizzle_local.h"
#include "conn_local.h"
#include "pack.h"
#include "state.h"
#include "sha1.h"

#endif /* __DRIZZLE_COMMON_H */
