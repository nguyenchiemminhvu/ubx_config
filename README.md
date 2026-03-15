# UBX Configuration Library

A **C++14** GNSS configuration subsystem for u-blox receivers running on embedded Linux (automotive).

Implements the UBX **CFG-VALGET** / **CFG-VALSET** binary protocol with SOLID architecture and strict little-endian serialisation.

---

## Directory Structure

```
ubx_config/
├── ubx_config_key.h              — All official key IDs (imported from u-blox ubxlib)
├── ubx_config_types.h            — config_value, config_entry, config_layer
├── ubx_protocol_utils.h          — Header-only LE serialisation + UBX framing
│
├── i_ubx_transport.h             — Interface: send bytes over UART
├── i_ini_config_provider.h       — Interface: load default config from INI file
├── i_ubx_config_repository.h     — Interface: store/fetch current GNSS config
│
├── ubx_cfg_valset_builder.h/cpp  — Build VALSET binary messages
├── ubx_cfg_valget_builder.h/cpp  — Build VALGET poll requests
├── ubx_config_repository.h/cpp   — In-memory implementation of the repository
├── ubx_cfg_key_registry.h/cpp    — String-name ↔ key-ID lookup table
├── ubx_config_sync_service.h/cpp — 4-step config synchronisation logic
├── ubx_config_manager.h/cpp      — Public API facade
│
├── example_usage.cpp             — Stub-based integration example
└── Makefile
```

---

## Architecture

### Component Responsibilities

| Component | Role |
|---|---|
| `ubx_config_manager` | Public API — set, poll, sync |
| `ubx_config_sync_service` | 4-step sync protocol state machine |
| `ubx_cfg_valset_builder` | Serialise VALSET payload (little-endian) |
| `ubx_cfg_valget_builder` | Serialise VALGET payload |
| `ubx_config_repository` | In-memory current-state cache |
| `ubx_cfg_key_registry` | Name → key-ID lookup table |
| `i_ubx_transport` | Abstracts UART send |
| `i_ini_config_provider` | Abstracts INI file parsing |
| `i_ubx_config_repository` | Abstracts repository storage |

---

## Synchronisation Flow

```
1. load_default_config(ini_path)
      → i_ini_config_provider::load()
      → cache default config_entry list

2. start_valget_poll()
      → build VALGET message for all default-config keys
      → i_ubx_transport::send()

3. on_valget_response(payload, length)            ← called by UBX parser
      → decode LE key-value pairs from payload
      → store in i_ubx_config_repository

4. apply_configuration(layer)
      → for each default key: compare repo value vs default value
      → if different: add to update list
      → build single VALSET message
      → i_ubx_transport::send()
```

---

## Build

```bash
make             # builds libubx_config.a
make example     # builds example_usage binary
make clean
```

---

## UBX Protocol Notes

- All multi-byte values in VALGET / VALSET payloads are **little-endian**.
- Key ID bits 27-24 encode value size: `1`/`2`→1 byte, `3`→2 bytes, `4`→4 bytes, `5`→8 bytes.
- VALSET layers: `RAM=0x01`, `BBR=0x02`, `FLASH=0x04` (combinable as bit flags).
- VALGET response version byte = `0x01`; poll request version = `0x00`.

---

## Architecture Diagrams (PlantUML)

### 1. Static View — Class Diagram

```plantuml
@startuml ubx_config_class_diagram
skinparam classAttributeIconSize 0
skinparam linetype ortho
skinparam packageStyle rectangle

package "Interfaces (DIP boundaries)" <<Frame>> {
    interface i_ubx_transport {
        + send(message : vector<uint8_t>) : bool
    }

    interface i_ini_config_provider {
        + load(path : string) : bool
        + get_all_entries() : vector<config_entry>
    }

    interface i_ubx_config_repository {
        + store(entry : config_entry)
        + fetch(key_id : uint32_t, out : config_value&) : bool
        + contains(key_id : uint32_t) : bool
        + all_keys() : vector<uint32_t>
        + clear()
    }
}

package "Data Types" <<Frame>> {
    class config_value {
        + raw : uint64_t
        + as_bool() / as_u8() / as_u16() / as_u32() / ...
    }

    class config_entry {
        + key_id : uint32_t
        + value : config_value
    }

    enum config_layer {
        ram   = 0x01
        bbr   = 0x02
        flash = 0x04
    }
}

package "Protocol Utilities" <<Frame>> {
    class ubx_protocol_utils << (H, orchid) header-only >> {
        + frame_ubx_message(cls, id, payload)
        + write_le16 / write_le32 / write_le64
        + read_le16 / read_le32 / read_le64
        + value_byte_size(key_id) : uint8_t
        + compute_checksum(data, len, ck_a, ck_b)
    }

    class ubx_cfg_key_registry {
        + {static} lookup_by_name(name, out_id) : bool
        + {static} lookup_by_id(key_id) : const char*
        + {static} table() : const key_info*
        + {static} table_size() : size_t
    }
}

package "Builders" <<Frame>> {
    class ubx_cfg_valset_builder {
        + build(entries, layer) : vector<uint8_t>
    }

    class ubx_cfg_valget_builder {
        + build(key_ids, layer, position) : vector<uint8_t>
        + {static} LAYER_RAM : uint8_t
        + {static} LAYER_BBR : uint8_t
        + {static} LAYER_FLASH : uint8_t
        + {static} LAYER_DEFAULT : uint8_t
    }
}

package "Repository" <<Frame>> {
    class ubx_config_repository {
        - entries_ : map<uint32_t, config_value>
        + store(entry)
        + fetch(key_id, out_value) : bool
        + contains(key_id) : bool
        + all_keys() : vector<uint32_t>
        + clear()
    }
}

package "Services" <<Frame>> {
    class ubx_config_sync_service {
        - default_config_ : vector<config_entry>
        + load_default_config(path) : bool
        + start_valget_poll() : bool
        + on_valget_response(payload, length)
        + apply_configuration(layer) : bool
        + default_config_size() : size_t
    }

    class ubx_config_manager {
        + set_config(entries, layer) : bool
        + poll_config(key_ids, layer) : bool
        + on_valget_response(payload, length)
        + start_sync(ini_path) : bool
        + apply_pending_sync(layer) : bool
        - valset_builder_ : ubx_cfg_valset_builder
        - valget_builder_ : ubx_cfg_valget_builder
        - sync_service_   : ubx_config_sync_service
    }
}

' Implementations
ubx_config_repository     ..|> i_ubx_config_repository
note bottom of i_ubx_transport        : Implemented by Location Service\n(UART driver)
note bottom of i_ini_config_provider  : Implemented by Location Service\n(wraps ini_parser library)

' Ownership / composition
ubx_config_manager        *-- ubx_cfg_valset_builder
ubx_config_manager        *-- ubx_cfg_valget_builder
ubx_config_manager        *-- ubx_config_sync_service

' Dependencies (injected)
ubx_config_manager        --> i_ubx_transport
ubx_config_manager        --> i_ubx_config_repository

ubx_config_sync_service   --> i_ubx_transport
ubx_config_sync_service   --> i_ini_config_provider
ubx_config_sync_service   --> i_ubx_config_repository
ubx_config_sync_service   --> ubx_cfg_valget_builder
ubx_config_sync_service   --> ubx_cfg_valset_builder

' Builders use protocol utils
ubx_cfg_valset_builder    ..> ubx_protocol_utils
ubx_cfg_valget_builder    ..> ubx_protocol_utils
ubx_config_sync_service   ..> ubx_protocol_utils

' Key registry used by INI provider implementors
ubx_cfg_key_registry      ..> config_entry

' Type relationships
config_entry              *-- config_value
@enduml
```

