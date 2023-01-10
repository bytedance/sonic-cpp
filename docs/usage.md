# Usage
Examples of how to use Sonic-Cpp.

## Include Sonic-Cpp
1. Copy all files under `include/` into your project include path
2. or using compiler option, such as `-I/path/to/sonic/include`.

## Build with arch option
Sonic-Cpp is a header-only library, you only need to add `-mavx2 -mpclmul -mbmi`
or `-march=haswell` to support.

## Basic Usage
### Parse and Serialize
Sonic-Cpp assumes all input strings are encoded using UTF-8 and won't verify
by default

#### Parse from a string
The simplest way is calling Parse() methods:
```c++
#include "sonic/sonic.h"
// ...
std::string json = "[1,2,3]";

sonic_json::Document doc;
doc.Parse(json);
if (doc.HasParseError()) {
  // error path
  // If parse failed, the type of doc is null.
}
```

#### Serialize to a string
```c++
#include "sonic/sonic.h"
// ...
sonic_json::WriteBuffer wb;
doc.Serialize(wb);
std::cout << wb.ToString() << std::endl;
```
### Node
Node is the present for JSON value and supports all JSON value manipulation.

### Document
Document is the manager of Nodes. Sonic-Cpp organizes JSON value as a tree. 
Document also the root of JSON value tree. There is an allocator in Document,
which you should use to allocate memory for Node and Document.

### Query in object
There are two ways to find members: `operator[]` or `FindMember`. We recommend 
using `FindMember`.
```c++
#include "sonic/sonic.h"
// ...
using AllocatorType = typename sonic_json::Allocator;
sonic_json::Node node;
AllocatorType alloc;
// Add members for node

// find member by key
if (node.IsObject()) { // Note: CHECK NODE TYPE IS VERY IMPORTANT.
  const char* key = "key";
  auto m = node.FindMember(key); // recommended
  if (m != node.MemberEnd()) {
    // do something
  }
}

// Second method
if (node.IsObject()) {
  const char* key1 = "key1";
  const char* key2 = "key2";
  // You must sure that the keys are all existed.
  sonic_json::Node& val = node[key1][key2];
  // If key doesn't exist, operator[] will return reference to a static node
  // which type is Null. You SHOULD NOT MODIFY this static node. In this case,
  // FindMember is a better choice.
  if (val.IsNull()) {
    // error path
  }
}
```

### Is\*, Get\* and Set\* Value
```c++
sonic_json::Node node(0.0);
if (node.IsDouble()) {
  std::cout << node.GetDouble() << std::endl;
}
node.SetInt(0);
if (node.IsInt64()) {
  std::cout << node.GetInt64() << std::endl;
}
```
The following Is\*, Get\* and Set\* methods are supported:

- IsNull(), SetNull()
- IsBoo(), GetBool(), SetBool(bool)
- IsString(), GetString(), GetStringView(), SetString(const char*, size_t)
- IsNumber()
- IsArray(), SetArray()
- IsObject(), SetObject()
- IsTrue(), IsFalse()
- IsDouble(), GetDouble(), SetDouble(double)
- IsInt64(), GetInt64(), SetInt64(int64_t)
- IsUint64(), GetUint64(), SetUint64_t(uint64_t)

> Note: GetString() will return std::string. GetStringView() has better
performance.

### Add Member for Object
`AddMember` method only accepts rvalue as the argument.
```c++
using NodeType = sonic_json::Node;
sonic_json::Document doc;
auto& alloc = doc.GetAllocator();

doc.SetObject();
doc.AddMember("key1", NodeType(1), alloc);

{
  NodeType node;
  node.SetArray();
  doc.AddMember("key2", std::move(node), alloc);
}

sonic_json::WriteBuffer wb;
doc.Serialize(wb);
std::cout << wb.ToString() << std::endl; // {"key1": 1, "key2":[]}
```

