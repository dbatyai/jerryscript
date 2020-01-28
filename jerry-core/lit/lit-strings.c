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

#include "lit-strings.h"

#include "jrt-libc-includes.h"

/**
 * Validate utf-8 string
 *
 * NOTE:
 *   Isolated surrogates are allowed.
 *   Correct pair of surrogates is not allowed, it should be represented as 4-byte utf-8 character.
 *
 * @return true if utf-8 string is well-formed
 *         false otherwise
 */
bool
lit_is_valid_utf8_string (const lit_utf8_byte_t *utf8_buf_p, /**< utf-8 string */
                          lit_utf8_size_t buf_size) /**< string size */
{
  lit_utf8_size_t idx = 0;

  bool is_prev_code_point_high_surrogate = false;
  while (idx < buf_size)
  {
    lit_utf8_byte_t c = utf8_buf_p[idx++];
    if ((c & LIT_UTF8_1_BYTE_MASK) == LIT_UTF8_1_BYTE_MARKER)
    {
      is_prev_code_point_high_surrogate = false;
      continue;
    }

    lit_code_point_t code_point = 0;
    lit_code_point_t min_code_point = 0;
    lit_utf8_size_t extra_bytes_count;
    if ((c & LIT_UTF8_2_BYTE_MASK) == LIT_UTF8_2_BYTE_MARKER)
    {
      extra_bytes_count = 1;
      min_code_point = LIT_UTF8_2_BYTE_CODE_POINT_MIN;
      code_point = ((uint32_t) (c & LIT_UTF8_LAST_5_BITS_MASK));
    }
    else if ((c & LIT_UTF8_3_BYTE_MASK) == LIT_UTF8_3_BYTE_MARKER)
    {
      extra_bytes_count = 2;
      min_code_point = LIT_UTF8_3_BYTE_CODE_POINT_MIN;
      code_point = ((uint32_t) (c & LIT_UTF8_LAST_4_BITS_MASK));
    }
    else if ((c & LIT_UTF8_4_BYTE_MASK) == LIT_UTF8_4_BYTE_MARKER)
    {
      extra_bytes_count = 3;
      min_code_point = LIT_UTF8_4_BYTE_CODE_POINT_MIN;
      code_point = ((uint32_t) (c & LIT_UTF8_LAST_3_BITS_MASK));
    }
    else
    {
      /* utf-8 string could not contain 5- and 6-byte sequences. */
      return false;
    }

    if (idx + extra_bytes_count > buf_size)
    {
      /* utf-8 string breaks in the middle */
      return false;
    }

    for (lit_utf8_size_t offset = 0; offset < extra_bytes_count; ++offset)
    {
      c = utf8_buf_p[idx + offset];
      if ((c & LIT_UTF8_EXTRA_BYTE_MASK) != LIT_UTF8_EXTRA_BYTE_MARKER)
      {
        /* invalid continuation byte */
        return false;
      }
      code_point <<= LIT_UTF8_BITS_IN_EXTRA_BYTES;
      code_point |= (c & LIT_UTF8_LAST_6_BITS_MASK);
    }

    if (code_point < min_code_point
        || code_point > LIT_UNICODE_CODE_POINT_MAX)
    {
      /* utf-8 string doesn't encode valid unicode code point */
      return false;
    }

    if (code_point >= LIT_UTF16_HIGH_SURROGATE_MIN
        && code_point <= LIT_UTF16_HIGH_SURROGATE_MAX)
    {
      is_prev_code_point_high_surrogate = true;
    }
    else if (code_point >= LIT_UTF16_LOW_SURROGATE_MIN
             && code_point <= LIT_UTF16_LOW_SURROGATE_MAX
             && is_prev_code_point_high_surrogate)
    {
      /* sequence of high and low surrogate is not allowed */
      return false;
    }
    else
    {
      is_prev_code_point_high_surrogate = false;
    }

    idx += extra_bytes_count;
  }

  return true;
} /* lit_is_valid_utf8_string */

/**
 * Validate cesu-8 string
 *
 * @return true if cesu-8 string is well-formed
 *         false otherwise
 */
