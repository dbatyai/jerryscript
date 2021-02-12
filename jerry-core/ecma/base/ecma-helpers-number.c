/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <math.h>

#include "ecma-conversion.h"
#include "lit-char-helpers.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmahelpers Helpers for operations with ECMA data types
 * @{
 */

JERRY_STATIC_ASSERT (sizeof (ecma_value_t) == sizeof (ecma_integer_value_t),
                     size_of_ecma_value_t_must_be_equal_to_the_size_of_ecma_integer_value_t);

JERRY_STATIC_ASSERT (ECMA_DIRECT_SHIFT == ECMA_VALUE_SHIFT + 1,
                     currently_directly_encoded_values_has_one_extra_flag);

JERRY_STATIC_ASSERT (((1 << (ECMA_DIRECT_SHIFT - 1)) | ECMA_TYPE_DIRECT) == ECMA_DIRECT_TYPE_SIMPLE_VALUE,
                     currently_directly_encoded_values_start_after_direct_type_simple_value);

JERRY_STATIC_ASSERT (sizeof (ecma_number_t) == sizeof (ecma_number_bits_t),
                     size_of_ecma_number_t_must_be_equal_to_number_bits_size);

/**
 * Position of the sign bit in ecma-numbers
 */
#define ECMA_NUMBER_SIGN_POS (ECMA_NUMBER_FRACTION_WIDTH + \
                              ECMA_NUMBER_BIASED_EXP_WIDTH)

/**
 * It makes possible to read/write an ecma_number_t as uint64_t without strict aliasing rule violation.
 */
typedef union
{
  ecma_number_t number;
  ecma_number_bits_t bits;
} ecma_number_accessor_t;

/**
 * Packing sign, fraction and biased exponent to ecma-number
 *
 * @return ecma-number with specified sign, biased_exponent and fraction
 */
extern inline ecma_number_t JERRY_ATTR_ALWAYS_INLINE
ecma_number_pack (bool sign, /**< sign */
                  uint32_t biased_exp, /**< biased exponent */
                  ecma_number_bits_t fraction) /**< fraction */
{
  JERRY_ASSERT ((biased_exp & ~((1u << ECMA_NUMBER_BIASED_EXP_WIDTH) - 1)) == 0);
  JERRY_ASSERT ((fraction & ~((1ull << ECMA_NUMBER_FRACTION_WIDTH) - 1)) == 0);

  ecma_number_bits_t packed_value = sign ? 1ull : 0ull;
  packed_value = packed_value << ECMA_NUMBER_BIASED_EXP_WIDTH;
  packed_value |= biased_exp;
  packed_value = packed_value << ECMA_NUMBER_FRACTION_WIDTH;
  packed_value |= fraction;

  ecma_number_accessor_t u;
  u.bits = packed_value;
  return u.number;
} /* ecma_number_pack */

/**
 * Unpacking sign, fraction and biased exponent from ecma-number
 */
extern inline void JERRY_ATTR_ALWAYS_INLINE
ecma_number_unpack (ecma_number_t num, /**< ecma-number */
                    bool *sign_p, /**< [out] sign (optional) */
                    uint32_t *biased_exp_p, /**< [out] biased exponent (optional) */
                    ecma_number_bits_t *fraction_p) /**< [out] fraction (optional) */
{
  JERRY_ASSERT (sign_p != NULL);
  JERRY_ASSERT (biased_exp_p != NULL);
  JERRY_ASSERT (fraction_p != NULL);

  ecma_number_accessor_t u;
  u.number = num;
  ecma_number_bits_t packed_value = u.bits;

  *sign_p = ((packed_value >> ECMA_NUMBER_SIGN_POS) != 0);
  *biased_exp_p = (((packed_value) & ~(1ull << ECMA_NUMBER_SIGN_POS)) >> ECMA_NUMBER_FRACTION_WIDTH);
  *fraction_p = (packed_value & ((1ull << ECMA_NUMBER_FRACTION_WIDTH) - 1));
} /* ecma_number_unpack */

