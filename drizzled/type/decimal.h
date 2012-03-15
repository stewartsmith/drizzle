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

#pragma once
#include <assert.h>
#include <drizzled/sql_string.h>
#include <drizzled/definitions.h>
#include <drizzled/type/time.h>

namespace drizzled {

typedef enum
{
  TRUNCATE= 0,
  HALF_EVEN,
  HALF_UP,
  CEILING,
  FLOOR
} decimal_round_mode;
typedef int32_t decimal_digit_t;

struct decimal_t {
  int    intg, frac, len;
  bool sign;
  decimal_digit_t *buf;

  /* set a decimal_t to zero */
  inline void set_zero()
  {							    
    buf[0]= 0;
    intg= 1;
    frac= 0;
    sign= 0; 
  }

  /* negate a decimal */
  inline void negate()
  {
    sign^=1;
  }

  int isZero() const;

};

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
void max_decimal(int precision, int frac, decimal_t *to);

inline int string2decimal(char *from, decimal_t *to, char **end)
{
  return internal_str2dec(from, to, end, false);
}

/*
  returns the length of the buffer to hold string representation
  of the decimal (including decimal dot, possible sign and \0)
*/

inline int decimal_string_size(const decimal_t *dec)
{
  return (dec->intg ? dec->intg : 1) + dec->frac + (dec->frac > 0) + 2;
}

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

/** maximum length of buffer in our big digits (uint32_t). */
#define DECIMAL_BUFF_LENGTH 9

/* the number of digits that type::Decimal can possibly contain */
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

inline int class_decimal_int_part(uint32_t precision, uint32_t decimals)
{
  return precision - ((decimals == DECIMAL_NOT_SPECIFIED) ? 0 : decimals);
}

int decimal_operation_results(int result);

inline void max_Decimal(type::Decimal *to, int precision, int frac)
{
  assert((precision <= DECIMAL_MAX_PRECISION)&&
              (frac <= DECIMAL_MAX_SCALE));
  max_decimal(precision, frac, (decimal_t*) to);
}

inline void max_internal_decimal(type::Decimal *to)
{
  max_Decimal(to, DECIMAL_MAX_PRECISION, 0);
}

inline int check_result(uint32_t mask, int result)
{
  if (result & mask)
    decimal_operation_results(result);
  return result;
}

namespace type {
/**
  type Decimal class limits 'decimal_t' type to what we need in MySQL.

  It contains internally all necessary space needed by the instance so
  no extra memory is needed. One should call fix_buffer_pointer() function
  when he moves type::Decimal objects in memory.
*/

class Decimal : public decimal_t
{
  decimal_digit_t buffer[DECIMAL_BUFF_LENGTH];

public:

  void init()
  {
    len= DECIMAL_BUFF_LENGTH;
    buf= buffer;
#if !defined (HAVE_VALGRIND)
    /* Set buffer to 'random' value to find wrong buffer usage */
    for (uint32_t i= 0; i < DECIMAL_BUFF_LENGTH; i++)
      buffer[i]= i;
#endif
  }

  Decimal()
  {
    init();
  }

  void fix_buffer_pointer() { buf= buffer; }
  bool sign() const { return decimal_t::sign; }
  void sign(bool s) { decimal_t::sign= s; }
  uint32_t precision() const { return intg + frac; }

  int val_int32(uint32_t mask, bool unsigned_flag, int64_t *l) const
  {
    type::Decimal rounded;
    /* decimal_round can return only E_DEC_TRUNCATED */
    decimal_round(static_cast<const decimal_t*>(this), &rounded, 0, HALF_UP);
    return check_result(mask, (unsigned_flag ?
                               decimal2uint64_t(&rounded, reinterpret_cast<uint64_t *>(l)) :
                               decimal2int64_t(&rounded, l)));
  }

  int string_length() const
  {
    return decimal_string_size(this);
  }

  int val_binary(uint32_t mask, unsigned char *bin, int prec, int scale) const;

  int store(uint32_t mask, const char *from, uint32_t length, const charset_info_st * charset);

  int store(uint32_t mask, char *str, char **end)
  {
    return check_result_and_overflow(mask, string2decimal(str, static_cast<decimal_t*>(this), end));
  }

  int store(uint32_t mask, const String *str)
  {
    return store(mask, str->ptr(), str->length(), str->charset());
  }

  int check_result_and_overflow(uint32_t mask, int result)
  {
    if (check_result(mask, result) & E_DEC_OVERFLOW)
    {
      bool _sign= sign();
      fix_buffer_pointer();
      max_internal_decimal(this);
      sign(_sign);
    }
    return result;
  }

