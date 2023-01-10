#include <iostream>
#include <string>

#include "sonic/sonic.h"

int main() {
  using NodeType = sonic_json::Node;
  using Allocator = typename NodeType::AllocatorType;
  sonic_json::Node node;
  Allocator alloc;

  node.SetObject();
  node.AddMember("Key", NodeType("Value", alloc), alloc);
  std::cout << "Add member successfully!\n";
  return 0;
}
// g++ -I../include/ -march=haswell --std=c++11 get_and_set.cpp -o get_and_set
