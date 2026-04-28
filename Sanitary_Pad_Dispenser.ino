/*  Combined BLE + Fingerprint + Stepper(IR) sketch
    - Default: fingerprint CHECK mode (LED on match) + BLE advertising
    - Enrollment starts ONLY when "StartScan_userid_" is received from app
    - Timeout durations kept as in your original enrollment routine
    - Locker relay = onboard LED blink
    - Dispenser = Stepper + IR sensor (napkin detect or 30s timeout)
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <Stepper.h>

// -------- CONFIG --------
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-cba987654321"

#define FINGERPRINT_RX_PIN 16
#define FINGERPRINT_TX_PIN 17
#define LOCKER_RELAY_PIN 27           // (Locker relay simulation)
#define MOTOR_RELAY_PIN 12           // rELAY PIN FOR STEPPER MOTOR
// Stepper motor config
const int stepsPerRevolution = 2048;  // 28BYJ-48 with gearbox
#define IN1 5
#define IN2 18
#define IN3 19
#define IN4 21
#define IR_SENSOR 15  

Stepper myStepper(stepsPerRevolution, IN1, IN3, IN2, IN4);

// -------- GLOBALS --------
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;

bool deviceConnected = false;
bool oldDeviceConnected = false;

HardwareSerial mySerial(2);                
Adafruit_Fingerprint finger(&mySerial);

volatile bool enrollmentRequested = false;
volatile bool enrollmentInProgress = false;
bool sensorReady = false;

int user_id = -1; // global extracted user ID

// -------- Helpers (BLE/fingerprint) --------
void sendToApp(const String &msg) {
  if (deviceConnected && pCharacteristic) {
    pCharacteristic->setValue(msg.c_str());
    pCharacteristic->notify();
    Serial.print("<<< Sent to App: ");
    Serial.println(msg);
    delay(80);
  } else {
    Serial.println(">>> WARN: Can't send to app (no client connected)");
  }
}

bool checkFingerprintSensor() {
  Serial.println(">>> Checking fingerprint sensor...");
  if (!mySerial) {
    Serial.println(">>> ERROR: Serial not started for fingerprint");
    return false;
  }
  if (finger.verifyPassword()) {
    Serial.println(">>> Sensor verified OK");
    return true;
  } else {
    Serial.println(">>> Sensor verify FAILED");
    return false;
  }
}

uint8_t waitForImageWithTimeout(unsigned long timeout_ms) {
  unsigned long start = millis();
  uint8_t p;
  while (millis() - start < timeout_ms) {
    p = finger.getImage();
    if (p == FINGERPRINT_OK) return FINGERPRINT_OK;
    if (p == FINGERPRINT_NOFINGER) {
      delay(40);
      continue;
    }
    if (p == FINGERPRINT_PACKETRECIEVEERR) {
      Serial.println(">>> Communication error while waiting for image");
      delay(100);
    } else if (p == FINGERPRINT_IMAGEFAIL) {
      Serial.println(">>> Image capture failure");
      delay(100);
    } else {
      Serial.print(">>> waitForImage unknown status: ");
      Serial.println(p);
      delay(100);
    }
  }
  return FINGERPRINT_TIMEOUT;
}

// -------- Stepper + IR helpers --------
void releaseMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void runDispenser() {
  digitalWrite(MOTOR_RELAY_PIN, LOW);
  unsigned long startTime = millis();
  Serial.println(">>> Dispenser motor started... waiting for napkin or timeout.");
  while (millis() - startTime < 30000UL) { // 30s timeout
    myStepper.step(1);  // rotate clockwise step by step
    
    if (digitalRead(IR_SENSOR) == LOW) {  
      Serial.println("✅ Napkin detected! Stopping motor...");
      digitalWrite(MOTOR_RELAY_PIN, HIGH);
      releaseMotor();
      return;
    }
  }

  Serial.println("⏱ Timeout reached. Stopping motor...");
  digitalWrite(MOTOR_RELAY_PIN, HIGH);
  releaseMotor();
}

void triggerLockerRelay() {
  Serial.println(">>> Triggering Locker Relay (LED blink 2s)");
  digitalWrite(LOCKER_RELAY_PIN, LOW);
  delay(3000);
  digitalWrite(LOCKER_RELAY_PIN, HIGH);
}

// -------- BLE Server Callbacks --------
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("=== BLE DEVICE CONNECTED ===");
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    enrollmentInProgress = false;
    enrollmentRequested = false;
    Serial.println("=== BLE DEVICE DISCONNECTED ===");
  }
};

// -------- BLE Characteristic Callbacks --------
class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    // Some versions of the BLE library return std::string — using Arduino String works with both
    String rxStd = pCharacteristic->getValue();
    if (rxStd.length() == 0) return;

    String value = rxStd;
    value.trim();
    while (value.length() && (value.endsWith("\n") || value.endsWith("\r"))) {
      value = value.substring(0, value.length() - 1);
    }

    Serial.println("--------------------------------");
    Serial.print(">>> Received from App: '");
    Serial.print(value);
    Serial.println("'");
    Serial.println("--------------------------------");

    // Check if message starts with "StartScan_"
    if (value.startsWith("StartScan_")) {
      // Find first and last underscores
      int firstUnderscore = value.indexOf('_');
      int secondUnderscore = value.indexOf('_', firstUnderscore + 1);

      if (firstUnderscore != -1 && secondUnderscore != -1) {
        String idStr = value.substring(firstUnderscore + 1, secondUnderscore);
        user_id = idStr.toInt();  // Convert to int

        Serial.print(">>> Extracted User ID: ");
        Serial.println(user_id);

        if (!enrollmentInProgress) {
          Serial.println(">>> StartScan command received -> enrollment requested");
          enrollmentRequested = true;
        } else {
          Serial.println(">>> Enrollment already in progress, ignoring StartScan");
        }
      } else {
        Serial.println(">>> Invalid format (missing underscores)");
      }
    } else {
      Serial.println(">>> Unknown BLE command (ignored)");
    }
  }
};

// -------- Enrollment routine (UNCHANGED from your original) --------
void enrollFingerprint(uint8_t id) {
  if (enrollmentInProgress) return;
  enrollmentInProgress = true;
  enrollmentRequested = false;

  Serial.println("========================================");
  Serial.print(  ">>> ENROLLMENT START (ID=");
  Serial.print(id);
  Serial.println(") <<<");
  Serial.println("========================================");

  int p = -1;
  sendToApp("Status_OK");

  // STEP 1: first scan
  Serial.println(">>> STEP 1: Waiting for first finger (30s timeout) ...");
  p = waitForImageWithTimeout(30000);
  if (p != FINGERPRINT_OK) {
    Serial.println(">>> ERROR: First scan timeout or error");
    enrollmentInProgress = false;
    return; // ⚡ No Error_FirstScan sent to app
  }
  Serial.println(">>> ✓ First image captured");
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.print(">>> ERROR: image2Tz(1) failed, code ");
    Serial.println(p);
    sendToApp("Error_Convert1");
    enrollmentInProgress = false;
    return;
  }
  Serial.println(">>> ✓ First image converted");

  // STEP 2: second scan
  sendToApp("StartSecondScan");
  Serial.println(">>> STEP 2: Remove finger then place same finger again");

  unsigned long startRemove = millis();
  while (millis() - startRemove < 10000) {
    if (finger.getImage() == FINGERPRINT_NOFINGER) break;
    delay(80);
  }
  Serial.println(">>> (assume finger removed)");

  p = waitForImageWithTimeout(30000);
  if (p != FINGERPRINT_OK) {
    Serial.println(">>> ERROR: Second scan timeout or error");
    sendToApp("Error_SecondScan");
    enrollmentInProgress = false;
    return;
  }
  Serial.println(">>> ✓ Second image captured");
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.print(">>> ERROR: image2Tz(2) failed, code ");
    Serial.println(p);
    sendToApp("Error_Convert2");
    enrollmentInProgress = false;
    return;
  }
  Serial.println(">>> ✓ Second image converted");

  // STEP 3: create model
  Serial.println(">>> STEP 3: Creating model...");
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.print(">>> ERROR: createModel failed, code ");
    Serial.println(p);
    sendToApp("Error_CreateModel");
    enrollmentInProgress = false;
    return;
  }
  Serial.println(">>> ✓ Model created");

  // STEP 4: store model
  Serial.print(">>> Storing model at ID ");
  Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("========================================");
    Serial.println(">>> ✅ FINGERPRINT ENROLLMENT SUCCESS! <<<");
    Serial.println("========================================");
    sendToApp("Completed");
  } else {
    Serial.print(">>> ERROR: storeModel failed, code ");
    Serial.println(p);
    sendToApp("Error_StoreModel");
    enrollmentInProgress = false;
    return;
  }

  enrollmentInProgress = false;
  Serial.println(">>> Enrollment finished, waiting 5s then disconnecting BLE (if connected) ...");
  delay(5000);

  if (deviceConnected && pServer) {
    Serial.println(">>> Attempting to disconnect BLE client...");
    pServer->disconnect(0);
    delay(200);
  } else {
    Serial.println(">>> No BLE client connected (or pServer null).");
  }

  Serial.println(">>> Returning to CHECK mode");
}

// -------- Fingerprint check mode (unchanged logic + integrated actions) --------
void fingerprintCheckLoop() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) {
    if (p == FINGERPRINT_NOFINGER) return;
    if (p == FINGERPRINT_PACKETRECIEVEERR) Serial.println(">>> Fingerprint comm error");
    else if (p == FINGERPRINT_IMAGEFAIL) Serial.println(">>> Image capture fail");
    return;
  }

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    Serial.print(">>> image2Tz failed: "); Serial.println(p);
    return;
  }

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.print(">>> MATCHED ID #");
    Serial.print(finger.fingerID);
    Serial.print("  confidence=");
    Serial.println(finger.confidence);

    if(finger.fingerID == 1 || finger.fingerID == 2 || finger.fingerID == 3 || finger.fingerID == 4 || finger.fingerID == 5 || finger.fingerID == 6 || finger.fingerID == 7 || finger.fingerID == 8 || finger.fingerID == 9|| finger.fingerID == 10){
      Serial.println("Trigger Locker Relay");
      triggerLockerRelay();
    }
    else{
      Serial.println("Trigger Dispenser Relay");
      runDispenser();
    }
  } else if (p == FINGERPRINT_NOTFOUND) {
    // no match
  } else {
    Serial.print(">>> Search error code: "); Serial.println(p);
  }
}

// -------- SETUP --------
void setup() {
  pinMode(LOCKER_RELAY_PIN, OUTPUT);
  pinMode(MOTOR_RELAY_PIN, OUTPUT);
  digitalWrite(LOCKER_RELAY_PIN, HIGH);
  digitalWrite(MOTOR_RELAY_PIN, HIGH);

  // Stepper pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(IR_SENSOR, INPUT);
  myStepper.setSpeed(10);   // Adjust RPM for smooth motion

  Serial.begin(115200);
  delay(300);

  Serial.println("========================================");
  Serial.println(">>> ESP32 BLE + Fingerprint + Stepper <<<");
  Serial.println("========================================");

  mySerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX_PIN, FINGERPRINT_TX_PIN);
  delay(100);
  finger.begin(57600);
  delay(200);
  sensorReady = checkFingerprintSensor();
  if (!sensorReady) {
    Serial.println(">>> WARNING: Fingerprint sensor not verified. Enrollment may fail.");
  }

  BLEDevice::init("Sanitary Pad Dispenser");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println(">>> BLE advertising started. SYSTEM READY (default = CHECK mode).");
  Serial.println("========================================");
}

// -------- MAIN LOOP --------
void loop() {
  if (!deviceConnected && oldDeviceConnected) {
    Serial.println(">>> Client disconnected — restarting advertising");
    BLEDevice::startAdvertising();
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    Serial.println(">>> Client connected.");
    oldDeviceConnected = deviceConnected;
  }

  if (enrollmentRequested && !enrollmentInProgress) {
    if (!sensorReady) sensorReady = checkFingerprintSensor();
    if (!sensorReady) {
      Serial.println(">>> ERROR: sensor not ready - aborting enrollment request");
      enrollmentRequested = false;
    } else {
      enrollFingerprint(user_id);
    }
  }

  if (!enrollmentInProgress) {
    fingerprintCheckLoop();
  }

  delay(40);
}
         