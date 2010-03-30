/*
 * Copyright (C) 2010 nobody (this is public domain)
 */

/**
 * @file
 * @brief SHA1 Declarations
 */

#ifndef DRIZZLED_ALGORITHM_SHA1_H
#define DRIZZLED_ALGORITHM_SHA1_H

#include <stdint.h>
#include <sys/types.h>

namespace drizzled
{

/**
 * @addtogroup sha1 SHA-1 in C
 * By Steve Reid <steve@edmweb.com>
 * 100% Public Domain
 * @{
 */

#define	SHA1_BLOCK_LENGTH		64
#define	SHA1_DIGEST_LENGTH		20
#define	SHA1_DIGEST_STRING_LENGTH	(SHA1_DIGEST_LENGTH * 2 + 1)

typedef struct {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[SHA1_BLOCK_LENGTH];
} SHA1_CTX;

void SHA1Init(SHA1_CTX *);
void SHA1Pad(SHA1_CTX *);
void SHA1Transform(uint32_t [5], const uint8_t [SHA1_BLOCK_LENGTH]);
void SHA1Update(SHA1_CTX *, const uint8_t *, size_t);
void SHA1Final(uint8_t [SHA1_DIGEST_LENGTH], SHA1_CTX *);

/** @} */

} /* namespace drizzled */

#endif /* DRIZZLED_ALGORITHM_SHA1_H */
