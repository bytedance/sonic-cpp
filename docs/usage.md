# Usage
Example for how to use Sonic-Cpp.

## including Sonic-Cpp
1. Copy all files under `include/` into your project inlcude path
2. or using compiler option, such as `-I/path/to/sonic/include`.

## Building with arch option
Sonic-Cpp is header-only library, you only need adding `-mavx2 -mpclmul -mbmi`
or `-march=haswell` to support.

## Basic Usage
### parsing and serializing
Sonic-Cpp assume all input strings are encoded using UTF-8 and won't vertify
defaultly.

#### parsing from a string
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

#### Serailizing to a string
```c++
#include "sonic/sonic.h"
// ...
sonic_json::WriteBuffer wb;
doc.Serialize(wb);
std::cout << wb.ToString() << std::endl;
```
### Node
Node is the present for JSON vlaue, and supportes all JSON value manipulation.

### Document
Document is the manager of Nodes. Sonic-Cpp organizes JSON value as tree. 
Document also the root of JSON value tree. There is an allocator in Document,
which one you should used to allocate memory for Node and Document.

### Find member in object
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
  // which type is Null. You SHOULD NOT MODIFIY this static node. In this case,
  // FindMember is better choice.
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
```markdown
- IsNull(), SetNull()
- IsBoo(), GetBool(), SetBool(bool)
- IsString(), GetString(), GetStringLength(), SetString(const char*, size_t)
- IsNumber()
- IsArray(), SetArray()
- IsObject(), SetObject()
- IsTrue(), IsFalse()
- IsDouble(), GetDouble(), SetDouble(double)
- IsInt64(), GetInt64(), SetInt64(int64_t)
- IsUint64(), GetUint64(), SetUint64_t(uint64_t)
```

### Add member for object
`AddMember` method only accept rvalue as argument.
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

### Add element for array
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

### Remove member in object
```c++
// doc = {"a": 1, "b": 2, "c": 3}
if (doc.IsObject()) {
  const char* key = "a";
  if (doc.RemoveMember(key, std::strlen(key))) {
    std::cout << "Remove " << key << " successfully!\n";
  } else {
    std::cout << "Object doesn't have " << key << "!\n";
  }
}
```

### Remove element in array
There are 2 methods to remove elements in array: `PopBack()` pops the last element 
and `Erase()` removes range elements.
```c++
// doc = [1, 2, 3, 0]

doc.PopBack(); // [1, 2, 3]
doc.Erase(doc.Begin(), doc.Begin() + 1); // [start, end), [2, 3]
```

## Advance
