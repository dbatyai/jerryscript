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

#ifndef ECMA_LINE_INFO_H
#define ECMA_LINE_INFO_H

#include "ecma-globals.h"

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
typedef enum {
  ECMA_LINE_INFO_OP_SET_ALL,
  ECMA_LINE_INFO_OP_DECR_LINE,
  ECMA_LINE_INFO_OP_END,
  ECMA_LINE_INFO_OP__COUNT
} ecma_line_info_opcode_t;

/**
 * TODO
 */
#define ECMA_LINE_INFO_MAX_ENCODED_ROW_SIZE 0x10

/**
 * TODO
 */
#define ECMA_LINE_INFO_MAX_DIRECT_OFFSET (UINT8_MAX - ECMA_LINE_INFO_OP__COUNT)

/**
 * TODO
 */
#define ECMA_LINE_INFO_DIRECT_OFFSET_BASE (ECMA_LINE_INFO_OP__COUNT)

/**
 * TODO
 */
#define ECMA_LINE_INFO_MAX_DIRECT_COLUMN (INT8_MAX)

/**
 * TODO
 */
#define ECMA_LINE_INFO_MAX_DIRECT_LINE (UINT8_MAX - ECMA_LINE_INFO_MAX_DIRECT_COLUMN)

/**
 * TODO
 */
#define ECMA_LINE_INFO_DIRECT_COLUMN_BASE (ECMA_LINE_INFO_MAX_DIRECT_LINE)

/**
 * TODO
 */
typedef struct {
  uint32_t size;
  uint32_t start_line;
  uint32_t start_column;
} ecma_line_info_t;

/**
 * TODO
 */
typedef struct {
  ecma_line_info_t *line_info_p;
  uint32_t current_offset;
  uint32_t current_line;
  uint32_t current_column;
} ecma_line_info_encoder_t;

void
ecma_line_info_initialize (ecma_line_info_encoder_t *encoder,
                           uint32_t line,
                           uint32_t column);
void
ecma_line_info_encode (ecma_line_info_encoder_t *encoder,
                       uint32_t offset,
                       uint32_t line,
                       uint32_t column);
const ecma_line_info_t *
ecma_line_info_finalize (ecma_line_info_encoder_t *encoder);
void
ecma_line_info_lookup (const ecma_line_info_t *line_info_p,
                       uint32_t offset,
                       uint32_t *line_p,
                       uint32_t *column_p);
void
ecma_line_info_release (const ecma_line_info_t *line_info_p);

/**
 * @}
 * @}
 */

#endif /* ENABLED (JERRY_LINE_INFO) */

#endif /* !ECMA_LINE_INFO_H */
