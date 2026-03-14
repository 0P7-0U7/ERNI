<p align="right">
  <a href="https://buymeacoffee.com/optoutbrussels">
    <img src="https://img.shields.io/badge/Buy%20Me%20a%20Coffee-ffdd00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black" alt="Buy Me a Coffee">
  </a>
</p>


## License (CNPL v4)
ERNI is released under the **Cooperative Non-Violent Public License**.

This means you are free to use it for art, education, and civilian research. However:
- **NO Military Use:** Use by military organizations or for weapons development is strictly prohibited.
- **NO Surveillance:** Use for state-sponsored surveillance is prohibited.
- **Cooperative Only:** Commercial use is reserved for worker-owned cooperatives.

----------------------------------------




<picture>
  <source media="(prefers-color-scheme: dark)" srcset="images/LOGO_DARK.png">
  <source media="(prefers-color-scheme: light)" srcset="images/LOGO_LIGHT.png">
  <img alt="ERNI by OPT-OUT" src="images/LOGO_LIGHT.png" width="100%">
</picture>

# ERNI by OPT-OUT
**ESP-NOW Reticulum Network Interface**

A stripped-down, ultra-high-speed C++ bridge that converts ESP-NOW hardware into a physical layer for the Reticulum Network Stack (RNS). Connects to Reticulum via `SerialInterface` using the standard KISS protocol.

### Features
* **Fragment Bridging:** Transparently slices Reticulum's 500-byte MTU into ESP-NOW compliant 248-byte chunks with a 2-byte `[PacketID][Sequence]` header. Inter-fragment pacing ensures clean reassembly on noisy channels.
* **Managed Flooding:** Every node is a repeater. FNV-1a 32-bit hashing with a fixed-size circular dedup cache provides O(1) duplicate detection with zero heap allocations.
* **Multi-Sender Safety:** Reassembly tracks sender MAC addresses, preventing fragment interleaving from simultaneous transmitters.
* **Queued Rebroadcast:** Mesh repeats are queued in a lock-free ring buffer and dispatched from the main `loop()` context, avoiding reentrancy inside ESP-NOW callbacks.
* **KISS Compliant:** Full KISS TNC specification with the standard `0x00` command byte. Plug into Reticulum's `SerialInterface` at 921600 baud.

---

## Quick Start

**ESP32 Hardware Compatibility:**
- **100% Variant Agnostic:** Native ESP-NOW architecture enables ERNI to run cleanly on all chips (ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6).

### 1. Flash
Clone and open the sketch in your Arduino IDE:
```bash
git clone https://github.com/0P7-0U7/ERNI.git
cd ERNI/examples/ERNI_Alpha
```
Flash `ERNI_Alpha.ino` to any ESP32. No dependencies beyond the standard Espressif `esp_now.h` framework.

### 2. Configure Reticulum
Plug the ESP32 into your PC via USB and add this to `~/.reticulum/config`:
```ini
  [[ERNI_Mesh]]
    type = SerialInterface
    interface_enabled = True
    outgoing = True
    port = /dev/ttyUSB0
    speed = 921600
    databits = 8
    parity = none
    stopbits = 1
```

### 3. Run
Launch NomadNet, Sideband, or any Reticulum application. ERNI is invisible — it just works as the radio layer. Any other ERNI node within ESP-NOW range will mesh automatically. Drop a standalone ESP32 (battery-powered, no PC) between nodes and it acts as a repeater with zero configuration.

---

### Full Documentation
For architecture diagrams, protocol details, and the v1.1 changelog, please download the library and open the included **`docs/index.html`** file in your browser.
OR JUST GO TO <a href="https://0p7-0u7.github.io/ERNI/" target="_blank">ERNI PAGE</a>
