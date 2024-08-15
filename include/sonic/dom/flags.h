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

// ParseFlag is one-hot encoded for different parsing option.
// User can define customed flags through combinations.
enum ParseFlag {
  kParseDefault = 0,
};

// SerializeFlags is one-hot encoded for different serializing option.
// User can define customed flags through combinations.
enum SerializeFlags {
  kSerializeDefault = 0,
  kSerializeAppendBuffer = 1,
};
