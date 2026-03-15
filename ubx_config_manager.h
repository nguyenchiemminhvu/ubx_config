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
#pragma once

#include "ubx_config_types.h"
#include "ubx_cfg_valset_builder.h"
#include "ubx_cfg_valget_builder.h"
#include "ubx_config_sync_service.h"
#include "i_ubx_transport.h"
#include "i_ini_config_provider.h"
#include "i_ubx_config_repository.h"
#include <cstdint>
#include <string>
#include <vector>

namespace ubx
{

// ─────────────────────────────────────────────────────────────────────────────
// ubx_config_manager
//
// The primary public API for GNSS configuration management.
//
// Responsibilities:
//   • Set configuration values (VALSET).
//   • Poll current configuration values (VALGET).
//   • Route incoming VALGET response payloads to the sync service.
//   • Orchestrate the full synchronisation cycle
//     (load INI → VALGET → compare → VALSET).
//
// All dependencies are injected at construction time.  The manager owns the
// concrete builder instances while accepting the transport, repository and
// INI provider through their abstract interfaces.
//
// Usage (Location Service startup sequence):
//
//   uart_transport  transport;      // your UART implementation
//   gnss_ini_provider ini_prov;     // your INI adapter
//   ubx::ubx_config_repository repo;
//   ubx::ubx_config_manager mgr(transport, ini_prov, repo);
//
//   // Startup config sync
//   mgr.synchronize("/etc/gnss_default_config.ini");
//
//   // Later, when UBX parser receives a VALGET response:
//   mgr.on_valget_response(payload_ptr, payload_len);
//
//   // Ad-hoc set
//   mgr.set_config({ {ubx::cfg_key::rate_meas, ubx::config_value(uint16_t(200))} });
// ─────────────────────────────────────────────────────────────────────────────

class ubx_config_manager
{
public:
    ubx_config_manager(i_ubx_transport&         transport,
                        i_ini_config_provider&   config_provider,
                        i_ubx_config_repository& repository);

    // Explicitly non-copyable — owns builder value objects.
    ubx_config_manager(const ubx_config_manager&)            = delete;
    ubx_config_manager& operator=(const ubx_config_manager&) = delete;

    // ── Direct VALSET ──────────────────────────────────────────────────────────
    // Build and send a UBX-CFG-VALSET message for the provided key-value pairs.
    // Returns true if the message was sent successfully.
    bool set_config(const std::vector<config_entry>& entries,
                    config_layer layer = config_layer::ram);

    // ── Direct VALGET ─────────────────────────────────────────────────────────
    // Build and send a UBX-CFG-VALGET poll request for the provided key IDs.
    // The response is delivered asynchronously through on_valget_response().
    // Returns true if the request was sent successfully.
    bool poll_config(const std::vector<uint32_t>& key_ids,
                     uint8_t layer = ubx_cfg_valget_builder::LAYER_RAM);

    // ── VALGET response callback ───────────────────────────────────────────────
    // Must be called by the UBX parser layer whenever a UBX-CFG-VALGET response
    // (class=0x06, id=0x8B, version byte=0x01) is received.
    //
    // 'payload' points to the raw UBX payload bytes (excluding sync, header and
    // checksum).  'length' is the payload byte count.
    void on_valget_response(const uint8_t* payload, uint16_t length);

    // ── Configuration synchronisation ─────────────────────────────────────────
    // Run step 1 + 2 of the synchronisation cycle:
    //   1. Load default configuration from 'ini_path'.
    //   2. Send VALGET to poll the chip's current configuration.
    //
    // Step 3 (response decoding) is handled via on_valget_response().
    // Step 4 (apply differences) must be called explicitly after the response
    // arrives — see apply_pending_sync() below.
    //
    // Returns false if the INI file cannot be loaded or the VALGET send fails.
    bool start_sync(const std::string& ini_path);

    // Apply the pending synchronisation after a VALGET response has been
    // received and stored.  Sends VALSET for keys that differ from defaults.
    // Returns true if the chip was already correct (no VALSET needed) or if
    // VALSET was sent successfully.
    bool apply_pending_sync(config_layer layer = config_layer::ram);

private:
    i_ubx_transport&         transport_;

    ubx_cfg_valset_builder   valset_builder_;
    ubx_cfg_valget_builder   valget_builder_;
    ubx_config_sync_service  sync_service_;
};

} // namespace ubx
