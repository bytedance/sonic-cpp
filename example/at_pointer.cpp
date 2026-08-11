#include <iostream>
#include <string>

#include "sonic/sonic.h"

using PointerType = sonic_json::GenericJsonPointer<sonic_json::StringView>;

int main() {
  std::string json = R"(
    {
      "a":1,
      "b":[
        {"a":1},
        {"b":2}
      ]
    }
  )";

  sonic_json::Document doc;
  if (doc.Parse(json).HasParseError()) {
    std::cout << "Parse failed!\n";
    return -1;
  }

  sonic_json::Node* node1 = doc.AtPointer(PointerType({"a"}));
  if (node1 != nullptr) {
    std::cout << "/a exists!\n";
  } else {
    std::cout << "/a doesn't exist!\n";
  }

  sonic_json::Node* node2 = doc.AtPointer(PointerType({"b", 1, "a"}));
  if (node2 != nullptr) {
    std::cout << "/b/1/a exists!\n";
  } else {
    std::cout << "/b/1/a doesn't exist!\n";
  }

  sonic_json::Node* node3 = doc.AtPointer("b", 1, "b");
  if (node3 != nullptr) {
    std::cout << "/b/1/b exists!\n";
  } else {
    std::cout << "/b/1/b doesn't exist!\n";
  }

  sonic_json::JsonPointer rfc_pointer;
  if (!rfc_pointer.Parse("/b/1/b")) {
    std::cout << "Invalid RFC 6901 pointer!\n";
    return -1;
  }
  sonic_json::Node* node4 = doc.AtPointer(rfc_pointer);
  if (node4 != nullptr) {
    std::cout << "RFC pointer /b/1/b exists!\n";
  } else {
    std::cout << "RFC pointer /b/1/b doesn't exist!\n";
  }

  return 0;
}

// g++ -I../include/ -march=haswell --std=c++11 at_pointer.cpp -o at_pointer
