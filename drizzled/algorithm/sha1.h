/*
 * Copyright (C) 2010 nobody (this is public domain)
 */

/**
 * @file
 * @brief SHA1 Declarations
 */

#pragma once

#include <stdint.h>
#include <sys/types.h>
#include <string.h>

#include <drizzled/visibility.h>

namespace drizzled
{

/**
 * @addtogroup sha1 SHA-1 in C
 * 
 * This file is based on public domain code.
 * Initial source code is in the public domain, 
 * so clarified by Steve Reid <steve@edmweb.com>
 *
 * @{
 */

#define	SHA1_BLOCK_LENGTH		64
#define	SHA1_DIGEST_LENGTH		20
#define	SHA1_DIGEST_STRING_LENGTH	(SHA1_DIGEST_LENGTH * 2 + 1)

typedef class sha1_ctx{
public:
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[SHA1_BLOCK_LENGTH];

    sha1_ctx():
    	count(0)
    {
      memset(state, 0, 5);
      memset(buffer, 0, SHA1_BLOCK_LENGTH);
    }
} SHA1_CTX;

DRIZZLED_API void SHA1Init(SHA1_CTX *);
DRIZZLED_API void SHA1Pad(SHA1_CTX *);
DRIZZLED_API void SHA1Transform(uint32_t [5], const uint8_t [SHA1_BLOCK_LENGTH]);
DRIZZLED_API void SHA1Update(SHA1_CTX *, const uint8_t *, size_t);
DRIZZLED_API void SHA1Final(uint8_t [SHA1_DIGEST_LENGTH], SHA1_CTX *);

/** @} */

} /* namespace drizzled */

