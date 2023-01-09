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

#include "sonic/dom/flags.h"
#include "sonic/dom/type.h"
#include "sonic/error.h"
#include "sonic/internal/ftoa.h"
#include "sonic/internal/itoa.h"
#include "sonic/internal/quote.h"
#include "sonic/writebuffer.h"

namespace sonic_json {

namespace internal {

template <unsigned serializeFlags, typename NodeType>
sonic_force_inline SonicError SerializeImpl(const NodeType* node,
                                            WriteBuffer& wb) {
  struct ParentCtx {
    uint64_t len;
    const NodeType* ptr;
  };

  /* preallocate buffer */
  constexpr size_t kExpectMinifyRatio = 18;
  constexpr size_t kNumberSize = 33;

  size_t node_nums = node->IsContainer() ? node->Size() : 1;
  size_t estimate = node_nums * kExpectMinifyRatio + 64;
  bool is_obj = node->IsObject();
  bool is_key, is_obj_nxt;
  uint32_t member_cnt = 0;
  uint32_t val_cnt, val_cnt_nxt;
  size_t str_len;
  long inc_len;
  const char* str_ptr;
  ssize_t rn = 0;
  internal::Stack stk;
  ParentCtx* parent;

  wb.Clear();
  wb.Reserve(estimate);

  bool is_single = (!node->IsContainer()) || node->Empty();
  if (sonic_unlikely(is_single)) {
    val_cnt = 1;
    goto val_begin;
  }
  val_cnt = node->Size() << is_obj;
  member_cnt = node->Size();
  wb.PushUnsafe<char>('[' | (uint8_t)(is_obj) << 5);
  node = is_obj ? node->getObjChildrenFirstUnsafe()
                : node->getArrChildrenFirstUnsafe();
val_begin:
  switch (node->getBasicType()) {
    case kString: {
      is_key = ((size_t)(is_obj) & (~val_cnt));
      str_len = node->Size();
      inc_len = str_len * 6 + 32 + 3;
      wb.Grow(inc_len);
      str_ptr = node->GetStringView().data();
      rn = internal::Quote(str_ptr, str_len, wb.End<char>()) - wb.End<char>();
      wb.PushSizeUnsafe<char>(rn);
      wb.PushUnsafe<char>(is_key ? ':' : ',');
      member_cnt -= is_key;
      break;
    }

    case kNumber: {
      wb.Grow(kNumberSize);
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
          rn = internal::F64toa(wb.End<char>(), node->GetDouble());
          if (rn <= 0) goto inf_err;
          break;
          default:
            break;
        }
      }
      sonic_assert(rn > 0 && rn <= 32);
      wb.PushSizeUnsafe<char>(rn);
      wb.PushUnsafe<char>(',');
      break;
    };
    case kBool: {
      wb.Push5_8(node->IsFalse() ? "false,  " : "true,   ",
                 5 + node->IsFalse());
      break;
    }
    case kNull: {
      wb.Push5_8("null,   ", 5);
      break;
    }
    case kObject:
    case kArray: {
      wb.Grow(3);
      is_obj_nxt = node->IsObject();
      val_cnt_nxt = node->Size();
      if (sonic_unlikely(val_cnt_nxt == 0)) {
        wb.PushUnsafe<char>('[' | (uint8_t)(is_obj_nxt) << 5);
        wb.PushUnsafe<char>(']' | (uint8_t)(is_obj_nxt) << 5);
        wb.PushUnsafe<char>(',');
        break;
      } else {
        // check the serialized member count
        // member_cnt is remained member counts, val_cnt is remained value
        // counts. If the object key is string type, "member_cnt * 2 + 1 =
        // val_cnt" is always true
        if (sonic_unlikely(is_obj && ((member_cnt << 1) + 1 != val_cnt))) {
          goto key_err;
        }
        stk.Push(ParentCtx{val_cnt << 1 | is_obj, node});
        val_cnt = val_cnt_nxt << is_obj_nxt;
        member_cnt = val_cnt_nxt;
        is_obj = is_obj_nxt;
        wb.PushUnsafe<char>('[' | (uint8_t)(is_obj) << 5);
        node = is_obj ? node->getObjChildrenFirstUnsafe()
                      : node->getArrChildrenFirstUnsafe();
        goto val_begin;
      }
      break;
    }
    case kRaw: {
      str_len = node->Size();
      wb.Grow(str_len + 1);
      wb.PushUnsafe(node->GetRaw().data(), str_len);
      wb.PushUnsafe<char>(',');
      break;
    }
    default:
      goto type_err;
  };
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
  wb.Grow(2);
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
