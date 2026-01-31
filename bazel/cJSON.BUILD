load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])
cc_library(
  name = "cJSON",
  srcs = ["cJSON.c"],
  hdrs = ["cJSON.h"],
  copts = ['-O3' ,'-DNDEBUG',],
)

