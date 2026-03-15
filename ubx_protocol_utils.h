// MIT License
//
// Copyright (c) 2026 nguyenchiemminhvu
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Header-only utilities for UBX binary protocol encoding / decoding.
//
// Covers:
//   - Key size extraction from key ID
//   - Little-endian serialisation and deserialisation
//   - UBX Fletcher-8 checksum calculation
//   - UBX frame assembly
//   - config_value serialisation helpers
//
// All functions are inline; no separate translation unit is needed.

#pragma once

#include "ubx_config_types.h"
#include <cstddef>

namespace ubx
{
namespace protocol
{

// ─── Key size extraction ───────────────────────────────────────────────────────
//
// UBX configuration key-ID format (32 bits):
//   Bits 31-28  : size code
//       1 → L   : 1-bit boolean, stored as 1 byte
//       2 → U1/I1/X1/E1 : 1 byte
//       3 → U2/I2/X2/E2 : 2 bytes
//       4 → U4/I4/X4/E4 : 4 bytes
//       5 → U8/I8/X8/R8 : 8 bytes
//   Bits 27-16  : group ID
//   Bits 15- 0  : item ID

inline uint8_t value_byte_size(uint32_t key_id)
{
    switch ((key_id >> 28u) & 0x0Fu)
    {
        case 1u: return 1u;
        case 2u: return 1u;
        case 3u: return 2u;
        case 4u: return 4u;
        case 5u: return 8u;
        default: return 0u;   // unknown / unsupported
    }
}

// ─── Little-endian write helpers ──────────────────────────────────────────────

inline void write_u8(std::vector<uint8_t>& buf, uint8_t v)
{
    buf.push_back(v);
}

inline void write_le16(std::vector<uint8_t>& buf, uint16_t v)
{
    buf.push_back(static_cast<uint8_t>( v        & 0xFFu));
    buf.push_back(static_cast<uint8_t>((v >>  8u) & 0xFFu));
}

inline void write_le32(std::vector<uint8_t>& buf, uint32_t v)
{
    buf.push_back(static_cast<uint8_t>( v        & 0xFFu));
    buf.push_back(static_cast<uint8_t>((v >>  8u) & 0xFFu));
    buf.push_back(static_cast<uint8_t>((v >> 16u) & 0xFFu));
    buf.push_back(static_cast<uint8_t>((v >> 24u) & 0xFFu));
}

inline void write_le64(std::vector<uint8_t>& buf, uint64_t v)
{
    for (unsigned i = 0u; i < 8u; ++i)
        buf.push_back(static_cast<uint8_t>((v >> (8u * i)) & 0xFFu));
}

// ─── Little-endian read helpers ───────────────────────────────────────────────

inline uint16_t read_le16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0])
         | (static_cast<uint16_t>(p[1]) << 8u);
}

inline uint32_t read_le32(const uint8_t* p)
{
    return  static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) <<  8u)
         | (static_cast<uint32_t>(p[2]) << 16u)
         | (static_cast<uint32_t>(p[3]) << 24u);
}

inline uint64_t read_le64(const uint8_t* p)
{
    uint64_t v = 0u;
    for (unsigned i = 0u; i < 8u; ++i)
        v |= (static_cast<uint64_t>(p[i]) << (8u * i));
    return v;
}

// ─── UBX Fletcher-8 checksum ──────────────────────────────────────────────────
// Computed over: class byte, id byte, length[2], payload bytes.

inline void compute_checksum(const uint8_t* data, std::size_t len,
                              uint8_t& ck_a, uint8_t& ck_b)
{
    ck_a = 0u;
    ck_b = 0u;
    for (std::size_t i = 0u; i < len; ++i)
    {
        ck_a = static_cast<uint8_t>(ck_a + data[i]);
        ck_b = static_cast<uint8_t>(ck_b + ck_a);
    }
}

// ─── UBX frame assembly ───────────────────────────────────────────────────────
// Returns a fully framed UBX message:
//   0xB5 | 0x62 | class | id | len_lo | len_hi | payload... | ck_a | ck_b

inline std::vector<uint8_t> frame_ubx_message(uint8_t msg_class,
                                               uint8_t msg_id,
                                               const std::vector<uint8_t>& payload)
{
    const uint16_t payload_len = static_cast<uint16_t>(payload.size());

    std::vector<uint8_t> msg;
    msg.reserve(6u + payload.size() + 2u);

    msg.push_back(UBX_SYNC_CHAR_1);
    msg.push_back(UBX_SYNC_CHAR_2);
    msg.push_back(msg_class);
    msg.push_back(msg_id);
    write_le16(msg, payload_len);
    msg.insert(msg.end(), payload.begin(), payload.end());

    // Checksum covers bytes starting at msg_class through end of payload
    uint8_t ck_a, ck_b;
    compute_checksum(msg.data() + 2u, 4u + payload.size(), ck_a, ck_b);
    msg.push_back(ck_a);
    msg.push_back(ck_b);

    return msg;
}

// ─── config_value serialisation ───────────────────────────────────────────────

inline void write_config_value(std::vector<uint8_t>& buf,
                                const config_value&   val,
                                uint8_t               byte_size)
{
    switch (byte_size)
    {
        case 1u: write_u8  (buf, val.as_u8());  break;
        case 2u: write_le16(buf, val.as_u16()); break;
        case 4u: write_le32(buf, val.as_u32()); break;
        case 8u: write_le64(buf, val.raw);       break;
        default: break;
    }
}

// ─── config_value deserialisation ────────────────────────────────────────────

inline config_value read_config_value(const uint8_t* p, uint8_t byte_size)
{
    config_value v;
    switch (byte_size)
    {
        case 1u: v.raw = p[0];           break;
        case 2u: v.raw = read_le16(p);   break;
        case 4u: v.raw = read_le32(p);   break;
        case 8u: v.raw = read_le64(p);   break;
        default: break;
    }
    return v;
}

} // namespace protocol
} // namespace ubx