### Add Element for Array
```c++
using NodeType = sonic_json::Node;
sonic_json::Document doc;
auto& alloc = doc.GetAllocator();

doc.SetArray();
doc.PushBack(NodeType(1.0), alloc);
{
  NodeType node;
  node.SetObject();
  doc.PushBack(node, alloc);
}

sonic_json::WriteBuffer wb;
doc.Serialize(wb);
std::cout << wb.ToString() << std::endl; // [1.0, {}]
```

### Remove Member in Object
```c++
// doc = {"a": 1, "b": 2, "c": 3}
if (doc.IsObject()) {
  const char* key = "a";
  if (doc.RemoveMember(key)) {
    std::cout << "Remove " << key << " successfully!\n";
  } else {
    std::cout << "Object doesn't have " << key << "!\n";
  }
}
```

### Remove Element in Array
There are 2 methods to remove elements in array: `PopBack()` pops the last
element and `Erase()` removes range elements.
```c++
// doc = [1, 2, 3, 0]

doc.PopBack(); // [1, 2, 3]
doc.Erase(doc.Begin(), doc.Begin() + 1); // [start, end), [2, 3]
```

## Advance
### Find Why Parse Failed and Where Cause This
Sonic provides `GetParseError()` to get the parse error code and `GetErrorOffset`
to get the position of the last parsing error.

Example:
```c++
#include <iostream>
#include <string>

#include "sonic/sonic.h"

void parse_json(const std::string& data) {
  sonic_json::Document doc;
  doc.Parse(data);
  if (doc.HasParseError()) {
    std::cout << sonic_json::ErrorMsg(doc.GetParseError()) << std::endl
        << "Json: \n" << data << std::endl
        << "Error offset is: " << doc.GetErrorOffset() << std::endl;
  } else {
    std::cout << "Parse json:\n" << data << "\n successfully";
  }
}

```

### How to Use Your Allocator
Sonic node has a template parameter `Allocator`. Users can define their
Allocator. If you want to provide a new allocator, you can define node and
document as:

```c++
using MyNode = sonic_json::DNode<MyAllocator>;
using MyDoc = sonic_json::GenericDocument<MyNode>;
```