bool
lit_is_valid_cesu8_string (const lit_utf8_byte_t *cesu8_buf_p, /**< cesu-8 string */
                           lit_utf8_size_t buf_size) /**< string size */
{
  lit_utf8_size_t idx = 0;

  while (idx < buf_size)
  {
    lit_utf8_byte_t c = cesu8_buf_p[idx++];
    if ((c & LIT_UTF8_1_BYTE_MASK) == LIT_UTF8_1_BYTE_MARKER)
    {
      continue;
    }

    lit_code_point_t code_point = 0;
    lit_code_point_t min_code_point = 0;
    lit_utf8_size_t extra_bytes_count;
    if ((c & LIT_UTF8_2_BYTE_MASK) == LIT_UTF8_2_BYTE_MARKER)
    {
      extra_bytes_count = 1;
      min_code_point = LIT_UTF8_2_BYTE_CODE_POINT_MIN;
      code_point = ((uint32_t) (c & LIT_UTF8_LAST_5_BITS_MASK));
    }
    else if ((c & LIT_UTF8_3_BYTE_MASK) == LIT_UTF8_3_BYTE_MARKER)
    {
      extra_bytes_count = 2;
      min_code_point = LIT_UTF8_3_BYTE_CODE_POINT_MIN;
      code_point = ((uint32_t) (c & LIT_UTF8_LAST_4_BITS_MASK));
    }
    else
    {
      return false;
    }

    if (idx + extra_bytes_count > buf_size)
    {
      /* cesu-8 string breaks in the middle */
      return false;
    }

    for (lit_utf8_size_t offset = 0; offset < extra_bytes_count; ++offset)
    {
      c = cesu8_buf_p[idx + offset];
      if ((c & LIT_UTF8_EXTRA_BYTE_MASK) != LIT_UTF8_EXTRA_BYTE_MARKER)
      {
        /* invalid continuation byte */
        return false;
      }
      code_point <<= LIT_UTF8_BITS_IN_EXTRA_BYTES;
      code_point |= (c & LIT_UTF8_LAST_6_BITS_MASK);
    }

    if (code_point < min_code_point)
    {
      /* cesu-8 string doesn't encode valid unicode code point */
      return false;
    }

    idx += extra_bytes_count;
  }

  return true;
} /* lit_is_valid_cesu8_string */

/**
 * Check if the code point is UTF-16 low surrogate
 *
 * @return true / false
 */
bool
lit_is_code_point_utf16_low_surrogate (lit_code_point_t code_point) /**< code point */
{
  return LIT_UTF16_LOW_SURROGATE_MIN <= code_point && code_point <= LIT_UTF16_LOW_SURROGATE_MAX;
} /* lit_is_code_point_utf16_low_surrogate */

/**
 * Check if the code point is UTF-16 high surrogate
 *
 * @return true / false
 */
bool
lit_is_code_point_utf16_high_surrogate (lit_code_point_t code_point) /**< code point */
{
  return LIT_UTF16_HIGH_SURROGATE_MIN <= code_point && code_point <= LIT_UTF16_HIGH_SURROGATE_MAX;
} /* lit_is_code_point_utf16_high_surrogate */

/**
 * Represents code point (>0xFFFF) as surrogate pair and returns its lower part
 *
 * @return lower code_unit of the surrogate pair
 */
static ecma_char_t
convert_code_point_to_low_surrogate (lit_code_point_t code_point) /**< code point, should be > 0xFFFF */
{
  JERRY_ASSERT (code_point > LIT_UTF16_CODE_UNIT_MAX);

  ecma_char_t code_unit_bits;
  code_unit_bits = (ecma_char_t) (code_point & LIT_UTF16_LAST_10_BITS_MASK);

  return (ecma_char_t) (LIT_UTF16_LOW_SURROGATE_MARKER | code_unit_bits);
} /* convert_code_point_to_low_surrogate */

/**
 * Represents code point (>0xFFFF) as surrogate pair and returns its higher part
 *
 * @return higher code_unit of the surrogate pair
 */
