#include <iostream>
#include <string>
#include <vector>

#include "sonic/sonic.h"

int main() {
  std::string json_schema = R"(
  {"obj":1}
  )";

  std::string json = R"(
{"it":1, "obj":{"a":{"b":1}, "b":[1]}}
  )";

  sonic_json::Document doc;
  doc.Parse(json_schema);
  if (doc.HasParseError()) std::cout << "error\n";
  // doc.Parse(json_schema);

  doc.ParseSchema(json);
  if (doc.HasParseError()) {
    std::cout << "json: " << json.substr(doc.GetErrorOffset()) << std::endl;
  }
  std::cout << "schema : " << doc.Dump() << std::endl;
}
// g++ -I../include/ -march=haswell --std=c++11 parse_schema.cpp -o
// parse_schema
