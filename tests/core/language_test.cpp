#include <cadence/core/language.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace cadence::core;

TEST_CASE("an explicit language wins over the system", "[language]") {
    CHECK(resolveLanguage("fr", {"es-MX"}) == "fr");
    CHECK(resolveLanguage("es", {}) == "es");
    CHECK(resolveLanguage("en", {"fr-FR"}) == "en");
}

TEST_CASE("system picks the first shipped language of the preference list", "[language]") {
    CHECK(resolveLanguage("system", {"es-MX", "en-US"}) == "es");
    CHECK(resolveLanguage("system", {"fr-CA", "fr", "en"}) == "fr");
    CHECK(resolveLanguage("system", {"de-DE", "en-GB"}) == "en");
    CHECK(resolveLanguage("system", {"de-DE", "it-IT"}) == "en");
    CHECK(resolveLanguage("system", {"de", "ES_es"}) == "es");
    CHECK(resolveLanguage("system", {}) == "en");
}

TEST_CASE("an unknown setting behaves like system", "[language]") {
    CHECK(resolveLanguage("klingon", {"fr-BE"}) == "fr");
    CHECK(resolveLanguage("", {}) == "en");
}
