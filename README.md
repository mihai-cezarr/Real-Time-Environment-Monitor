# ⚡ Real-Time IoT Environment Monitor on Zybo Z7-20 (Zynq-7000 SoC)

![Zynq-7000](https://img.shields.io/badge/SoC-Xilinx%20Zynq--7000-FF6600?style=for-the-badge&logo=xilinx)
![ARM Cortex-A9](https://img.shields.io/badge/CPU-Dual--Core%20ARM%20Cortex--A9-0091BD?style=for-the-badge&logo=arm)
![Vivado](https://img.shields.io/badge/FPGA-Vivado%202024.1-FF6600?style=for-the-badge&logo=xilinx)
![Vitis](https://img.shields.io/badge/Embedded-Vitis%20IDE%202024.1-5C2D91?style=for-the-badge)
![Wi-Fi](https://img.shields.io/badge/Connectivity-ESP--01%20(ESP8266)-000000?style=for-the-badge)

A complete hardware/software co-design IoT environment monitoring platform implemented on the Digilent Zybo Z7-20 board. The system continuously samples 3 analog channels via the integrated 12-bit XADC converter, processes telemetry on an ARM Cortex-A9 processor, and serves an embedded dynamic web dashboard over Wi-Fi using an ESP-01 module configured as a wireless Access Point.

---

## 📌 Architecture Overview

```text
       +-------------------------------------------------------------+
       |                     Zybo Z7-20 SoC                          |
       |                                                             |
 3x Analog    +-------------+      AXI4-Lite     +-----------------+ |
 Inputs ----> | XADC Wizard | <----------------> | ARM Cortex-A9   | |
 (Potentiom.) | (12-bit ADC)|                    | Processing Syst.| |
              +-------------+                    +--------+--------+ |
                                                          |          |
       +--------------------------------------------------+          |
       |                  |                                          |
       v UART0 (115200)   v UART1 (115200)                           |
 +------------+     +------------+                                   |
 | ESP-01     |     | PC Serial  |                                   |
 | Wi-Fi AP   |     | Terminal   |                                   |
 +-----+------+     +------------+                                   |
       |                                                             |
       v Wireless HTTP (Port 80)                                     |
 +------------+                                                      |
 | Web Client |                                                      |
 +------------+                                                      |
+--------------------------------------------------------------------+
```

---

## 🔑 Technical Highlights

* **Hardware/Software Co-Design:** Configured in Vivado 2024.1 using Zynq PS connected to XADC Wizard IP via AXI Interconnect (`ps7_0_axi_periph`).
* **Continuous Multi-Channel ADC Sampling:** Reads analog inputs on channels `VAUX6`, `VAUX7`, and `VAUX14` with 12-bit effective resolution: `V = (RAW >> 4) * (3.0 / 4096.0)`.
* **Dual-UART Pipeline:**
  * **UART0 (115200 Baud):** Handles ESP-01 AT-command configuration, socket setup, and HTTP response transmission.
  * **UART1 (115200 Baud):** Independent debug channel streaming real-time diagnostic logs to PC serial terminals (`hTerm`).
* **Chunked Packet Protocol:** Custom serial driver transmitting HTTP payloads in 64-byte chunks with 10ms delays, preventing UART hardware FIFO overrun on the ESP-01 module.
* **Embedded Web Server:** Serves dynamic HTML/CSS dashboards directly from ARM memory, featuring real-time voltage threshold alert cards (*Normal*, *Attention*, *Critical*) and 2-second auto-refresh.

---

## 🔌 Hardware Pin Mapping

| Component | Signal | Zybo Z7-20 Pin | Voltage | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **ESP-01 Wi-Fi** | `TX` -> `RX` | `W16` (Pmod JE) | 3.3V | Serial Communication (UART0) |
| **ESP-01 Wi-Fi** | `RX` <- `TX` | `V12` (Pmod JE) | 3.3V | Serial Communication (UART0) |
| **Potentiometer 1** | `VAUX6` | Channel 6 | 0.0 - 3.3V | Auxiliary Analog Input |
| **Potentiometer 2** | `VAUX7` | Channel 7 | 0.0 - 3.3V | Auxiliary Analog Input |
| **Potentiometer 3** | `VAUX14` | Channel 14 | 0.0 - 3.3V | Auxiliary Analog Input |

---

## 💻 Flashing & Execution Guide

### 1. Hardware Generation (Vivado)
1. Open `UART0_UART1_Test.xpr` in **Vivado 2024.1**.
2. Run **Generate Bitstream**.
3. Export Hardware Spec (`File -> Export -> Export Hardware`), including the Bitstream file (`design_1_wrapper.xsa`).

### 2. Firmware Build & Flash (Vitis)
1. Open **Vitis IDE 2024.1**.
2. Create a platform component referencing `design_1_wrapper.xsa`.
3. Import `firmware/main.c` into a Standalone Application.
4. Build the application and program the Zybo board (**Run on Hardware**).

---

## 🌐 Connecting to Telemetry Dashboard

1. Connect your PC/Smartphone to Wi-Fi Network: **`Monitorizare_v2`** (Password: `potentiometru`).
2. Open any modern web browser and navigate to: `http://192.168.4.1`.
3. The dashboard will load and automatically refresh analog sensor values every 2 seconds.

---
*Developed by **Mihai-Cezar Turceniuc** (Computer Science, Technical University of Cluj-Napoca).*