/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef DRIZZLED_DECIMAL_H
#define DRIZZLED_DECIMAL_H
#include <assert.h>
#include <drizzled/sql_string.h>
#include "drizzled/definitions.h"
#include "drizzled/drizzle_time.h"
namespace drizzled
{

typedef enum
{TRUNCATE=0, HALF_EVEN, HALF_UP, CEILING, FLOOR}
  decimal_round_mode;
typedef int32_t decimal_digit_t;

typedef struct st_decimal_t {
  int    intg, frac, len;
  bool sign;
  decimal_digit_t *buf;
} decimal_t;

int internal_str2dec(char *from, decimal_t *to, char **end,
                     bool fixed);
int decimal2string(const decimal_t *from, char *to, int *to_len,
                   int fixed_precision, int fixed_decimals,
                   char filler);
int decimal2uint64_t(const decimal_t *from, uint64_t *to);
int uint64_t2decimal(const uint64_t from, decimal_t *to);
int decimal2int64_t(const decimal_t *from, int64_t *to);
int int64_t2decimal(const int64_t from, decimal_t *to);
int decimal2double(const decimal_t *from, double *to);
int double2decimal(const double from, decimal_t *to);
int decimal_actual_fraction(decimal_t *from);
int decimal2bin(const decimal_t *from, unsigned char *to, int precision, int scale);
int bin2decimal(const unsigned char *from, decimal_t *to, int precision, int scale);

int decimal_bin_size(int precision, int scale);

int decimal_intg(const decimal_t *from);
int decimal_add(const decimal_t *from1, const decimal_t *from2, decimal_t *to);
int decimal_sub(const decimal_t *from1, const decimal_t *from2, decimal_t *to);
int decimal_cmp(const decimal_t *from1, const decimal_t *from2);
int decimal_mul(const decimal_t *from1, const decimal_t *from2, decimal_t *to);
int decimal_div(const decimal_t *from1, const decimal_t *from2, decimal_t *to,
                int scale_incr);
int decimal_mod(const decimal_t *from1, const decimal_t *from2, decimal_t *to);
int decimal_round(const decimal_t *from, decimal_t *to, int new_scale,
                  decimal_round_mode mode);
int decimal_is_zero(const decimal_t *from);
void max_decimal(int precision, int frac, decimal_t *to);

#define string2decimal(A,B,C) internal_str2dec((A), (B), (C), 0)

/* set a decimal_t to zero */

#define decimal_make_zero(dec)        do {                \
                                        (dec)->buf[0]=0;    \
                                        (dec)->intg=1;      \
                                        (dec)->frac=0;      \
                                        (dec)->sign=0;      \
                                      } while(0)

/*
  returns the length of the buffer to hold string representation
  of the decimal (including decimal dot, possible sign and \0)
*/

#define decimal_string_size(dec) (((dec)->intg ? (dec)->intg : 1) + \
				  (dec)->frac + ((dec)->frac > 0) + 2)

/* negate a decimal */
#define decimal_neg(dec) do { (dec)->sign^=1; } while(0)

/*
  conventions:

    decimal_smth() == 0     -- everything's ok
    decimal_smth() <= 1     -- result is usable, but precision loss is possible
    decimal_smth() <= 2     -- result can be unusable, most significant digits
                               could've been lost
    decimal_smth() >  2     -- no result was generated
*/

#define E_DEC_OK                0
#define E_DEC_TRUNCATED         1
#define E_DEC_OVERFLOW          2
#define E_DEC_DIV_ZERO          4
#define E_DEC_BAD_NUM           8
#define E_DEC_OOM              16

#define E_DEC_ERROR            31
#define E_DEC_FATAL_ERROR      30
#define DECIMAL_LONGLONG_DIGITS 22
#define DECIMAL_LONG_DIGITS 10
#define DECIMAL_LONG3_DIGITS 8

/** maximum length of buffer in our big digits (uint32_t). */
#define DECIMAL_BUFF_LENGTH 9

/* the number of digits that my_decimal can possibly contain */
#define DECIMAL_MAX_POSSIBLE_PRECISION (DECIMAL_BUFF_LENGTH * 9)


/**
  maximum guaranteed precision of number in decimal digits (number of our
  digits * number of decimal digits in one our big digit - number of decimal
  digits in one our big digit decreased by 1 (because we always put decimal
  point on the border of our big digits))
*/
#define DECIMAL_MAX_PRECISION (DECIMAL_MAX_POSSIBLE_PRECISION - 8*2)
#define DECIMAL_MAX_SCALE 30
#define DECIMAL_NOT_SPECIFIED 31

