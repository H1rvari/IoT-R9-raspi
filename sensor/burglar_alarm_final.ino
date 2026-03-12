#include <Arduino_LSM9DS1.h>
#include <Arduino_APDS9960.h>
#include <ArduinoBLE.h>

#define LED_RED 6
#define LED_GREEN 5
#define LED_BLUE 3
#define BUILTIN_LED LED_BUILTIN

// BLE service + characteristic UUIDs
#define SERVICE_UUID      "12345678-1234-5678-1234-56789abcdef0"
#define MOTION_CHAR_UUID  "abcdef01-1234-5678-1234-56789abcdef0"

// Motion state bytes
#define STATE_BYTE__ALL_CLEAR 0x00
#define STATE_BYTE_ALARM      0xFF

// Motion detection threshold
float x, y, z;
float accelerationMagnitude = 0;
const float threshold = 1.5; // trigger above 1.5g

BLEService motionService(SERVICE_UUID);
BLEByteCharacteristic motionChar(MOTION_CHAR_UUID, BLERead | BLENotify);

// Timing
unsigned long lastMotionTime = 0;
const unsigned long interval = 5000; // 5 seconds for testing "all good"
const unsigned long alarm_interval = 2000;

void setup() {
  Serial.begin(9600);
  //while (!Serial);

  // LED setup
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);

  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(BUILTIN_LED, HIGH);

  // IMU init (acceleration)
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  // Gesture sensor init
  if (!APDS.begin()) {
    Serial.println("Failed to initialize APDS9960!");
  }

  // BLE init
  if (!BLE.begin()) {
    Serial.println("BLE startup failed!");
    while (1);
  }

  BLE.setLocalName("NanoMotion");
  BLE.setDeviceName("NanoMotion");


  BLE.setAdvertisedService(motionService);
  motionService.addCharacteristic(motionChar);
  
  BLE.addService(motionService);
  
  motionChar.writeValue(STATE_BYTE__ALL_CLEAR);
  BLE.advertise();

  Serial.println("BLE device active");
}

void loop() {
  BLE.poll();
  BLEDevice central = BLE.central();
  if (central) {

    Serial.print("Connected to central: ");

    Serial.println(central.address());

    while (central.connected()) {
    BLE.poll();
      // Gesture detection
      if (APDS.gestureAvailable()) {

        int gesture = APDS.readGesture();

        if ((gesture == GESTURE_UP || gesture == GESTURE_DOWN || 
            gesture == GESTURE_LEFT || gesture == GESTURE_RIGHT)
            && millis() - lastMotionTime >= alarm_interval) {

            Serial.println("Gesture detected!");
            lastMotionTime = millis();

            motionChar.writeValue(STATE_BYTE_ALARM);
        }
      }

      // IMU motion detection (acceleration)
      if (IMU.accelerationAvailable()) {

        IMU.readAcceleration(x, y, z);

        accelerationMagnitude = sqrt(x * x + y * y + z * z);

        if (accelerationMagnitude > threshold
        && millis() - lastMotionTime >= alarm_interval) {

          Serial.println("Motion detected!");

          digitalWrite(LED_RED, HIGH);

          lastMotionTime = millis();

          motionChar.writeValue(STATE_BYTE_ALARM);
        }

        // Send "all clear" after timeout
        if (millis() - lastMotionTime >= interval) {

          motionChar.writeValue(STATE_BYTE__ALL_CLEAR);

          lastMotionTime = millis();
          Serial.println("All clear!");
        }
      }

      // Short loop delay for stability
      delay(1);
    }

    Serial.println("Central disconnected");
  }
  BLE.advertise();
}