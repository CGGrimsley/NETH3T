# **NETH3T Penetration Testing Device**

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
- **DSTIKE D-duino 32 (ESP32-based)**: The core of the device, handling Wi-Fi, Bluetooth, and control of other connected modules.

### **Modules:**
1. **PN5180 (NFC/RFID Reader/Writer)**:
   - Used for scanning, cloning, and emulating NFC/RFID tags.
2. **CC1101 RF Module (Sub-GHz)**:
   - For RF signal capturing, replaying, and jamming in Sub-GHz frequencies.
3. **NRF24L01+ Module (2.4 GHz RF)**:
   - Captures and manipulates 2.4 GHz proprietary RF communication protocols.
   
### **IR Components:**
1. **IR LED (Transmitter)**: Used to transmit recorded or custom IR signals.
2. **TSOP38238 (IR Receiver)**: Captures incoming IR signals from remote controls.

### **Peripherals:**
1. **Dual OLED Displays (I2C)**: Provides real-time feedback and a menu system for navigation.
2. **5-Way Navigation Button**: Allows easy navigation of the device's menu and options.
3. **MicroSD Card Reader (SPI)**: For storing logs and loading custom scripts for automation.

## **Planned Features**
1. **Custom Menu System:**
   - A simple, user-friendly menu displayed on the **OLED screens**, navigated using the **5-way button**. The menu will allow users to select Wi-Fi testing, RF testing, NFC/RFID functions, IR control, and view logs or stored scripts.

2. **Modular Functionality:**
   - The device will support different modes for each type of penetration testing (Wi-Fi, RF, NFC, IR), with each module operating independently, allowing for multi-tasking and seamless switching between functions.

3. **Logs and Scripts:**
   - The **MicroSD card** will allow storing the results of tests, as well as loading pre-written attack scripts, enabling users to automate penetration testing tasks.

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

### **Building and Flashing the Firmware**
Once the hardware is wired and the software dependencies are installed, you can build and flash the firmware to the device.

## **Contributing**
Contributions to this project are welcome. If you have ideas for new features, improvements, or additional modules, feel free to create an issue or submit a pull request.

## **License**
This project is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for more information.