  void convert(double &value) const;
};

} // type

std::ostream& operator<<(std::ostream& output, const type::Decimal &dec);

inline uint32_t class_decimal_length_to_precision(uint32_t length, uint32_t scale,
                                                  bool unsigned_flag)
{
  return (uint32_t) (length - (scale>0 ? 1:0) - (unsigned_flag ? 0:1));
}

inline uint32_t class_decimal_precision_to_length(uint32_t precision, uint8_t scale,
                                                  bool unsigned_flag)
{
  set_if_smaller(precision, (uint32_t)DECIMAL_MAX_PRECISION);
  return static_cast<uint32_t>(precision + (scale>0 ? 1:0) + (unsigned_flag ? 0:1));
}


inline
int class_decimal_max_length(const type::Decimal *d)
{
  /* -1 because we do not count \0 */
  return decimal_string_size(d) - 1;
}


inline
int class_decimal_get_binary_size(uint32_t precision, uint32_t scale)
{
  return decimal_bin_size(static_cast<int>(precision), static_cast<int>(scale));
}


inline
void class_decimal2decimal(const type::Decimal *from, type::Decimal *to)
{
  *to= *from;
  to->fix_buffer_pointer();
}


inline
int binary2_class_decimal(uint32_t mask, const unsigned char *bin, type::Decimal *d, int prec,
		      int scale)
{
  return check_result(mask, bin2decimal(bin, static_cast<decimal_t*>(d), prec, scale));
}


inline
int class_decimal_round(uint32_t mask, const type::Decimal *from, int scale,
                     bool truncate, type::Decimal *to)
{
  return check_result(mask, decimal_round(static_cast<const decimal_t*>(from), to, scale,
                                          (truncate ? TRUNCATE : HALF_UP)));
}


inline
int class_decimal_floor(uint32_t mask, const type::Decimal *from, type::Decimal *to)
{
  return check_result(mask, decimal_round(static_cast<const decimal_t*>(from), to, 0, FLOOR));
}


inline
int class_decimal_ceiling(uint32_t mask, const type::Decimal *from, type::Decimal *to)
{
  return check_result(mask, decimal_round(static_cast<const decimal_t*>(from), to, 0, CEILING));
}


int class_decimal2string(const type::Decimal *d,
                         uint32_t fixed_dec, String *str);


inline
int class_decimal2double(uint32_t, const type::Decimal *d, double *result)
{
  /* No need to call check_result as this will always succeed */
  return decimal2double(static_cast<const decimal_t*>(d), result);
}


type::Decimal *date2_class_decimal(type::Time *ltime, type::Decimal *dec);


inline
int double2_class_decimal(uint32_t mask, double val, type::Decimal *d)
{
  return d->check_result_and_overflow(mask, double2decimal(val, static_cast<decimal_t*>(d)));
}


inline
int int2_class_decimal(uint32_t mask, int64_t i, bool unsigned_flag, type::Decimal *d)
{
  return check_result(mask, (unsigned_flag ?
			     uint64_t2decimal(static_cast<uint64_t>(i), d) :
			     int64_t2decimal(i, d)));
}


inline
void class_decimal_neg(decimal_t *arg)
{
  if (arg->isZero())
  {
    arg->sign= 0;
    return;
  }
  arg->negate();
}


inline
int class_decimal_add(uint32_t mask, type::Decimal *res, const type::Decimal *a,
		   const type::Decimal *b)
{
  return res->check_result_and_overflow(mask,
                                        decimal_add(static_cast<const decimal_t*>(a),
                                                    static_cast<const decimal_t*>(b), res));
}


inline
int class_decimal_sub(uint32_t mask, type::Decimal *res, const type::Decimal *a,
		   const type::Decimal *b)
{
  return res->check_result_and_overflow(mask,
                                        decimal_sub(static_cast<const decimal_t*>(a),
                                                    static_cast<const decimal_t*>(b), res));
}


inline
int class_decimal_mul(uint32_t mask, type::Decimal *res, const type::Decimal *a,
		   const type::Decimal *b)
{
  return res->check_result_and_overflow(mask,
                                        decimal_mul(static_cast<const decimal_t*>(a),
                                                    static_cast<const decimal_t*>(b),res));
}


inline
int class_decimal_div(uint32_t mask, type::Decimal *res, const type::Decimal *a,
		   const type::Decimal *b, int div_scale_inc)
{
  return res->check_result_and_overflow(mask,
                                        decimal_div(static_cast<const decimal_t*>(a),
                                                    static_cast<const decimal_t*>(b),res,
                                                    div_scale_inc));
}


inline
int class_decimal_mod(uint32_t mask, type::Decimal *res, const type::Decimal *a,
		   const type::Decimal *b)
{
  return res->check_result_and_overflow(mask,
                                        decimal_mod(static_cast<const decimal_t*>(a),
                                                    static_cast<const decimal_t*>(b),res));
}


/**
  @return
    -1 if a<b, 1 if a>b and 0 if a==b
*/
inline
int class_decimal_cmp(const type::Decimal *a, const type::Decimal *b)
{
  return decimal_cmp(static_cast<const decimal_t*>(a),
                     static_cast<const decimal_t*>(b));
}


inline
int class_decimal_intg(const type::Decimal *a)
{
  return decimal_intg(static_cast<const decimal_t*>(a));
}


void class_decimal_trim(uint32_t *precision, uint32_t *scale);

inline type::Decimal &decimal_zero_const()
{
  static type::Decimal _decimal_zero;
  return _decimal_zero;
}

double my_double_round(double value, int64_t dec, bool dec_unsigned,
                       bool truncate);


#define decimal_zero decimal_zero_const()

} /* namespace drizzled */


