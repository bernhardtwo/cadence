#include <cadence/spotify/pkce.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <string>

using namespace cadence::spotify;

namespace {

std::string hex(const std::array<std::uint8_t, 32>& digest) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    for (const std::uint8_t byte : digest) {
        out += digits[byte >> 4];
        out += digits[byte & 15];
    }
    return out;
}

} // namespace

TEST_CASE("sha256 matches the known digests", "[spotify][pkce]") {
    CHECK(hex(sha256("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(hex(sha256("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    // 56 bytes: the padding needs a second block.
    CHECK(hex(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(hex(sha256(std::string(1000, 'a'))) ==
          "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
}

TEST_CASE("the code challenge matches the RFC 7636 appendix B vector", "[spotify][pkce]") {
    const std::string verifier = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
    CHECK(isValidVerifier(verifier));
    CHECK(codeChallenge(verifier) == "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM");
}

TEST_CASE("the code verifier from the RFC random bytes is the RFC verifier", "[spotify][pkce]") {
    const std::array<std::uint8_t, 32> bytes{116, 24,  223, 180, 151, 153, 224, 37,  79,  250, 96,
                                             125, 216, 173, 187, 186, 22,  212, 37,  77,  105, 214,
                                             191, 240, 91,  88,  5,   88,  83,  132, 141, 121};
    CHECK(codeVerifier(bytes) == "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk");
}

TEST_CASE("base64url leaves out padding and uses the url alphabet", "[spotify][pkce]") {
    const std::array<std::uint8_t, 1> one{0xfb};
    const std::array<std::uint8_t, 2> two{0xfb, 0xff};
    const std::array<std::uint8_t, 3> three{0xfb, 0xff, 0xbf};
    CHECK(base64Url(one) == "-w");
    CHECK(base64Url(two) == "-_8");
    CHECK(base64Url(three) == "-_-_");
    CHECK(base64Url(std::span<const std::uint8_t>{}).empty());
}

TEST_CASE("verifier validation follows the RFC length and alphabet", "[spotify][pkce]") {
    CHECK_FALSE(isValidVerifier(std::string(42, 'a')));
    CHECK(isValidVerifier(std::string(43, 'a')));
    CHECK(isValidVerifier(std::string(128, '~')));
    CHECK_FALSE(isValidVerifier(std::string(129, 'a')));
    CHECK_FALSE(isValidVerifier(std::string(43, '+')));
}
