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

  sonic_json::WriteBuffer wb;
  doc.Serialize(wb);
  std::cout << wb.ToString() << std::endl;
  return 0;
}
// g++ -I../include/ -march=haswell --std=c++11 parse_and_serialize.cpp -o
// parse_and_serialize
