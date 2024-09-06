![N3 Logo](https://i.imgur.com/xraBuH7.png)

# **NETH3T Penetration Testing Device**
### (Network Exploits and Testing Tool 3) 

## **Project Overview**
This project aims to create a **multifunctional, portable penetration testing device** inspired by the capabilities of the **Flipper Zero**, but with enhanced functionality. The device leverages the **DSTIKE D-duino 32 (ESP32-based)** microcontroller and integrates several RF, NFC/RFID, IR, and Wi-Fi testing capabilities. It allows users to perform **Wi-Fi deauthentication attacks**, **RF signal sniffing and jamming**, **NFC/RFID cloning**, and **IR signal replaying**, all while being compact, portable, and flexible.

This device is built to be used in **wireless penetration testing, RF analysis**, and **physical security assessments**, providing a versatile tool for security researchers and hobbyists.

## **Key Features**
### **1. Wi-Fi Penetration Testing (via ESP32)**
- **Network Scanning:** Detect nearby Wi-Fi networks.
- **Deauthentication Attacks:** Disconnect devices from selected Wi-Fi networks.
- **Packet Injection:** Inject custom packets into Wi-Fi networks for testing.
- **Packet Sniffing:** Capture Wi-Fi packets for analysis.

### **2. Bluetooth Penetration Testing (via ESP32)**
- **Bluetooth Device Scanning:** Discover nearby Bluetooth and BLE devices.
- **Bluetooth Attacks:** Conduct basic Bluetooth and BLE attacks such as fuzzing and packet injection.

### **3. Sub-GHz RF Testing (via CC1101)**
- **Frequency Range:** Operates in the **Sub-GHz range** (433 MHz, 868 MHz, 915 MHz).
- **Signal Sniffing:** Capture and decode signals from RF-based devices like key fobs and remote controls.
- **Signal Jamming:** Disrupt RF communication in the Sub-GHz frequency range.
- **Replay Attacks:** Capture and replay RF signals to interact with RF-based devices.

### **4. 2.4 GHz RF Testing (via NRF24L01+)**
- **Sniffing and Interception:** Capture proprietary 2.4 GHz RF signals (non-Wi-Fi/Bluetooth) used by certain IoT devices and wireless peripherals.
- **Packet Injection and Replay:** Inject and replay packets to manipulate or disrupt communication in proprietary RF systems.

### **5. NFC/RFID Testing (via PN5180)**
- **NFC Tag Scanning and Reading:** Detect and read NFC tags and RFID cards.
- **Tag Cloning and Emulation:** Clone and emulate RFID/NFC cards to test access control systems.
- **Relay Attacks:** Perform NFC relay attacks for security testing.

### **6. IR Signal Capture and Transmission (via IR LED/Receiver)**
- **IR Signal Capture:** Record IR signals from remotes (e.g., TV, air conditioner remotes).
- **Signal Replay:** Replay captured IR signals to control IR-based devices.

### **7. Data Logging and Custom Scripts (via MicroSD)**
- **Log Storage:** Store captured data, signals, and test results on a MicroSD card.
- **Custom Scripts:** Load and execute custom attack scripts from the MicroSD card to automate pen-testing tasks.

## **Components**
The project uses several hardware components to enable all the features described:

### **Microcontroller:**
- **DSTIKE D-duino 32 (ESP32-based)**: The core of the device, handling Wi-Fi, Bluetooth, and control of other connected modules. [click here](https://dstike.com/products/d-duino-32-ii-dual-screen)

### **Modules:**
1. **PN5180 (NFC/RFID Reader/Writer)**: [click here](https://www.aliexpress.us/item/2255800898209354.html?spm=a2g0o.productlist.main.3.426ede69lRwYd9&algo_pvid=5be9b316-9c80-437f-acb0-cde0fe82ce23&algo_exp_id=5be9b316-9c80-437f-acb0-cde0fe82ce23-1&pdp_npi=4%40dis%21USD%214.40%214.40%21%21%214.40%214.40%21%402101e9a217256492677022260e3a15%2110000014282558858%21sea%21US%216045824045%21X&curPageLogUid=f0MugwJMs5DN&utparam-url=scene%3Asearch%7Cquery_from%3A)
   - Used for scanning, cloning, and emulating NFC/RFID tags.
2. **CC1101 RF Module (Sub-GHz)**: [click here](https://www.aliexpress.us/item/3256805747351167.html?spm=a2g0o.order_list.order_list_main.41.1f1c1802CPXAcc&gatewayAdapt=glo2usa)
   - For RF signal capturing, replaying, and jamming in Sub-GHz frequencies.
3. **NRF24L01+ Module (2.4 GHz RF)**: [click here](https://www.aliexpress.us/item/3256805993151494.html?spm=a2g0o.order_list.order_list_main.59.1f1c1802CPXAcc&gatewayAdapt=glo2usa)
   - Captures and manipulates 2.4 GHz proprietary RF communication protocols.
   
### **IR Components:**
1. **IR LED (Transmitter)**: Used to transmit recorded or custom IR signals. [click here](https://www.adafruit.com/product/5639) 
2. **TSOP38238 (IR Receiver)**: Captures incoming IR signals from remote controls. [click here](https://www.adafruit.com/product/157)

### **Peripherals:**
1. **Dual OLED Displays (I2C)**: Provides real-time feedback and a menu system for navigation. (Built into the Dstike D-Duino 32 Dual II)
2. **5-Way Navigation Button**: Allows easy navigation of the device's menu and options. (Can use the Embedded controls on the D-Duino if preferred) [click here](https://www.aliexpress.us/item/2251832767971032.html?spm=a2g0o.productlist.main.5.4777pKZUpKZU2u&algo_pvid=25cf2911-590e-4dc9-be68-7eefd06300b9&algo_exp_id=25cf2911-590e-4dc9-be68-7eefd06300b9-2&pdp_npi=4%40dis%21USD%210.97%210.97%21%21%210.97%210.97%21%402101c5b117256490502064231e3596%2166348607719%21sea%21US%216045824045%21X&curPageLogUid=FY31OUa9dGcH&utparam-url=scene%3Asearch%7Cquery_from%3A) (use whatever you want here)
3. **MicroSD Card Reader (SPI)**: For storing logs and loading custom scripts for automation. (Built into the Dstike D-Duino 32 Dual II)

### **Power:**
1. **3.7v Lipo Battery**: Provides the device with power. Make sure to research LiPo batteries before you start playing with them. [click here](https://www.amazon.com/dp/B08T6QS58J?ref=nb_sb_ss_w_as-reorder_k2_1_7&amp=&crid=1QM1M5728G1J9&amp=&sprefix=3.7v+ba)
2. **Adafruit Powerboost 1000c**: Load-sharing charger circuit that allows charging of batteries and power supply to the device. [click here](https://www.adafruit.com/product/2465)

### **Storage:**
1. 32GB MicroSD card: Stores logs, programs and data. 32GB is the largest supported partition on ESP32s. [click here](https://www.amazon.com/SanDisk-Ultra-UHS-I-Memory-Adapter/dp/B00M55C0NS/ref=sr_1_7?crid=IN8E9C5QACD8&dib=eyJ2IjoiMSJ9.B95zlcmYonfz4dY7353CikOTGq4gxvPdpZ9JTVXWNlwkRO7PGJOQbtTqrKfmuztrDqjv9dmItno_1id_mIwg75OkS0ERdIgQVOD7iDgSCJ7LWdwiEXjhNEu5JqyRj7bHo-L92ieL9e2flMOaB00h4MkDH_MgWAcNq9DQQcFh545Z0qihgtOkcxZQEtsdmVfzW4eYJQvfecmWeloKFozWE4zMlm9JVMMgD8W_f35QteY.EAqy466m6iTcsbn8g_uaxNl6Q1PJGHPRw3mz1jb8eBg&dib_tag=se&keywords=microsd+32gb&qid=1725649398&sprefix=microsd+32gb%2Caps%2C88&sr=8-7)

## **Planned Features**
1. **Custom Menu System:**
   - A simple, user-friendly menu displayed on the **OLED screens**, navigated using the **5-way button**. The menu will allow users to select Wi-Fi testing, RF testing, NFC/RFID functions, IR control, and view logs or stored scripts.

2. **Modular Functionality:**
   - The device will support different modes for each type of penetration testing (Wi-Fi, RF, NFC, IR), with each module operating independently, allowing for multi-tasking and seamless switching between functions.

3. **Logs and Scripts:**
   - The **MicroSD card** will allow storing the results of tests, as well as loading pre-written attack scripts, enabling users to automate penetration testing tasks.
  
4. **Modularity:**
   - This repository will hopefully host a myriad of **3D printable modules and accesories** for the NETH3T.
   - Ability to plug and play **I2C** and **SPI** modules to customize your device.

## **Getting Started**
### **Prerequisites**
- **ESP32 Arduino Core**: Ensure you have the ESP32 libraries installed in your development environment.
- **SPI and I2C Libraries**: Required to interface with the various modules.
- **PN5180, CC1101, and NRF24L01+ libraries**: To control the NFC/RFID, Sub-GHz RF, and 2.4 GHz RF modules.

### **Hardware Setup**
1. **Wiring the Modules**:
   - Connect the **PN5180, CC1101, and NRF24L01+** modules to the **ESP32** via **SPI**.
   - Connect the **OLED screens** via **I2C**.
   - Wire the **5-way navigation button** to the designated GPIO pins.
   - Attach the **IR LED** and **IR Receiver** for IR signal control.

  ## **Pinout Table**

| Component                         | GPIO Pins Assigned                | Communication Protocol   |
|------------------------------------|-----------------------------------|--------------------------|
| **PN5180 NFC/RFID Module**         | GPIO23 (MOSI), GPIO19 (MISO),     | SPI                      |
|                                    | GPIO18 (SCK), GPIO5 (CS),         |                          |
|                                    | GPIO16 (IRQ), GPIO17 (RST)        |                          |
| **CC1101 RF Module (Sub-GHz)**     | GPIO23 (MOSI), GPIO19 (MISO),     | SPI                      |
|                                    | GPIO18 (SCK), GPIO25 (CS)         |                          |
| **NRF24L01+ RF Module (2.4 GHz)**  | GPIO23 (MOSI), GPIO19 (MISO),     | SPI                      |
|                                    | GPIO18 (SCK), GPIO26 (CS)         |                          |
| **IR LED (Transmitter)**           | GPIO15                            | Digital Output            |
| **IR Receiver (TSOP38238)**        | GPIO17                            | Digital Input             |
| **Dual OLED Displays**             | GPIO21 (SDA), GPIO22 (SCL)        | I2C                       |
| **MicroSD Card Reader**            | GPIO23 (MOSI), GPIO19 (MISO),     | SPI                      |
|                                    | GPIO18 (SCK), GPIO4 (CS)          |                          |
| **5-Way Navigation Button**        | GPIO32 (Up), GPIO33 (Down),       | Digital Input             |
|                                    | GPIO34 (Left), GPIO35 (Right),    |                          |
|                                    | GPIO36 (Center/Select)            |                          |



### **Building and Flashing the Firmware**
Once the hardware is wired and the software dependencies are installed, you can build and flash the firmware to the device.

## **Contributing**
Contributions to this project are welcome. If you have ideas for new features, improvements, or additional modules, feel free to create an issue or submit a pull request.

## **License**
This project is licensed under the **GNU General Public License v3.0**. See the [LICENSE](LICENSE) file for more information.
