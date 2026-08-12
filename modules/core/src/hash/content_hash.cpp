#include <woki/hash/content_hash.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>

namespace woki {
namespace {

constexpr std::array<u32, 64> kRoundConstants{
    0x428a2f98U,
    0x71374491U,
    0xb5c0fbcfU,
    0xe9b5dba5U,
    0x3956c25bU,
    0x59f111f1U,
    0x923f82a4U,
    0xab1c5ed5U,
    0xd807aa98U,
    0x12835b01U,
    0x243185beU,
    0x550c7dc3U,
    0x72be5d74U,
    0x80deb1feU,
    0x9bdc06a7U,
    0xc19bf174U,
    0xe49b69c1U,
    0xefbe4786U,
    0x0fc19dc6U,
    0x240ca1ccU,
    0x2de92c6fU,
    0x4a7484aaU,
    0x5cb0a9dcU,
    0x76f988daU,
    0x983e5152U,
    0xa831c66dU,
    0xb00327c8U,
    0xbf597fc7U,
    0xc6e00bf3U,
    0xd5a79147U,
    0x06ca6351U,
    0x14292967U,
    0x27b70a85U,
    0x2e1b2138U,
    0x4d2c6dfcU,
    0x53380d13U,
    0x650a7354U,
    0x766a0abbU,
    0x81c2c92eU,
    0x92722c85U,
    0xa2bfe8a1U,
    0xa81a664bU,
    0xc24b8b70U,
    0xc76c51a3U,
    0xd192e819U,
    0xd6990624U,
    0xf40e3585U,
    0x106aa070U,
    0x19a4c116U,
    0x1e376c08U,
    0x2748774cU,
    0x34b0bcb5U,
    0x391c0cb3U,
    0x4ed8aa4aU,
    0x5b9cca4fU,
    0x682e6ff3U,
    0x748f82eeU,
    0x78a5636fU,
    0x84c87814U,
    0x8cc70208U,
    0x90befffaU,
    0xa4506cebU,
    0xbef9a3f7U,
    0xc67178f2U,
};

constexpr u8 HexValue(const char value) noexcept {
    if (value >= '0' && value <= '9') {
        return static_cast<u8>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<u8>(value - 'a' + 10);
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<u8>(value - 'A' + 10);
    }
    return std::numeric_limits<u8>::max();
}

void ProcessBlock(const u8* block, std::array<u32, 8>& state) noexcept {
    std::array<u32, 64> words{};
    for (std::size_t i = 0; i < 16; ++i) {
        const std::size_t offset = i * 4;
        words[i] = (static_cast<u32>(block[offset]) << 24U) | (static_cast<u32>(block[offset + 1]) << 16U) | (static_cast<u32>(block[offset + 2]) << 8U) | static_cast<u32>(block[offset + 3]);
    }
    for (std::size_t i = 16; i < words.size(); ++i) {
        const u32 s0 = std::rotr(words[i - 15], 7) ^ std::rotr(words[i - 15], 18) ^ (words[i - 15] >> 3U);
        const u32 s1 = std::rotr(words[i - 2], 17) ^ std::rotr(words[i - 2], 19) ^ (words[i - 2] >> 10U);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }

    u32 a = state[0];
    u32 b = state[1];
    u32 c = state[2];
    u32 d = state[3];
    u32 e = state[4];
    u32 f = state[5];
    u32 g = state[6];
    u32 h = state[7];
    for (std::size_t i = 0; i < words.size(); ++i) {
        const u32 sum1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const u32 choice = (e & f) ^ (~e & g);
        const u32 temporary1 = h + sum1 + choice + kRoundConstants[i] + words[i];
        const u32 sum0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const u32 majority = (a & b) ^ (a & c) ^ (b & c);
        const u32 temporary2 = sum0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

} // namespace

Result<ContentHash> ContentHash::Parse(const std::string_view hex) {
    if (hex.size() != kSize * 2) {
        return Err(ErrorCode::ParseInvalidFormat, "content hash must contain 64 hexadecimal characters");
    }

    std::array<u8, kSize> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const u8 high = HexValue(hex[i * 2]);
        const u8 low = HexValue(hex[i * 2 + 1]);
        if (high > 15 || low > 15) {
            return Err(ErrorCode::ParseInvalidFormat, "content hash contains a non-hexadecimal character");
        }
        bytes[i] = static_cast<u8>((high << 4U) | low);
    }
    return Ok(ContentHash(bytes));
}

std::string ContentHash::Hex() const {
    constexpr std::string_view kHex = "0123456789abcdef";
    std::string result(kSize * 2, '0');
    for (std::size_t i = 0; i < bytes_.size(); ++i) {
        result[i * 2] = kHex[bytes_[i] >> 4U];
        result[i * 2 + 1] = kHex[bytes_[i] & 0x0fU];
    }
    return result;
}

ContentHash Sha256(const std::span<const std::byte> bytes) noexcept {
    std::array<u32, 8> state{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    const auto* data = reinterpret_cast<const u8*>(bytes.data());
    std::size_t offset = 0;
    while (bytes.size() - offset >= 64) {
        ProcessBlock(data + offset, state);
        offset += 64;
    }

    std::array<u8, 128> final_blocks{};
    const std::size_t remaining = bytes.size() - offset;
    if (remaining != 0) {
        std::memcpy(final_blocks.data(), data + offset, remaining);
    }
    final_blocks[remaining] = 0x80U;
    const std::size_t final_size = remaining < 56 ? 64 : 128;
    const u64 bit_size = static_cast<u64>(bytes.size()) * 8U;
    for (std::size_t i = 0; i < 8; ++i) {
        final_blocks[final_size - 1 - i] = static_cast<u8>(bit_size >> (i * 8U));
    }
    ProcessBlock(final_blocks.data(), state);
    if (final_size == 128) {
        ProcessBlock(final_blocks.data() + 64, state);
    }

    std::array<u8, ContentHash::kSize> digest{};
    for (std::size_t i = 0; i < state.size(); ++i) {
        digest[i * 4] = static_cast<u8>(state[i] >> 24U);
        digest[i * 4 + 1] = static_cast<u8>(state[i] >> 16U);
        digest[i * 4 + 2] = static_cast<u8>(state[i] >> 8U);
        digest[i * 4 + 3] = static_cast<u8>(state[i]);
    }
    return ContentHash(digest);
}

ContentHash Sha256(const std::string_view text) noexcept {
    return Sha256(std::as_bytes(std::span(text.data(), text.size())));
}

} // namespace woki

std::size_t std::hash<woki::ContentHash>::operator()(const woki::ContentHash& value) const noexcept {
    std::size_t result = 0;
    constexpr std::size_t kBytes = std::min(sizeof(result), woki::ContentHash::kSize);
    for (std::size_t i = 0; i < kBytes; ++i) {
        result = (result << 8U) | value.Bytes()[i];
    }
    return result;
}
