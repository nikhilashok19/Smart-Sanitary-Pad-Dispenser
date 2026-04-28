# Define the ASCII circuit diagram
ascii_diagram = """
           +---------------------------+
           |           ESP32           |
           |                           |
           |  [TX2]  17 >--------[RX]  |   +-----------------------+
           |  [RX2]  16 <--------[TX]  |---| Fingerprint Sensor    |
           |            GND <----[GND] |   | (R307/AS608)          |
           |           3.3V >----[VCC] |   +-----------------------+
           |                           |
           |  [GPIO] 15 <--------[OUT] |   +-----------------------+
           |            GND <----[GND] |---| IR Obstacle Sensor    |
           |           3.3V >----[VCC] |   +-----------------------+
           |                           |
           |  [GPIO]  5 >--------[IN1] |   +-----------------------+
           |  [GPIO] 18 >--------[IN2] |---| ULN2003 Driver        |
           |  [GPIO] 19 >--------[IN3] |   | (to 28BYJ-48 Stepper) |
           |  [GPIO] 21 >--------[IN4] |   +-----------------------+
           |                           |
           |  [GPIO] 12 >--------[SIG] |   +-----------------------+
           |            GND <----[GND] |---| Motor Relay           |
           |             5V >----[VCC] |   +-----------------------+
           |                           |
           |  [GPIO] 27 >--------[SIG] |   +-----------------------+
           |            GND <----[GND] |---| Locker Relay          |
           |             5V >----[VCC] |   +-----------------------+
           +---------------------------+
"""

# Define the full README content
readme_content = f"""# Sanitary Pad Dispenser (IoT & Biometric)

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

## 🔌 Circuit Diagram (ASCII)

The following diagram represents the connections defined in the source code:
