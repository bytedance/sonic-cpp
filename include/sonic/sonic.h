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

#include "sonic/dom/dynamicnode.h"
#include "sonic/dom/generic_document.h"

#define SONIC_MAJOR_VERSION 1
#define SONIC_MINOR_VERSION 0
#define SONIC_PATCH_VERSION 2
#define SONIC_STRS(s) #s
#define SONIC_VERSION_STRING \
  SONIC_STRS(SONIC_MAJOR_VERSION.SONIC_MINOR_VERSION.SONIC_PATCH.VERSION)

namespace sonic_json {}  // namespace sonic_json
