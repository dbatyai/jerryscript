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

#include "ecma-line-info.h"
#include "jmem.h"

#if ENABLED (JERRY_LINE_INFO)

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup TODO
 * @{
 */

/**
 * TODO
 */
void
ecma_line_info_initialize (ecma_line_info_encoder_t *encoder_p,
                           uint32_t line,
                           uint32_t column)
{
  encoder_p->current_offset = 0;
  encoder_p->current_line = line;
  encoder_p->current_column = column;

  const uint32_t initial_size = sizeof (ecma_line_info_t);
  ecma_line_info_t *line_info_p = jmem_heap_alloc_block (initial_size);
  line_info_p->start_line = line;
  line_info_p->start_column = column;
  line_info_p->size = initial_size;

  encoder_p->line_info_p = line_info_p;
} /* ecma_line_info_initialize */

/**
 * TODO
 */
static void
ecma_line_info_append (ecma_line_info_encoder_t *encoder_p,
                       uint8_t *data_p,
                       uint32_t data_size)
{
  const uint32_t old_size = encoder_p->line_info_p->size;
  const uint32_t new_size = old_size + data_size;
  encoder_p->line_info_p = jmem_heap_realloc_block (encoder_p->line_info_p, old_size, new_size);
  encoder_p->line_info_p->size = new_size;

  uint8_t *dest_p = ((uint8_t *) encoder_p->line_info_p) + old_size;
  memcpy (dest_p, data_p, data_size);
} /* ecma_line_info_append */

/**
 * TODO
 */
void
ecma_line_info_encode (ecma_line_info_encoder_t *encoder_p,
                       uint32_t offset,
                       uint32_t line,
                       uint32_t column)
{
  if (line == encoder_p->current_line && column == encoder_p->current_column)
  {
    return;
  }

  uint8_t row[ECMA_LINE_INFO_MAX_ENCODED_ROW_SIZE];
  uint8_t *current_p = row;

  const uint32_t offset_delta = offset - encoder_p->current_offset;
  const int32_t column_delta = (int32_t) (column - encoder_p->current_column);

  JERRY_ASSERT (offset > encoder_p->current_offset && offset_delta > 0);

  if (column_delta <= INT8_MAX && column_delta >= INT8_MIN
      && /* TODO */ offset_delta > 0)
  {
    if (line < encoder_p->current_line && offset_delta <= UINT8_MAX)
    {
      const uint32_t line_delta = encoder_p->current_line - line;
      if (line_delta < UINT8_MAX)
      {
        *current_p++ = ECMA_LINE_INFO_OP_DECR_LINE;
        *current_p++ = (uint8_t) offset_delta;
        *current_p++ = (uint8_t) line_delta;
        *current_p++ = (uint8_t) column_delta;
        goto append_row;
      }
    }

    if (offset_delta <= ECMA_LINE_INFO_MAX_DIRECT_OFFSET)
    {
      JERRY_ASSERT (line >= encoder_p->current_line);
      const uint32_t line_delta = line - encoder_p->current_line;

      if (line_delta == 0 && column_delta > 0)
      {
        *current_p++ = (uint8_t) (offset_delta + ECMA_LINE_INFO_DIRECT_OFFSET_BASE);
        *current_p++ = (uint8_t) (column_delta + ECMA_LINE_INFO_DIRECT_COLUMN_BASE);
        goto append_row;
      }
      else if (line_delta <= ECMA_LINE_INFO_MAX_DIRECT_LINE)
      {
        *current_p++ = (uint8_t) offset_delta + ECMA_LINE_INFO_DIRECT_OFFSET_BASE;
        *current_p++ = (uint8_t) line_delta;
        *current_p++ = (uint8_t) column_delta;
        goto append_row;
      }
    }
  }

  /* TODO: use variadic encoding */
  *current_p++ = ECMA_LINE_INFO_OP_SET_ALL;
  memcpy (current_p, &offset, sizeof (uint32_t));
  current_p += sizeof (uint32_t);
  memcpy (current_p, &line, sizeof (uint32_t));
  current_p += sizeof (uint32_t);
  memcpy (current_p, &column, sizeof (uint32_t));
  current_p += sizeof (uint32_t);

append_row:
  encoder_p->current_offset = offset;
  encoder_p->current_line = line;
  encoder_p->current_column = column;

  const uint32_t row_size = (uint32_t) (current_p - row);
  JERRY_ASSERT (row_size <= ECMA_LINE_INFO_MAX_ENCODED_ROW_SIZE);
  ecma_line_info_append (encoder_p, row, row_size);

  printf ("encode (%u %u %u) -> (", offset, line, column);
  for (uint32_t i = 0; i < row_size; i++)
  {
    printf("%hhu ", row[i]);
  }

  printf(")\n");
} /* ecma_line_info_encode */

/**
 * TODO
 */
const ecma_line_info_t *
ecma_line_info_finalize (ecma_line_info_encoder_t *encoder_p)
{
  uint8_t row[] = { ECMA_LINE_INFO_OP_END };
  ecma_line_info_append (encoder_p, row, sizeof (row));
  return (const ecma_line_info_t *) encoder_p->line_info_p;
} /* ecma_line_info_finalize */

/**
 * TODO
 */
void
ecma_line_info_lookup (const ecma_line_info_t *line_info_p,
                       uint32_t offset,
                       uint32_t *line_p,
                       uint32_t *column_p)
{
  uint32_t line = line_info_p->start_line;
  uint32_t column = line_info_p->start_column;
  uint32_t current_offset = 0;

  const uint8_t *data_p = (const uint8_t *) (line_info_p + 1);

  while (true)
  {
    JERRY_ASSERT (data_p < ((uint8_t *) line_info_p) + line_info_p->size);
    const uint8_t opcode = *data_p++;
    switch (opcode)
    {
      case ECMA_LINE_INFO_OP_SET_ALL:
      {
        memcpy (&current_offset, data_p, sizeof (uint32_t));

        if (current_offset > offset)
        {
          goto found;
        }

        data_p += sizeof (uint32_t);
        memcpy (&line, data_p, sizeof (uint32_t));
        data_p += sizeof (uint32_t);
        memcpy (&column, data_p, sizeof (uint32_t));
        data_p += sizeof (uint32_t);

        break;
      }
      case ECMA_LINE_INFO_OP_DECR_LINE:
      {
        const uint32_t offset_delta = *data_p++;
        current_offset += offset_delta;

        if (current_offset > offset)
        {
          goto found;
        }

        const uint8_t line_delta = *data_p++;
        const int8_t column_delta = (int8_t) *data_p++;

        line -= line_delta;
        column = (uint32_t) ((int32_t) column + column_delta);
        break;
      }
      case ECMA_LINE_INFO_OP_END:
      {
        goto found;
      }
      default:
      {
        JERRY_ASSERT (opcode > ECMA_LINE_INFO_OP__COUNT);
        const uint8_t offset_delta = opcode - ECMA_LINE_INFO_DIRECT_OFFSET_BASE;
        current_offset += offset_delta;

        if (current_offset > offset)
        {
          goto found;
        }

        if (*data_p <= ECMA_LINE_INFO_MAX_DIRECT_LINE)
        {
          const uint8_t line_delta = *data_p++;
          line += line_delta;

          const int8_t column_delta = (int8_t) *data_p++;
          column = (uint32_t) ((int32_t) column + column_delta);
        } else {
          const uint8_t column_delta = *data_p++ - ECMA_LINE_INFO_DIRECT_COLUMN_BASE;
          column += column_delta;
        }

        break;
      }
    }
  }

found:
  *line_p = line;
  *column_p = column;
  printf ("%u: %u %u\n", offset, line, column);
} /* ecma_line_info_lookup */

/**
 * TODO
 */
void
ecma_line_info_release (const ecma_line_info_t *line_info_p)
{
  jmem_heap_free_block ((void *) line_info_p, line_info_p->size);
} /*ecma_line_info_release */

/**
 * @}
 * @}
 */

#endif /* ENABLED (JERRY_LINE_INFO) */
