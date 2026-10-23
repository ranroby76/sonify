// D:\workspace\Fanan_web_hub\src\Ed25519.h

#pragma once

#include <cstddef>
#include <cstdint>

//==============================================================================
/**
    Ed25519 signatures, checked (RFC 8032): how Sonify knows a licence came from Fanan's
    server. The server signs with its private key, which never leaves it; Sonify holds only
    the public key, so nobody else can make a licence that passes.

    Checking only (nothing here signs), with SHA-512, in plain C++ with no library: after
    TweetNaCl's design (D. J. Bernstein et al., public domain).
*/
namespace Ed25519
{
    /** True when signature (64 bytes) is publicKey's (32 bytes) signature of the message. */
    bool verify (const std::uint8_t signature[64], const std::uint8_t* message, std::size_t messageSize,
                 const std::uint8_t publicKey[32]) noexcept;

    /** SHA-512 (FIPS 180-4) of the data: 64 bytes. */
    void sha512 (const std::uint8_t* data, std::size_t size, std::uint8_t out[64]) noexcept;
}