Sonic uses rapidjson's allocator, you can define your own allocator follow
[rapidjson allocaotr](http://rapidjson.org/md_doc_internals.html#InternalAllocator)

### JSON Pointer
Sonic provides a JsonPointer class but doesn't support resolving the JSON pointer
syntax of [RFC 6901](https://www.rfc-editor.org/rfc/rfc6901). We will support
it in the future.

```c++
#include "sonic/sonic.h"

// Sonic JSON pointer need a template parameter that describes using which string
// type, such as std::string and std::string_view. string_view can avoid coping
// string data. But, the user should keep the memory is always valid when using
// string_view as template parameter.
using PointerType = sonic_json::GenericJsonPointer<sonic_json::StringView>;

// Sonic also defines a typename JsonPointerView using string_view.
// example:
//  using PointerType = sonic_json::JsonPointerView;
// The typename JsonPointer in sonic is defined as:
//  using JsonPointer =
//    GenericJsonPointer<SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE>;
// The macro SONIC_JSON_POINTER_NODE_STRING_DEFAULT_TYPE is std::string.

// query by JSON pointer

  sonic_json::Document doc;
  /* parsing code */
  /*   ...   */

// Construct JSON pointer by initializer list.
  sonic_json::Node* node1 = doc.AtPointer(PointerType({"a"}));

// error check!!!
  if (node1 != nullptr) {
    std::cout << "/a exists!\n";
  } else {
    std::cout << "/a doesn't exist!\n";
  }

  sonic_json::Node* node2 = doc.AtPointer(PointerType({"b", 1, "a"}));
  if (node2 != nullptr) {
    std::cout << "/b/1/a Eixsts!\n";
  } else {
    std::cout << "/b/1/a doesn't exist!\n";
  }

```

### Better AtPointer for String Literal
There is an optimized implementation of AtPointer when the argument is string
literal.
```c++
  sonic_force_inline const NodeType* AtPointer() const { return downCast(); }

  template <typename... Args>
  sonic_force_inline const NodeType* AtPointer(size_t idx, Args... args) const {
    if (!IsArray()) {
      return nullptr;
    }
    if (idx >= Size()) {
      return nullptr;
    }
    return (*this)[idx].AtPointer(args...);
  }

  template <typename... Args>
  sonic_force_inline const NodeType* AtPointer(StringView key,
                           Args... args) const {
    if (!IsObject()) {
      return nullptr;
    }
    auto m = FindMember(key);
    if (m == MemberEnd()) {
      return nullptr;
    }
    return m->value.AtPointer(args...);
  }
```
Actually, `std::string_view sv; sv == "hello world"` is faster than
`std::string s; s == "hello world"`. The compiler will optimize for string\_view.
The above implementation can avoid converting string literal to std::string.

Example:
```c++
  sonic_json::Node* node3 = doc.AtPointer("b", 1, "b");
  if (node3 != nullptr) {
    std::cout << "/b/1/b Eixsts!\n";
  } else {
    std::cout << "/b/1/b doesn't exist!\n";
  }
```

### Parse OnDemand
Sonic supports parsing specific Json Value by JSON pointer. The target JSON
Value can be anyone (object, array, string, number...).

```c++
#include "sonic/sonic.h"

std::string json = R"(
{
  "a": {
    "a0":[0,1,2,3,4,5,6,7,8,9],
    "a1": "hi"
  },
  "b":[
    {"b0":1},
    {"b1":2}
  ]
}
)";

int main() {
  // The target exists in JSON
  {
    sonic_json::Document doc;
    // doc only contain one Json Value: /a/a0/8
    doc.ParseOnDemand(json, {"a", "a0", 8});
    if (doc.HasParseError()) {
      return -1;
    }
    uint64_t val = doc.GetUint64();
    std::cout << "Parse OnDemand result is " << val << std::endl;
    // output: Parse OnDemand result is 8
  }

  // The target does not exist in JSON
  {
    sonic_json::Document doc;
    doc.ParseOnDemand(json, {"a", "a1", "unknown"});
    if (doc.HasParseError()) {
      sonic_json::SonicError err = doc.GetParseError();
      size_t error_position = doc.GetErrorOffset();
      std::cout << "Parse Error: " << sonic_json::ErrorMsg(err)
          << ". Error Position At " << error_position << std::endl;
      // output: Parse Error: ParseOnDemand: the target type is not matched..
      // Error Position At 55
    }
  }
  return 0;
}
```

### Create Map for Object
The members of JSON object value are organized as a vector in Sonic-cpp. This
makes Sonic-cpp parsing fast but maybe causes the query slow when the object
size is very large. Sonic-cpp provides `CreateMap` method to create a
`std::multimap`. This map records every member index in vector. The `FindMember`
method will use the map first if it exists. Actually, using a map isn't always
fast, especially when the object size is small. The users can call the
`DestroyMap` method to destroy the created map.

Example:
```
#include "sonic/sonic.h"

#include <iostream>
#include <string>

std::string get_json_string() {
  return R"(
    {
      "a":[
        {"b":1, "c":2, "d":3, "e":4}
      ]
    }
  )";
}

int main() {
  std::string json = get_json_string();
  sonic_json::Document doc;

  if (doc.Parse(json).HasParseError()) {
    std::cout << "Parse failed!\n";
    return -1;
  }

  sonic_json::Node* node = doc.AtPointer("a", 0);
  if (node == nullptr || !node->IsObject()) {
    std::cout << "/a/0 doesn't exist or isn't an object!\n";
    return -1;
  }

  if (node->FindMember("e") == node->MemberEnd()) {
    std::cout << "/a/0/e doesn't exist!\n";
  }

  // Create a map. If node already has a map, do nothing.
  node->CreateMap(doc.GetAllocator()); // Need Allocator
  // Use the map to query. This is same as above.
  if (node->FindMember("e") == node->MemberEnd()) {
    std::cout << "/a/0/e doesn't exist!\n";
  }

  // Not need the map anymore.
  node->DestroyMap();

  std::cout << "Quering finish!\n";
  return 0;

}
```
