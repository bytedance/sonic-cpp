/*
 * Copyright 2022 ByteDance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <immintrin.h>

namespace sonic_json {

namespace internal {

#define DATA_MADDUBS()                               \
  {                                                  \
    __m128i q = _mm_set1_epi64x(0x010A010A010A010A); \
    data = _mm_maddubs_epi16(data, q);               \
  }

#define DATA_MADD()                                  \
  {                                                  \
    __m128i q = _mm_set1_epi64x(0x0001006400010064); \
    data = _mm_madd_epi16(data, q);                  \
  }

#define DATA_PACK_AND_MADD()                                   \
  {                                                            \
    data = _mm_packus_epi32(data, data);                       \
    __m128i q = _mm_set_epi16(0, 0, 0, 0, 1, 10000, 1, 10000); \
    data = _mm_madd_epi16(data, q);                            \
  }

sonic_force_inline uint64_t simd_str2int_sse(const char* c, int& man_nd) {
  __m128i data = _mm_loadu_si128((const __m128i*)c);
  __m128i zero = _mm_setzero_si128();
  __m128i nine = _mm_set1_epi8(9);
  __m128i zero_c = _mm_set1_epi8('0');

  data = _mm_sub_epi8(data, zero_c);
  __m128i lt_zero = _mm_cmpgt_epi8(zero, data);
  __m128i gt_nine = _mm_cmpgt_epi8(data, nine);

  int num_end_idx = 16;

  __m128i is_num_end = _mm_or_si128(lt_zero, gt_nine);
  int is_num_end_int = _mm_movemask_epi8(is_num_end);

  if (is_num_end_int) {
    num_end_idx = __builtin_ctz(is_num_end_int);
  }
  man_nd = man_nd < num_end_idx ? man_nd : num_end_idx;

  switch (man_nd) {
    case 1:
      return _mm_extract_epi8(data, 0);
    case 2:
      return _mm_extract_epi8(data, 0) * 10 + _mm_extract_epi8(data, 1);
    case 3:
      data = _mm_slli_si128(data, 16 - 3);
      DATA_MADDUBS();
      return _mm_extract_epi16(data, 6) * 100 + _mm_extract_epi16(data, 7);
    case 4:
      data = _mm_slli_si128(data, 16 - 4);
      DATA_MADDUBS();
      return _mm_extract_epi16(data, 6) * 100 + _mm_extract_epi16(data, 7);
    case 5:
      data = _mm_slli_si128(data, 16 - 5);
      DATA_MADDUBS();
      DATA_MADD();
      return _mm_extract_epi32(data, 2) * 10000 + _mm_extract_epi32(data, 3);
    case 6:
      data = _mm_slli_si128(data, 16 - 6);
      DATA_MADDUBS();
      DATA_MADD();
      return _mm_extract_epi32(data, 2) * 10000 + _mm_extract_epi32(data, 3);
    case 7:
      data = _mm_slli_si128(data, 16 - 7);
      DATA_MADDUBS();
      DATA_MADD();
      return _mm_extract_epi32(data, 2) * 10000 + _mm_extract_epi32(data, 3);
    case 8:
      data = _mm_slli_si128(data, 16 - 8);
      DATA_MADDUBS();
      DATA_MADD();
      return _mm_extract_epi32(data, 2) * 10000 + _mm_extract_epi32(data, 3);
    case 9:
      data = _mm_slli_si128(data, 16 - 9);
      break;
    case 10:
      data = _mm_slli_si128(data, 16 - 10);
      break;
    case 11:
      data = _mm_slli_si128(data, 16 - 11);
      break;
    case 12:
      data = _mm_slli_si128(data, 16 - 12);
      break;
    case 13:
      data = _mm_slli_si128(data, 16 - 13);
      break;
    case 14:
      data = _mm_slli_si128(data, 16 - 14);
      break;
    case 15:
      data = _mm_slli_si128(data, 16 - 15);
      break;
    case 16:
      break;
    default:
      return 0;
  }

  DATA_MADDUBS();
  DATA_MADD();
  DATA_PACK_AND_MADD();
  return (uint64_t)(_mm_extract_epi32(data, 0)) * 100000000 +
         _mm_extract_epi32(data, 1);
}

}  // namespace internal
}  // namespace sonic_json
