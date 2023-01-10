#include <iostream>
#include <string>

#include "sonic/sonic.h"

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
  node->CreateMap(doc.GetAllocator());  // Need Allocator
  // Use the map to query. This is same as above.
  if (node->FindMember("e") == node->MemberEnd()) {
    std::cout << "/a/0/e doesn't exist!\n";
  }

  // Not need the map anymore.
  node->DestroyMap();

  std::cout << "Quering finish!\n";
  return 0;
}
// g++ -I../include/ -march=haswell --std=c++11 create_and_destroy_map.cpp -o
// create_and_destroy_map
