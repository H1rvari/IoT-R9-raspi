#include <ArduinoBLE.h>

// BLE IDENTIFIERS
#define SERVICE_UUID    "19b10000-e8f2-537e-4f6c-d104768a1214"
#define ALARM_CHAR_UUID "19b10002-e8f2-537e-4f6c-d104768a1214"  // Pi writes state here
#define ACK_CHAR_UUID   "19b10001-e8f2-537e-4f6c-d104768a1214"  // Pi uses notify() to subscribe
#define DEVICE_NAME     "BurglaryRemote"

#define ACK_BYTE_REQUEST 255

// Pins
#define LED_RED    2
#define LED_GREEN  3
#define LED_BLUE   4
#define BUTTON_PIN 5

// Timing
#define DEBOUNCE_DELAY  50
#define LONG_PRESS_MS   5000
#define HOLD_FEEDBACK_1 1500
#define HOLD_FEEDBACK_2 3000
#define HOLD_FEEDBACK_3 4000

// BLE
// alarmStateChar: Pi writes 3 bytes [active, armed, alarm] to this
//   -> needs BLEWrite only (Pi calls write_command)
// ackChar: Pi calls indicate() on this to subscribe, Arduino writes 0xFF to trigger Pi
//   -> needs BLEIndicate (NOT BLENotify — Pi uses indicate(), not notify())
BLEService            remoteService(SERVICE_UUID);
BLECharacteristic     alarmStateChar(ALARM_CHAR_UUID, BLEWriteWithoutResponse, 3);
BLEByteCharacteristic ackChar(ACK_CHAR_UUID, BLERead | BLENotify);

// System state
bool isConnected     = false;
bool isArmed         = false;
bool isAlarm         = false;
bool isSensorOffline = false;

// LED blink timers
unsigned long lastBlueToggle  = 0;
bool          blueState       = false;
unsigned long lastRedToggle   = 0;
bool          redState        = false;
unsigned long lastGreenToggle = 0;
bool          greenState      = false;
bool          holdGreenActive = false;

// Button state
bool          lastButtonReading  = HIGH;
unsigned long buttonPressTime    = 0;
bool          buttonIsHeld       = false;
bool          longPressTriggered = false;

// Helpers
void setLED(int pin, bool on) {
  digitalWrite(pin, on ? HIGH : LOW);
}

void applyLEDs() {
  if (!holdGreenActive) {
    setLED(LED_GREEN, isArmed);
  }
  setLED(LED_BLUE, isConnected);
  if (!isAlarm && !isSensorOffline) {
    setLED(LED_RED, false);
  }
}

void resetRedBlink() {
  lastRedToggle = 0;
  redState      = false;
  setLED(LED_RED, false);
}

void requestStateFromPi() {
  holdGreenActive = false;

  if (!isConnected) {
    Serial.println("ERROR: Cannot request state. No Pi connected.");
    applyLEDs();
    return;
  }

  ackChar.writeValue((byte)ACK_BYTE_REQUEST);
  Serial.println("5s hold: sent 0xFF to Pi via indicate, waiting for state update.");
  applyLEDs();
}

void setup() {
  Serial.begin(9600);
  while (!Serial);  

  pinMode(LED_RED,    OUTPUT);
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_BLUE,   OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  setLED(LED_GREEN, false);
  setLED(LED_BLUE,  false);
  setLED(LED_RED,   false);

  if (!BLE.begin()) {
    Serial.println("BLE failed to start!");
    while (true) {
      setLED(LED_RED, true);  delay(100);
      setLED(LED_RED, false); delay(100);
    }
  }

  BLE.setDeviceName(DEVICE_NAME);
  remoteService.addCharacteristic(alarmStateChar);  
  remoteService.addCharacteristic(ackChar);
  BLE.addService(remoteService);                    
  BLE.setAdvertisedService(remoteService);          

  byte initState[3] = {0, 0, 0};
  alarmStateChar.writeValue(initState, 3);
  ackChar.writeValue((byte)0);

  BLE.advertise();
  Serial.println("BurglaryRemote advertising...");
}

