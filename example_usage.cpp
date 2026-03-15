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
// example_usage.cpp
//
// Demonstrates how to wire up and use the UBX configuration library from
// a Location Service.  The UART transport and INI provider implementations
// shown here are minimal stubs — replace with real platform code.

#include "ubx_config_manager.h"
#include "ubx_config_repository.h"
#include "ubx_config_key.h"
#include "i_ubx_transport.h"
#include "i_ini_config_provider.h"

#include <cstdio>
#include <cstring>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Stub: UART transport (replace with real fd-based implementation)
// ─────────────────────────────────────────────────────────────────────────────

class uart_transport : public ubx::i_ubx_transport
{
public:
    explicit uart_transport(int fd) : fd_(fd) {}

    bool send(const std::vector<uint8_t>& message) override
    {
        // write() / writev() the message to the UART file descriptor
        // Return true if all bytes were written without error
        (void)message;
        std::printf("[UART] send %zu bytes\n", message.size());
        return true;   // stub: always succeeds
    }

private:
    int fd_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Stub: INI config provider (replace with real ini_parser adapter)
//
// A real implementation would:
//   1. Call ini_parser::ini_parser::load(path)
//   2. Iterate parser keys via get_keys()
//   3. Use ubx_cfg_key_registry::lookup_by_name() to convert each name to
//      a key_id, then push {key_id, config_value} into entries_
// ─────────────────────────────────────────────────────────────────────────────

class gnss_ini_config_provider : public ubx::i_ini_config_provider
{
public:
    bool load(const std::string& path) override
    {
        std::printf("[INI] loading %s\n", path.c_str());

        // Hardcoded defaults for demonstration only
        entries_ = {
            { ubx::cfg_key::rate_meas,
              ubx::config_value(static_cast<uint16_t>(1000u)) },

            { ubx::cfg_key::navspg_dynmodel,
              ubx::config_value(static_cast<uint8_t>(6u)) },

            { ubx::cfg_key::msgout_ubx_nav_pvt_uart1,
              ubx::config_value(static_cast<uint8_t>(1u)) },

            { ubx::cfg_key::signal_gps_ena,
              ubx::config_value(true) },
        };
        return true;
    }

    std::vector<ubx::config_entry> get_all_entries() const override
    {
        return entries_;
    }

private:
    std::vector<ubx::config_entry> entries_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Location Service startup
// ─────────────────────────────────────────────────────────────────────────────

int main()
{
    // Wire up dependencies
    uart_transport            transport(/*fd=*/-1);
    gnss_ini_config_provider  ini_provider;
    ubx::ubx_config_repository repository;

    ubx::ubx_config_manager manager(transport, ini_provider, repository);

    // ── Startup synchronisation (steps 1 & 2) ──────────────────────────────

    if (!manager.start_sync("/etc/gnss_default_config.ini"))
    {
        std::fprintf(stderr, "Failed to start GNSS config sync\n");
        return 1;
    }

    // ── Simulate receiving a VALGET response from the chip ─────────────────
    //
    // In production this is called from the UBX parser callback when a
    // UBX-CFG-VALGET response (class=0x06, id=0x8B) is received on UART.
    //
    // Here we fabricate a response payload with mismatched rate_meas (500 ms)
    // so that the synchronisation step produces a VALSET:
    //
    //   [version=0x01][layer=0x00][position=0x0000]
    //   [key_id: rate_meas   0x30210001 LE][value: 500 LE uint16_t]
    //   [key_id: navspg_dynmodel 0x20110021 LE][value: 6 LE uint8_t]
    //   [key_id: msgout_ubx_nav_pvt_uart1 0x20910007 LE][value: 1 LE uint8_t]
    //   [key_id: signal_gps_ena 0x1031001F LE][value: 1 LE uint8_t]

    const uint8_t simulated_valget_response[] = {
        // Header
        0x01u, 0x00u, 0x00u, 0x00u,
        // rate_meas (key size 3 → 2 bytes) — value 500, differs from default 1000
        0x01u, 0x00u, 0x21u, 0x30u,   // key LE
        0xF4u, 0x01u,                   // 500 LE
        // navspg_dynmodel (key size 2 → 1 byte)
        0x21u, 0x00u, 0x11u, 0x20u,   // key LE
        0x06u,
        // msgout_ubx_nav_pvt_uart1 (key size 2 → 1 byte)
        0x07u, 0x00u, 0x91u, 0x20u,   // key LE
        0x01u,
        // signal_gps_ena (key size 1 → 1 byte)
        0x1Fu, 0x00u, 0x31u, 0x10u,   // key LE
        0x01u,
    };

    manager.on_valget_response(simulated_valget_response,
                                static_cast<uint16_t>(sizeof(simulated_valget_response)));

    // ── Step 4: compare and apply differences ──────────────────────────────

    if (!manager.apply_pending_sync(ubx::config_layer::ram))
    {
        std::fprintf(stderr, "Failed to apply GNSS config sync\n");
        return 1;
    }

    // ── Ad-hoc set after sync ───────────────────────────────────────────────

    manager.set_config({
        { ubx::cfg_key::rate_meas,
          ubx::config_value(static_cast<uint16_t>(200u)) },

        { ubx::cfg_key::navspg_dynmodel,
          ubx::config_value(static_cast<uint8_t>(4u)) },
    }, ubx::config_layer::ram);

    // ── Ad-hoc poll ────────────────────────────────────────────────────────

    manager.poll_config({
        ubx::cfg_key::rate_meas,
        ubx::cfg_key::navspg_dynmodel,
        ubx::cfg_key::signal_gps_ena,
    });

    std::printf("[example] done\n");
    return 0;
}