/**
  maximum length of string representation (number of maximum decimal
  digits + 1 position for sign + 1 position for decimal point)
*/
#define DECIMAL_MAX_STR_LENGTH (DECIMAL_MAX_POSSIBLE_PRECISION + 2)

/**
  maximum size of packet length.
*/
#define DECIMAL_MAX_FIELD_SIZE DECIMAL_MAX_PRECISION

inline int my_decimal_int_part(uint32_t precision, uint32_t decimals)
{
  return precision - ((decimals == DECIMAL_NOT_SPECIFIED) ? 0 : decimals);
}


/**
  my_decimal class limits 'decimal_t' type to what we need in MySQL.

  It contains internally all necessary space needed by the instance so
  no extra memory is needed. One should call fix_buffer_pointer() function
  when he moves my_decimal objects in memory.
*/

class my_decimal :public decimal_t
{
  decimal_digit_t buffer[DECIMAL_BUFF_LENGTH];

public:

  void init()
  {
    len= DECIMAL_BUFF_LENGTH;
    buf= buffer;
	#if !defined (HAVE_purify)
		/* Set buffer to 'random' value to find wrong buffer usage */
		for (uint32_t i= 0; i < DECIMAL_BUFF_LENGTH; i++)
		  buffer[i]= i;
	#endif
  }
  my_decimal()
  {
    init();
  }
  void fix_buffer_pointer() { buf= buffer; }
  bool sign() const { return decimal_t::sign; }
  void sign(bool s) { decimal_t::sign= s; }
  uint32_t precision() const { return intg + frac; }
};

int decimal_operation_results(int result);

inline void max_my_decimal(my_decimal *to, int precision, int frac)
{
  assert((precision <= DECIMAL_MAX_PRECISION)&&
              (frac <= DECIMAL_MAX_SCALE));
  max_decimal(precision, frac, (decimal_t*) to);
}

inline void max_internal_decimal(my_decimal *to)
{
  max_my_decimal(to, DECIMAL_MAX_PRECISION, 0);
}

inline int check_result(uint32_t mask, int result)
{
  if (result & mask)
    decimal_operation_results(result);
  return result;
}

inline int check_result_and_overflow(uint32_t mask, int result, my_decimal *val)
{
  if (check_result(mask, result) & E_DEC_OVERFLOW)
  {
    bool sign= val->sign();
    val->fix_buffer_pointer();
    max_internal_decimal(val);
    val->sign(sign);
  }
  return result;
}

inline uint32_t my_decimal_length_to_precision(uint32_t length, uint32_t scale,
                                           bool unsigned_flag)
{
  return (uint32_t) (length - (scale>0 ? 1:0) - (unsigned_flag ? 0:1));
}

inline uint32_t my_decimal_precision_to_length(uint32_t precision, uint8_t scale,
                                             bool unsigned_flag)
{
  set_if_smaller(precision, (uint32_t)DECIMAL_MAX_PRECISION);
  return static_cast<uint32_t>(precision + (scale>0 ? 1:0) + (unsigned_flag ? 0:1));
}

inline
int my_decimal_string_length(const my_decimal *d)
{
  return decimal_string_size(d);
}


inline
int my_decimal_max_length(const my_decimal *d)
{
  /* -1 because we do not count \0 */
  return decimal_string_size(d) - 1;
}


inline
int my_decimal_get_binary_size(uint32_t precision, uint32_t scale)
{
  return decimal_bin_size(static_cast<int>(precision), static_cast<int>(scale));
}


inline
void my_decimal2decimal(const my_decimal *from, my_decimal *to)
{
  *to= *from;
  to->fix_buffer_pointer();
}


int my_decimal2binary(uint32_t mask, const my_decimal *d, unsigned char *bin, int prec,
		      int scale);


inline
int binary2my_decimal(uint32_t mask, const unsigned char *bin, my_decimal *d, int prec,
		      int scale)
{
  return check_result(mask, bin2decimal(bin, static_cast<decimal_t*>(d), prec, scale));
}


inline
int my_decimal_set_zero(my_decimal *d)
{
  decimal_make_zero(static_cast<decimal_t*> (d));
  return 0;
}


inline
bool my_decimal_is_zero(const my_decimal *decimal_value)
{
  return decimal_is_zero(static_cast<const decimal_t*>(decimal_value));
}


inline
int my_decimal_round(uint32_t mask, const my_decimal *from, int scale,
                     bool truncate, my_decimal *to)
{
  return check_result(mask, decimal_round(static_cast<const decimal_t*>(from), to, scale,
                                          (truncate ? TRUNCATE : HALF_UP)));
}


inline
int my_decimal_floor(uint32_t mask, const my_decimal *from, my_decimal *to)
{
  return check_result(mask, decimal_round(static_cast<const decimal_t*>(from), to, 0, FLOOR));
}


