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
#include "ubx_config_manager.h"

namespace ubx
{

ubx_config_manager::ubx_config_manager(i_ubx_transport&         transport,
                                         i_ini_config_provider&   config_provider,
                                         i_ubx_config_repository& repository)
    : transport_     (transport)
    , valset_builder_()
    , valget_builder_()
    , sync_service_  (transport, config_provider, repository,
                       valget_builder_, valset_builder_)
{}

// ── Direct VALSET ─────────────────────────────────────────────────────────────

bool ubx_config_manager::set_config(const std::vector<config_entry>& entries,
                                      config_layer layer)
{
    if (entries.empty())
    {
        return false;
    }
    const std::vector<uint8_t> msg = valset_builder_.build(entries, layer);
    if (msg.empty())
    {
        return false;
    }
    return transport_.send(msg);
}

// ── Direct VALGET ─────────────────────────────────────────────────────────────

bool ubx_config_manager::poll_config(const std::vector<uint32_t>& key_ids,
                                      uint8_t layer)
{
    if (key_ids.empty())
    {
        return false;
    }
    const std::vector<uint8_t> msg = valget_builder_.build(key_ids, layer);
    if (msg.empty())
    {
        return false;
    }
    return transport_.send(msg);
}

// ── VALGET response callback ──────────────────────────────────────────────────

void ubx_config_manager::on_valget_response(const uint8_t* payload,
                                              uint16_t length)
{
    sync_service_.on_valget_response(payload, length);
}

// ── Configuration synchronisation ────────────────────────────────────────────

bool ubx_config_manager::start_sync(const std::string& ini_path)
{
    if (!sync_service_.load_default_config(ini_path))
    {
        return false;
    }
    return sync_service_.start_valget_poll();
}

bool ubx_config_manager::apply_pending_sync(config_layer layer)
{
    return sync_service_.apply_configuration(layer);
}

} // namespace ubx
