#include <iostream>
#include <string>
#include <utility>

#include "sonic/sonic.h"

int main() {
  using NodeType = sonic_json::Node;

  sonic_json::Document doc;
  auto& allocator = doc.GetAllocator();
  doc.SetObject();

  doc.AddMember("name", NodeType("Sonic-Cpp", allocator), allocator);
  doc.AddMember("enabled", NodeType(true), allocator);

  NodeType versions;
  versions.SetArray();
  versions.PushBack(NodeType(1), allocator);
  versions.PushBack(NodeType(2), allocator);
  versions.PushBack(NodeType(3), allocator);
  doc.AddMember("versions", std::move(versions), allocator);

  NodeType metadata;
  metadata.SetObject();
  metadata.AddMember("language", NodeType("C++", allocator), allocator);
  metadata.AddMember("license", NodeType("Apache-2.0", allocator), allocator);
  doc.AddMember("metadata", std::move(metadata), allocator);

  auto enabled = doc.FindMember("enabled");
  if (enabled == doc.MemberEnd()) {
    std::cerr << "The enabled member is missing.\n";
    return 1;
  }
  enabled->value.SetBool(false);

  auto version_list = doc.FindMember("versions");
  if (version_list == doc.MemberEnd() || !version_list->value.IsArray()) {
    std::cerr << "The versions member is not an array.\n";
    return 1;
  }
  version_list->value.PopBack();

  auto metadata_object = doc.FindMember("metadata");
  if (metadata_object == doc.MemberEnd() ||
      !metadata_object->value.IsObject() ||
      !metadata_object->value.RemoveMember("license")) {
    std::cerr << "The metadata member could not be updated.\n";
    return 1;
  }

  sonic_json::WriteBuffer buffer;
  const sonic_json::SonicError error = doc.Serialize(buffer);
  if (error != sonic_json::kErrorNone) {
    std::cerr << "Serialize failed: " << sonic_json::ErrorMsg(error) << '\n';
    return 1;
  }

  const std::string result = buffer.ToString();
  const std::string expected =
      R"({"name":"Sonic-Cpp","enabled":false,"versions":[1,2],"metadata":{"language":"C++"}})";
  std::cout << result << '\n';
  return result == expected ? 0 : 1;
}

// g++ -I../include/ -march=haswell --std=c++11 document_api.cpp -o
// document_api