static ecma_char_t
convert_code_point_to_high_surrogate (lit_code_point_t code_point) /**< code point, should be > 0xFFFF */
{
  JERRY_ASSERT (code_point > LIT_UTF16_CODE_UNIT_MAX);
  JERRY_ASSERT (code_point <= LIT_UNICODE_CODE_POINT_MAX);

  ecma_char_t code_unit_bits;
  code_unit_bits = (ecma_char_t) ((code_point - LIT_UTF16_FIRST_SURROGATE_CODE_POINT) >> LIT_UTF16_BITS_IN_SURROGATE);

  return (LIT_UTF16_HIGH_SURROGATE_MARKER | code_unit_bits);
} /* convert_code_point_to_high_surrogate */

/**
 * UTF16 Encoding method for a code point
 *
 * See also:
 *          ECMA-262 v6, 10.1.1
 *
 * @return uint8_t, the number of returning code points
 */
uint8_t
lit_utf16_encode_code_point (lit_code_point_t cp, /**< the code point we encode */
                             ecma_char_t *cu_p) /**< result of the encoding */
{
  if (cp <= LIT_UTF16_CODE_UNIT_MAX)
  {
    cu_p[0] = (ecma_char_t) cp;
    return 1;
  }

  cu_p[0] = convert_code_point_to_high_surrogate (cp);
  cu_p[1] = convert_code_point_to_low_surrogate (cp);
  return 2;
} /* lit_utf16_encode_code_point */

/**
 * Calculate size of a zero-terminated utf-8 string
 *
 * NOTE:
 *   - string cannot be NULL
 *   - string should not contain zero characters in the middle
 *
 * @return size of a string
 */
lit_utf8_size_t
lit_zt_utf8_string_size (const lit_utf8_byte_t *utf8_str_p) /**< zero-terminated utf-8 string */
{
  JERRY_ASSERT (utf8_str_p != NULL);
  return (lit_utf8_size_t) strlen ((const char *) utf8_str_p);
} /* lit_zt_utf8_string_size */

/**
 * Calculate length of a cesu-8 encoded string
 *
 * @return UTF-16 code units count
 */
ecma_length_t
lit_utf8_string_length (const lit_utf8_byte_t *utf8_buf_p, /**< utf-8 string */
                        lit_utf8_size_t utf8_buf_size) /**< string size */
{
  ecma_length_t length = 0;
  const lit_utf8_byte_t *const utf8_buf_end_p = (lit_utf8_byte_t *) (utf8_buf_p + utf8_buf_size);

  while (utf8_buf_p < utf8_buf_end_p)
  {
    lit_utf8_incr (&utf8_buf_p);
    length++;
  }

  JERRY_ASSERT (utf8_buf_p == utf8_buf_end_p);

  return length;
} /* lit_utf8_string_length */

/**
 * Calculate the required size of an utf-8 encoded string from cesu-8 encoded string
 *
 * @return size of an utf-8 encoded string
 */
lit_utf8_size_t
lit_get_utf8_size_of_cesu8_string (const lit_utf8_byte_t *cesu8_buf_p, /**< cesu-8 string */
                                   const lit_utf8_size_t cesu8_buf_size) /**< string size */
{
  const lit_utf8_byte_t *const cesu8_end_p = (lit_utf8_byte_t *) (cesu8_buf_p + cesu8_buf_size);
  lit_utf8_size_t utf8_buf_size = cesu8_buf_size;
  ecma_char_t prev_ch = 0;

  while (cesu8_buf_p < cesu8_end_p)
  {
    const ecma_char_t ch = lit_utf8_read_code_unit (&cesu8_buf_p);

    if (lit_is_code_point_utf16_low_surrogate (ch) && lit_is_code_point_utf16_high_surrogate (prev_ch))
    {
      utf8_buf_size -= 2;
    }

    prev_ch = ch;
  }

  JERRY_ASSERT (cesu8_buf_p == cesu8_end_p);

  return utf8_buf_size;
} /* lit_get_utf8_size_of_cesu8_string */

/**
 * Calculate length of an utf-8 encoded string from cesu-8 encoded string
 *
 * @return length of an utf-8 encoded string
 */
