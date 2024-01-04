/*
 * Copyright 2023 ByteDance Inc.
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

#include "gtest/gtest.h"
#include "sonic/sonic.h"

namespace {

using namespace sonic_json;

void TestSuccess(const std::string schema, const std::string json,
                 const std::string expect) {
  Document doc;
  doc.Parse(schema);
  EXPECT_FALSE(doc.HasParseError())
      << "failed parsing schema: " << schema << std::endl;
  if (doc.HasParseError()) return;
  doc.ParseSchema(json);
  EXPECT_FALSE(doc.HasParseError())
      << "failed parsing json: " << json << std::endl;
  if (doc.HasParseError()) return;
  Document expect_doc;
  expect_doc.Parse(expect);
  EXPECT_FALSE(doc.HasParseError())
      << "failed parsing expect: " << expect << std::endl;
  if (expect_doc.HasParseError()) return;

  EXPECT_TRUE(doc == expect_doc)
      << "doc: " << doc.Dump() << std::endl
      << "expect doc: " << expect_doc.Dump() << std::endl;
}

TEST(ParseSchema, SuccessBasic) {
  TestSuccess(
      R"({"true": null, "false": null, "null":null, "int": null, "double":null, 
        "string": null, "object": null, "array": null
    })",
      R"({"true": true, "false": false, "null": null, "int": 1, "double": 1.0, "string": "string", 
        "object": {
            "object": {},
            "array": []
        },
        "array": [{}, []]
    })",
      R"({"true": true, "false": false, "null": null, "int": 1, "double": 1.0, "string": "string", 
        "object": {
            "object": {},
            "array": []
        },
        "array": [{}, []]
    })");
  TestSuccess(
      R"({"true": false, "false": true, "null": {}, "int": 2, "double":2.0, 
        "string": "", "object": null, "array": []})",
      R"({"true": true, "false": false, "null": null, "int": 1, "double": 1.0, "string": "string", 
        "object": {
            "object": {},
            "array": [{}, [], [{}, []], true, null, "str", 1, 1.0]
        },
        "array": [{}, [], [{}, []], true, null, "str", 1, 1.0]
    })",
      R"({"true": true, "false": false, "null": null, "int": 1, "double": 1.0, "string": "string", 
        "object": {
            "object": {},
            "array": [{}, [], [{}, []], true, null, "str", 1, 1.0]
        },
        "array": [{}, [], [{}, []], true, null, "str", 1, 1.0]
    })");
  TestSuccess(
      R"({"true": null, "false": null, "null":null, "int": null, "double":null, 
        "string": null, "object": null, "array": null})",
      R"([])", R"([])");
  TestSuccess(R"({"obj":{}})", R"({"obj":{"a":1}})", R"({"obj":{"a":1}})");
  TestSuccess(R"({"obj":{"a":2}})", R"({"obj":{"a":1, "b":1}})",
              R"({"obj":{"a":1}})");
  TestSuccess(
      R"({"bool2bool":true, "bool2int":1, "bool2dbl": 1.0, "bool2str": "str",
        "bool2null": null, "bool2obj": {}, "bool2arr": [],
        "int2bool":true, "int2int":1, "int2dbl": 1.0, "int2str": "str",
        "int2null": null, "int2obj": {}, "int2arr": [],
        "dbl2bool":true, "dbl2int":1, "dbl2dbl": 1.0, "dbl2str": "str",
        "dbl2null": null, "dbl2obj": {}, "dbl2arr": [],
        "str2bool":true, "str2int":1, "str2dbl": 1.0, "str2str": "str",
        "str2null": null, "str2obj": {}, "str2arr": [],
        "null2bool":true, "null2int":1, "null2dbl": 1.0, "null2str": "str",
        "null2null": null, "null2obj": {}, "null2arr": [],
        "obj2bool":true, "obj2int":1, "obj2dbl": 1.0, "obj2str": "str",
        "obj2null": null, "obj2obj": {}, "obj2arr": [],
        "arr2bool":true, "arr2int":1, "arr2dbl": 1.0, "arr2str": "str",
        "arr2null": null, "arr2obj": {}, "arr2arr": []
        })",
      R"({
       "bool2bool":false, "bool2int":false, "bool2dbl": false, "bool2str": false,
        "bool2null": false, "bool2obj": false, "bool2arr": false,
        "int2bool":2, "int2int":2, "int2dbl": 2, "int2str": 2,
        "int2null": 2, "int2obj": 2, "int2arr": 2,
        "dbl2bool":3.0, "dbl2int":3.0, "dbl2dbl": 3.0, "dbl2str": 3.0,
        "dbl2null": 3.0, "dbl2obj": 3.0, "dbl2arr": 3.0,
        "str2bool":"string", "str2int":"string", "str2dbl": "string", "str2str": "string",
        "str2null": "string", "str2obj": "string", "str2arr": "string",
        "null2bool":null, "null2int":null, "null2dbl": null, "null2str": null,
        "null2null": null, "null2obj": null, "null2arr": null,
        "obj2bool": {"a":1}, "obj2int":{"a":1}, "obj2dbl": {"a":1}, "obj2str":{"a":1},
        "obj2null": {"a":1}, "obj2obj": {"a":1}, "obj2arr": {"a":1},
        "arr2bool":[1], "arr2int":[1], "arr2dbl": [1], "arr2str": [1],
        "arr2null": [1], "arr2obj": [1], "arr2arr": [1] 
    })",
      R"({
       "bool2bool":false, "bool2int":false, "bool2dbl": false, "bool2str": false,
        "bool2null": false, "bool2obj": false, "bool2arr": false,
        "int2bool":2, "int2int":2, "int2dbl": 2, "int2str": 2,
        "int2null": 2, "int2obj": 2, "int2arr": 2,
        "dbl2bool":3.0, "dbl2int":3.0, "dbl2dbl": 3.0, "dbl2str": 3.0,
        "dbl2null": 3.0, "dbl2obj": 3.0, "dbl2arr": 3.0,
        "str2bool":"string", "str2int":"string", "str2dbl": "string", "str2str": "string",
        "str2null": "string", "str2obj": "string", "str2arr": "string",
        "null2bool":null, "null2int":null1, "null2dbl": null, "null2str": null,
        "null2null": null, "null2obj": null, "null2arr": null,
        "obj2bool": {"a":1}, "obj2int":{"a":1}, "obj2dbl": {"a":1}, "obj2str":{"a":1},
        "obj2null": {"a":1}, "obj2obj": {"a":1}, "obj2arr": {"a":1},
        "arr2bool":[1], "arr2int":[1], "arr2dbl": [1], "arr2str": [1],
        "arr2null": [1], "arr2obj": [1], "arr2arr": [1] 
    })");
}

}  // namespace