BLEDevice central;  

void loop() {
  unsigned long now = millis();

  if (!central) {
    central = BLE.central();
  }

  bool connected = central && central.connected();
  if (connected != isConnected) {
    isConnected     = connected;
    isAlarm         = false;
    isSensorOffline = false;
    resetRedBlink();
    if (isConnected) {
      Serial.print("Pi connected: ");
      Serial.println(central.address());
    } else {
      Serial.println("Pi disconnected. Advertising again.");
      central = BLEDevice();  // reset so we accept next connection
      BLE.advertise();        // restart advertising
    }
    applyLEDs();
  }

  // Handle writes from Pi: 3 bytes [active, armed, alarm]
  if (isConnected && alarmStateChar.written()) {
    const uint8_t* buf = alarmStateChar.value();
    byte piActive = buf[0];
    byte piArmed  = buf[1];
    byte piAlarm  = buf[2];

    Serial.print("Pi wrote [active="); Serial.print(piActive);
    Serial.print(", armed=");          Serial.print(piArmed);
    Serial.print(", alarm=");          Serial.print(piAlarm);
    Serial.println("]");

    bool newArmed         = (piArmed  == 1);
    bool newAlarm         = (piAlarm  == 1);
    bool newSensorOffline = (piActive == 0);

    bool greenChanged = (newArmed != isArmed);
    bool redChanged   = (newAlarm != isAlarm) || (newSensorOffline != isSensorOffline);

    isArmed         = newArmed;
    isAlarm         = newAlarm;
    isSensorOffline = newSensorOffline;

    if (redChanged)   resetRedBlink();
    if (greenChanged) Serial.println(isArmed ? "State: ARMED" : "State: DISARMED");
    applyLEDs();
  }

  // Button handling
  bool reading = digitalRead(BUTTON_PIN);

  if (reading == LOW) {
    if (lastButtonReading == HIGH) {
      buttonPressTime    = now;
      buttonIsHeld       = true;
      longPressTriggered = false;
      holdGreenActive    = false;
      lastGreenToggle    = now;
      greenState         = true;
      setLED(LED_RED, true); setLED(LED_GREEN, true); setLED(LED_BLUE, true);
      delay(80);
      applyLEDs();
    }

    if (buttonIsHeld && !longPressTriggered) {
      unsigned long heldFor = now - buttonPressTime;

      if (heldFor >= HOLD_FEEDBACK_1) {
        holdGreenActive = true;
        unsigned long interval = (heldFor >= HOLD_FEEDBACK_3) ? 100
                               : (heldFor >= HOLD_FEEDBACK_2) ? 200 : 400;
        if (now - lastGreenToggle >= interval) {
          greenState = !greenState;
          setLED(LED_GREEN, greenState);
          lastGreenToggle = now;
        }
      }

      if (heldFor >= LONG_PRESS_MS) {
        longPressTriggered = true;
        requestStateFromPi();
      }
    }

  } else {
    if (lastButtonReading == LOW && !longPressTriggered) {
      holdGreenActive = false;
      setLED(LED_GREEN, isArmed);
    }
    buttonIsHeld = false;
  }

  lastButtonReading = reading;
  updateLEDs();
}

void updateLEDs() {
  unsigned long now = millis();

  // BLUE — blink when disconnected, solid when connected
  if (!isConnected) {
    if (now - lastBlueToggle >= 500) {
      blueState = !blueState;
      setLED(LED_BLUE, blueState);
      lastBlueToggle = now;
    }
  }

  // RED — fast blink = alarm (200ms), slow blink = sensor offline (1000ms)
  if (isAlarm) {
    if (now - lastRedToggle >= 200) {
      redState = !redState;
      setLED(LED_RED, redState);
      lastRedToggle = now;
    }
  } else if (isSensorOffline) {
    if (now - lastRedToggle >= 1000) {
      redState = !redState;
      setLED(LED_RED, redState);
      lastRedToggle = now;
    }
  }
}
