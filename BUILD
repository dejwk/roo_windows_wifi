load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "roo_windows_wifi",
    srcs = glob(
        [
            "src/**/*.cpp",
            "src/**/*.h",
        ],
        exclude = ["test/**"],
    ),
    defines = [
        "ROO_TESTING",
        "ARDUINO=10805",
    ],
    includes = [
        "src",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@roo_wifi",
        "@roo_windows",
    ],
)
