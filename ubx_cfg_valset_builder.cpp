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
#include "ubx_cfg_valset_builder.h"
#include "ubx_protocol_utils.h"

namespace ubx
{

std::vector<uint8_t>
ubx_cfg_valset_builder::build(const std::vector<config_entry>& entries,
                               config_layer layer) const
{
    if (entries.empty())
    {
        return {};
    }

    std::vector<uint8_t> payload;
    payload.reserve(4u + entries.size() * 8u);

    // Payload header
    protocol::write_u8  (payload, UBX_VALSET_VERSION);
    protocol::write_u8  (payload, static_cast<uint8_t>(layer));
    protocol::write_le16(payload, 0x0000u);  // reserved

    for (const config_entry& e : entries)
    {
        const uint8_t sz = protocol::value_byte_size(e.key_id);
        if (sz == 0u)
        {
            continue;  // unrecognised size code — skip rather than corrupt message
        }
        protocol::write_le32(payload, e.key_id);
        protocol::write_config_value(payload, e.value, sz);
    }

    // If every entry was skipped, return nothing
    if (payload.size() <= 4u)
    {
        return {};
    }

    return protocol::frame_ubx_message(UBX_CLASS_CFG, UBX_ID_CFG_VALSET, payload);
}

} // namespace ubx
