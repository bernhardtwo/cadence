#include <cadence/core/version.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace cadence::core;

TEST_CASE("versionString matches the version configured by the build", "[core][version]") {
    CHECK(versionString() == CADENCE_EXPECTED_VERSION);
}

TEST_CASE("version components agree with versionString", "[core][version]") {
    const Version v = version();
    const std::string expected =
        std::to_string(v.major) + '.' + std::to_string(v.minor) + '.' + std::to_string(v.patch);

    CHECK(v.major >= 0);
    CHECK(v.minor >= 0);
    CHECK(v.patch >= 0);
    CHECK(expected == versionString());
}

TEST_CASE("Version supports equality comparison", "[core][version]") {
    CHECK(version() == version());
    CHECK(Version{0, 1, 0} == Version{0, 1, 0});
    CHECK_FALSE(Version{0, 1, 0} == Version{0, 1, 1});
}
