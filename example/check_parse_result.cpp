#include <iostream>
#include <string>

#include "sonic/sonic.h"

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
  } else {
    std::cout << "Parse successful!\n";
  }
  return 0;
}
// g++ -I../include/ -march=haswell --std=c++11 check_parse_result.cpp -o
// check_parse_result