ecma_length_t
lit_get_utf8_length_of_cesu8_string (const lit_utf8_byte_t *cesu8_buf_p, /**< cesu-8 string */
                                     const lit_utf8_size_t cesu8_buf_size) /**< string size */
{
  const lit_utf8_byte_t *const cesu8_end_p = (lit_utf8_byte_t *) (cesu8_buf_p + cesu8_buf_size);
  ecma_length_t utf8_length = 0;
  ecma_char_t prev_ch = 0;

  while (cesu8_buf_p < cesu8_end_p)
  {
    const ecma_char_t ch = lit_utf8_read_code_unit (&cesu8_buf_p);

    if (!lit_is_code_point_utf16_low_surrogate (ch) || !lit_is_code_point_utf16_high_surrogate (prev_ch))
    {
      utf8_length++;
    }

    prev_ch = ch;
  }

  JERRY_ASSERT (cesu8_buf_p == cesu8_end_p);

  return utf8_length;
} /* lit_get_utf8_length_of_cesu8_string */

/**
 * TODO
 */
inline static lit_code_point_t JERRY_ATTR_ALWAYS_INLINE
lit_utf8_decode (const lit_utf8_byte_t **buf_p,  /**< [in/out] buffer with characters */
                 lit_utf8_size_t *size_p)
{
  static const uint8_t remaining_bytes[] = {1, 1, 2, 3};

#define LIT_UTF8_PAYLOAD_MASK 0x3F
#define LIT_UTF8_MARKER_MASK 0xC0

  JERRY_ASSERT (buf_p != NULL);
  const lit_utf8_byte_t* current_p = *buf_p;
  JERRY_ASSERT (current_p != NULL);

  lit_code_point_t cp = *current_p++;
  if (JERRY_LIKELY ((cp & LIT_UTF8_1_BYTE_MASK) == LIT_UTF8_1_BYTE_MARKER))
  {
    *size_p = 1;
    *buf_p = current_p;
    return cp;
  }

  const uint32_t marker = (cp - LIT_UTF8_MARKER_MASK) >> 4;
  uint8_t bytes = remaining_bytes[marker];
  cp &= (lit_code_point_t) (LIT_UTF8_PAYLOAD_MASK >> bytes);
  *size_p = (lit_utf8_size_t) (bytes + 1);

  while (bytes--)
  {
    cp <<= LIT_UTF8_BITS_IN_EXTRA_BYTES;
    cp |= (*current_p++ & LIT_UTF8_LAST_6_BITS_MASK);
  }

  *buf_p = current_p;
  return cp;
} /* lit_utf8_decode */

/**
 * TODO Decodes a unicode code point from non-empty utf-8-encoded buffer
 *
 * @return number of bytes occupied by code point in the string
 */
lit_code_point_t
lit_utf8_read_code_point (const lit_utf8_byte_t **buf_p) /**< [in/out] buffer with characters */
{
  lit_utf8_size_t size;
  return lit_utf8_decode (buf_p, &size);
} /* lit_utf8_read_code_point */

/**
 * TODO
 */
lit_code_point_t
lit_utf8_read_code_point_size (const lit_utf8_byte_t *buf_p,
                               lit_utf8_size_t *size_p)
{
  return lit_utf8_decode (&buf_p, size_p);
} /* lit_utf8_read_code_point_size */

/**
 * Decodes a unicode code unit from non-empty cesu-8-encoded buffer
 *
 * @return number of bytes occupied by code point in the string
 */
ecma_char_t
lit_utf8_read_code_unit (const lit_utf8_byte_t **buf_p) /**< [in/out] buffer with characters */
{
  JERRY_ASSERT (buf_p != NULL);
  const lit_code_point_t cp = lit_utf8_read_code_point (buf_p);

  JERRY_ASSERT (cp <= LIT_UTF16_CODE_UNIT_MAX);

  return (ecma_char_t) cp;
} /* lit_utf8_read_code_unit */

/**
 * Decodes a unicode code unit from non-empty cesu-8-encoded buffer
 *
 * @return number of bytes occupied by code point in the string
 */
ecma_char_t
lit_utf8_read_code_unit_size (const lit_utf8_byte_t *buf_p, /**< buffer with characters */
                              lit_utf8_size_t *size_p)
{
  JERRY_ASSERT (buf_p != NULL);
  const lit_code_point_t cp = lit_utf8_read_code_point_size (buf_p, size_p);

  JERRY_ASSERT (cp <= LIT_UTF16_CODE_UNIT_MAX);

  return (ecma_char_t) cp;
} /* lit_utf8_read_code_unit */

