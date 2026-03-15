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
#include "ubx_config_sync_service.h"
#include "ubx_protocol_utils.h"

namespace ubx
{

// VALGET response payload layout (UBX spec §3.4.6):
//   Byte  0     : version (0x01 for response)
//   Byte  1     : layer
//   Bytes 2-3   : position (uint16_t LE, entry offset for chunked responses)
//   Bytes 4+    : key-value records
//                   key   : 4 bytes LE uint32_t
//                   value : N bytes LE, N = value_byte_size(key_id)
static constexpr uint16_t VALGET_RESP_HEADER_SIZE = 4u;

ubx_config_sync_service::ubx_config_sync_service(
        i_ubx_transport&         transport,
        i_ini_config_provider&   config_provider,
        i_ubx_config_repository& repository,
        ubx_cfg_valget_builder&  valget_builder,
        ubx_cfg_valset_builder&  valset_builder)
    : transport_       (transport)
    , config_provider_ (config_provider)
    , repository_      (repository)
    , valget_builder_  (valget_builder)
    , valset_builder_  (valset_builder)
{}

// ── Step 1 ────────────────────────────────────────────────────────────────────

bool ubx_config_sync_service::load_default_config(const std::string& ini_path)
{
    if (!config_provider_.load(ini_path))
    {
        return false;
    }
    default_config_ = config_provider_.get_all_entries();
    return !default_config_.empty();
}

// ── Step 2 ────────────────────────────────────────────────────────────────────

bool ubx_config_sync_service::start_valget_poll()
{
    if (default_config_.empty())
    {
        return false;
    }

    // Collect key IDs for polling
    std::vector<uint32_t> key_ids;
    key_ids.reserve(default_config_.size());
    for (const config_entry& e : default_config_)
    {
        key_ids.push_back(e.key_id);
    }

    // Clear stale chip-state before issuing a fresh poll
    repository_.clear();

    const std::vector<uint8_t> msg =
        valget_builder_.build(key_ids, ubx_cfg_valget_builder::LAYER_RAM);

    if (msg.empty())
    {
        return false;
    }

    return transport_.send(msg);
}

// ── Step 3 ────────────────────────────────────────────────────────────────────

void ubx_config_sync_service::on_valget_response(const uint8_t* payload,
                                                   uint16_t       length)
{
    // Minimum payload: 4 header bytes + at least 1 key (4 bytes) + 1 value byte
    if (!payload || length < VALGET_RESP_HEADER_SIZE + 5u)
    {
        return;
    }

    // version byte must be VALGET response version
    if (payload[0] != UBX_VALGET_VERSION_RESP)
    {
        return;
    }

    // Walk key-value records starting after the 4-byte header
    uint16_t offset = VALGET_RESP_HEADER_SIZE;

    while (offset + 4u <= length)
    {
        const uint32_t key_id = protocol::read_le32(payload + offset);
        offset += 4u;

        const uint8_t sz = protocol::value_byte_size(key_id);
        if (sz == 0u || offset + sz > length)
        {
            break;  // malformed or truncated
        }

        const config_value val = protocol::read_config_value(payload + offset, sz);
        offset += sz;

        repository_.store({key_id, val});
    }
}

// ── Step 4 ────────────────────────────────────────────────────────────────────

bool ubx_config_sync_service::apply_configuration(config_layer layer)
{
    std::vector<config_entry> updates;

    for (const config_entry& desired : default_config_)
    {
        config_value current_val;
        const bool present = repository_.fetch(desired.key_id, current_val);

        if (!present || current_val != desired.value)
        {
            updates.push_back(desired);
        }
    }

    if (updates.empty())
    {
        return true;  // chip already matches desired configuration
    }

    const std::vector<uint8_t> msg = valset_builder_.build(updates, layer);
    if (msg.empty())
    {
        return false;
    }

    return transport_.send(msg);
}

// ─────────────────────────────────────────────────────────────────────────────

std::size_t ubx_config_sync_service::default_config_size() const
{
    return default_config_.size();
}

} // namespace ubx