inline
int my_decimal_ceiling(uint32_t mask, const my_decimal *from, my_decimal *to)
{
  return check_result(mask, decimal_round(static_cast<const decimal_t*>(from), to, 0, CEILING));
}


int my_decimal2string(uint32_t mask, const my_decimal *d, uint32_t fixed_prec,
                      uint32_t fixed_dec, char filler, String *str);

inline
int my_decimal2int(uint32_t mask, const my_decimal *d, bool unsigned_flag,
                   int64_t *l)
{
  my_decimal rounded;
  /* decimal_round can return only E_DEC_TRUNCATED */
  decimal_round(static_cast<const decimal_t*>(d), &rounded, 0, HALF_UP);
  return check_result(mask, (unsigned_flag ?
			     decimal2uint64_t(&rounded, reinterpret_cast<uint64_t *>(l)) :
			     decimal2int64_t(&rounded, l)));
}


inline
int my_decimal2double(uint32_t, const my_decimal *d, double *result)
{
  /* No need to call check_result as this will always succeed */
  return decimal2double(static_cast<const decimal_t*>(d), result);
}


inline
int str2my_decimal(uint32_t mask, char *str, my_decimal *d, char **end)
{
  return check_result_and_overflow(mask, string2decimal(str, static_cast<decimal_t*>(d),end),
                                   d);
}


int str2my_decimal(uint32_t mask, const char *from, uint32_t length,
                   const CHARSET_INFO * charset, my_decimal *decimal_value);

inline
int string2my_decimal(uint32_t mask, const String *str, my_decimal *d)
{
  return str2my_decimal(mask, str->ptr(), str->length(), str->charset(), d);
}


my_decimal *date2my_decimal(DRIZZLE_TIME *ltime, my_decimal *dec);


inline
int double2my_decimal(uint32_t mask, double val, my_decimal *d)
{
  return check_result_and_overflow(mask, double2decimal(val, static_cast<decimal_t*>(d)), d);
}


inline
int int2my_decimal(uint32_t mask, int64_t i, bool unsigned_flag, my_decimal *d)
{
  return check_result(mask, (unsigned_flag ?
			     uint64_t2decimal(static_cast<uint64_t>(i), d) :
			     int64_t2decimal(i, d)));
}


inline
void my_decimal_neg(decimal_t *arg)
{
  if (decimal_is_zero(arg))
  {
    arg->sign= 0;
    return;
  }
  decimal_neg(arg);
}


inline
int my_decimal_add(uint32_t mask, my_decimal *res, const my_decimal *a,
		   const my_decimal *b)
{
  return check_result_and_overflow(mask,
                                   decimal_add(static_cast<const decimal_t*>(a),
                                               static_cast<const decimal_t*>(b), res),
                                   res);
}


inline
int my_decimal_sub(uint32_t mask, my_decimal *res, const my_decimal *a,
		   const my_decimal *b)
{
  return check_result_and_overflow(mask,
                                   decimal_sub(static_cast<const decimal_t*>(a),
                                               static_cast<const decimal_t*>(b), res),
                                   res);
}


inline
int my_decimal_mul(uint32_t mask, my_decimal *res, const my_decimal *a,
		   const my_decimal *b)
{
  return check_result_and_overflow(mask,
                                   decimal_mul(static_cast<const decimal_t*>(a),
                                               static_cast<const decimal_t*>(b),res),
                                   res);
}


inline
int my_decimal_div(uint32_t mask, my_decimal *res, const my_decimal *a,
		   const my_decimal *b, int div_scale_inc)
{
  return check_result_and_overflow(mask,
                                   decimal_div(static_cast<const decimal_t*>(a),
                                               static_cast<const decimal_t*>(b),res,
                                               div_scale_inc),
                                   res);
}


inline
int my_decimal_mod(uint32_t mask, my_decimal *res, const my_decimal *a,
		   const my_decimal *b)
{
  return check_result_and_overflow(mask,
                                   decimal_mod(static_cast<const decimal_t*>(a),
                                               static_cast<const decimal_t*>(b),res),
                                   res);
}


/**
  @return
    -1 if a<b, 1 if a>b and 0 if a==b
*/
inline
int my_decimal_cmp(const my_decimal *a, const my_decimal *b)
{
  return decimal_cmp(static_cast<const decimal_t*>(a),
                     static_cast<const decimal_t*>(b));
}


inline
int my_decimal_intg(const my_decimal *a)
{
  return decimal_intg(static_cast<const decimal_t*>(a));
}


void my_decimal_trim(uint32_t *precision, uint32_t *scale);

} /* namespace drizzled */

#endif /* DRIZZLED_DECIMAL_H */

