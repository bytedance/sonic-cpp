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
#include <string>

namespace sonic_json {

template <typename NodeType>
class JsonHandler {
 public:
  static bool UseStackDirect();

  static void SetNull(NodeType& node);
  static void SetBool(NodeType& node, bool val);
  static void SetDouble(NodeType& node, double val);
  static void SetString(NodeType& node, char* data, size_t len);

  // set object and return the next node address
  static NodeType* SetObject(NodeType& node, NodeType* begin, NodeType* end,
                             size_t count);
  // set array and return the next node address
  static NodeType* SetArray(NodeType& node, NodeType* begin, NodeType* end,
                            size_t count);
};

}  // namespace sonic_json
