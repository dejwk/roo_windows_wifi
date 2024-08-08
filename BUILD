cc_library(
    name = "roo_windows_wifi",
    visibility = ["//visibility:public"],
    srcs = glob(
        [
            "src/**/*.cpp",
            "src/**/*.h",
        ],
        exclude = ["test/**"],
    ),
    includes = [
        "src",
    ],
    defines = [
        "ROO_TESTING",
        "ARDUINO=10805",
    ],
    deps = [
        "//lib/roo_wifi",
        "//lib/roo_windows",
    ],
)
