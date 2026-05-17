// Copyright (C) 2019 Yaoyuan <ibireme@gmail.com>.

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file may have been modified by ByteDance authors. All ByteDance
// Modifications are Copyright 2022 ByteDance Authors.

#pragma once

#include <cmath>
#include <cstring>
#include <limits>

#include "sonic/dom/flags.h"
#include "sonic/dom/type.h"
#include "sonic/error.h"
#include "sonic/internal/arch/simd_quote.h"
#include "sonic/internal/ftoa.h"
#include "sonic/internal/itoa.h"
#include "sonic/internal/stack.h"
#include "sonic/writebuffer.h"

namespace sonic_json {

namespace internal {

template <SerializeFlags serializeFlags, typename NodeType>
sonic_force_inline SonicError SerializeImpl(const NodeType* node,
                                            WriteBuffer& wb) {
  using MemberNode = typename NodeType::MemberNode;
  static_assert(sizeof(MemberNode) == sizeof(NodeType) * 2,
                "SerializeImpl relies on compact object member layout");
  struct ParentCtx {
    size_t len;
    const NodeType* ptr;
  };

  /* preallocate buffer */
  constexpr size_t kExpectMinifyRatio = 18;
  constexpr size_t kNumberSize = 33;

  size_t node_nums = node->IsContainer() ? node->Size() : 1;
  if (sonic_unlikely(node_nums > (std::numeric_limits<size_t>::max() - 64) /
                                     kExpectMinifyRatio)) {
    return kErrorNoMem;
  }
  size_t estimate = node_nums * kExpectMinifyRatio + 64;
  bool is_obj = node->IsObject();
  bool is_key, is_obj_nxt;
  size_t member_cnt = 0;
  size_t val_cnt, val_cnt_nxt;
  size_t str_len;
  long inc_len;
  const char* str_ptr;
  ssize_t rn = 0;
  internal::Stack stk;
  ParentCtx* parent;
  if constexpr ((serializeFlags & SerializeFlags::kSerializeAppendBuffer) ==
                0) {
    wb.Clear();
    if (sonic_unlikely(!wb.Reserve(estimate))) return kErrorNoMem;
  } else {
    if (sonic_unlikely(estimate >
                       std::numeric_limits<size_t>::max() - wb.Size())) {
      return kErrorNoMem;
    }
    if (sonic_unlikely(!wb.Reserve(estimate + wb.Size()))) return kErrorNoMem;
  }

  bool is_single = (!node->IsContainer()) || node->Empty();
  if (sonic_unlikely(is_single)) {
    val_cnt = 1;
    goto val_begin;
  }
  if (sonic_unlikely(is_obj &&
                     node->Size() > std::numeric_limits<size_t>::max() / 2)) {
    return kErrorNoMem;
  }
  val_cnt = node->Size() << is_obj;
  member_cnt = node->Size();
  wb.PushUnsafe<char>('[' | (uint8_t)(is_obj) << 5);
  if (is_obj) {
    node = &node->getObjChildrenFirstUnsafe()->name;
  } else {
    node = node->getArrChildrenFirstUnsafe();
  }
val_begin:
  switch (node->getBasicType()) {
    case kString: {
      is_key = is_obj && ((val_cnt & 1) == 0);
      str_len = node->Size();
      if (sonic_unlikely(str_len >
                         (std::numeric_limits<size_t>::max() - 35) / 6)) {
        return kErrorNoMem;
      }
      inc_len = static_cast<long>(str_len * 6 + 32 + 3);
      if (sonic_unlikely(wb.Grow(inc_len) == nullptr)) return kErrorNoMem;
      str_ptr = node->GetStringView().data();
      rn = internal::Quote<serializeFlags>(str_ptr, str_len, wb.End<char>()) -
           wb.End<char>();
      wb.PushSizeUnsafe<char>(rn);
      wb.PushUnsafe<char>(is_key ? ':' : ',');
      member_cnt -= is_key;
      break;
    }

    case kNumber: {
      if (sonic_unlikely(wb.Grow(kNumberSize) == nullptr)) return kErrorNoMem;
      switch (node->GetType()) {
        case kSint:
          rn = internal::I64toa(wb.End<char>(), node->GetInt64()) -
               wb.End<char>();
          break;
        case kUint:
          rn = internal::U64toa(wb.End<char>(), node->GetUint64()) -
               wb.End<char>();
          break;
        case kReal: {
          const double d = node->GetDouble();
          rn = internal::F64toa<serializeFlags>(wb.End<char>(), d);
          // support Infinity/-Infinity or NaN/-NaN
          if (sonic_unlikely(rn <= 0)) {
            if (serializeFlags & SerializeFlags::kSerializeInfNan) {
              if (sonic_unlikely(std::isinf(d))) {
                const bool neg_inf = std::signbit(d);
                const char* s = neg_inf ? "\"-Infinity\"" : "\"Infinity\"";
                rn = neg_inf ? 11 : 10;
                std::memcpy(wb.End<char>(), s, (size_t)rn);
              } else if (sonic_unlikely(std::isnan(d))) {
                const bool neg_nan = std::signbit(d);
                const char* s = neg_nan ? "\"-NaN\"" : "\"NaN\"";
                rn = neg_nan ? 6 : 5;
                std::memcpy(wb.End<char>(), s, (size_t)rn);
              } else {
                goto inf_err;
              }
            } else {
              goto inf_err;
            }
          }
          break;
        }
        case kNumStr: {
          rn = 0;
          str_len = node->Size();
          if (sonic_unlikely(str_len == std::numeric_limits<size_t>::max())) {
            return kErrorNoMem;
          }
          if (sonic_unlikely(wb.Grow(str_len + 1) == nullptr)) {
            return kErrorNoMem;
          }
          wb.PushUnsafe(node->GetStringNumber().data(), str_len);
          break;
        }
        default:
          break;
      }
      sonic_assert(rn >= 0 && rn <= 32);
      wb.PushSizeUnsafe<char>(rn);
      wb.PushUnsafe<char>(',');
      break;
    }
    case kBool: {
      if (sonic_unlikely(!wb.Push5_8(node->IsFalse() ? "false,  " : "true,   ",
                                     5 + node->IsFalse()))) {
        return kErrorNoMem;
      }
      break;
    }
    case kNull: {
      if (sonic_unlikely(!wb.Push5_8("null,   ", 5))) return kErrorNoMem;
      break;
    }
    case kObject:
    case kArray: {
      if (sonic_unlikely(wb.Grow(3) == nullptr)) return kErrorNoMem;
      is_obj_nxt = node->IsObject();
      val_cnt_nxt = node->Size();
      if (sonic_unlikely(val_cnt_nxt == 0)) {
        wb.PushUnsafe<char>('[' | (uint8_t)(is_obj_nxt) << 5);
        wb.PushUnsafe<char>(']' | (uint8_t)(is_obj_nxt) << 5);
        wb.PushUnsafe<char>(',');
        break;
      } else {
        if (sonic_unlikely(is_obj &&
                           member_cnt >
                               (std::numeric_limits<size_t>::max() - 1) / 2)) {
          return kErrorNoMem;
        }
        // check the serialized member count
        // member_cnt is remained member counts, val_cnt is remained value
        // counts. If the object key is string type, "member_cnt * 2 + 1 =
        // val_cnt" is always true
        if (sonic_unlikely(is_obj && ((member_cnt << 1) + 1 != val_cnt))) {
          goto key_err;
        }
        if (sonic_unlikely(!stk.Push(ParentCtx{val_cnt << 1 | is_obj, node}))) {
          return kErrorNoMem;
        }
        if (sonic_unlikely(is_obj_nxt &&
                           val_cnt_nxt >
                               std::numeric_limits<size_t>::max() / 2)) {
          return kErrorNoMem;
        }
        val_cnt = val_cnt_nxt << is_obj_nxt;
        member_cnt = val_cnt_nxt;
        is_obj = is_obj_nxt;
        wb.PushUnsafe<char>('[' | (uint8_t)(is_obj) << 5);
        if (is_obj) {
          node = &node->getObjChildrenFirstUnsafe()->name;
        } else {
          node = node->getArrChildrenFirstUnsafe();
        }
        goto val_begin;
      }
      break;
    }
    case kRaw: {
      str_len = node->Size();
      if (sonic_unlikely(str_len == std::numeric_limits<size_t>::max())) {
        return kErrorNoMem;
      }
      if (sonic_unlikely(wb.Grow(str_len + 1) == nullptr)) return kErrorNoMem;
      wb.PushUnsafe(node->GetRaw().data(), str_len);
      wb.PushUnsafe<char>(',');
      break;
    }
    default:
      goto type_err;
  }
  val_cnt--;
  if (sonic_likely(val_cnt != 0)) {
    node = node->next();
    goto val_begin;
  }
scope_end:
  wb.Pop<char>(1);
  // check the serialized member count
  if (sonic_unlikely((member_cnt && is_obj) != 0)) {
    goto key_err;
  }
  if (sonic_unlikely(wb.Grow(2) == nullptr)) return kErrorNoMem;
  wb.PushUnsafe<char>(']' | (uint8_t)(is_obj) << 5);
  wb.PushUnsafe<char>(',');
  if (sonic_unlikely(stk.Size() == 0)) goto doc_end;
  parent = stk.Top<ParentCtx>();
  val_cnt = parent->len >> 1;
  is_obj = parent->len & 1;
  member_cnt = (val_cnt - 1) >> 1;
  val_cnt--;
  node = parent->ptr->next();
  stk.Pop<ParentCtx>(1);
  if (sonic_likely(val_cnt > 0)) goto val_begin;
  goto scope_end;

doc_end:
  wb.Pop<char>(1 + is_single);
  return kErrorNone;

type_err:
  return kSerErrorUnsupportedType;
inf_err:
  return kSerErrorInfinity;
key_err:
  return kSerErrorInvalidObjKey;
}

}  // namespace internal
}  // namespace sonic_json