/**
 * Decodes a unicode code unit from non-empty cesu-8-encoded buffer
 *
 * @return next code unit
 */
inline ecma_char_t JERRY_ATTR_ALWAYS_INLINE
lit_cesu8_read_next (const lit_utf8_byte_t **buf_p) /**< [in,out] buffer with characters */
{
  JERRY_ASSERT (buf_p != NULL && *buf_p != NULL);

  return lit_utf8_read_code_unit (buf_p);
} /* lit_cesu8_read_next */

/**
 * Decodes a unicode code unit from non-empty cesu-8-encoded buffer
 *
 * @return previous code unit
 */
inline ecma_char_t JERRY_ATTR_ALWAYS_INLINE
lit_cesu8_read_prev (const lit_utf8_byte_t **buf_p) /**< [in,out] buffer with characters */
{
  JERRY_ASSERT (buf_p != NULL && *buf_p != NULL);

  lit_utf8_decr (buf_p);
  return lit_cesu8_peek_next (*buf_p);
} /* lit_cesu8_read_prev */

/**
 * Decodes a unicode code unit from non-empty cesu-8-encoded buffer
 *
 * @return next code unit
 */
ecma_char_t
lit_cesu8_peek_next (const lit_utf8_byte_t *buf_p) /**< [in,out] buffer with characters */
{
  JERRY_ASSERT (buf_p != NULL);
  lit_utf8_size_t size;

  const lit_code_point_t cp = lit_utf8_decode (&buf_p, &size);
  JERRY_ASSERT (cp <= LIT_UTF16_CODE_UNIT_MAX);

  return (ecma_char_t) cp;
} /* lit_cesu8_peek_next */

/**
 * Decodes a unicode code unit from non-empty cesu-8-encoded buffer
 *
 * @return previous code unit
 */
inline ecma_char_t JERRY_ATTR_ALWAYS_INLINE
lit_cesu8_peek_prev (const lit_utf8_byte_t *buf_p) /**< [in,out] buffer with characters */
{
  JERRY_ASSERT (buf_p != NULL);
  lit_utf8_decr (&buf_p);
  return lit_cesu8_peek_next (buf_p);
} /* lit_cesu8_peek_prev */

/**
 * Increase cesu-8 encoded string pointer by one code unit.
 */
inline void JERRY_ATTR_ALWAYS_INLINE
lit_utf8_incr (const lit_utf8_byte_t **buf_p) /**< [in,out] buffer with characters */
{
  JERRY_ASSERT (*buf_p != NULL);

  *buf_p += lit_utf8_get_encoded_size (**buf_p);
} /* lit_utf8_incr */

/**
 * Decrease cesu-8 encoded string pointer by one code unit.
 */
inline void JERRY_ATTR_ALWAYS_INLINE
lit_utf8_decr (const lit_utf8_byte_t **buf_p) /**< [in,out] buffer with characters */
{
  JERRY_ASSERT (*buf_p);
  const lit_utf8_byte_t *current_p = *buf_p;

  do
  {
    current_p--;
  }
  while ((*(current_p) & LIT_UTF8_EXTRA_BYTE_MASK) == LIT_UTF8_EXTRA_BYTE_MARKER);

  *buf_p = current_p;
} /* lit_utf8_decr */

/**
 * Calc hash using the specified hash_basis.
 *
 * NOTE:
 *   This is implementation of FNV-1a hash function, which is released into public domain.
 *   Constants used, are carefully picked primes by the authors.
 *   More info: http://www.isthe.com/chongo/tech/comp/fnv/
 *
 * @return ecma-string's hash
 */
inline lit_string_hash_t JERRY_ATTR_ALWAYS_INLINE
lit_utf8_string_hash_combine (lit_string_hash_t hash_basis, /**< hash to be combined with */
                              const lit_utf8_byte_t *utf8_buf_p, /**< characters buffer */
                              lit_utf8_size_t utf8_buf_size) /**< number of characters in the buffer */
{
  JERRY_ASSERT (utf8_buf_p != NULL || utf8_buf_size == 0);

  uint32_t hash = hash_basis;

  for (uint32_t i = 0; i < utf8_buf_size; i++)
  {
    /* 16777619 is 32 bit FNV_prime = 2^24 + 2^8 + 0x93 = 16777619 */
    hash = (hash ^ utf8_buf_p[i]) * 16777619;
  }

  return (lit_string_hash_t) hash;
} /* lit_utf8_string_hash_combine */

