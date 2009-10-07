/* 
 * The crc32 functions and data was originally written by Spencer
 * Garrett <srg@quick.com> and was gleaned from the PostgreSQL source
 * tree via the files contrib/ltree/crc32.[ch] and from FreeBSD at
 * src/usr.bin/cksum/crc32.c.
 */

#ifndef DRIZZLED_HASH_CRC32_H
#define DRIZZLED_HASH_CRC32_H

namespace drizzled
{
namespace hash
{

uint32_t crc32(const char *key, size_t key_length);

} /* namespace hash */
} /* namespace drizzled */

#endif /* DRIZZLED_HASH_CRC32_H */