/**
 * Check if ecma-number is NaN
 *
 * @return true - if biased exponent is filled with 1 bits and
                  fraction is filled with anything but not all zero bits,
 *         false - otherwise
 */
extern inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_number_is_nan (ecma_number_t num) /**< ecma-number */
{
  return isnan (num);
} /* ecma_number_is_nan */

/**
 * Make a NaN.
 *
 * @return NaN value
 */
extern inline ecma_number_t JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_number_make_nan (void)
{
  return NAN;
} /* ecma_number_make_nan */

/**
 * Make an Infinity.
 *
 * @return if !sign - +Infinity value,
 *         else - -Infinity value.
 */
extern inline ecma_number_t JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_number_make_infinity (bool sign) /**< true - for negative Infinity,
                                           false - for positive Infinity */
{
  return sign ? -INFINITY : INFINITY;
} /* ecma_number_make_infinity */

/**
 * Check if ecma-number is negative
 *
 * @return true - if sign bit of ecma-number is set
 *         false - otherwise
 */
extern inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_number_is_negative (ecma_number_t num) /**< ecma-number */
{
#if defined (__GNUC__) || defined (__clang__)
  return __builtin_signbit (num);
#else
  ecma_number_accessor_t u;
  u.number = num;
  return (u.bits >> ECMA_NUMBER_SIGN_POS) != 0;
#endif
} /* ecma_number_is_negative */

/**
 * Check if ecma-number is zero
 *
 * @return true - if fraction is zero and biased exponent is zero,
 *         false - otherwise
 */
extern inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_number_is_zero (ecma_number_t num) /**< ecma-number */
{
  return num == ECMA_NUMBER_ZERO;
} /* ecma_number_is_zero */

/**
 * Checks whether the passed number is +0.0
 *
 * @return true, if it is +0.0, false otherwise
 */
extern inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_number_is_positive_zero (ecma_number_t num) /**< number */
{
  ecma_number_accessor_t u;
  u.number = num;
  return u.bits == 0;
} /* ecma_is_number_equal_to_positive_zero */


/**
 * Check if number is infinity
 *
 * @return true - if biased exponent is filled with 1 bits and
 *                fraction is filled with zero bits,
 *         false - otherwise
 */
extern inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_number_is_infinity (ecma_number_t num) /**< ecma-number */
{
#if defined (__GNUC__) || defined (__clang__)
  return __builtin_isinf (num);
#else /* !defined (__GNUC__) && !defined (__clang__) */
  return isinf (num);
#endif /* !defined (__GNUC__) && !defined (__clang__) */
} /* ecma_number_is_infinity */

/**
 * Check if number is finite
 *
 * @return true  - if number is finite
 *         false - if number is NaN or infinity
 */
extern inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_number_is_finite (ecma_number_t num) /**< ecma-number */
{
#if defined (__GNUC__) || defined (__clang__)
  return __builtin_isfinite (num);
#else /* !defined (__GNUC__) && !defined (__clang__) */
  return isfinite (num);
#endif /* !defined (__GNUC__) && !defined (__clang__) */
} /* ecma_number_is_finite */

/**
 * Make Number of given sign from given mantissa value and binary exponent
 *
 * @return ecma-number (possibly Infinity of specified sign)
 */