/**
 * Calculate hash from the buffer.
 *
 * @return ecma-string's hash
 */
inline lit_string_hash_t JERRY_ATTR_ALWAYS_INLINE
lit_utf8_string_calc_hash (const lit_utf8_byte_t *utf8_buf_p, /**< characters buffer */
                           lit_utf8_size_t utf8_buf_size) /**< number of characters in the buffer */
{
  JERRY_ASSERT (utf8_buf_p != NULL || utf8_buf_size == 0);

  /* 32 bit offset_basis for FNV = 2166136261 */
  return lit_utf8_string_hash_combine ((lit_string_hash_t) 2166136261, utf8_buf_p, utf8_buf_size);
} /* lit_utf8_string_calc_hash */

/**
 * Return code unit at the specified position in string
 *
 * NOTE:
 *   code_unit_offset should be less then string's length
 *
 * @return code unit value
 */
ecma_char_t
lit_utf8_string_code_unit_at (const lit_utf8_byte_t *utf8_buf_p, /**< utf-8 string */
                              lit_utf8_size_t utf8_buf_size, /**< string size in bytes */
                              ecma_length_t code_unit_offset) /**< ofset of a code_unit */
{
  const lit_utf8_byte_t *current_p = (lit_utf8_byte_t *) utf8_buf_p;

  while (code_unit_offset--)
  {
    JERRY_ASSERT (current_p < utf8_buf_p + utf8_buf_size);
    lit_utf8_incr (&current_p);
  }

  return lit_utf8_read_code_unit (&current_p);
} /* lit_utf8_string_code_unit_at */

static const uint8_t utf8_lengths[] = {2, 2, 3, 4};

/**
 * Get CESU-8 encoded size of character
 *
 * @return number of bytes occupied in CESU-8
 */
lit_utf8_size_t
lit_utf8_get_encoded_size (const lit_utf8_byte_t first_byte) /**< buffer with characters */
{
  if (JERRY_LIKELY ((first_byte & LIT_UTF8_1_BYTE_MASK) == LIT_UTF8_1_BYTE_MARKER))
  {
    return 1;
  }

  const uint8_t marker = (uint8_t) ((first_byte - 0xC0) >> 4);
  return (lit_utf8_size_t) utf8_lengths[marker];
} /* lit_utf8_get_encoded_size */

/**
 * Convert code unit to cesu-8 representation
 *
 * @return byte count required to represent the code unit
 */
