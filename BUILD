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
    includes = [
        "src",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@roo_wifi",
        "@roo_windows",
    ],
)

load("@rules_cc//cc:cc_test.bzl", "cc_test")
cc_library(
    name = "model",
    hdrs = ["src/roo_windows_wifi/model.h"],
    includes = ["src"],
    deps = ["@roo_wifi"],
)
cc_test(
    name = "model_test",
    srcs = ["test/model_test.cpp"],
    deps = [":model", "@roo_wifi//test:test_support", "@googletest//:gtest_main"],
)

cc_test(
    name = "material3_presentation_model_test",
    srcs = ["test/material3_presentation_model_test.cpp"],
    deps = [
        ":roo_windows_wifi",
        "@googletest//:gtest_main",
        "@roo_wifi//test:test_support",
    ],
)

cc_test(
    name = "material3_network_row_test",
    srcs = ["test/material3_network_row_test.cpp"],
    deps = [
        ":roo_windows_wifi",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "material3_settings_destination_test",
    srcs = ["test/material3_settings_destination_test.cpp"],
    deps = [
        ":roo_windows_wifi",
        "@googletest//:gtest_main",
        "@roo_wifi//test:test_support",
    ],
)

cc_test(
    name = "material3_saved_networks_test",
    srcs = ["test/material3_saved_networks_test.cpp"],
    deps = [
        ":roo_windows_wifi",
        "@googletest//:gtest_main",
        "@roo_wifi//test:test_support",
    ],
)

cc_test(
    name = "material3_details_test",
    srcs = ["test/material3_details_test.cpp"],
    deps = [
        ":roo_windows_wifi",
        "@googletest//:gtest_main",
        "@roo_wifi//test:test_support",
    ],
)

cc_test(
    name = "material3_edit_network_test",
    srcs = ["test/material3_edit_network_test.cpp"],
    deps = [
        ":roo_windows_wifi",
        "@googletest//:gtest_main",
        "@roo_wifi//test:test_support",
    ],
)
