load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "com_github_google_benchmark",
    branch = "main",
    remote = "https://github.com/google/benchmark.git",
)

new_git_repository(
    name = "gtest",
    branch = "main",
    build_file = "//:bazel/gtest.BUILD",
    remote = "https://github.com/google/googletest.git",
)

new_git_repository(
    name = "rapidjson",
    branch = "master",
    build_file = "//:bazel/rapidjson.BUILD",
    remote = "https://github.com/Tencent/rapidjson.git",
)

new_git_repository(
    name = "cJSON",
    branch = "master",
    build_file = "//:bazel/cJSON.BUILD",
    remote = "https://github.com/DaveGamble/cJSON.git",
)

new_git_repository(
    name = "simdjson",
    commit = "203c03e90b73a32e641ad94f4bc4ab267ac0b6d5",
    build_file = "//:bazel/simdjson.BUILD",
    remote = "https://github.com/simdjson/simdjson.git",
)

new_git_repository(
    name = "yyjson",
    branch = "master",
    build_file = "//:bazel/yyjson.BUILD",
    remote = "https://github.com/ibireme/yyjson.git",
)

git_repository(
    name = "gflags",
    branch = "master",
    remote = "https://github.com/gflags/gflags.git",
)