lit_utf8_size_t
lit_code_unit_to_utf8 (ecma_char_t code_unit, /**< code unit */
                       lit_utf8_byte_t *buf_p) /**< buffer where to store the result and its size
                                                *   should be at least LIT_UTF8_MAX_BYTES_IN_CODE_UNIT */
{
  if (code_unit <= LIT_UTF8_1_BYTE_CODE_POINT_MAX)
  {
    buf_p[0] = (lit_utf8_byte_t) code_unit;
    return 1;
  }
  else if (code_unit <= LIT_UTF8_2_BYTE_CODE_POINT_MAX)
  {
    uint32_t code_unit_bits = code_unit;
    lit_utf8_byte_t second_byte_bits = (lit_utf8_byte_t) (code_unit_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_unit_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t first_byte_bits = (lit_utf8_byte_t) (code_unit_bits & LIT_UTF8_LAST_5_BITS_MASK);
    JERRY_ASSERT (first_byte_bits == code_unit_bits);

    buf_p[0] = LIT_UTF8_2_BYTE_MARKER | first_byte_bits;
    buf_p[1] = LIT_UTF8_EXTRA_BYTE_MARKER | second_byte_bits;
    return 2;
  }
  else
  {
    uint32_t code_unit_bits = code_unit;
    lit_utf8_byte_t third_byte_bits = (lit_utf8_byte_t) (code_unit_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_unit_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t second_byte_bits = (lit_utf8_byte_t) (code_unit_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_unit_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t first_byte_bits = (lit_utf8_byte_t) (code_unit_bits & LIT_UTF8_LAST_4_BITS_MASK);
    JERRY_ASSERT (first_byte_bits == code_unit_bits);

    buf_p[0] = LIT_UTF8_3_BYTE_MARKER | first_byte_bits;
    buf_p[1] = LIT_UTF8_EXTRA_BYTE_MARKER | second_byte_bits;
    buf_p[2] = LIT_UTF8_EXTRA_BYTE_MARKER | third_byte_bits;
    return 3;
  }
} /* lit_code_unit_to_utf8 */

/**
 * Convert code point to cesu-8 representation
 *
 * @return byte count required to represent the code point
 */
lit_utf8_size_t
lit_code_point_to_cesu8 (lit_code_point_t code_point, /**< code point */
                         lit_utf8_byte_t *buf) /**< buffer where to store the result,
                                                *   its size should be at least 6 bytes */
{
  if (code_point <= LIT_UTF16_CODE_UNIT_MAX)
  {
    return lit_code_unit_to_utf8 ((ecma_char_t) code_point, buf);
  }
  else
  {
    lit_utf8_size_t offset = lit_code_unit_to_utf8 (convert_code_point_to_high_surrogate (code_point), buf);
    offset += lit_code_unit_to_utf8 (convert_code_point_to_low_surrogate (code_point), buf + offset);
    return offset;
  }
} /* lit_code_point_to_cesu8 */

/**
 * Convert code point to utf-8 representation
 *
 * @return byte count required to represent the code point
 */
lit_utf8_size_t
lit_code_point_to_utf8 (lit_code_point_t code_point, /**< code point */
                        lit_utf8_byte_t *buf) /**< buffer where to store the result,
                                              *   its size should be at least 4 bytes */
{
  if (code_point <= LIT_UTF8_1_BYTE_CODE_POINT_MAX)
  {
    buf[0] = (lit_utf8_byte_t) code_point;
    return 1;
  }
  else if (code_point <= LIT_UTF8_2_BYTE_CODE_POINT_MAX)
  {
    uint32_t code_point_bits = code_point;
    lit_utf8_byte_t second_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_point_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t first_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_5_BITS_MASK);
    JERRY_ASSERT (first_byte_bits == code_point_bits);

    buf[0] = LIT_UTF8_2_BYTE_MARKER | first_byte_bits;
    buf[1] = LIT_UTF8_EXTRA_BYTE_MARKER | second_byte_bits;
    return 2;
  }
  else if (code_point <= LIT_UTF8_3_BYTE_CODE_POINT_MAX)
  {
    uint32_t code_point_bits = code_point;
    lit_utf8_byte_t third_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_point_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t second_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_point_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t first_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_4_BITS_MASK);
    JERRY_ASSERT (first_byte_bits == code_point_bits);

    buf[0] = LIT_UTF8_3_BYTE_MARKER | first_byte_bits;
    buf[1] = LIT_UTF8_EXTRA_BYTE_MARKER | second_byte_bits;
    buf[2] = LIT_UTF8_EXTRA_BYTE_MARKER | third_byte_bits;
    return 3;
  }
  else
  {
    JERRY_ASSERT (code_point <= LIT_UTF8_4_BYTE_CODE_POINT_MAX);

    uint32_t code_point_bits = code_point;
    lit_utf8_byte_t fourth_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_point_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t third_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_point_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t second_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_6_BITS_MASK);
    code_point_bits >>= LIT_UTF8_BITS_IN_EXTRA_BYTES;

    lit_utf8_byte_t first_byte_bits = (lit_utf8_byte_t) (code_point_bits & LIT_UTF8_LAST_3_BITS_MASK);
    JERRY_ASSERT (first_byte_bits == code_point_bits);

    buf[0] = LIT_UTF8_4_BYTE_MARKER | first_byte_bits;
    buf[1] = LIT_UTF8_EXTRA_BYTE_MARKER | second_byte_bits;
    buf[2] = LIT_UTF8_EXTRA_BYTE_MARKER | third_byte_bits;
    buf[3] = LIT_UTF8_EXTRA_BYTE_MARKER | fourth_byte_bits;
    return 4;
  }
} /* lit_code_point_to_utf8 */

