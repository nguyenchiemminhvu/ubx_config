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
#include "ubx_cfg_valget_builder.h"
#include "ubx_cfg_valset_builder.h"
#include "i_ubx_transport.h"
#include "i_ini_config_provider.h"
#include "i_ubx_config_repository.h"
#include <cstdint>
#include <string>
#include <vector>

namespace ubx
{

// ─────────────────────────────────────────────────────────────────────────────
// ubx_config_sync_service
//
// Implements the four-step GNSS configuration synchronisation protocol:
//
//   Step 1 – Load default configuration from INI file via i_ini_config_provider.
//   Step 2 – Send UBX-CFG-VALGET for all default-config keys to poll the
//             chip's current values.
//   Step 3 – Receive the VALGET response (called back by the UBX parser layer)
//             and store decoded key-value pairs in the repository.
//   Step 4 – Compare repository vs. defaults; send UBX-CFG-VALSET for every
//             key whose current value differs from the desired default.
//
// The service is split into asynchronous phases to accommodate the interrupt-
// driven UART receive path typical in an embedded Linux system:
//
//   location_service calls:
//     1. sync_service.load_default_config(path)
//     2. sync_service.start_valget_poll()
//     ... (UART receive + UBX parser callback) ...
//     3. sync_service.on_valget_response(payload, length)   [called by UBX parser]
//     4. sync_service.apply_configuration(layer)
//
// Dependencies are injected at construction time (Dependency Inversion).
// ─────────────────────────────────────────────────────────────────────────────

class ubx_config_sync_service
{
public:
    ubx_config_sync_service(i_ubx_transport&          transport,
                             i_ini_config_provider&    config_provider,
                             i_ubx_config_repository&  repository,
                             ubx_cfg_valget_builder&   valget_builder,
                             ubx_cfg_valset_builder&   valset_builder);

    // ── Step 1 ────────────────────────────────────────────────────────────────
    // Load the default configuration from the file at 'ini_path' using the
    // injected config provider.  The loaded entries are cached internally.
    // Returns false if the provider fails to load the file.
    bool load_default_config(const std::string& ini_path);

    // ── Step 2 ────────────────────────────────────────────────────────────────
    // Build and send a UBX-CFG-VALGET message polling the chip for every key
    // that was loaded in step 1.  The repository is cleared before the poll so
    // stale values from a previous cycle are not used.
    // Returns false if load_default_config() has not been called, the default
    // config is empty, or the transport send fails.
    bool start_valget_poll();

    // ── Step 3 ────────────────────────────────────────────────────────────────
    // Decode a raw UBX-CFG-VALGET response payload received from the GNSS chip
    // and store the resulting key-value pairs in the repository.
    //
    // 'payload' must point to the raw payload bytes (version byte through end
    // of key-value records, i.e. bytes 0..length-1 of the UBX payload field).
    // 'length'  is the total number of payload bytes.
    //
    // This method is safe to call multiple times for chunked responses — each
    // call simply appends decoded entries to the repository.
    void on_valget_response(const uint8_t* payload, uint16_t length);

    // ── Step 4 ────────────────────────────────────────────────────────────────
    // Compare the repository (current GNSS config) with the default config
    // loaded in step 1.  For every key where the current value differs from the
    // desired default, accumulate a VALSET update entry.  Send a single
    // UBX-CFG-VALSET message targeting 'layer' if any differences were found.
    //
    // Returns true if:
    //   - no differences were detected (chip already matches default config), or
    //   - a VALSET message was built and sent successfully.
    // Returns false if the VALSET build produced no message or the transport
    // send failed.
    bool apply_configuration(config_layer layer = config_layer::ram);

    // Returns the number of default config entries loaded in step 1.
    std::size_t default_config_size() const;

private:
    i_ubx_transport&         transport_;
    i_ini_config_provider&   config_provider_;
    i_ubx_config_repository& repository_;
    ubx_cfg_valget_builder&  valget_builder_;
    ubx_cfg_valset_builder&  valset_builder_;

    std::vector<config_entry> default_config_;
};

} // namespace ubx