---

### 2. Dynamic View — Synchronisation Sequence Diagram

```plantuml
@startuml ubx_config_sync_sequence
skinparam sequenceMessageAlign center
skinparam responseMessageBelowArrow true

actor "location_service" as LS
participant "ubx_config_manager" as MGR
participant "ubx_config_sync_service" as SYNC
participant "i_ini_config_provider" as INI
participant "ubx_cfg_valget_builder" as VGBLD
participant "ubx_cfg_valset_builder" as VSBLD
participant "i_ubx_config_repository" as REPO
participant "i_ubx_transport" as UART
participant "gnss_receiver" as GNSS

== System Startup ==

LS -> MGR : start_sync("/etc/gnss_default_config.ini")

group Step 1 — Load Default Configuration
    MGR -> SYNC : load_default_config(path)
    SYNC -> INI : load(path)
    INI --> SYNC : true
    SYNC -> INI : get_all_entries()
    INI --> SYNC : vector<config_entry> [default_config]
    SYNC -> SYNC : cache default_config_
    SYNC --> MGR : true
end

group Step 2 — Poll Current GNSS Configuration
    MGR -> SYNC : start_valget_poll()
    SYNC -> REPO : clear()
    SYNC -> VGBLD : build(key_ids, LAYER_RAM)
    VGBLD --> SYNC : valget_msg bytes
    SYNC -> UART : send(valget_msg)
    UART -> GNSS : UBX-CFG-VALGET (binary)
    UART --> SYNC : true
    SYNC --> MGR : true
    MGR --> LS : true
end

== Asynchronous UART receive path ==

GNSS -> UART : UBX-CFG-VALGET response (binary)
note right of UART : UBX parser (external)\nidentifies class=0x06, id=0x8B\nversion=0x01

group Step 3 — Decode VALGET Response
    LS -> MGR : on_valget_response(payload, length)
    MGR -> SYNC : on_valget_response(payload, length)
    loop for each key-value record in payload
        SYNC -> SYNC : read_le32(key_id)
        SYNC -> SYNC : value_byte_size(key_id)
        SYNC -> SYNC : read_config_value(p, sz)
        SYNC -> REPO : store({key_id, value})
    end
    MGR --> LS : (return)
end

== Apply Differences ==

LS -> MGR : apply_pending_sync(config_layer::ram)

group Step 4 — Compare and Apply VALSET
    MGR -> SYNC : apply_configuration(layer)
    loop for each entry in default_config_
        SYNC -> REPO : fetch(key_id, current_value)
        REPO --> SYNC : current_value (or not present)
        alt current_value != desired_value
            SYNC -> SYNC : push to updates list
        end
    end
    alt updates is not empty
        SYNC -> VSBLD : build(updates, layer)
        VSBLD --> SYNC : valset_msg bytes
        SYNC -> UART : send(valset_msg)
        UART -> GNSS : UBX-CFG-VALSET (binary)
        UART --> SYNC : true
    else no differences
        SYNC -> SYNC : return true (no-op)
    end
    SYNC --> MGR : true
    MGR --> LS : true
end
@enduml
```

---

## Example Configuration File (`/etc/gnss_default_config.ini`)

```ini
[gnss]
; Measurement rate in milliseconds
rate_meas=1000

; Dynamic platform model: 6 = automotive
navspg_dynmodel=6

; Enable UBX-NAV-PVT output on UART1
msgout_ubx_nav_pvt_uart1=1

; Enable GPS constellation
signal_gps_ena=1
signal_gps_l1ca_ena=1
signal_gps_l2c_ena=1
```

> Key names must match the snake_case identifiers in `ubx_config_key.h`.
> The INI adapter implementation uses `ubx_cfg_key_registry::lookup_by_name()`
> to resolve each name to its numeric key ID.