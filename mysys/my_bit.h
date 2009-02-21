/*
  Some useful bit functions
*/

#ifdef __cplusplus
extern "C" {
#endif

extern const char _my_bits_nbits[256];
extern const unsigned char _my_bits_reverse_table[256];

/*
  Find smallest X in 2^X >= value
  This can be used to divide a number with value by doing a shift instead
*/

static inline uint32_t my_bit_log2(uint32_t value)
{
  uint32_t bit;
  for (bit=0 ; value > 1 ; value>>=1, bit++) ;
  return bit;
}

static inline uint32_t my_count_bits(uint64_t v)
{
  /* The following code is a bit faster on 16 bit machines than if we would
     only shift v */
  uint32_t v2=(uint32_t) (v >> 32);
  return (uint32_t) (unsigned char) (_my_bits_nbits[(unsigned char)  v] +
                         _my_bits_nbits[(unsigned char) (v >> 8)] +
                         _my_bits_nbits[(unsigned char) (v >> 16)] +
                         _my_bits_nbits[(unsigned char) (v >> 24)] +
                         _my_bits_nbits[(unsigned char) (v2)] +
                         _my_bits_nbits[(unsigned char) (v2 >> 8)] +
                         _my_bits_nbits[(unsigned char) (v2 >> 16)] +
                         _my_bits_nbits[(unsigned char) (v2 >> 24)]);
}

static inline uint32_t my_count_bits_uint16(uint16_t v)
{
  return _my_bits_nbits[v];
}


/*
  Next highest power of two

  SYNOPSIS
    my_round_up_to_next_power()
    v		Value to check

  RETURN
    Next or equal power of 2
    Note: 0 will return 0

  NOTES
    Algorithm by Sean Anderson, according to:
    http://graphics.stanford.edu/~seander/bithacks.html
    (Orignal code public domain)

    Comments shows how this works with 01100000000000000000000000001011
*/

static inline uint32_t my_round_up_to_next_power(uint32_t v)
{
  v--;			/* 01100000000000000000000000001010 */
  v|= v >> 1;		/* 01110000000000000000000000001111 */
  v|= v >> 2;		/* 01111100000000000000000000001111 */
  v|= v >> 4;		/* 01111111110000000000000000001111 */
  v|= v >> 8;		/* 01111111111111111100000000001111 */
  v|= v >> 16;		/* 01111111111111111111111111111111 */
  return v+1;		/* 10000000000000000000000000000000 */
}

static inline uint32_t my_clear_highest_bit(uint32_t v)
{
  uint32_t w=v >> 1;
  w|= w >> 1;
  w|= w >> 2;
  w|= w >> 4;
  w|= w >> 8;
  w|= w >> 16;
  return v & w;
}

static inline uint32_t my_reverse_bits(uint32_t key)
{
  return
    (_my_bits_reverse_table[ key      & 255] << 24) |
    (_my_bits_reverse_table[(key>> 8) & 255] << 16) |
    (_my_bits_reverse_table[(key>>16) & 255] <<  8) |
     _my_bits_reverse_table[(key>>24)      ];
}

#ifdef __cplusplus
}
#endif

