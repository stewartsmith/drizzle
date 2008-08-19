#ifndef BINLOG_ENCODE_H_INCLUDED
#define BINLOG_ENCODE_H_INCLUDED

#include <cstdlib>
#include <cassert>
#include <cstring>
#include <stdint.h>

#define LENGTH_ENCODE_MAX_BYTES (sizeof(size_t) + 1)

inline unsigned char *
length_encode(size_t length, unsigned char *buf)
{
  unsigned char *ptr= buf;
  assert(length > 1);
  if (length < 256)
    *ptr++= length & 0xFF;
  else {
    int_fast8_t log2m1= -1;        // ceil(log2(ptr - buf)) - 1
    uint_fast8_t pow2= 1;          // pow2(log2m1 + 1)
    while (length > 0) {
      // Check the invariants
      assert(pow2 == (1 << (log2m1 + 1)));
      assert((ptr - buf) <= (1 << (log2m1 + 1)));

      // Write the least significant byte of the current
      // length. Prefix increment is used to make space for the first
      // byte that will hold log2m1.
      *++ptr= length & 0xFF;
      length >>= 8;

      // Ensure the invariant holds by correcting it if it doesn't,
      // that is, the number of bytes written is greater than the
      // nearest power of two.
      if (ptr - buf > pow2) {
        ++log2m1;
        pow2 <<= 1;
      }
    }
    // Clear the remaining bytes up to the next power of two
    memset(ptr + 1, 0, pow2 - (ptr - buf));
    *buf= log2m1;
    ptr= buf + pow2 + 1;
  }
  return ptr;
}

inline unsigned char *
length_decode(unsigned char *buf, size_t *plen)
{
  if (*buf > 1) {
    *plen = *buf;
    return buf + 1;
  }

  size_t bytes= 1 << (*buf + 1);
  unsigned char *ptr= buf + 1;
  size_t length= 0;
  for (unsigned int i = 0 ; i < bytes ; ++i)
    length |= *ptr++ << (8 * i);
  *plen= length;
  return ptr;
}

/**
   Compute how many bytes are use for the length.

   The number of bytes that make up th length can be computed based on
   the first byte of the length field. By supplying this byte to the
   function, the number of bytes that is needed for the length is
   computed.

 */
inline size_t
length_decode_bytes(int peek)
{
  return (peek < 2) ? (1 << (peek + 1)) + 1 : 1;
}

#endif /* BINLOG_ENCODE_H_INCLUDED */
