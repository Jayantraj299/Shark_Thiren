## **Shark Thiren V1: System Documentation**

**Shark Thiren V1** is a handheld, custom-built embedded multi-tool designed for local network reconnaissance, hardware diagnostics, and retro emulation. Engineered entirely on a single-core ESP32-C3 architecture, the system utilizes an aggressive dynamic memory management strategy and a FreeRTOS task scheduler to run memory-intensive Wi-Fi and Bluetooth Low Energy (BLE) stacks within a strict 400KB SRAM limit.

### **Hardware Specifications**

| Component | Specification / Configuration |
| :---- | :---- |
| **Microcontroller** | ESP32-C3 Super Mini (Single-Core RISC-V, 160MHz) |
| **Display** | 1.8-inch ST7735 TFT SPI LCD (128x160 Resolution, Portrait Orientation) |
| **Input Interface** | Custom 3x3 Matrix Keypad (Mapped to D-Pad, A, B, C, D, SELECT) |
| **Storage** | MicroSD Card Module (SPI, CS Pin 7\) |
| **Power Management** | Integrated battery monitor reading voltage via Analog Pin 0 |
| **Matrix Row Pins** | GPIO 20, 1, 2 |
| **Matrix Col Pins** | GPIO 3, 21, 8 |

### **Software Architecture & OS Design**

The operating system is built in C++ using the Arduino framework, structured around a custom Text User Interface (TUI) and a decoupled radio architecture to prevent heap exhaustion.

> * **FreeRTOS Core Pinning:** Due to the ESP32-C3's single-core RISC-V architecture, the main UI loop (tuiTask) is explicitly pinned to Core 0 with a localized 4096-byte stack, preventing the fatal crash loops common when porting dual-core ESP32 code.  
> * **Dynamic Radio Decoupling:** The Wi-Fi and BLE hardware radios are initialized in WIFI\_MODE\_NULL (deep sleep) on boot. The stacks are dynamically awakened, allocated memory, and completely torn down only when specific tools are launched and exited via the TUI.  
> * **Mount-on-Demand File System:** To preserve \~30KB of working RAM, the SD card FATFS driver is only mounted during direct ROM access or system diagnostics, remaining unmounted during radio reconnaissance operations.  
> * **Debounced Matrix Scanner:** Input is handled via a non-blocking, multiplexed column-scanning loop within the TUI task, utilizing a 30ms physical debounce delay to prevent phantom key presses.

### **Primary Feature Modules**

| Module | Description | Technical Implementation |
| :---- | :---- | :---- |
| **Wi-Fi Sniffer** | Passive 802.11 management frame capture. | Puts radio in promiscuous mode (WIFI\_MODE\_STA), hops across channels 1-13 sequentially, and parses raw packet buffers to extract SSIDs and RSSI values. |
| **BLE Recon** | Bluetooth Low Energy target acquisition and skimmer detection. | Utilizes BLEScan to actively ping surrounding devices, extracting MAC addresses and filtering for suspicious nomenclature (e.g., HC-0, JDY-). |
| **Apple Spoofer** | BLE advertisement injection tool. | Broadcasts randomized, high-power 0x4C (Apple) manufacturer payloads to trigger proximity pairing alerts on surrounding iOS devices. |
| **CHIP-8 Emulator** | Native retro gaming environment. | Reads .ch8 ROM files from the SD card (/Shark\_Thiren/games/), executing opcodes in a localized 4KB virtual machine with matrix keypad mapping. |
| **System Tools** | Hardware diagnostics suite. | Real-time D-Pad matrix testing, SD card volume indexing, ESP32 CPU/RAM profiling, and live analog battery percentage calculation. |

### **Interface & Experience**

The graphical environment is built on the Adafruit GFX library, operating in INITR\_BLACKTAB mode. The interface relies on a compact, nested folder structure (Wi-Fi, Bluetooth, Games, Settings) to organize tools cleanly on the 128x160 display. Visual feedback is prioritized through color-coded status bars (Red for recording, Blue for BLE, Green for battery health) and a dynamic, space-padded string rendering system that allows for smooth dot-animations without forcing full-screen flicker refreshes.  
The system's idle state features **Massicot**, the custom cyber-shark mascot, rendered as a direct RGB bitmap from PROGMEM.  

