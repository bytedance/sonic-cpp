#include <iostream>
#include <string>

#include "sonic/sonic.h"

void parse_json(const std::string& data) {
  sonic_json::Document doc;
  doc.Parse(data);
  if (doc.HasParseError()) {
    std::cout << sonic_json::ErrorMsg(doc.GetParseError()) << std::endl
              << "Json: \n"
              << data << std::endl
              << "Error offset is: " << doc.GetErrorOffset() << std::endl;
  } else {
    std::cout << "Parse json:\n" << data << "\n successfully";
  }
}

int main() {
  parse_json(R"({"a":"b",)");
  parse_json(R"([1,2,3],[1,2,3])");
  parse_json(R"({"a","b"})");
}
// g++ -I../include/ -march=haswell --std=c++11 check_error_offset.cpp -o
// check_error_offset