ecma_number_t
ecma_number_make_from_sign_mantissa_and_exponent (bool sign, /**< true - for negative sign,
                                                                  false - for positive sign */
                                                  uint64_t mantissa, /**< mantissa */
                                                  int32_t exponent) /**< binary exponent */
{
  /* Rounding mantissa to fit into fraction field width */
  /* Rounded mantissa looks like the following: |00...0|1|fraction_width mantissa bits| */
  uint64_t rightmost_bit = 0;
  while ((mantissa & ~((1ull << (ECMA_NUMBER_FRACTION_WIDTH + 1)) - 1)) != 0)
  {
    exponent++;
    rightmost_bit = (mantissa & 1);
    mantissa >>= 1;
  }

  /* Rounding to nearest value */
  mantissa += rightmost_bit;

  /* Normalizing mantissa */
  if (mantissa != 0)
  {
    while ((mantissa & (1ull << ECMA_NUMBER_FRACTION_WIDTH)) == 0)
    {
      exponent--;
      mantissa <<= 1;
    }
  }

  /* Moving floating point */
  exponent += ECMA_NUMBER_FRACTION_WIDTH - 1;

  int32_t biased_exp_signed = exponent + ECMA_NUMBER_EXPONENT_BIAS;

  if (biased_exp_signed < 1)
  {
    /* Denormalizing mantissa if biased_exponent is less than zero */
    while (biased_exp_signed < 0)
    {
      biased_exp_signed++;
      mantissa >>= 1;
    }

    /* Rounding to nearest value */
    mantissa += 1;
    mantissa >>= 1;

    /* Encoding denormalized exponent */
    biased_exp_signed = 0;
  }
  else
  {
    /* Clearing highest mantissa bit that should have been non-zero if mantissa is non-zero */
    mantissa &= ~(1ull << ECMA_NUMBER_FRACTION_WIDTH);
  }

  uint32_t biased_exp = (uint32_t) biased_exp_signed;

  if (biased_exp >= ((1u << ECMA_NUMBER_BIASED_EXP_WIDTH) - 1))
  {
    return ecma_number_make_infinity (sign);
  }

  JERRY_ASSERT (biased_exp < (1u << ECMA_NUMBER_BIASED_EXP_WIDTH) - 1);
  JERRY_ASSERT ((mantissa & ~((1ull << ECMA_NUMBER_FRACTION_WIDTH) - 1)) == 0);

  return ecma_number_pack (sign,
                           biased_exp,
                           mantissa);
} /* ecma_number_make_from_sign_mantissa_and_exponent */

/**
 * Truncate fractional part of the number
 *
 * @return integer part of the number
 */
ecma_number_t
ecma_number_trunc (ecma_number_t num) /**< ecma-number */
{
  JERRY_ASSERT (!ecma_number_is_nan (num));

  bool sign;
  uint32_t exp;
  ecma_number_bits_t fraction;
  ecma_number_unpack (num, &sign, &exp, &fraction);

  if (exp < ECMA_NUMBER_EXPONENT_BIAS)
  {
    return ECMA_NUMBER_ZERO;
  }

  if (exp < ECMA_NUMBER_FRACTION_WIDTH + ECMA_NUMBER_EXPONENT_BIAS)
  {
    fraction &= ~((1ull << (ECMA_NUMBER_FRACTION_WIDTH + ECMA_NUMBER_EXPONENT_BIAS - exp)) - 1);
    return ecma_number_pack (sign, exp, fraction);
  }

  return num;
} /* ecma_number_trunc */

/**
 * Calculate remainder of division of two numbers,
 * as specified in ECMA-262 v5, 11.5.3, item 6.
 *
 * Note:
 *      operands shouldn't contain NaN, Infinity, or zero.
 *
 * @return number - calculated remainder.
 */
ecma_number_t
ecma_number_calc_remainder (ecma_number_t left_num, /**< left operand */
                            ecma_number_t right_num) /**< right operand */
{
  JERRY_ASSERT (!ecma_number_is_nan (left_num)
                && !ecma_number_is_zero (left_num)
                && !ecma_number_is_infinity (left_num));
  JERRY_ASSERT (!ecma_number_is_nan (right_num)
                && !ecma_number_is_zero (right_num)
                && !ecma_number_is_infinity (right_num));

  const ecma_number_t q = ecma_number_trunc (left_num / right_num);
  ecma_number_t r = left_num - right_num * q;

  if (ecma_number_is_zero (r)
      && ecma_number_is_negative (left_num))
  {
    r = -r;
  }

  return r;
} /* ecma_number_calc_remainder */

/**
 * Compute power operation according to the ES standard.
 *
 * @return x ** y
 */
