from weasyprint import HTML
An ESP32-based automated sanitary pad dispenser featuring **Biometric Authentication (Fingerprint)** and **Mobile Integration (BLE)**. This system allows for secure user enrollment via a smartphone app and automated dispensing using a stepper motor and IR sensor feedback.

## 🚀 Key Features
- **Dual-Mode Operation**:
    - **Check Mode (Default)**: Authenticates saved fingerprints to trigger the dispenser or locker.
    - **Enrollment Mode**: Triggered via BLE command (`StartScan_userid_`) to register new users.
- **Biometric Security**: Integrated with Adafruit Fingerprint sensor library for high-accuracy matching.
- **Precision Dispensing**: Uses a 28BYJ-48 Stepper motor for controlled movement and an IR sensor for real-time napkin detection.
- **BLE Connectivity**: Custom Service and Characteristic UUIDs for secure communication with a mobile application.
- **Safety Timeouts**: Built-in 30-second failsafes for both fingerprint scanning and motor rotation to prevent hardware strain.

---

## 🏗 System Architecture


The system transitions between modes based on BLE interrupts. When a user is authenticated, the system distinguishes between administrative IDs (1-10) and standard users to trigger different hardware responses.

---

## 🛠 Hardware Configuration

### Components
- **Microcontroller**: ESP32
- **Biometric**: Adafruit Optical Fingerprint Sensor (R307/AS608)
- **Actuator**: 28BYJ-48 Stepper Motor with ULN2003 Driver
- **Sensor**: IR Obstacle Avoidance Sensor (for napkin detection)
- **Indicators**: Onboard LED (Locker Simulation) and Relays

### Pin Mapping (ESP32)
| Component | Function | ESP32 Pin |
| :--- | :--- | :--- |
| **Fingerprint Sensor** | RX | GPIO 16 |
| **Fingerprint Sensor** | TX | GPIO 17 |
| **Stepper Motor** | IN1 | GPIO 5 |
| **Stepper Motor** | IN2 | GPIO 18 |
| **Stepper Motor** | IN3 | GPIO 19 |
| **Stepper Motor** | IN4 | GPIO 21 |
| **IR Sensor** | Data | GPIO 15 |
| **Motor Relay** | Trigger | GPIO 12 |
| **Locker Relay** | Trigger | GPIO 27 |

---

## 💻 Software Logic

### 1. Enrollment Process
Enrollment is only accessible when the device is connected via BLE.
1. The App sends a string: `StartScan_[USER_ID]_`.
2. The system extracts the ID and enters `enrollFingerprint()`.
3. **Scan 1**: Waits 30s for a finger. Converts to template.
4. **Scan 2**: Waits for finger removal, then re-scans to verify.
5. **Storage**: Creates a model and stores it at the specified ID index.

### 2. Dispensing Logic (`runDispenser`)
When a valid fingerprint (ID > 10) is detected:
- The motor relay is triggered.
- The stepper motor rotates clockwise.
- The system monitors the **IR Sensor** (GPIO 15).
- If a napkin is detected (`LOW`), the motor stops immediately.
- **Failsafe**: If no napkin is detected within 30 seconds, the motor stops to prevent overheating.

---

## 📶 BLE Specifications
- **Service UUID**: `12345678-1234-1234-1234-123456789abc`
- **Characteristic UUID**: `87654321-4321-4321-4321-cba987654321`
- **Commands**:
    - `StartScan_ID_`: Initiates registration.
- **Notifications**:
    - `Status_OK`: Ready for first scan.
    - `StartSecondScan`: Prompt user for second scan.
    - `Completed`: Successful enrollment.
    - `Error_[Type]`: Various error codes (Timeout, Conversion, Storage).

---

## 🔧 Installation & Setup
1. Install the following libraries in Arduino IDE:
   - `Adafruit Fingerprint Sensor Library`
   - `Stepper` (Built-in)
   - `BLE Device` (ESP32 Board Package)
2. Connect the hardware as per the pin mapping.
3. Update the `SERVICE_UUID` and `CHARACTERISTIC_UUID` if necessary.
4. Flash the code to the ESP32.

---

## ⚠️ Important Notes
- **Power**: The 28BYJ-48 motor requires an external 5V-12V power supply; do not power it directly from the ESP32 pins.
- **Baud Rate**: The fingerprint sensor is configured for `57600`. Ensure your sensor hardware matches this setting.
- **Security**: IDs 1 through 10 are hardcoded to trigger the **Locker Relay**, while higher IDs trigger the **Dispenser**.

