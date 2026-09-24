#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

// RFC 7636 helpers, Qt free so they can be checked against the RFC test vector.
namespace cadence::spotify {

std::array<std::uint8_t, 32> sha256(std::string_view data);

// Base64url without padding, as RFC 7636 appendix A specifies.
std::string base64Url(std::span<const std::uint8_t> bytes);

// A code verifier from 32 random bytes: 43 unreserved characters.
std::string codeVerifier(std::span<const std::uint8_t, 32> randomBytes);

// The S256 code challenge of a verifier.
std::string codeChallenge(std::string_view verifier);

// 43 to 128 characters from [A-Za-z0-9-._~].
bool isValidVerifier(std::string_view verifier) noexcept;

} // namespace cadence::spotify