/**
 * Convert cesu-8 string to an utf-8 string and put it into the buffer.
 * It is the caller's responsibility to make sure that the string fits in the buffer.
 *
 * @return number of bytes copied to the buffer.
 */
lit_utf8_size_t
lit_convert_cesu8_string_to_utf8_string (const lit_utf8_byte_t *cesu8_string, /**< cesu-8 string */
                                         lit_utf8_size_t cesu8_size, /**< size of cesu-8 string */
                                         lit_utf8_byte_t *utf8_string, /**< destination utf-8 buffer pointer
                                                                        * (can be NULL if buffer_size == 0) */
                                         lit_utf8_size_t utf8_size) /**< size of utf-8 buffer */
{
  const lit_utf8_byte_t *cesu8_pos = cesu8_string;
  const lit_utf8_byte_t *cesu8_end_pos = cesu8_string + cesu8_size;

  lit_utf8_byte_t *utf8_pos = utf8_string;
  lit_utf8_byte_t *utf8_end_pos = utf8_string + utf8_size;

  lit_utf8_size_t size = 0;

  ecma_char_t prev_ch = 0;

  while (cesu8_pos < cesu8_end_pos)
  {
    const lit_utf8_size_t current_size = lit_utf8_get_encoded_size (*cesu8_pos);
    const ecma_char_t ch = lit_cesu8_peek_next (cesu8_pos);

    if (lit_is_code_point_utf16_low_surrogate (ch) && lit_is_code_point_utf16_high_surrogate (prev_ch))
    {
      utf8_pos -= LIT_UTF8_MAX_BYTES_IN_CODE_UNIT;
      lit_code_point_t code_point = lit_convert_surrogate_pair_to_code_point (prev_ch, ch);
      lit_code_point_to_utf8 (code_point, utf8_pos);
      size++;
    }
    else
    {
      memcpy (utf8_pos, cesu8_pos, current_size);
      size += current_size;
    }

    lit_utf8_incr (&cesu8_pos);
    utf8_pos = utf8_string + size;
    prev_ch = ch;
  }

  JERRY_ASSERT (cesu8_pos == cesu8_end_pos);
  JERRY_ASSERT (utf8_pos <= utf8_end_pos);

  return size;
} /* lit_convert_cesu8_string_to_utf8_string */

/**
 * Convert surrogate pair to code point
 *
 * @return code point
 */
lit_code_point_t
lit_convert_surrogate_pair_to_code_point (ecma_char_t high_surrogate, /**< high surrogate code point */
                                          ecma_char_t low_surrogate) /**< low surrogate code point */
{
  JERRY_ASSERT (lit_is_code_point_utf16_high_surrogate (high_surrogate));
  JERRY_ASSERT (lit_is_code_point_utf16_low_surrogate (low_surrogate));

  lit_code_point_t code_point;
  code_point = (uint16_t) (high_surrogate - LIT_UTF16_HIGH_SURROGATE_MIN);
  code_point <<= LIT_UTF16_BITS_IN_SURROGATE;

  code_point += LIT_UTF16_FIRST_SURROGATE_CODE_POINT;

  code_point |= (uint16_t) (low_surrogate - LIT_UTF16_LOW_SURROGATE_MIN);
  return code_point;
} /* lit_convert_surrogate_pair_to_code_point */

/**
 * Relational compare of cesu-8 strings
 *
 * First string is less than second string if:
 *  - strings are not equal;
 *  - first string is prefix of second or is lexicographically less than second.
 *
 * @return true - if first string is less than second string,
 *         false - otherwise
 */
bool lit_compare_utf8_strings_relational (const lit_utf8_byte_t *string1_p, /**< utf-8 string */
                                          lit_utf8_size_t string1_size, /**< string size */
                                          const lit_utf8_byte_t *string2_p, /**< utf-8 string */
                                          lit_utf8_size_t string2_size) /**< string size */
{
  const lit_utf8_size_t compare_size = JERRY_MIN (string1_size, string2_size);
  const int cmp = memcmp (string1_p, string2_p, compare_size);

  return ((cmp < 0) || ((cmp == 0) && (string1_size < string2_size)));
} /* lit_compare_utf8_strings_relational */
