#include "sonic/sonic.h"

std::string json = R"(
{
    "a": {
    "a0":[0,1,2,3,4,5,6,7,8,9],
    "a1": "hi"
    },
    "b":[
    {"b0":1},
    {"b1":2}
    ]
}
)";

int main() {
  // The target is exsited in JSON
  {
    sonic_json::Document doc;
    doc.ParseOnDemand(json, {"a", "a0", 8});
    if (doc.HasParseError()) {
      return -1;
    }
    uint64_t val = doc.GetUint64();
    std::cout << "Parse ondemand result is " << val << std::endl;
    // output: Parse ondemand result is 8
  }

  // The target is not exsited in JSON
  {
    sonic_json::Document doc;
    doc.ParseOnDemand(json, {"a", "a1", "unknown"});
    if (doc.HasParseError()) {
      sonic_json::SonicError err = doc.GetParseError();
      size_t error_position = doc.GetErrorOffset();
      std::cout << "Parse Error: " << sonic_json::ErrorMsg(err)
                << ". Error Position At " << error_position << std::endl;
      // output: Parse Error: ParseOnDemand: the target type is not matched..
      // Error Position At 55
    }
  }
  return 0;
}
// g++ -I../include/ -march=haswell --std=c++11 parseondemand.cpp -o
// parseondemand