ecma_number_t
ecma_number_pow (ecma_number_t x, /**< left operand */
                 ecma_number_t y) /**< right operand */
{
  if (ecma_number_is_nan (y) ||
      (ecma_number_is_infinity (y) && (x == 1.0 || x == -1.0)))
  {
    /* Handle differences between ES5.1 and ISO C standards for pow. */
    return ecma_number_make_nan ();
  }

  if (ecma_number_is_zero (y))
  {
    /* Handle differences between ES5.1 and ISO C standards for pow. */
    return (ecma_number_t) 1.0;
  }

  return DOUBLE_TO_ECMA_NUMBER_T (pow (x, y));
} /* ecma_number_pow */

/**
 * ECMA-integer number multiplication.
 *
 * @return number - result of multiplication.
 */
extern inline ecma_value_t JERRY_ATTR_ALWAYS_INLINE
ecma_integer_multiply (ecma_integer_value_t left_integer, /**< left operand */
                       ecma_integer_value_t right_integer) /**< right operand */
{
#if defined (__GNUC__) || defined (__clang__)
  /* Check if left_integer is power of 2 */
  if (JERRY_UNLIKELY ((left_integer & (left_integer - 1)) == 0))
  {
    /* Right shift right_integer with log2 (left_integer) */
    return ecma_make_integer_value (right_integer << (__builtin_ctz ((unsigned int) left_integer)));
  }
  else if (JERRY_UNLIKELY ((right_integer & (right_integer - 1)) == 0))
  {
    /* Right shift left_integer with log2 (right_integer) */
    return ecma_make_integer_value (left_integer << (__builtin_ctz ((unsigned int) right_integer)));
  }
#endif /* defined (__GNUC__) || defined (__clang__) */
  return ecma_make_integer_value (left_integer * right_integer);
} /* ecma_integer_multiply */

