load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])
cc_library(
  name = "gflags",
  srcs = ["lib/libgflags.a",
  ],
  hdrs = glob(["include/gflags/*.h",
  ]),
  includes = ["include"],
)
