#include <iostream>
#include <string>

#include "sonic/sonic.h"

using member_itr_type = typename sonic_json::Document::MemberIterator;

void print_member(member_itr_type m) {
  const sonic_json::Node& key = m->name;
  sonic_json::Node& value = m->value;
  if (key.IsString()) {
    std::cout << "Key is: " << key.GetString() << std::endl;
  } else {
    std::cout << "Incoreect key type!\n";
    return;
  }
  if (value.IsInt64()) {
    std::cout << "Value is " << value.GetInt64() << std::endl;
  }

  return;
}

void set_new_value(member_itr_type m) {
  sonic_json::Node& value = m->value;
  value.SetInt64(2);
  return;
}

int main() {
  std::string json = R"(
    {
      "a": 1,
      "b": 2
    }
  )";

  sonic_json::Document doc;
  doc.Parse(json);

  if (doc.HasParseError()) {
    std::cout << "Parse failed!\n";
    return -1;
  }

  // Find member by key
  if (!doc.IsObject()) {  // Check JSON value type.
    std::cout << "Incorrect doc type!\n";
    return -1;
  }
  auto m = doc.FindMember("a");
  if (m != doc.MemberEnd()) {
    std::cout << "Before Setting new value:\n";
    print_member(m);
    std::cout << "After Setting value:\n";
    set_new_value(m);
    print_member(m);
  } else {
    std::cout << "Find key doesn't exist!\n";
  }
  return 0;
}
// g++ -I../include/ -march=haswell --std=c++11 get_and_set.cpp -o get_and_set