/**
 * The Number object's 'parseInt' routine
 *
 * See also:
 *          ECMA-262 v5, 15.1.2.2
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_number_parse_int (const lit_utf8_byte_t *string_buff, /**< routine's first argument's
                                                            *   string buffer */
                        lit_utf8_size_t string_buff_size, /**< routine's first argument's
                                                           *   string buffer's size */
                        ecma_value_t radix) /**< routine's second argument */
{
  if (string_buff_size == 0)
  {
    return ecma_make_nan_value ();
  }

   /* 2. Remove leading whitespace. */

  const lit_utf8_byte_t *string_end_p = string_buff + string_buff_size;
  const lit_utf8_byte_t *start_p = ecma_string_trim_front (string_buff, string_end_p);
  const lit_utf8_byte_t *string_curr_p = start_p;
  const lit_utf8_byte_t *end_p = string_end_p;

  if (string_curr_p >= string_end_p)
  {
    return ecma_make_nan_value ();
  }

  /* 3. */
  int sign = 1;

  /* 4. */
  ecma_char_t current = lit_cesu8_read_next (&string_curr_p);
  if (current == LIT_CHAR_MINUS)
  {
    sign = -1;
  }

  /* 5. */
  if (current == LIT_CHAR_MINUS || current == LIT_CHAR_PLUS)
  {
    start_p = string_curr_p;
    if (string_curr_p < string_end_p)
    {
      current = lit_cesu8_read_next (&string_curr_p);
    }
  }

  /* 6. */
  ecma_number_t radix_num;
  radix = ecma_op_to_number (radix, &radix_num);

  if (ECMA_IS_VALUE_ERROR (radix))
  {
    return ECMA_VALUE_ERROR;
  }

  int32_t rad = ecma_number_to_int32 (radix_num);

  /* 7.*/
  bool strip_prefix = true;

  /* 8. */
  if (rad != 0)
  {
    /* 8.a */
    if (rad < 2 || rad > 36)
    {
      return ecma_make_nan_value ();
    }
    /* 8.b */
    else if (rad != 16)
    {
      strip_prefix = false;
    }
  }
  /* 9. */
  else
  {
    rad = 10;
  }

  /* 10. */
  if (strip_prefix
      && ((end_p - start_p) >= 2)
      && (current == LIT_CHAR_0))
  {
    ecma_char_t next = *string_curr_p;
    if (next == LIT_CHAR_LOWERCASE_X || next == LIT_CHAR_UPPERCASE_X)
    {
      /* Skip the 'x' or 'X' characters. */
      start_p = ++string_curr_p;
      rad = 16;
    }
  }

  /* 11. Check if characters are in [0, Radix - 1]. We also convert them to number values in the process. */
  string_curr_p = start_p;
  while (string_curr_p < string_end_p)
  {
    ecma_char_t current_char = *string_curr_p++;
    int32_t current_number;

    if ((current_char >= LIT_CHAR_LOWERCASE_A && current_char <= LIT_CHAR_LOWERCASE_Z))
    {
      current_number = current_char - LIT_CHAR_LOWERCASE_A + 10;
    }
    else if ((current_char >= LIT_CHAR_UPPERCASE_A && current_char <= LIT_CHAR_UPPERCASE_Z))
    {
      current_number = current_char - LIT_CHAR_UPPERCASE_A + 10;
    }
    else if (lit_char_is_decimal_digit (current_char))
    {
      current_number = current_char - LIT_CHAR_0;
    }
    else
    {
      /* Not a valid number char, set value to radix so it fails to pass as a valid character. */
      current_number = rad;
    }

    if (!(current_number < rad))
    {
      end_p = --string_curr_p;
      break;
    }
  }

  /* 12. */
  if (end_p == start_p)
  {
    return ecma_make_nan_value ();
  }

  ecma_number_t value = ECMA_NUMBER_ZERO;
  ecma_number_t multiplier = 1.0f;

  /* 13. and 14. */
  string_curr_p = end_p;

  while (string_curr_p > start_p)
  {
    ecma_char_t current_char = *(--string_curr_p);
    ecma_number_t current_number = ECMA_NUMBER_MINUS_ONE;

    if ((current_char >= LIT_CHAR_LOWERCASE_A && current_char <= LIT_CHAR_LOWERCASE_Z))
    {
      current_number = (ecma_number_t) current_char - LIT_CHAR_LOWERCASE_A + 10;
    }
    else if ((current_char >= LIT_CHAR_UPPERCASE_A && current_char <= LIT_CHAR_UPPERCASE_Z))
    {
      current_number = (ecma_number_t) current_char - LIT_CHAR_UPPERCASE_A + 10;
    }
    else
    {
      JERRY_ASSERT (lit_char_is_decimal_digit (current_char));
      current_number = (ecma_number_t) current_char - LIT_CHAR_0;
    }

    value += current_number * multiplier;
    multiplier *= (ecma_number_t) rad;
  }

  /* 15. */
  if (sign < 0)
  {
    value *= (ecma_number_t) sign;
  }
  return ecma_make_number_value (value);
} /* ecma_number_parse_int */

