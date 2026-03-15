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
#include "ubx_cfg_valget_builder.h"
#include "ubx_protocol_utils.h"

namespace ubx
{

std::vector<uint8_t>
ubx_cfg_valget_builder::build(const std::vector<uint32_t>& key_ids,
                               uint8_t  layer,
                               uint16_t position) const
{
    if (key_ids.empty())
    {
        return {};
    }

    std::vector<uint8_t> payload;
    payload.reserve(4u + key_ids.size() * 4u);

    protocol::write_u8  (payload, UBX_VALGET_VERSION_POLL);
    protocol::write_u8  (payload, layer);
    protocol::write_le16(payload, position);

    for (const uint32_t key_id : key_ids)
    {
        protocol::write_le32(payload, key_id);
    }

    return protocol::frame_ubx_message(UBX_CLASS_CFG, UBX_ID_CFG_VALGET, payload);
}

} // namespace ubx
