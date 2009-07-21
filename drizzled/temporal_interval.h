#include <drizzled/item.h>

namespace drizzled {
  /**
   *  Stores time interval for date/time manipulation
   */
  class TemporalInterval
  {
    public:
      TemporalInterval(uint32_t in_year,
          uint32_t in_month,
          uint32_t in_day,
          uint32_t in_hour,
          uint64_t in_minute,
          uint64_t in_second,
          uint64_t in_second_part,
          bool in_neg)
        :
          year(in_year),
          month(in_month),
          day(in_day),
          hour(in_hour),
          minute(in_minute),
          second(in_second),
          second_part(in_second_part),
          neg(in_neg)
    {}

      TemporalInterval()
        :
          year(0),
          month(0),
          day(0),
          hour(0),
          minute(0),
          second(0),
          second_part(0),
          neg(false)
    {}

      /**
       * Sets whether or not this object specifies a negative interval
       * @param[in] in_neg true if this is a negative interval, false if not
       */
      void setNegative(bool in_neg=true)
      {
        neg = in_neg;
      }

      bool getNeg() const
      {
        return neg;
      }

      /**
       * Populate this TemporalInterval from a string value
       *
       * To make code easy, allow interval objects without separators.
       *
       * @param args argument Item structure
       * @param int_type type of interval to create
       * @param str_value String pointer to the input value
       * @return true if the string would result in a null interval
       * 
       */
      bool value_from_item(Item *args, interval_type int_type, String *str_value);

      /**
       * Adds this interval to a DRIZZLE_LTIME structure
       *
       * @param[in,out] ltime the interval will be added to ltime directly in the ltime structure
       * @param[in] int_type the type of interval requested
       * @retval true date was added and value stored properly
       * @retval false result of addition is a null value
       */
      bool add_date(DRIZZLE_TIME *ltime, interval_type int_type);

    private:

      /**
       *  @details
       *  Get a array of positive numbers from a string object.
       *  Each number is separated by 1 non digit character
       *  Return error if there is too many numbers.
       *  If there is too few numbers, assume that the numbers are left out
       *  from the high end. This allows one to give:
       *  DAY_TO_SECOND as "D MM:HH:SS", "MM:HH:SS" "HH:SS" or as seconds.

       *  @param length:         length of str
       *  @param cs:             charset of str
       *  @param values:         array of results
       *  @param count:          count of elements in result array
       *  @param transform_msec: if value is true we suppose
       *  that the last part of string value is microseconds
       *  and we should transform value to six digit value.
       *  For example, '1.1' -> '1.100000'
       */
      bool get_interval_info(const char *str,uint32_t length, const CHARSET_INFO * const cs,
          uint32_t count, uint64_t *values,
          bool transform_msec);

      uint32_t  year;
      uint32_t  month;
      uint32_t  day;
      uint32_t  hour;
      uint64_t  minute;
      uint64_t  second;
      uint64_t  second_part;
      bool      neg;

  };
}