/**
 * The Number object's 'parseFloat' routine
 *
 * See also:
 *          ECMA-262 v5, 15.1.2.2
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_number_parse_float (const lit_utf8_byte_t *string_buff, /**< routine's first argument's
                                                              *   string buffer */
                          lit_utf8_size_t string_buff_size) /**< routine's first argument's
                                                             *   string buffer's size */
{
  if (string_buff_size == 0)
  {
    return ecma_make_nan_value ();
  }

  /* 2. Remove leading whitespace. */

  const lit_utf8_byte_t *str_end_p = string_buff + string_buff_size;
  const lit_utf8_byte_t *start_p = ecma_string_trim_front (string_buff, str_end_p);
  const lit_utf8_byte_t *str_curr_p = start_p;
  const lit_utf8_byte_t *end_p = str_end_p;

  bool sign = false;
  ecma_char_t current;

  if (str_curr_p < str_end_p)
  {
    /* Check if sign is present. */
    current = *str_curr_p;
    if (current == LIT_CHAR_MINUS)
    {
      sign = true;
    }

    if (current == LIT_CHAR_MINUS || current == LIT_CHAR_PLUS)
    {
      /* Set starting position to be after the sign character. */
      start_p = ++str_curr_p;
    }
  }

  /* Check if string is equal to "Infinity". */
  const lit_utf8_byte_t *infinity_str_p = lit_get_magic_string_utf8 (LIT_MAGIC_STRING_INFINITY_UL);
  const lit_utf8_size_t infinity_length = lit_get_magic_string_size (LIT_MAGIC_STRING_INFINITY_UL);

  /* The input string should be at least the length of "Infinity" to be correctly processed as
   * the infinity value.
   */
  if ((str_end_p - str_curr_p) >= (int) infinity_length
      && memcmp (infinity_str_p, str_curr_p, infinity_length) == 0)
  {
    /* String matched Infinity. */
    return ecma_make_number_value (ecma_number_make_infinity (sign));
  }

  /* Reset to starting position. */
  str_curr_p = start_p;

  /* String ended after sign character, or was empty after removing leading whitespace. */
  if (str_curr_p >= str_end_p)
  {
    return ecma_make_nan_value ();
  }

  /* Reset to starting position. */
  str_curr_p = start_p;

  current = *str_curr_p;

  bool has_whole_part = false;
  bool has_fraction_part = false;

  /* Check digits of whole part. */
  if (lit_char_is_decimal_digit (current))
  {
    has_whole_part = true;
    str_curr_p++;

    while (str_curr_p < str_end_p)
    {
      current = *str_curr_p++;
      if (!lit_char_is_decimal_digit (current))
      {
        str_curr_p--;
        break;
      }
    }
  }

  /* Set end position to the end of whole part. */
  end_p = str_curr_p;
  if (str_curr_p < str_end_p)
  {
    current = *str_curr_p;

    /* Check decimal point. */
    if (current == LIT_CHAR_DOT)
    {
      str_curr_p++;

      if (str_curr_p < str_end_p)
      {
        current = *str_curr_p;

        if (lit_char_is_decimal_digit (current))
        {
          has_fraction_part = true;

          /* Check digits of fractional part. */
          while (str_curr_p < str_end_p)
          {
            current = *str_curr_p++;
            if (!lit_char_is_decimal_digit (current))
            {
              str_curr_p--;
              break;
            }
          }

          /* Set end position to end of fraction part. */
          end_p = str_curr_p;
        }
      }
    }
  }

  if (str_curr_p < str_end_p)
  {
    current = *str_curr_p++;
  }

  /* Check exponent. */
  if ((current == LIT_CHAR_LOWERCASE_E || current == LIT_CHAR_UPPERCASE_E)
      && (has_whole_part || has_fraction_part)
      && str_curr_p < str_end_p)
  {
    current = *str_curr_p++;

    /* Check sign of exponent. */
    if ((current == LIT_CHAR_PLUS || current == LIT_CHAR_MINUS)
         && str_curr_p < str_end_p)
    {
      current = *str_curr_p++;
    }

    if (lit_char_is_decimal_digit (current))
    {
      /* Check digits of exponent part. */
      while (str_curr_p < str_end_p)
      {
        current = *str_curr_p++;
        if (!lit_char_is_decimal_digit (current))
        {
          str_curr_p--;
          break;
        }
      }

      /* Set end position to end of exponent part. */
      end_p = str_curr_p;
    }
  }

  /* String did not contain a valid number. */
  if (start_p == end_p)
  {
    return ecma_make_nan_value ();
  }

  /* 5. */
  ecma_number_t ret_num = ecma_utf8_string_to_number (start_p, (lit_utf8_size_t) (end_p - start_p), 0);

  if (sign)
  {
    ret_num *= ECMA_NUMBER_MINUS_ONE;
  }

  return ecma_make_number_value (ret_num);
} /* ecma_number_parse_float */

/**
 * @}
 * @}
 */
