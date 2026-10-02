#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cdfpp/cdf-io/md5.hpp"
#include <array>
#include <fmt/core.h>
#include <string>
#include <string_view>
#include <utility>

std::string hex_md5(std::string_view text, std::size_t chunk_size)
{
    cdf::io::md5 hash;
    for (std::size_t start = 0; start < std::size(text); start += chunk_size)
        hash.update(text.data() + start, std::min(chunk_size, std::size(text) - start));
    std::string hex;
    for (const auto byte : hash.digest())
        hex += fmt::format("{:02x}", static_cast<unsigned char>(byte));
    return hex;
}

// RFC 1321, appendix A.5
SCENARIO("MD5 gives the digests of the RFC 1321 test suite", "[md5]")
{
    const std::array<std::pair<std::string, std::string>, 7> suite = { {
        { "", "d41d8cd98f00b204e9800998ecf8427e" },
        { "a", "0cc175b9c0f1b6a831c399e269772661" },
        { "abc", "900150983cd24fb0d6963f7d28e17f72" },
        { "message digest", "f96b697d7cb7938d525a2f31aaf161d0" },
        { "abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b" },
        { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
            "d174ab98d277d9f5a5611c2c9f419d9f" },
        { "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
            "57edf4a22be3c955ac49da2e2107b67a" },
    } };
    for (const auto& [text, digest] : suite)
        for (const std::size_t chunk_size : { 1UL, 3UL, 63UL, 64UL, 65UL, 1000UL })
        {
            INFO(text << " in chunks of " << chunk_size);
            REQUIRE(hex_md5(text, chunk_size) == digest);
        }
}
