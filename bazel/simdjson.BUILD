package(default_visibility = ["//visibility:public"])
cc_library(
  name = "simdjson",
  srcs = ["singleheader/simdjson.cpp"],
  hdrs = ["singleheader/simdjson.h"],
  includes = ["singleheader"],
  copts = ['-O3', '-DNDEBUG'],
)
