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
#include "ecma-init-finalize.h"
#include "jmem.h"

#include "test-common.h"

int
main (void)
{
  TEST_INIT ();

  jmem_init ();
  ecma_init ();

  {
    ecma_line_info_encoder_t enc;
    ecma_line_info_initialize (&enc, 1, 1);

    ecma_line_info_encode (&enc, 1, 1, 2);
    ecma_line_info_encode (&enc, 2, 1, 3);
    ecma_line_info_encode (&enc, 3, 1, 3);
    ecma_line_info_encode (&enc, 4, 2, 1);

    ecma_line_info_encode (&enc, 300, 3, 1);
    ecma_line_info_encode (&enc, 301, 300, 1);
    ecma_line_info_encode (&enc, 302, 300, 300);
    ecma_line_info_encode (&enc, 303, 301, 1);
    ecma_line_info_encode (&enc, 304, 302, 300);
    ecma_line_info_encode (&enc, 307, 296, 250);
    ecma_line_info_encode (&enc, 309, 296, 220);

    const ecma_line_info_t *line_info_p = ecma_line_info_finalize (&enc);

    uint32_t line;
    uint32_t column;

    ecma_line_info_lookup (line_info_p, 0, &line, &column);
    TEST_ASSERT (line == 1 && column == 1);

    ecma_line_info_lookup (line_info_p, 1, &line, &column);
    TEST_ASSERT (line == 1 && column == 2);

    ecma_line_info_lookup (line_info_p, 2, &line, &column);
    TEST_ASSERT (line == 1 && column == 3);

    ecma_line_info_lookup (line_info_p, 3, &line, &column);
    TEST_ASSERT (line == 1 && column == 3);

    ecma_line_info_lookup (line_info_p, 4, &line, &column);
    TEST_ASSERT (line == 2 && column == 1);

    ecma_line_info_lookup (line_info_p, 5, &line, &column);
    TEST_ASSERT (line == 2 && column == 1);

    ecma_line_info_lookup (line_info_p, 6, &line, &column);
    TEST_ASSERT (line == 2 && column == 1);

    ecma_line_info_lookup (line_info_p, 300, &line, &column);
    TEST_ASSERT (line == 3 && column == 1);

    ecma_line_info_lookup (line_info_p, 301, &line, &column);
    TEST_ASSERT (line == 300 && column == 1);

    ecma_line_info_lookup (line_info_p, 302, &line, &column);
    TEST_ASSERT (line == 300 && column == 300);

    ecma_line_info_lookup (line_info_p, 303, &line, &column);
    TEST_ASSERT (line == 301 && column == 1);

    ecma_line_info_lookup (line_info_p, 304, &line, &column);
    TEST_ASSERT (line == 302 && column == 300);

    ecma_line_info_lookup (line_info_p, 305, &line, &column);
    TEST_ASSERT (line == 302 && column == 300);

    ecma_line_info_lookup (line_info_p, 306, &line, &column);
    TEST_ASSERT (line == 302 && column == 300);

    ecma_line_info_lookup (line_info_p, 307, &line, &column);
    TEST_ASSERT (line == 296 && column == 250);

    ecma_line_info_lookup (line_info_p, 308, &line, &column);
    TEST_ASSERT (line == 296 && column == 250);

    ecma_line_info_lookup (line_info_p, 309, &line, &column);
    TEST_ASSERT (line == 296 && column == 220);

    ecma_line_info_lookup (line_info_p, 310, &line, &column);
    TEST_ASSERT (line == 296 && column == 220);

    ecma_line_info_release (line_info_p);
  }

  ecma_finalize ();
  jmem_finalize ();

  return 0;
} /* main */
